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

	ELotHand GetHand() const { return Hand; }
	/** True if the hand item can be taken: the bowl and the trowel always, the scoop and the bucket once bought. */
	bool CanSelectHand(ELotHand NewHand) const;
	/**
	 * Takes another item into the hand and clears the pour stream. Returns true at once if it is already in hand;
	 * false if it cannot be taken or the hands are busy. Without the quick swap the hands stay busy for SwapSeconds.
	 */
	bool SelectHand(ELotHand NewHand);
	/** True while a swap is in progress; no hand action works meanwhile. */
	bool IsHandBusy() const { return SwapRemaining > 0.0; }
	/** Litres the item holds (each item keeps its own content); 0 for the trowel and an invalid item. */
	double GetHandLitres(ELotHand Item) const;
	double GetHandCapacity(ELotHand Item) const;

	/**
	 * Holding the scoop button for Dt seconds: every ScoopCycleSeconds one stroke moves litres from the plant stock
	 * into the bowl (SpoonLitres, ScoopLitres once the scoop is owned) or the scoop in hand, as much as fits and as
	 * the stock allows. A stroke that moves nothing resets the stroke timer. Returns the litres moved by this call.
	 */
	double ScoopFromTrough(double Dt);
	/** Moves DispenseRateLPerS * Dt from the plant stock into the bucket; the bucket must be owned and not in hand. Returns litres moved. */
	double FillBucket(ELotSource Source, double Dt);
	/**
	 * Pours from the bowl or the scoop in hand for Dt seconds: the stream budget grows by HandPourRateLPerS * Dt (at
	 * most the content), is spent on PourDisc around Aim (nearest to From first) and the part below one texel's
	 * price is carried to the next call.
	 */
	FLotEvents PourHand(FVector2D Aim, FVector2D From, double Dt);
	/** Empties the bucket in hand into the cell under Aim; what does not fit stays in the bucket. */
	FLotEvents DumpBucket(FVector2D Aim, FVector2D From);
	/** Turns the raw asphalt under the strip A->B (TrowelWidthCm wide) into finished asphalt; trowel in hand only. */
	FLotEvents Trowel(FVector2D A, FVector2D B);

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
	/** True if hand actions are allowed: no swap in progress. */
	bool HandsFree() const;
	/** Litres moved per second from a source into the bucket; 0 for sources that are not available yet. */
	double GetSourceRate(ELotSource Source) const;
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
	ELotHand Hand = ELotHand::Bowl;
	double HandLitres[static_cast<int32>(ELotHand::Count)] = {};
	/** Seconds left of the current swap. */
	double SwapRemaining = 0.0;
	/** Seconds accumulated towards the next scoop stroke. */
	double StrokeTimer = 0.0;
	/** Litres of the pour stream earned but not yet spent (below one texel's price between calls). */
	double StreamBudget = 0.0;
};
