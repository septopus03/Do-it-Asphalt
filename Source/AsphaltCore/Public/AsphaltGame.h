// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoverageGrid.h"
#include "HexMap.h"

/** The upgrades that can be bought at any time (the specials of the roads are separate). */
enum class EAsphaltUpgrade : uint8
{
	Backpack,
	Tool,
	Plant,
};

/** Everything that happened during one game operation; operations can be summed with Append. */
struct FAsphaltEvents
{
	double LitresPoured = 0.0;
	double MoneyEarned = 0.0;
	int32 ShardsEarned = 0;
	TArray<int32> CompletedCells;
	TArray<int32> CompletedRoads;
	bool bExitOpened = false;

	void Append(const FAsphaltEvents& Other);
};

/**
 * The whole game state and its rules, independent of any engine object. Units: centimetres, litres, seconds.
 *
 * Asphalt flows plant -> backpack -> ground. The plant produces litres into a stock, RefillBackpack moves them
 * into the backpack, pouring spends them on the coverage grid; every litre poured earns money and every cell
 * that gets completed earns shards. Money buys Tool and Plant upgrades, shards buy Backpack upgrades.
 */
class ASPHALTCORE_API FAsphaltGame
{
public:
	FAsphaltGame();

	const FHexMap& GetMap() const;
	FCoverageGrid& GetGrid();
	const FCoverageGrid& GetGrid() const;

	double GetMoney() const;
	int32 GetShards() const;
	double GetPlaytimeSeconds() const;
	double GetTotalLitresPoured() const;

	int32 GetLevel(EAsphaltUpgrade Upgrade) const;
	int32 GetMaxLevel(EAsphaltUpgrade Upgrade) const;
	/** Price of the next level: shards for Backpack, money otherwise; -1 at the maximum level. */
	double GetNextPrice(EAsphaltUpgrade Upgrade) const;
	bool CanBuy(EAsphaltUpgrade Upgrade) const;
	/** Buys the next level; false (and nothing changes) if it is not affordable or the level is maximal. */
	bool Buy(EAsphaltUpgrade Upgrade);

	double GetBackpackCapacity() const;
	double GetBackpackLitres() const;
	/** Width of the pouring bar in cm. */
	double GetToolWidthCm() const;
	double GetPlantRatePerMin() const;
	/** Largest amount of asphalt the plant can hold. */
	double GetPlantStorage() const;
	double GetPlantStock() const;

	/** Advances the game by Dt seconds; any Dt is accepted and processed in steps of at most 0.25 s. */
	FAsphaltEvents Tick(double Dt);

	/** Moves min(capacity / RefillSeconds * Dt, free backpack space, plant stock) litres; returns the amount. */
	double RefillBackpack(double Dt);

	/** Pours from the backpack along the axis A->B with the current tool width; spends, earns and reports. */
	FAsphaltEvents PourFromBackpack(FVector2D A, FVector2D B);

	void DebugGive(double Money, int32 Shards);
	void DebugSetLevel(EAsphaltUpgrade Upgrade, int32 Level);
	/** Completes the cell and rewards it exactly as if the missing litres had been poured (backpack untouched). */
	FAsphaltEvents DebugCompleteCell(int32 Cell);

private:
	/** Largest simulation step in seconds. */
	static constexpr double MaxStepSeconds = 0.25;

	void TickStep(double Step);
	/** Turns litres used on the grid into earnings and bookkeeping; the one place where pouring pays. */
	FAsphaltEvents ApplyPourResult(const FPourResult& Result);
	/** Keeps the backpack and the plant stock within their capacities after a level change. */
	void ClampToCapacities();

	FHexMap Map;
	FCoverageGrid Grid;

	// Economy.
	double Money = 0.0;
	int32 Shards = 0;
	double PlaytimeSeconds = 0.0;
	double TotalLitresPoured = 0.0;

	// Upgrades, indexed by EAsphaltUpgrade.
	int32 Levels[3] = {0, 0, 0};

	// Asphalt on its way to the ground.
	double BackpackLitres = 0.0;
	double PlantStock = 0.0;
};
