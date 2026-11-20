// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** All balance numbers of the game. Units: centimetres, litres, seconds. */
namespace AsphaltBalance
{
	inline constexpr double LitresPerCell = 20.0;
	inline constexpr double MoneyPerLitre = 1.0;
	inline constexpr int32 ShardsPerCell = 1;
	/** A cell counts as complete once this fraction of its texels is covered. */
	inline constexpr double CompletionThreshold = 0.97;

	// Backpack: 13 levels, 12 upgrades.
	inline constexpr double BackpackCapacityL[13] = {5, 7, 10, 15, 20, 30, 40, 60, 80, 100, 135, 150, 200};
	inline constexpr int32 BackpackPriceShards[12] = {1, 2, 4, 6, 9, 13, 18, 24, 31, 39, 48, 58};

	// Tool (pouring bar): 7 levels, 6 upgrades.
	inline constexpr double ToolWidthM[7] = {0.2, 0.3, 0.5, 0.8, 1.0, 1.5, 2.0};
	inline constexpr double ToolPriceMoney[6] = {25, 60, 120, 220, 380, 600};

	// Asphalt plant: 13 levels, 12 upgrades.
	inline constexpr double PlantRateLPerMin[13] = {15, 20, 25, 35, 50, 80, 100, 125, 180, 300, 500, 750, 1000};
	inline constexpr double PlantPriceMoney[12] = {30, 50, 75, 105, 145, 190, 240, 290, 335, 370, 395, 405};

	/** The TwoHands special multiplies the width of the pouring bar by this factor. */
	inline constexpr double TwoHandsWidthMultiplier = 2.0;

	// Specials: Sprint, TwoHands, PortableStation, AutoPaver, PaverCar.
	inline constexpr double SpecialPriceMoney[5] = {100, 200, 300, 450, 800};
	/** Index of the road on which each special is sold. */
	inline constexpr int32 SpecialRoad[5] = {3, 2, 4, 1, 0};
	inline constexpr int32 ExitRoad = 5;

	/** Largest simulation step in seconds; FAsphaltGame::Tick splits longer spans into steps of this size. */
	inline constexpr double MaxStepSeconds = 0.25;

	inline constexpr double PlantStorageMinutes = 4.0;
	inline constexpr double RefillSeconds = 1.5;
	/** How far in front of the player the pouring bar is held. */
	inline constexpr double NozzleAheadCm = 80.0;
	/** Both ends of a pouring stamp are extended by this much. */
	inline constexpr double StampPadCm = 5.0;

	inline constexpr double WalkSpeedCmS = 450.0;
	inline constexpr double PourSpeedCmS = 200.0;
	inline constexpr double SprintMultiplier = 1.6;

	inline constexpr double AutoPaverRateLPerMin = 20.0;
	inline constexpr double AutoPaverSpeedCmS = 150.0;
	/** The paver starts pouring once its centre is this close to the centre of its target cell. */
	inline constexpr double AutoPaverReachCm = 5.0;
	/** Largest amount of unspent litres the paver carries over from one step to the next. */
	inline constexpr double AutoPaverMaxBudgetL = 1.0;

	inline constexpr double CarTankL = 500.0;
	inline constexpr double CarWidthCm = 300.0;
	inline constexpr double CarSpeedCmS = 600.0;
	inline constexpr double CarBarBehindCm = 230.0;

	/** Visual tier of the backpack model: levels 0..2 -> 0, 3..6 -> 1, 7..9 -> 2, 10..12 -> 3. */
	constexpr int32 BackpackTier(int32 Level)
	{
		return Level <= 2 ? 0 : Level <= 6 ? 1 : Level <= 9 ? 2 : 3;
	}
}
