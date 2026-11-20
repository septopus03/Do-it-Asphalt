// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"
#include "LotGrid.h"
#include "LotMap.h"

/** Everything that happened during one game operation; operations can be summed with Append. */
struct ASPHALTCORE_API FLotEvents
{
	/** Litres taken from the ground budget by the operation. */
	double LitresPoured = 0.0;
	/** Credits paid out (CreditsPerCell for each completed cell). */
	double CreditsEarned = 0.0;
	/** Cells that became complete, each reported exactly once per game. */
	TArray<int32> CompletedCells;
	/** Entry segments whose twelfth cell was completed by the operation. */
	TArray<int32> CompletedEntries;
	/** Unlock steps whose conditions held for the first time after the operation. */
	TArray<ELotUnlock> Unlocked;
	/** True if the exit opened during the operation. */
	bool bExitOpened = false;

	void Append(const FLotEvents& Other);
};

/**
 * The whole game state and its rules, independent of any engine object. Units: centimetres, litres, seconds.
 *
 * Credits are earned only by completed cells. The plant produces litres into a stock; the shop sells items for
 * credits; an unlock step opens once its entry segment is complete AND enough cells are complete. Unlocks are
 * never stored, they are computed from the grid. All state is in plain members and every pour is booked by
 * ApplyPour, the single place that pays credits and reports segments, unlock steps and the exit.
 */
class ASPHALTCORE_API FLotGame
{
public:
	FLotGame();

	FLotGrid& GetGrid() { return Grid; }
	const FLotGrid& GetGrid() const { return Grid; }

	double GetCredits() const { return Credits; }
	int32 GetCompletedCells() const { return Grid.NumCompleteCells(); }
	double GetPlaytimeSeconds() const { return PlaytimeSeconds; }
	/** Litres poured onto the ground since the start. */
	double GetTotalLitres() const { return TotalLitres; }

	/** Completed cells (0..12) of an entry segment; 0 for an invalid segment. */
	int32 GetEntryProgress(int32 Entry) const;
	bool IsEntryComplete(int32 Entry) const;
	/** Incomplete cell whose centre is nearest to From (ties: lower index); INDEX_NONE if everything is complete. */
	int32 FindNearestIncomplete(FVector2D From) const;
	/** True if the entry segment of the step is complete AND LotBalance::UnlockCells of the step cells are complete. */
	bool IsUnlocked(ELotUnlock Unlock) const;

	bool Owns(ELotItem Item) const;
	/** Price in credits; -1 for an invalid item. */
	double GetItemPrice(ELotItem Item) const;
	/** True if the item is not owned, its unlock step is open and the item it requires is owned. */
	bool IsItemOffered(ELotItem Item) const;
	bool CanBuyItem(ELotItem Item) const;
	bool BuyItem(ELotItem Item);
	int32 GetNumItemsOwned() const;

	/** Plant level, always 0..5. */
	int32 GetPlantLevel() const { return PlantLevel; }
	/** Price of the next plant level; -1 at the maximum. */
	double GetPlantNextPrice() const;
	bool CanBuyPlant() const;
	bool BuyPlant();
	double GetPlantRatePerMin() const;
	/** Litres the plant storage holds: PlantStorageMinutes of output. */
	double GetPlantStorage() const;
	double GetPlantStock() const { return PlantStock; }

	/** Advances the game by Dt seconds in steps of at most MaxStepSeconds; bad Dt is ignored, more than a day is clipped. */
	FLotEvents Tick(double Dt);

	/** True once the exit step is unlocked. */
	bool IsExitOpen() const;
	bool HasWon() const { return bWon; }
	/** Marks the game as won; ignored until the exit is open. */
	void SetWon();

	void DebugGive(double CreditsToAdd);
	void DebugGrantItem(ELotItem Item);
	/** Sets the plant level; a level outside 0..5 is ignored. The stock is clipped to the new storage. */
	void DebugSetPlantLevel(int32 Level);
	/** Sets the plant stock, clipped to 0..GetPlantStorage(). */
	void DebugSetPlantStock(double Litres);
	FLotEvents DebugCompleteCell(int32 Cell);
	/** Completes the first Count incomplete cells by index, reported as one event. */
	FLotEvents DebugCompleteCells(int32 Count);

private:
	/** Books a pour: pays credits, adds litres, compares the unlock state before and after. */
	FLotEvents ApplyPour(const FLotPour& Pour);

	FLotGrid Grid;
	double Credits = 0.0;
	double PlaytimeSeconds = 0.0;
	double TotalLitres = 0.0;
	double PlantStock = LotBalance::PlantStartStockL;
	int32 PlantLevel = 0;
	bool bOwned[static_cast<int32>(ELotItem::Count)] = {};
	bool bWon = false;
};
