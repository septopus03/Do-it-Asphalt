// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"
#include "LotGrid.h"
#include "LotLayout.h"
#include "LotMap.h"

/** Everything that happened during one game operation; operations can be summed with Append. */
struct ASPHALTCORE_API FLotEvents
{
	/** Litres taken from the ground budget by the operation. */
	double LitresPoured = 0.0;
	/** Credits paid out (CreditsPerCell for each completed cell, CreditsPerLitre for each litre poured). */
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

/** What the auto paver is doing: it works a sector from its tank, and drives to a vehicle station when the tank is empty. */
enum class ELotPaverPhase : uint8 { Idle, ToSector, Working, ToStation, Loading, Count };

/** Where one movable stands. The engine layer moves it; the core only keeps the position so it can be saved. */
struct ASPHALTCORE_API FLotMovable
{
	FVector2D Pos = FVector2D::ZeroVector;
	double YawDeg = 0.0;
	bool bHeld = false;
};

/** Positions of the player and of everything that moves; plain state, no rules. */
struct ASPHALTCORE_API FLotWorldState
{
	FVector2D PlayerPos = LotLayout::PlayerStart;
	double PlayerYawDeg = LotLayout::PlayerStartYawDeg;
	FLotMovable Movables[static_cast<int32>(ELotMovable::Count)];
	FVector2D CarPos = FVector2D::ZeroVector;
	double CarYawDeg = 0.0;
	bool bPlayerInCar = false;
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

	/**
	 * Lets the asphalt that is poured run before it sets (FLotGrid::SetFluidEnabled); the setting survives LoadFromBytes. Tick
	 * advances the fluid, and what it covers books like any pour (the litres were paid when poured).
	 */
	void SetFluidEnabled(bool bEnabled) { Grid.SetFluidEnabled(bEnabled); }
	bool IsFluidEnabled() const { return Grid.IsFluidEnabled(); }
	bool IsFluidActive() const { return Grid.IsFluidActive(); }
	/** Lets all the asphalt that runs set at once (a save holds only the ground); returns what that completed. */
	FLotEvents SettleFluid();

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
	/** True if the hand item can be taken: the bowl always, a vessel once bought. */
	bool CanSelectHand(ELotHand NewHand) const;
	/**
	 * Takes another item into the hand and clears the pour stream. Returns true at once if it is already in hand;
	 * false if it cannot be taken or the hands are busy. Without the quick swap the hands stay busy for SwapSeconds.
	 */
	bool SelectHand(ELotHand NewHand);
	/** True while a swap is in progress; no hand action works meanwhile. */
	bool IsHandBusy() const { return SwapRemaining > 0.0; }
	/** Litres the item holds (each item keeps its own content); 0 for an invalid item. */
	double GetHandLitres(ELotHand Item) const;
	double GetHandCapacity(ELotHand Item) const;

	/**
	 * Holding the scoop button for Dt seconds: every ScoopCycleSeconds one spoon stroke moves SpoonLitres from the plant stock
	 * into the bowl in hand, as much as fits and as the stock allows. A stroke that moves nothing resets the stroke timer. Returns the litres moved by this call.
	 */
	double ScoopFromTrough(double Dt);
	/** True for the hand items that are filled at a tap and poured as a stream: the bucket, the canister and the tub. */
	static bool IsVessel(ELotHand Item);
	/** The shop item behind a vessel; ELotItem::Count for the bowl. */
	static ELotItem VesselItem(ELotHand Vessel);
	/**
	 * Moves litres from the source into the vessel: DispenseRateLPerS * Dt from the plant stock, TapRateLPerS * Dt from
	 * the content of the suitcase or the cart. The vessel must be owned (it is filled while in hand or not), a movable source must be owned
	 * and not held. Limited by the source content and the free room in the vessel. Returns litres moved.
	 */
	double FillVessel(ELotHand Vessel, ELotSource Source, double Dt);
	/**
	 * Pours the item in hand as a stream for Dt seconds, where Aim is: the stream budget grows by HandPourRateLPerS * Dt (at most the
	 * content) and is spent on a disc of PourRadiusCm around Aim (nearest to From first). With the fluid on the asphalt is laid at the aim point and runs
	 * from there. Texels that are already covered cost nothing, so the vessel keeps what it could not lay; the part below one texel's price is carried to the next call.
	 */
	FLotEvents PourHand(FVector2D Aim, FVector2D From, double Dt);
	/** Empties the vessel in hand into the cell under Aim; what does not fit stays in the vessel. */
	FLotEvents DumpVessel(FVector2D Aim, FVector2D From);

	FLotWorldState& World() { return WorldState; }
	const FLotWorldState& World() const { return WorldState; }
	/** True if the item behind the movable is bought. */
	bool OwnsMovable(ELotMovable Movable) const;
	/** The movable the player holds; ELotMovable::Count means nothing. While something is held, no hand action works. */
	ELotMovable GetHeld() const { return Held; }
	/** Takes an owned movable; false if it is not owned, something is already held or the player sits in the car. */
	bool Grab(ELotMovable Movable);
	/** Lets go of the held movable at the given place (a non-finite position keeps the old one); nothing held does nothing. */
	void Release(FVector2D Pos, double YawDeg);
	/** Litres in the suitcase or the cart; 0 for the others. */
	double GetMovableLitres(ELotMovable Movable) const;
	double GetMovableCapacity(ELotMovable Movable) const;
	/** Moves DispenseRateLPerS * Dt from the plant stock into the owned suitcase or cart; the engine decides where it stands. Returns litres moved. */
	double FillMovable(ELotMovable Movable, double Dt);
	/**
	 * The owned, not held suitcase doses asphalt into the cell under Mouth for Dt seconds: the dose budget grows by
	 * SuitcaseDoseRateLPerS * Dt (at most the content) and the part below one texel's price is carried to the next call.
	 */
	FLotEvents DoseSuitcase(FVector2D Mouth, FVector2D From, double Dt);
	/** The held cart lays a strip CartStripWidthCm wide along A->B; its content is the budget. */
	FLotEvents LayCartStrip(FVector2D A, FVector2D B);

	/** Auto paver level by upgrades bought: 0..2; -1 without the auto paver. */
	int32 GetAutoPaverLevel() const;
	/** Cells per minute the auto paver paves at its level; 0 without it. */
	double GetAutoPaverRateCellsPerMin() const;
	/**
	 * Sends the owned auto paver to a sector (0..74): it travels to the centre of the sector's first cell, then works
	 * through the sector's 400 cells inside Tick. Every assignment, the current sector too, restarts: the step and the budget go back to 0 and the paver travels to the first cell again. False without the
	 * auto paver or for an invalid sector.
	 */
	bool AssignAutoPaverSector(int32 Sector);
	/** Sector the auto paver is assigned to; INDEX_NONE while it waits. */
	int32 GetAutoPaverSector() const { return AutoPaverSector; }
	/** Cell of the sector being worked on (0..399). */
	int32 GetAutoPaverStep() const { return AutoPaverStep; }
	FVector2D GetAutoPaverPos() const { return AutoPaverPos; }
	/** True while the auto paver drives (to its sector or to a station). */
	bool IsAutoPaverTravelling() const { return AutoPaverPhase == ELotPaverPhase::ToSector || AutoPaverPhase == ELotPaverPhase::ToStation; }
	ELotPaverPhase GetAutoPaverPhase() const { return AutoPaverPhase; }
	/** Litres in the tank of the auto paver, and the size of the tank. */
	double GetAutoPaverTank() const { return AutoPaverTank; }
	double GetAutoPaverTankCapacity() const { return LotBalance::AutoPaverTankL; }
	/** Where the vehicles load: the fixed station and, once bought and set down, the portable one. */
	int32 GetNumStations() const;
	FVector2D GetStationPos(int32 Index) const;
	/** The station nearest to From (index into GetNumStations); the fixed station is 0. */
	int32 FindNearestStation(FVector2D From) const;
	/** True if Pos is within VehicleStationRadiusCm of a station. */
	bool IsAtStation(FVector2D Pos) const;

	/** Car level: 0 plain bar, 1 wide bar; -1 without the car. */
	int32 GetCarLevel() const;
	/** Litres in the tank of the car, and the size of the tank (0 without the car). */
	double GetCarTank() const { return CarTank; }
	double GetCarTankCapacity() const;
	/**
	 * The car stands in a station for Dt seconds: VehicleLoadRateLPerS * Dt litres move from the plant stock into the tank,
	 * as much as fits. Nothing unless the car is owned and its position (World().CarPos) is within a station's radius. Returns litres moved.
	 */
	double LoadCar(double Dt);
	/**
	 * The car lays asphalt under its bar along A->B (CarWidthCm of the level wide) from its tank.
	 * Nothing without the car.
	 */
	FLotEvents PourFromCar(FVector2D A, FVector2D B);

	/** First four bytes of a save ("DIA2" in little-endian order). */
	static constexpr uint32 SaveMagic = 0x32414944;
	static constexpr int32 SaveVersion = 3;

	/**
	 * Serialises the whole game state (little-endian, fixed order, exact length) for the autosave slot. The pour
	 * stream budgets, the scoop stroke timer and the movables' held flags are not saved; the last is derived on load.
	 */
	TArray<uint8> SaveToBytes() const;
	/**
	 * Replaces the game by the saved one. Returns false and leaves the game untouched unless the whole blob is valid,
	 * consistent with the rules (owned items, vessel capacities, unlocks) and exactly as long as a save is.
	 */
	bool LoadFromBytes(TConstArrayView<uint8> Bytes);

	void DebugGive(double CreditsToAdd);
	void DebugGrantItem(ELotItem Item);
	/** Sets the plant level; a level outside 0..5 is ignored. The stock is clipped to the new storage. */
	void DebugSetPlantLevel(int32 Level);
	/** Sets the plant stock, clipped to 0..GetPlantStorage(). */
	void DebugSetPlantStock(double Litres);
	FLotEvents DebugCompleteCell(int32 Cell);
	/** Completes the first Count incomplete cells by index, reported as one event. */
	FLotEvents DebugCompleteCells(int32 Count);
	/** Fills the item in hand to its capacity without touching the stock. */
	void DebugFillHand();
	/** Fills the tanks of the auto paver and the car (a car or paver that is not owned stays empty). */
	void DebugFillTanks();

private:
	/** True if hand actions are allowed: no swap in progress. */
	bool HandsFree() const;
	/** Gives a movable bought item its place on the lot. */
	void PlaceBoughtItem(ELotItem Item);
	/**
	 * Grows a stream budget by Earned (never above Content), spends it through Spend, charges the litres used to both
	 * and drops a rest the grid could still pay for. Shared by the hand pour and the suitcase dose.
	 */
	FLotPour SpendStream(double& Budget, double& Content, double Earned, TFunctionRef<FLotPour(double)> Spend);
	/** Advances the auto paver by one sub-step: drive, work through the sector from its tank, or load at a station; appends what it pours to Events. */
	void TickAutoPaver(double Step, FLotEvents& Events);
	/** Books a pour: pays credits, adds litres, compares the unlock state before and after. */
	FLotEvents ApplyPour(const FLotPour& Pour);

	FLotGrid Grid;
	/** Seconds of game time the fluid has not stepped through yet (below one step). */
	double FluidClock = 0.0;
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
	FLotWorldState WorldState;
	ELotMovable Held = ELotMovable::Count;
	/** Litres of the suitcase and the cart; the other movables stay at 0. */
	double MovableLitres[static_cast<int32>(ELotMovable::Count)] = {};
	/** Litres of the suitcase dose earned but not yet spent (below one texel's price between calls). */
	double DoseBudget = 0.0;
	/** Sector the auto paver works in; INDEX_NONE while it waits. */
	int32 AutoPaverSector = INDEX_NONE;
	/** Cell of the sector being worked on. */
	int32 AutoPaverStep = 0;
	FVector2D AutoPaverPos = FVector2D::ZeroVector;
	ELotPaverPhase AutoPaverPhase = ELotPaverPhase::Idle;
	/** Litres the auto paver has earned but not yet spent. */
	double AutoPaverBudget = 0.0;
	/** Litres in the tank of the auto paver and of the car. */
	double AutoPaverTank = 0.0;
	double CarTank = 0.0;
};
