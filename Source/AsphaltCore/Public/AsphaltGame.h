// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltLayout.h"
#include "CoverageGrid.h"
#include "HexMap.h"

/** The upgrades that can be bought at any time (the specials of the roads are separate). */
enum class EAsphaltUpgrade : uint8
{
	Backpack,
	Tool,
	Plant,
};

/** Specials sold on the roads; each is unlocked by completing its road (see AsphaltBalance::SpecialRoad). */
enum class EAsphaltSpecial : uint8
{
	Sprint,
	TwoHands,
	PortableStation,
	AutoPaver,
	PaverCar,
	Count,
};

/** Positions owned by the engine layer but stored with the game so that they are saved with it. */
struct FAsphaltWorldState
{
	FVector2D PlayerPos = AsphaltLayout::PlayerStart;
	double PlayerYawDeg = 0.0;
	FVector2D StationPos = FVector2D::ZeroVector;
	bool bStationCarried = false;
	FVector2D CarPos = FVector2D::ZeroVector;
	double CarYawDeg = 0.0;
	bool bPlayerInCar = false;
};

/** Everything that happened during one game operation; operations can be summed with Append. */
struct ASPHALTCORE_API FAsphaltEvents
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
	/** First four bytes of every save: "DIA1" in little-endian order. */
	static constexpr uint32 SaveMagic = 0x31414944;
	static constexpr int32 SaveVersion = 1;

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

	/**
	 * Advances the game by Dt seconds in steps of at most AsphaltBalance::MaxStepSeconds: the plant produces and
	 * the auto paver (if owned) paves; the events are the sum of the paver's pours. Non-positive and non-finite Dt
	 * is ignored. Dt beyond one day is cut off so that absurd values cannot hang the loop; callers that really
	 * want to simulate longer spans must chunk them into several calls.
	 */
	FAsphaltEvents Tick(double Dt);

	/** Moves min(capacity / RefillSeconds * Dt, free backpack space, plant stock) litres; returns the amount. */
	double RefillBackpack(double Dt);

	/** Pours from the backpack along the axis A->B with the current tool width; spends, earns and reports. */
	FAsphaltEvents PourFromBackpack(FVector2D A, FVector2D B);

	// Specials.
	bool HasSpecial(EAsphaltSpecial S) const;
	/** Price in money; -1 for an invalid special. */
	double GetSpecialPrice(EAsphaltSpecial S) const;
	/** Road whose completion unlocks the special; INDEX_NONE for an invalid special. */
	int32 GetSpecialRoad(EAsphaltSpecial S) const;
	/** Not owned yet, its road is complete and the money suffices. */
	bool CanBuySpecial(EAsphaltSpecial S) const;
	/** Buys the special (the station and the car appear at SpecialSpawn of their road); false changes nothing. */
	bool BuySpecial(EAsphaltSpecial S);

	// Roads and exit. Both are derived from the coverage grid, so they follow every way a cell can complete.
	/** Completed cells of the road, 0..5. */
	int32 GetRoadProgress(int32 Road) const;
	bool IsRoadComplete(int32 Road) const;
	/** All 499 cells are complete. */
	bool IsExitOpen() const;
	bool HasWon() const;
	/** Marks the game as won; ignored unless the exit is open. */
	void SetWon();

	// Paver car: a second, large tank filled from the same plant stock.
	double GetCarLitres() const;
	/** Like RefillBackpack, for the car tank (capacity CarTankL); 0 without the car. */
	double RefillCar(double Dt);
	/** Pours from the car tank along A->B with the width AsphaltBalance::CarWidthCm; nothing without the car. */
	FAsphaltEvents PourFromCar(FVector2D A, FVector2D B);

	// Auto paver.
	FVector2D GetAutoPaverPos() const;
	/** Cell the paver is working on; INDEX_NONE when idle. */
	int32 GetAutoPaverTarget() const;

	FAsphaltWorldState& World();
	const FAsphaltWorldState& World() const;

	/**
	 * Serialises the whole game state (little-endian): header (magic, version, cell count, grid size), economy,
	 * levels, specials, containers, auto paver, world state and the packed coverage mask. Road progress and the
	 * exit are derived from the coverage mask and therefore not stored.
	 */
	TArray<uint8> SaveToBytes() const;
	/**
	 * Restores a state written by SaveToBytes. Every value is range checked (known magic and version, matching map and
	 * grid size, levels within their maximum, litres within their capacities, finite non-negative numbers, auto paver
	 * target INDEX_NONE or a valid cell, exact length). On any failure it returns false and *this is left untouched.
	 */
	bool LoadFromBytes(TConstArrayView<uint8> Bytes);

	void DebugGive(double Money, int32 Shards);
	/** Gives the special for free (the station and the car are placed like after a purchase). */
	void DebugGrantSpecial(EAsphaltSpecial S);
	void DebugSetLevel(EAsphaltUpgrade Upgrade, int32 Level);
	/** Completes the cell and rewards it exactly as if the missing litres had been poured (backpack untouched). */
	FAsphaltEvents DebugCompleteCell(int32 Cell);

private:
	/** Advances one step of at most MaxStepSeconds: plant production and auto paver. */
	FAsphaltEvents TickStep(double Step);
	/** Moves the auto paver towards its target or pours into it. */
	FAsphaltEvents TickAutoPaver(double Step);
	/** Nearest incomplete cell to the paver (ties: lower index); INDEX_NONE if every cell is complete. */
	int32 PickAutoPaverTarget() const;
	/** Moves litres from the plant stock into a container; returns the amount moved. */
	double MoveFromPlant(double Capacity, double& Container, double Dt);
	/**
	 * Turns litres used on the grid into earnings and bookkeeping; the one place where pouring pays. It also
	 * reports the roads and the exit that this result completed.
	 */
	FAsphaltEvents ApplyPourResult(const FPourResult& Result);
	void GrantSpecial(EAsphaltSpecial S);
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
	double CarLitres = 0.0;

	// Specials, indexed by EAsphaltSpecial.
	bool bOwned[static_cast<int32>(EAsphaltSpecial::Count)] = {};
	bool bWon = false;

	FAsphaltWorldState WorldState;

	// Auto paver.
	FVector2D AutoPaverPos = FVector2D::ZeroVector;
	int32 AutoPaverTarget = INDEX_NONE;
	/** Litres earned by the paver but not yet spent (a texel costs about 0.05 l). */
	double AutoPaverBudget = 0.0;
};
