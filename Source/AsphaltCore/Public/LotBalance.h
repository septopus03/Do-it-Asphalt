// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Things the player can buy. Order is binding: the tables in LotBalance are indexed by it. */
enum class ELotItem : uint8 { Scoop, Bucket, Suitcase, Cart, Station, Finisher, AutoPaver, AutoPaverFast, AutoPaverFaster, Car, CarWideBar, Count };

/** Unlock steps; step i belongs to sector i of the progression and opens after LotBalance::UnlockCells[i] cells. */
enum class ELotUnlock : uint8 { QuickSwap, Sprint, StripWork, AutoPaver, Car, Exit, Count };

/** What the player holds in the hand. */
enum class ELotHand : uint8 { Bowl, Scoop, Bucket, Trowel, Count };

/** Equipment the player can carry or push around the lot. */
enum class ELotMovable : uint8 { Suitcase, Cart, Station, Finisher, Count };

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
	/** A cell counts as complete once this fraction of its area is covered. */
	inline constexpr double CompletionThreshold = 0.95;

	/** Litres in one spoon of the bowl. */
	inline constexpr double SpoonLitres = 0.25;
	/** Litres in one scoop. */
	inline constexpr double ScoopLitres = 1.0;
	/** Litres the hand holds, indexed by ELotHand (the trowel holds nothing). */
	inline constexpr double HandCapacityL[4] = {2.5, 1.0, 10.0, 0.0};

	/** Price in credits, indexed by ELotItem. */
	inline constexpr int32 ItemPrice[11] = {10, 35, 110, 340, 900, 1400, 3000, 9000, 22000, 12000, 24000};
	/** Unlock step an item needs, indexed by ELotItem; ELotUnlock::Count means no condition. */
	inline constexpr ELotUnlock ItemUnlock[11] =
	{
		ELotUnlock::Count, ELotUnlock::Count, ELotUnlock::Count, ELotUnlock::Count,
		ELotUnlock::StripWork, ELotUnlock::StripWork,
		ELotUnlock::AutoPaver, ELotUnlock::AutoPaver, ELotUnlock::AutoPaver,
		ELotUnlock::Car, ELotUnlock::Car
	};
	/** Item that must be owned first, indexed by ELotItem; ELotItem::Count means nothing. */
	inline constexpr ELotItem ItemRequires[11] =
	{
		ELotItem::Count, ELotItem::Count, ELotItem::Count, ELotItem::Count,
		ELotItem::Count, ELotItem::Count,
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

	/** Cells per minute the finisher lays. */
	inline constexpr double FinisherRateCellsPerMin = 85.0;
	/** Width of the finisher strip. */
	inline constexpr double FinisherWidthCm = 100.0;

	/** Cells per minute for the three auto paver levels. */
	inline constexpr double AutoPaverRateCellsPerMin[3] = {120, 170, 220};

	/** Cells per minute of the car: plain bar, wide bar. */
	inline constexpr double CarRateCellsPerMin[2] = {260, 300};
	/** Width of the car's bar: plain bar, wide bar. */
	inline constexpr double CarWidthCm[2] = {300, 350};

	// ---- Not fixed by the specification ----

	/** Seconds of one scoop or trowel stroke. */
	inline constexpr double ScoopCycleSeconds = 0.6; // tunable
	/** Litres per second poured from the bowl and the scoop. */
	inline constexpr double HandPourRateLPerS = 1.0; // tunable
	/** Footprint radius of the pour, indexed by ELotHand. */
	inline constexpr double PourRadiusCm[4] = {15, 10, 0, 0}; // tunable
	static_assert(UE_ARRAY_COUNT(PourRadiusCm) == (int32)ELotHand::Count, "PourRadiusCm must have one entry per ELotHand");
	/** Seconds a tool swap takes before the quick swap is unlocked (afterwards 0). */
	inline constexpr double SwapSeconds = 1.5; // tunable
	/** Width of the trowel. */
	inline constexpr double TrowelWidthCm = 30.0; // tunable
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
	inline constexpr double CartPushSpeedCmS = 250.0; // tunable
	/** Speed while pushing the cart and laying asphalt. */
	inline constexpr double CartLaySpeedCmS = 100.0; // tunable
	/** Speed with which the auto paver moves into its sector. */
	inline constexpr double AutoPaverSpeedCmS = 600.0; // tunable
	/** Speed of the car when not laying asphalt. */
	inline constexpr double CarSpeedCmS = 600.0; // tunable
	/** The plant storage holds this many minutes of output. */
	inline constexpr double PlantStorageMinutes = 4.0; // tunable

	// ---- Taken over from v1 ----

	/** Largest simulation step in seconds; longer spans are split into steps of this size. */
	inline constexpr double MaxStepSeconds = 0.25;
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

	// ---- Derived ----

	/** Speed at which the finisher must move to lay FinisherRateCellsPerMin: rate x cell length / 60. */
	constexpr double FinisherSpeedCmS()
	{
		return FinisherRateCellsPerMin * 100.0 / 60.0;
	}

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
