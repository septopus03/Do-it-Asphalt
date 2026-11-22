// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Things the player can buy. Order is binding: the tables in LotBalance are indexed by it. */
enum class ELotItem : uint8 { Scoop, Bucket, Suitcase, Cart, Station, AutoPaver, AutoPaverFast, AutoPaverFaster, Car, CarWideBar, Count };

/** Unlock steps; step i belongs to entry segment i of the progression and opens after LotBalance::UnlockCells[i] cells. */
enum class ELotUnlock : uint8 { QuickSwap, Sprint, StripWork, AutoPaver, Car, Exit, Count };

/** What the player holds in the hand. */
enum class ELotHand : uint8 { Bowl, Scoop, Bucket, Count };

/** Equipment the player can carry or push around the lot. */
enum class ELotMovable : uint8 { Suitcase, Cart, Station, Count };

/** Where asphalt is drawn from. */
enum class ELotSource : uint8 { Plant, Suitcase, Cart };

/** All balance numbers of the game. Units: centimetres, litres, seconds. */
namespace LotBalance
{
	// ---- Fixed by the specification ----

	/** Litres of asphalt that one cell takes. */
	inline constexpr double LitresPerCell = 10.0;
	/** Credits earned for each completed cell. */
	inline constexpr int32 CreditsPerCell = 10;
	/** Credits earned for each litre of asphalt poured (paid when the litre leaves the vessel, on top of CreditsPerCell). */
	inline constexpr int32 CreditsPerLitre = 1;
	/** A cell counts as complete once this fraction of its area is covered. */
	inline constexpr double CompletionThreshold = 0.95;

	/** Litres in one spoon of the bowl. */
	inline constexpr double SpoonLitres = 0.25;
	/** Litres in one scoop. */
	inline constexpr double ScoopLitres = 1.0;
	/** Litres the hand holds, indexed by ELotHand. */
	inline constexpr double HandCapacityL[3] = {2.5, 1.0, 10.0};

	/** Price in credits, indexed by ELotItem. */
	inline constexpr int32 ItemPrice[10] = {10, 35, 110, 340, 900, 3000, 9000, 22000, 12000, 24000};
	/** Unlock step an item needs, indexed by ELotItem; ELotUnlock::Count means no condition. */
	inline constexpr ELotUnlock ItemUnlock[10] =
	{
		ELotUnlock::Count, ELotUnlock::Count, ELotUnlock::Count, ELotUnlock::Count,
		ELotUnlock::StripWork,
		ELotUnlock::AutoPaver, ELotUnlock::AutoPaver, ELotUnlock::AutoPaver,
		ELotUnlock::Car, ELotUnlock::Car
	};
	/** Item that must be owned first, indexed by ELotItem; ELotItem::Count means nothing. */
	inline constexpr ELotItem ItemRequires[10] =
	{
		ELotItem::Count, ELotItem::Count, ELotItem::Count, ELotItem::Count,
		ELotItem::Count,
		ELotItem::Count, ELotItem::AutoPaver, ELotItem::AutoPaverFast,
		ELotItem::Count, ELotItem::Car
	};
	static_assert(UE_ARRAY_COUNT(ItemPrice) == (int32)ELotItem::Count, "ItemPrice must have one entry per ELotItem");
	static_assert(UE_ARRAY_COUNT(ItemUnlock) == (int32)ELotItem::Count, "ItemUnlock must have one entry per ELotItem");
	static_assert(UE_ARRAY_COUNT(ItemRequires) == (int32)ELotItem::Count, "ItemRequires must have one entry per ELotItem");
	static_assert(UE_ARRAY_COUNT(HandCapacityL) == (int32)ELotHand::Count, "HandCapacityL must have one entry per ELotHand");

	/** Completed cells needed for each unlock step, indexed by ELotUnlock. */
	inline constexpr int32 UnlockCells[6] = {10, 100, 350, 800, 6000, 30000};
	static_assert(UE_ARRAY_COUNT(UnlockCells) == (int32)ELotUnlock::Count, "UnlockCells must have one entry per ELotUnlock");

	/** Plant output in litres per minute for plant levels 0..5. */
	inline constexpr double PlantRateLPerMin[6] = {150, 300, 750, 2000, 4200, 5200};
	/** Price in credits of the upgrade to plant level 1..5. */
	inline constexpr int32 PlantPrice[5] = {100, 400, 1400, 8000, 24000};
	/** Litres in the plant storage at the start of a new game. */
	inline constexpr double PlantStartStockL = 80.0;
	/** Plant level that adds the first add-on. */
	inline constexpr int32 PlantAddonALevel = 2;
	/** Plant level that adds the second add-on. */
	inline constexpr int32 PlantAddonBLevel = 4;

	/** Litres the suitcase holds. */
	inline constexpr double SuitcaseCapacityL = 60.0;
	/** Litres the cart holds. */
	inline constexpr double CartCapacityL = 200.0;

	/** Cells per minute for the three auto paver levels. */
	inline constexpr double AutoPaverRateCellsPerMin[3] = {120, 170, 220};

	/** Cells per minute of the car: plain bar, wide bar. */
	inline constexpr double CarRateCellsPerMin[2] = {260, 300};
	/** Width of the car's bar: plain bar, wide bar. */
	inline constexpr double CarWidthCm[2] = {300, 350};

	// ---- Not fixed by the specification ----

	/** Seconds of one scoop stroke. */
	inline constexpr double ScoopCycleSeconds = 0.5; // tunable
	/** Litres per second poured from the bowl and the scoop. */
	inline constexpr double HandPourRateLPerS = 1.0; // tunable
	/** Footprint radius of the pour, indexed by ELotHand. */
	inline constexpr double PourRadiusCm[3] = {15, 10, 0}; // tunable
	static_assert(UE_ARRAY_COUNT(PourRadiusCm) == (int32)ELotHand::Count, "PourRadiusCm must have one entry per ELotHand");
	/** The bucket pours as a stream where the player aims: litres per second, and the radius of the stream's footprint. */
	inline constexpr double BucketPourRateLPerS = 4.0; // tunable
	inline constexpr double BucketPourRadiusCm = 30.0; // tunable
	/** Seconds a tool swap takes before the quick swap is unlocked (afterwards 0). */
	inline constexpr double SwapSeconds = 1.5; // tunable
	/** Farthest distance from the player at which the aim point works. */
	inline constexpr double ReachCm = 250.0; // tunable
	/** Litres per second through the suitcase flap. */
	inline constexpr double SuitcaseDoseRateLPerS = 2.0; // tunable
	/** Litres per second when filling the bucket from the suitcase or the cart. */
	inline constexpr double TapRateLPerS = 5.0; // tunable
	/** Width of the strip the cart lays. */
	inline constexpr double CartStripWidthCm = 100.0; // tunable

	/** Walking speed. */
	inline constexpr double WalkSpeedCmS = 450.0; // tunable
	/** Speed multiplier while sprinting. */
	inline constexpr double SprintMultiplier = 1.6; // tunable
	/** Speed while pulling the suitcase. */
	inline constexpr double SuitcasePullSpeedCmS = 350.0; // tunable
	/** Speed while pushing the cart. */
	inline constexpr double CartPushSpeedCmS = 400.0; // tunable
	/** Speed while pushing the cart and laying asphalt. */
	inline constexpr double CartLaySpeedCmS = 200.0; // tunable
	/** Speed while carrying the portable station. */
	inline constexpr double StationCarrySpeedCmS = 400.0; // tunable
	/** Speed with which the auto paver moves into its sector. */
	inline constexpr double AutoPaverSpeedCmS = 600.0; // tunable
	/** Most litres the auto paver may hold as work budget; stops a long wait from storing a burst. */
	inline constexpr double AutoPaverBudgetCapL = LitresPerCell; // tunable
	/** Speed of the car when not laying asphalt. */
	inline constexpr double CarSpeedCmS = 600.0; // tunable
	/** The plant storage holds this many minutes of output. */
	inline constexpr double PlantStorageMinutes = 4.0; // tunable

	// ---- Taken over from v1 ----

	/** Largest simulation step in seconds; longer spans are split into steps of this size. */
	inline constexpr double MaxStepSeconds = 0.25;
	/** Ties the tunable budget cap to the sub-step so that tuning cannot clip the top auto paver rate. */
	static_assert(AutoPaverRateCellsPerMin[2] * LitresPerCell * CompletionThreshold / 60.0 * MaxStepSeconds <= AutoPaverBudgetCapL,
		"AutoPaverBudgetCapL must hold the litres the top auto paver rate earns in one sub-step");
	/** Both ends of a pouring stamp are extended by this much. */
	inline constexpr double StampPadCm = 5.0;
	/** How close the player must be to interact with an object. */
	inline constexpr double InteractionRangeCm = 150.0;
	/** Seconds between autosaves. */
	inline constexpr double AutosaveIntervalSeconds = 30.0;
	/** Distance of the car's bar behind the car's centre. */
	inline constexpr double CarBarBehindCm = 230.0;
	/** Car turn rate in degrees per second. */
	inline constexpr double CarTurnRateDegPerSec = 90.0;
	/** Reversing speed as a fraction of the forward speed. */
	inline constexpr double CarReverseSpeedFactor = 0.5;
	/** Car acceleration in cm/s^2. */
	inline constexpr double CarAccelerationCmS2 = 1500.0;

	// ---- Fluid (the asphalt flows before it sets) ----
	// Thickness is counted in "texel volumes": 1 is the film one texel holds when the asphalt has settled (0.025 l, 1 cm on a 5 cm texel).

	/** The fluid is simulated in fixed steps of this length. */
	inline constexpr double FluidStepSeconds = 1.0 / 30.0;
	/** Steps one Tick may run at most; a longer frame does not make the simulation catch up. */
	inline constexpr int32 FluidMaxStepsPerTick = 6;
	/** Thickness a pour piles on the texels it lands on; the pile is what makes the asphalt run. */
	inline constexpr double FluidPileUnits = 4.0;
	/** Most a texel may be given by one pour, so that the last free texels of a footprint do not swallow a whole stroke. */
	inline constexpr double FluidMaxPileUnits = 8.0;
	/** Asphalt thinner than this does not run: it is the thickness it settles at. */
	inline constexpr double FluidFilmUnits = 1.0;
	/** A free texel that holds this much sets as ground. */
	inline constexpr double FluidCoverUnits = 0.5;
	/** Share of the height difference to a neighbour that flows across per step at full heat. */
	inline constexpr double FluidStiffness = 0.2;
	/** Share of a flow that is still there in the next step: the inertia that makes the asphalt overshoot and ripple. */
	inline constexpr double FluidMomentum = 0.85;
	/** Seconds in which the heat of the asphalt falls to a third; the colder it is, the slower it runs. */
	inline constexpr double FluidCoolSeconds = 8.0;
	/** Below this heat (1 = just poured) the asphalt has set where it lies. */
	inline constexpr double FluidFreezeHeat = 0.12;
	/** Longest the asphalt is let to run when the game has to settle it at once (saving). */
	inline constexpr double FluidSettleSeconds = 60.0;

	// ---- Derived ----

	/** Speed at which the car must move to lay its rate with its bar width; Level 0 = plain, 1 = wide bar. */
	constexpr double CarPaveSpeedCmS(int32 Level)
	{
		return CarRateCellsPerMin[Level] * 10000.0 / (60.0 * CarWidthCm[Level]);
	}

	/** Litres per second the plant dispenses at the given plant level. */
	constexpr double DispenseRateLPerS(int32 PlantLevel)
	{
		return PlantRateLPerMin[PlantLevel] / 60.0;
	}
}
