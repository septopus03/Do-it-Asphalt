// Copyright Do it Asphalt. All Rights Reserved.

#include "LotGame.h"
#include "LotSaveIO.h"

namespace
{
	/** For the length of its life the grid lays what is poured at once, not as running asphalt (the machines lay an even strip). */
	struct FInstantPour
	{
		explicit FInstantPour(FLotGrid& InGrid) : Grid(InGrid), bBefore(InGrid.IsInstantPour()) { Grid.SetInstantPour(true); }
		~FInstantPour() { Grid.SetInstantPour(bBefore); }
		FLotGrid& Grid;
		bool bBefore;
	};

	/**
	 * Longest span a single Tick call simulates (one day). Only a guard against absurd Dt values such as 1e15,
	 * which would otherwise keep the loop running for ever; real callers pass a frame time.
	 */
	constexpr double MaxTickSeconds = 86400.0;
	/** An idle auto paver that found no sector to pave looks again after this many seconds. */
	constexpr double AutoPaverSearchSeconds = 2.0;
	/** The tolerance with which FLotGrid turns litres into texels; a stream rest this close to a texel's price is payable. */
	constexpr double StreamEpsilon = 1e-9;
	constexpr int32 NumItems = static_cast<int32>(ELotItem::Count);
	constexpr int32 NumUnlocks = static_cast<int32>(ELotUnlock::Count);
	constexpr int32 MaxPlantLevel = static_cast<int32>(UE_ARRAY_COUNT(LotBalance::PlantRateLPerMin)) - 1;

	static_assert(NumUnlocks == AsphaltLot::NumEntries, "unlock step i belongs to entry segment i");
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(LotBalance::PlantPrice)) == MaxPlantLevel, "one plant price per upgrade");

	bool IsValidItem(ELotItem Item)
	{
		return static_cast<int32>(Item) >= 0 && static_cast<int32>(Item) < NumItems;
	}

	/** The robot a shop item buys; INDEX_NONE for the other items. */
	int32 PaverIndexOf(ELotItem Item)
	{
		for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
		{
			if (AutoPaverItem[Paver] == Item)
			{
				return Paver;
			}
		}
		return INDEX_NONE;
	}

	bool IsPaverItem(ELotItem Item)
	{
		return PaverIndexOf(Item) != INDEX_NONE;
	}

	constexpr int32 NumPaverUpgrades = static_cast<int32>(ELotPaverUpgrade::Count);

	bool IsValidPaverUpgrade(ELotPaverUpgrade Upgrade)
	{
		return static_cast<int32>(Upgrade) >= 0 && static_cast<int32>(Upgrade) < NumPaverUpgrades;
	}

	/** Litres the tank of a robot holds at the given capacity level. */
	double TankCapacityAt(int32 CapacityLevel)
	{
		return LotBalance::AutoPaverTankLByLevel[CapacityLevel];
	}

	/** Positions of the shop items in the saves before version 7, which had the global speed items AutoPaverFast and AutoPaverFaster at 7 and 8. */
	namespace OldItem
	{
		constexpr int32 AutoPaver = 6;
		constexpr int32 AutoPaverFast = 7;
		constexpr int32 AutoPaverFaster = 8;
		constexpr int32 Num = 15; // version 6; version 5 stored 13 (to CarT4), version 4 stored 11 (to CarT2)
		/** The current item of an old index; the two speed items map to Count (they became speed levels). */
		ELotItem Map(int32 Old)
		{
			if (Old <= AutoPaver)
			{
				return static_cast<ELotItem>(Old);
			}
			if (Old == AutoPaverFast || Old == AutoPaverFaster)
			{
				return ELotItem::Count;
			}
			return static_cast<ELotItem>(Old - 2);
		}
	}

	bool IsValidHand(ELotHand Item)
	{
		return static_cast<int32>(Item) >= 0 && static_cast<int32>(Item) < static_cast<int32>(ELotHand::Count);
	}

	bool IsFinitePoint(const FVector2D& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	constexpr int32 NumMovables = static_cast<int32>(ELotMovable::Count);

	bool IsValidMovable(ELotMovable Movable)
	{
		return static_cast<int32>(Movable) >= 0 && static_cast<int32>(Movable) < NumMovables;
	}

	/** Only the trolley and the cart hold asphalt. */
	bool HoldsLitres(ELotMovable Movable)
	{
		return Movable == ELotMovable::Trolley || Movable == ELotMovable::Cart;
	}

	/** Hand items that hold litres: the bowl, the scoop and the bucket. */
	constexpr int32 NumLitreHands = static_cast<int32>(ELotHand::Count);
	/** Rounding slack when a loaded amount of litres is compared with its limit. */
	constexpr double LoadSlackL = 1e-6;

	/** True if the stored held-movable value is a movable or "nothing" (ELotMovable::Count). */
	bool IsValidHeld(int32 Value)
	{
		return Value >= 0 && Value <= NumMovables;
	}

	/** The shop item behind a movable. */
	ELotItem MovableItem(ELotMovable Movable)
	{
		switch (Movable)
		{
		case ELotMovable::Trolley: return ELotItem::Trolley;
		case ELotMovable::Cart: return ELotItem::Cart;
		default: return ELotItem::Station;
		}
	}

	bool IsValidUnlock(ELotUnlock Unlock)
	{
		return static_cast<int32>(Unlock) >= 0 && static_cast<int32>(Unlock) < NumUnlocks;
	}
}

void FLotEvents::Append(const FLotEvents& Other)
{
	LitresPoured += Other.LitresPoured;
	CreditsEarned += Other.CreditsEarned;
	CompletedCells.Append(Other.CompletedCells);
	CompletedEntries.Append(Other.CompletedEntries);
	Unlocked.Append(Other.Unlocked);
	bExitOpened = bExitOpened || Other.bExitOpened;
}

FLotGame::FLotGame()
{
}

int32 FLotGame::GetEntryProgress(int32 Entry) const
{
	if (Entry < 0 || Entry >= AsphaltLot::NumEntries)
	{
		return 0;
	}
	int32 Progress = 0;
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		Progress += Grid.IsCellComplete(AsphaltLot::EntryCell(Entry, I)) ? 1 : 0;
	}
	return Progress;
}

bool FLotGame::IsEntryComplete(int32 Entry) const
{
	return GetEntryProgress(Entry) == AsphaltLot::CellsPerEntry;
}

int32 FLotGame::FindNearestIncomplete(FVector2D From) const
{
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Cell = 0; Cell < AsphaltLot::NumCells; ++Cell)
	{
		if (Grid.IsCellComplete(Cell))
		{
			continue;
		}
		const double Distance = FVector2D::DistSquared(From, AsphaltLot::CellCentre(Cell));
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Cell;
		}
	}
	return Best;
}

bool FLotGame::IsUnlocked(ELotUnlock Unlock) const
{
	if (!IsValidUnlock(Unlock))
	{
		return false;
	}
	const int32 Step = static_cast<int32>(Unlock);
	return IsEntryComplete(Step) && GetCompletedCells() >= LotBalance::UnlockCells[Step];
}

bool FLotGame::Owns(ELotItem Item) const
{
	return IsValidItem(Item) && bOwned[static_cast<int32>(Item)];
}

double FLotGame::GetItemPrice(ELotItem Item) const
{
	return IsValidItem(Item) ? static_cast<double>(LotBalance::ItemPrice[static_cast<int32>(Item)]) : -1.0;
}

bool FLotGame::IsItemOffered(ELotItem Item) const
{
	if (!IsValidItem(Item) || Owns(Item))
	{
		return false;
	}
	const int32 Index = static_cast<int32>(Item);
	const ELotUnlock Needed = LotBalance::ItemUnlock[Index];
	const ELotItem Required = LotBalance::ItemRequires[Index];
	return (Needed == ELotUnlock::Count || IsUnlocked(Needed)) && (Required == ELotItem::Count || Owns(Required));
}

bool FLotGame::CanBuyItem(ELotItem Item) const
{
	return IsItemOffered(Item) && Credits >= GetItemPrice(Item);
}

bool FLotGame::BuyItem(ELotItem Item)
{
	if (!CanBuyItem(Item))
	{
		return false;
	}
	Credits -= GetItemPrice(Item);
	bOwned[static_cast<int32>(Item)] = true;
	PlaceBoughtItem(Item);
	return true;
}

int32 FLotGame::GetNumItemsOwned() const
{
	int32 Count = 0;
	for (bool bItem : bOwned)
	{
		Count += bItem ? 1 : 0;
	}
	return Count;
}

double FLotGame::GetPlantNextPrice() const
{
	return PlantLevel < MaxPlantLevel ? static_cast<double>(LotBalance::PlantPrice[PlantLevel]) : -1.0;
}

bool FLotGame::CanBuyPlant() const
{
	return PlantLevel < MaxPlantLevel && Credits >= GetPlantNextPrice();
}

bool FLotGame::BuyPlant()
{
	if (!CanBuyPlant())
	{
		return false;
	}
	Credits -= GetPlantNextPrice();
	++PlantLevel;
	return true;
}

double FLotGame::GetPlantRatePerMin() const
{
	return LotBalance::PlantRateLPerMin[PlantLevel];
}

double FLotGame::GetPlantStorage() const
{
	return LotBalance::PlantStorageMinutes * GetPlantRatePerMin();
}

FLotEvents FLotGame::Tick(double Dt)
{
	FLotEvents Events;
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return Events;
	}

	double Remaining = FMath::Min(Dt, MaxTickSeconds);
	while (Remaining > 0.0)
	{
		const double Step = FMath::Min(Remaining, LotBalance::MaxStepSeconds);
		PlaytimeSeconds += Step;
		PlantStock = FMath::Min(PlantStock + GetPlantRatePerMin() / 60.0 * Step, GetPlantStorage());
		SwapRemaining = FMath::Max(0.0, SwapRemaining - Step);
		// In the order of the numbers: robots at a station share the stock in that order.
		const int32 NumPavers = GetNumAutoPavers();
		for (int32 Paver = 0; Paver < NumPavers; ++Paver)
		{
			TickAutoPaver(Paver, Step, Events);
		}
		Remaining -= Step;
	}

	if (Grid.IsFluidActive())
	{
		// Fixed steps, so that the asphalt runs the same on every machine; a long frame does not make it catch up.
		FluidClock += FMath::Min(Dt, MaxTickSeconds);
		int32 Steps = 0;
		while (FluidClock >= LotBalance::FluidStepSeconds && Steps < LotBalance::FluidMaxStepsPerTick && Grid.IsFluidActive())
		{
			FluidClock -= LotBalance::FluidStepSeconds;
			++Steps;
			Events.Append(ApplyPour(Grid.StepFluid()));
		}
		FluidClock = FMath::Min(FluidClock, LotBalance::FluidStepSeconds);
	}
	else
	{
		FluidClock = 0.0;
	}
	return Events;
}

FLotEvents FLotGame::SettleFluid()
{
	FluidClock = 0.0;
	return ApplyPour(Grid.SettleFluid(LotBalance::FluidSettleSeconds));
}

bool FLotGame::IsExitOpen() const
{
	return IsUnlocked(ELotUnlock::Exit);
}

void FLotGame::SetWon()
{
	if (IsExitOpen())
	{
		bWon = true;
	}
}

int32 FLotGame::GetPaverUpgradeLevel(int32 Paver, ELotPaverUpgrade Upgrade) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot && IsValidPaverUpgrade(Upgrade) ? Robot->Upgrade[static_cast<int32>(Upgrade)] : -1;
}

double FLotGame::GetPaverUpgradePrice(int32 Paver, ELotPaverUpgrade Upgrade) const
{
	const int32 Level = GetPaverUpgradeLevel(Paver, Upgrade);
	return Level >= 0 && Level < MaxPaverUpgradeLevel ? static_cast<double>(LotBalance::PaverUpgradePrice[static_cast<int32>(Upgrade)][Level]) : -1.0;
}

bool FLotGame::CanBuyPaverUpgrade(int32 Paver, ELotPaverUpgrade Upgrade) const
{
	const double Price = GetPaverUpgradePrice(Paver, Upgrade);
	return Price >= 0.0 && Credits >= Price;
}

bool FLotGame::BuyPaverUpgrade(int32 Paver, ELotPaverUpgrade Upgrade)
{
	if (!CanBuyPaverUpgrade(Paver, Upgrade))
	{
		return false;
	}
	Credits -= GetPaverUpgradePrice(Paver, Upgrade);
	++Pavers[Paver].Upgrade[static_cast<int32>(Upgrade)];
	return true;
}

double FLotGame::GetAutoPaverRateCellsPerMin(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? LotBalance::AutoPaverRateCellsPerMin[Robot->Upgrade[static_cast<int32>(ELotPaverUpgrade::Speed)]] : 0.0;
}

int32 FLotGame::GetNumAutoPavers() const
{
	// The items require each other in order, so the owned robots are always the first ones.
	int32 Count = 0;
	while (Count < MaxAutoPavers && Owns(AutoPaverItem[Count]))
	{
		++Count;
	}
	return Count;
}

const FLotPaver* FLotGame::OwnedPaver(int32 Paver) const
{
	return Paver >= 0 && Paver < GetNumAutoPavers() ? &Pavers[Paver] : nullptr;
}

bool FLotGame::AssignAutoPaverSector(int32 Paver, int32 Sector)
{
	if (!OwnedPaver(Paver) || Sector < 0 || Sector >= AsphaltLot::NumSectors)
	{
		return false;
	}
	// A sector has one robot: whoever had it is sent back to waiting, keeping its tank and its place.
	for (int32 Other = 0; Other < MaxAutoPavers; ++Other)
	{
		if (Other != Paver && Pavers[Other].Sector == Sector)
		{
			Pavers[Other].Sector = INDEX_NONE;
			Pavers[Other].Step = 0;
			Pavers[Other].Budget = 0.0;
			Pavers[Other].Phase = ELotPaverPhase::Idle;
		}
	}
	FLotPaver& Robot = Pavers[Paver];
	Robot.Sector = Sector;
	Robot.Step = 0;
	Robot.Budget = 0.0;
	// An empty tank goes to a station first; with something in it the robot starts at once and fetches more when it runs dry.
	Robot.Phase = Robot.Tank > LotBalance::AutoPaverTankEmptyL ? ELotPaverPhase::ToSector : ELotPaverPhase::ToStation;
	return true;
}

int32 FLotGame::FindAutoPaverInSector(int32 Sector) const
{
	for (int32 Paver = 0; Paver < GetNumAutoPavers(); ++Paver)
	{
		if (Pavers[Paver].Sector == Sector && Sector != INDEX_NONE)
		{
			return Paver;
		}
	}
	return INDEX_NONE;
}

bool FLotGame::IsAutoPaverStarved(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot && Robot->Phase == ELotPaverPhase::Loading && PlantStock <= 0.0;
}

int32 FLotGame::GetAutoPaverSector(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? Robot->Sector : FLotPaver().Sector;
}

int32 FLotGame::GetAutoPaverStep(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? Robot->Step : FLotPaver().Step;
}

FVector2D FLotGame::GetAutoPaverPos(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? Robot->Pos : FLotPaver().Pos;
}

bool FLotGame::IsAutoPaverTravelling(int32 Paver) const
{
	const ELotPaverPhase Phase = GetAutoPaverPhase(Paver);
	return Phase == ELotPaverPhase::ToSector || Phase == ELotPaverPhase::ToStation;
}

ELotPaverPhase FLotGame::GetAutoPaverPhase(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? Robot->Phase : FLotPaver().Phase;
}

double FLotGame::GetAutoPaverTank(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? Robot->Tank : FLotPaver().Tank;
}

double FLotGame::GetAutoPaverTankCapacity(int32 Paver) const
{
	const FLotPaver* Robot = OwnedPaver(Paver);
	return Robot ? TankCapacityAt(Robot->Upgrade[static_cast<int32>(ELotPaverUpgrade::Capacity)]) : 0.0;
}

void FLotGame::SetAutoPaverAuto(bool bAuto)
{
	bAutoPaverAuto = bAuto;
	for (FLotPaver& Robot : Pavers)
	{
		Robot.SearchCooldown = 0.0;
	}
}

int32 FLotGame::FindSectorToPave(FVector2D From) const
{
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
	{
		const double Distance = FVector2D::DistSquared(From, AsphaltLot::SectorCentre(Sector));
		if (Distance >= BestDistance || FindAutoPaverInSector(Sector) != INDEX_NONE)
		{
			continue; // not nearer than the best, or another robot has it: no need to look inside
		}
		for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
		{
			if (!Grid.IsCellComplete(AsphaltLot::SectorCell(Sector, Step)))
			{
				Best = Sector;
				BestDistance = Distance;
				break;
			}
		}
	}
	return Best;
}

int32 FLotGame::GetNumStations() const
{
	return 1 + ((OwnsMovable(ELotMovable::Station) && Held != ELotMovable::Station) ? 1 : 0);
}

FVector2D FLotGame::GetStationPos(int32 Index) const
{
	return (Index == 1 && GetNumStations() == 2) ? WorldState.Movables[static_cast<int32>(ELotMovable::Station)].Pos : LotLayout::VehicleStationPos;
}

int32 FLotGame::FindNearestStation(FVector2D From) const
{
	int32 Best = 0;
	double BestDistance = FVector2D::DistSquared(From, GetStationPos(0));
	for (int32 I = 1; I < GetNumStations(); ++I)
	{
		const double Distance = FVector2D::DistSquared(From, GetStationPos(I));
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = I;
		}
	}
	return Best;
}

bool FLotGame::IsAtStation(FVector2D Pos) const
{
	for (int32 I = 0; I < GetNumStations(); ++I)
	{
		if (FVector2D::Distance(Pos, GetStationPos(I)) <= LotBalance::VehicleStationRadiusCm)
		{
			return true;
		}
	}
	return false;
}

int32 FLotGame::GetCarLevel() const
{
	if (!Owns(ELotItem::Car))
	{
		return -1;
	}
	// The tiers are bought one after another; the level is the last unbroken one.
	int32 Level = 0;
	for (const ELotItem Tier : {ELotItem::CarT2, ELotItem::CarT3, ELotItem::CarT4})
	{
		if (!Owns(Tier))
		{
			break;
		}
		++Level;
	}
	return Level;
}

double FLotGame::GetCarTankCapacity() const
{
	const int32 Level = GetCarLevel();
	return Level >= 0 ? LotBalance::CarTankL[Level] : 0.0;
}

double FLotGame::LoadCar(double Dt)
{
	if (GetCarLevel() < 0 || !(Dt > 0.0) || !FMath::IsFinite(Dt) || !IsAtStation(WorldState.CarPos))
	{
		return 0.0;
	}
	const double Moved = FMath::Max(0.0, FMath::Min3(LotBalance::VehicleLoadRateLPerS * FMath::Min(Dt, MaxTickSeconds), GetCarTankCapacity() - CarTank, PlantStock));
	CarTank += Moved;
	PlantStock -= Moved;
	return Moved;
}

FLotEvents FLotGame::PourFromCar(FVector2D A, FVector2D B)
{
	// Without a known heading the bar stands across the way it travels.
	const FVector2D Axis = (B - A).GetSafeNormal();
	return PourFromCar(A, B, Axis, Axis);
}

FLotEvents FLotGame::PourFromCar(FVector2D A, FVector2D B, FVector2D ForwardA, FVector2D ForwardB)
{
	const int32 Level = GetCarLevel();
	if (Level < 0 || !IsFinitePoint(A) || !IsFinitePoint(B) || !IsFinitePoint(ForwardA) || !IsFinitePoint(ForwardB))
	{
		return FLotEvents();
	}
	const double Width = LotBalance::CarWidthCm[Level];
	const FInstantPour Instant(Grid);
	const FLotPour Pour = Grid.StampSweep(A, B, ForwardA, ForwardB, Width, CarTank);
	CarTank = FMath::Max(0.0, CarTank - Pour.LitresUsed);
	return ApplyPour(Pour);
}

namespace
{
	/** Moves Pos towards Target by Reach; true once it is there. */
	bool DriveTowards(FVector2D& Pos, const FVector2D& Target, double Reach)
	{
		const double Distance = FVector2D::Distance(Pos, Target);
		if (Reach >= Distance)
		{
			Pos = Target;
			return true;
		}
		Pos += (Target - Pos) * (Reach / Distance);
		return false;
	}
}

void FLotGame::TickAutoPaver(int32 Paver, double Step, FLotEvents& Events)
{
	FLotPaver& Robot = Pavers[Paver];
	const double Reach = LotBalance::AutoPaverSpeedCmS * Step;
	switch (Robot.Phase)
	{
	case ELotPaverPhase::Idle:
		if (bAutoPaverAuto)
		{
			Robot.SearchCooldown -= Step;
			if (Robot.SearchCooldown <= 0.0)
			{
				const int32 Sector = FindSectorToPave(Robot.Pos);
				if (Sector != INDEX_NONE)
				{
					AssignAutoPaverSector(Paver, Sector);
				}
				else
				{
					Robot.SearchCooldown = AutoPaverSearchSeconds;
				}
			}
		}
		return;
	case ELotPaverPhase::Count:
		return;
	case ELotPaverPhase::ToStation:
		if (DriveTowards(Robot.Pos, GetStationPos(FindNearestStation(Robot.Pos)), Reach))
		{
			Robot.Phase = ELotPaverPhase::Loading;
		}
		return;
	case ELotPaverPhase::Loading:
	{
		const double Capacity = GetAutoPaverTankCapacity(Paver);
		const double Moved = FMath::Max(0.0, FMath::Min3(LotBalance::VehicleLoadRateLPerS * Step, Capacity - Robot.Tank, PlantStock));
		Robot.Tank += Moved;
		PlantStock -= Moved;
		// Full, or the plant has nothing more to give and the tank holds enough to be worth the drive: the stock of a small plant
		// is smaller than the tank, so waiting for a full tank would be waiting for ever.
		if (Robot.Tank >= Capacity - LoadSlackL || (PlantStock <= LoadSlackL && Robot.Tank >= LotBalance::AutoPaverMinLoadL))
		{
			Robot.Phase = ELotPaverPhase::ToSector;
		}
		return;
	}
	case ELotPaverPhase::ToSector:
		// After a trip to the station it carries on with the cell it left.
		if (DriveTowards(Robot.Pos, AsphaltLot::CellCentre(AsphaltLot::SectorCell(Robot.Sector, Robot.Step)), Reach))
		{
			Robot.Phase = ELotPaverPhase::Working;
		}
		return;
	case ELotPaverPhase::Working:
		break;
	}

	const double Earned = GetAutoPaverRateCellsPerMin(Paver) * LotBalance::LitresPerCell * LotBalance::CompletionThreshold / 60.0 * Step;
	Robot.Budget = FMath::Min3(Robot.Budget + Earned, LotBalance::AutoPaverBudgetCapL, Robot.Tank);
	while (Robot.Step < AsphaltLot::CellsPerSector)
	{
		const int32 Cell = AsphaltLot::SectorCell(Robot.Sector, Robot.Step);
		const FVector2D Centre = AsphaltLot::CellCentre(Cell);
		// The walk starts where the machine stands and continues from the centre of the cell before.
		const FVector2D From = Robot.Step > 0 ? AsphaltLot::CellCentre(AsphaltLot::SectorCell(Robot.Sector, Robot.Step - 1)) : Robot.Pos;
		Robot.Pos = Centre;
		if (!Grid.IsCellComplete(Cell))
		{
			if (Robot.Budget > 0.0)
			{
				const FInstantPour Instant(Grid);
				const FLotPour Pour = Grid.DepositIntoCell(Cell, Robot.Budget, From);
				Robot.Budget = FMath::Max(0.0, Robot.Budget - Pour.LitresUsed);
				Robot.Tank = FMath::Max(0.0, Robot.Tank - Pour.LitresUsed);
				Events.Append(ApplyPour(Pour));
			}
			if (!Grid.IsCellComplete(Cell))
			{
				if (Robot.Tank <= LotBalance::AutoPaverTankEmptyL)
				{
					// Dry: back to the nearest station for more.
					Robot.Budget = 0.0;
					Robot.Phase = ELotPaverPhase::ToStation;
				}
				return;
			}
		}
		++Robot.Step;
	}
	Robot.Sector = INDEX_NONE;
	Robot.Step = 0;
	Robot.Budget = 0.0;
	Robot.Phase = ELotPaverPhase::Idle;
}

void FLotGame::DebugGive(double CreditsToAdd)
{
	Credits += CreditsToAdd;
}

void FLotGame::DebugGrantItem(ELotItem Item)
{
	if (IsValidItem(Item))
	{
		const bool bWasOwned = Owns(Item);
		bOwned[static_cast<int32>(Item)] = true;
		// A machine that stands somewhere already is not carried back to its spawn.
		if (!bWasOwned || (!IsPaverItem(Item) && Item != ELotItem::Car))
		{
			PlaceBoughtItem(Item);
		}
	}
}

void FLotGame::DebugSetPaverUpgrade(int32 Paver, ELotPaverUpgrade Upgrade, int32 Level)
{
	if (!OwnedPaver(Paver) || !IsValidPaverUpgrade(Upgrade))
	{
		return;
	}
	FLotPaver& Robot = Pavers[Paver];
	Robot.Upgrade[static_cast<int32>(Upgrade)] = FMath::Clamp(Level, 0, MaxPaverUpgradeLevel);
	// A lowered capacity cuts what the robot holds, and the work budget can never be above the tank (the loader rejects such a state).
	Robot.Tank = FMath::Min(Robot.Tank, GetAutoPaverTankCapacity(Paver));
	Robot.Budget = FMath::Min(Robot.Budget, Robot.Tank);
}

void FLotGame::DebugSetPlantLevel(int32 Level)
{
	if (Level < 0 || Level > MaxPlantLevel)
	{
		return;
	}
	PlantLevel = Level;
	PlantStock = FMath::Min(PlantStock, GetPlantStorage());
}

void FLotGame::DebugSetPlantStock(double Litres)
{
	PlantStock = FMath::Clamp(Litres, 0.0, GetPlantStorage());
}

FLotEvents FLotGame::DebugCompleteCell(int32 Cell)
{
	if (Cell < 0 || Cell >= AsphaltLot::NumCells || Grid.IsCellComplete(Cell))
	{
		return FLotEvents();
	}
	return ApplyPour(Grid.CompleteCell(Cell));
}

FLotEvents FLotGame::DebugCompleteCells(int32 Count)
{
	FLotPour Total;
	for (int32 Cell = 0; Cell < AsphaltLot::NumCells && Count > 0; ++Cell)
	{
		if (Grid.IsCellComplete(Cell))
		{
			continue;
		}
		const FLotPour Pour = Grid.CompleteCell(Cell);
		Total.LitresUsed += Pour.LitresUsed;
		Total.TexelsChanged += Pour.TexelsChanged;
		Total.CompletedCells.Append(Pour.CompletedCells);
		--Count;
	}
	return ApplyPour(Total);
}

void FLotGame::DebugFillHand()
{
	HandLitres[static_cast<int32>(Hand)] = GetHandCapacity(Hand);
}

void FLotGame::DebugFillTanks()
{
	for (int32 Paver = 0; Paver < GetNumAutoPavers(); ++Paver)
	{
		Pavers[Paver].Tank = GetAutoPaverTankCapacity(Paver);
	}
	CarTank = GetCarTankCapacity();
}

bool FLotGame::CanSelectHand(ELotHand NewHand) const
{
	switch (NewHand)
	{
	case ELotHand::Bowl:
		return true;
	case ELotHand::Bucket:
	case ELotHand::Canister:
	case ELotHand::Tub:
		return Owns(VesselItem(NewHand));
	default:
		return false;
	}
}

bool FLotGame::IsVessel(ELotHand Item)
{
	return Item == ELotHand::Bucket || Item == ELotHand::Canister || Item == ELotHand::Tub;
}

ELotItem FLotGame::VesselItem(ELotHand Vessel)
{
	switch (Vessel)
	{
	case ELotHand::Bucket: return ELotItem::Bucket;
	case ELotHand::Canister: return ELotItem::Canister;
	case ELotHand::Tub: return ELotItem::Tub;
	default: return ELotItem::Count;
	}
}

bool FLotGame::SelectHand(ELotHand NewHand)
{
	if (!CanSelectHand(NewHand))
	{
		return false;
	}
	if (NewHand == Hand)
	{
		return true;
	}
	if (!HandsFree())
	{
		return false;
	}
	Hand = NewHand;
	StreamBudget = 0.0;
	StrokeTimer = 0.0;
	SwapRemaining = IsUnlocked(ELotUnlock::QuickSwap) ? 0.0 : LotBalance::SwapSeconds;
	return true;
}

double FLotGame::GetHandLitres(ELotHand Item) const
{
	return IsValidHand(Item) ? HandLitres[static_cast<int32>(Item)] : 0.0;
}

double FLotGame::GetHandCapacity(ELotHand Item) const
{
	return IsValidHand(Item) ? LotBalance::HandCapacityL[static_cast<int32>(Item)] : 0.0;
}

double FLotGame::ScoopFromTrough(double Dt)
{
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return 0.0;
	}
	// Only the bowl is filled with the spoon; the vessels are filled at a tap.
	const double Stroke = LotBalance::SpoonLitres;
	double& Content = HandLitres[static_cast<int32>(Hand)];
	if (Hand != ELotHand::Bowl || !HandsFree() || Content >= GetHandCapacity(Hand) || !(PlantStock > 0.0))
	{
		StrokeTimer = 0.0;
		return 0.0;
	}

	double Moved = 0.0;
	StrokeTimer += FMath::Min(Dt, MaxTickSeconds);
	while (StrokeTimer >= LotBalance::ScoopCycleSeconds)
	{
		StrokeTimer -= LotBalance::ScoopCycleSeconds;
		const double Amount = FMath::Min3(Stroke, GetHandCapacity(Hand) - Content, PlantStock);
		if (!(Amount > 0.0))
		{
			StrokeTimer = 0.0;
			break;
		}
		Content += Amount;
		PlantStock -= Amount;
		Moved += Amount;
	}
	return Moved;
}

double FLotGame::FillVessel(ELotHand Vessel, ELotSource Source, double Dt)
{
	if (!IsVessel(Vessel) || !(Dt > 0.0) || !FMath::IsFinite(Dt) || !Owns(VesselItem(Vessel)) || !HandsFree())
	{
		return 0.0;
	}
	// Per source: the litres it holds and the rate they leave at.
	double* SourceLitres = nullptr;
	double Rate = 0.0;
	switch (Source)
	{
	case ELotSource::Plant:
		SourceLitres = &PlantStock;
		Rate = LotBalance::DispenseRateLPerS(PlantLevel);
		break;
	case ELotSource::Trolley:
	case ELotSource::Cart:
	{
		const ELotMovable Movable = Source == ELotSource::Trolley ? ELotMovable::Trolley : ELotMovable::Cart;
		if (OwnsMovable(Movable) && Held != Movable)
		{
			SourceLitres = &MovableLitres[static_cast<int32>(Movable)];
			Rate = LotBalance::TapRateLPerS;
		}
		break;
	}
	default:
		break;
	}
	if (SourceLitres == nullptr)
	{
		return 0.0;
	}
	double& Content = HandLitres[static_cast<int32>(Vessel)];
	const double Moved = FMath::Max(0.0, FMath::Min3(Rate * FMath::Min(Dt, MaxTickSeconds), GetHandCapacity(Vessel) - Content, *SourceLitres));
	Content += Moved;
	*SourceLitres -= Moved;
	return Moved;
}

FLotEvents FLotGame::PourHand(FVector2D Aim, FVector2D From, double Dt)
{
	if (!HandsFree() || !(Dt > 0.0) || !FMath::IsFinite(Dt) || !IsFinitePoint(Aim) || !IsFinitePoint(From))
	{
		return FLotEvents();
	}
	const int32 Index = static_cast<int32>(Hand);
	const double Radius = LotBalance::PourRadiusCm[Index];
	double& Content = HandLitres[Index];
	const FLotPour Pour = SpendStream(StreamBudget, Content, LotBalance::HandPourRateLPerS[Index] * FMath::Min(Dt, MaxTickSeconds), [&](double Budget)
	{
		return Grid.PourDisc(Aim, Radius, Budget, From);
	});
	return ApplyPour(Pour);
}

FLotEvents FLotGame::DumpVessel(FVector2D Aim, FVector2D From)
{
	double& Content = HandLitres[static_cast<int32>(Hand)];
	const int32 Cell = IsFinitePoint(Aim) ? AsphaltLot::CellAt(Aim) : INDEX_NONE;
	if (!IsVessel(Hand) || !HandsFree() || Cell == INDEX_NONE || !(Content > 0.0))
	{
		return FLotEvents();
	}
	const FLotPour Pour = Grid.DepositIntoCell(Cell, Content, From);
	Content = FMath::Max(0.0, Content - Pour.LitresUsed);
	return ApplyPour(Pour);
}

bool FLotGame::HandsFree() const
{
	return !IsHandBusy() && Held == ELotMovable::Count;
}

bool FLotGame::OwnsMovable(ELotMovable Movable) const
{
	return IsValidMovable(Movable) && Owns(MovableItem(Movable));
}

bool FLotGame::Grab(ELotMovable Movable)
{
	if (!OwnsMovable(Movable) || Held != ELotMovable::Count || WorldState.bPlayerInCar)
	{
		return false;
	}
	Held = Movable;
	WorldState.Movables[static_cast<int32>(Movable)].bHeld = true;
	return true;
}

void FLotGame::Release(FVector2D Pos, double YawDeg)
{
	if (Held == ELotMovable::Count)
	{
		return;
	}
	FLotMovable& Movable = WorldState.Movables[static_cast<int32>(Held)];
	if (IsFinitePoint(Pos))
	{
		Movable.Pos = Pos;
	}
	if (FMath::IsFinite(YawDeg))
	{
		Movable.YawDeg = YawDeg;
	}
	Movable.bHeld = false;
	Held = ELotMovable::Count;
}

double FLotGame::GetMovableLitres(ELotMovable Movable) const
{
	return HoldsLitres(Movable) ? MovableLitres[static_cast<int32>(Movable)] : 0.0;
}

double FLotGame::GetMovableCapacity(ELotMovable Movable) const
{
	switch (Movable)
	{
	case ELotMovable::Trolley: return LotBalance::TrolleyCapacityL;
	case ELotMovable::Cart: return LotBalance::CartCapacityL;
	default: return 0.0;
	}
}

double FLotGame::FillMovable(ELotMovable Movable, double Dt)
{
	if (!HoldsLitres(Movable) || !OwnsMovable(Movable) || !(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return 0.0;
	}
	double& Content = MovableLitres[static_cast<int32>(Movable)];
	const double Moved = FMath::Max(0.0, FMath::Min3(LotBalance::DispenseRateLPerS(PlantLevel) * FMath::Min(Dt, MaxTickSeconds), GetMovableCapacity(Movable) - Content, PlantStock));
	Content += Moved;
	PlantStock -= Moved;
	return Moved;
}

FLotEvents FLotGame::LayStrip(FVector2D A, FVector2D B)
{
	if (Held != ELotMovable::Cart && Held != ELotMovable::Trolley)
	{
		return FLotEvents();
	}
	double& Content = MovableLitres[static_cast<int32>(Held)];
	const double Width = Held == ELotMovable::Cart ? LotBalance::CartStripWidthCm : LotBalance::TrolleyStripWidthCm;
	const FInstantPour Instant(Grid);
	const FLotPour Pour = Grid.Stamp(A, B, Width, Content);
	Content = FMath::Max(0.0, Content - Pour.LitresUsed);
	return ApplyPour(Pour);
}

void FLotGame::PlaceBoughtItem(ELotItem Item)
{
	for (int32 I = 0; I < NumMovables; ++I)
	{
		if (MovableItem(static_cast<ELotMovable>(I)) == Item)
		{
			WorldState.Movables[I].Pos = LotLayout::ItemSpawn(Item);
			WorldState.Movables[I].YawDeg = 0.0;
			WorldState.Movables[I].bHeld = false;
		}
	}
	const int32 Paver = PaverIndexOf(Item);
	if (Paver != INDEX_NONE)
	{
		Pavers[Paver] = FLotPaver();
		Pavers[Paver].Pos = LotLayout::PaverDock(Paver);
	}
	else if (Item == ELotItem::Car)
	{
		WorldState.CarPos = LotLayout::ItemSpawn(Item);
		WorldState.CarYawDeg = LotLayout::CarSpawnYawDeg;
	}
}

FLotPour FLotGame::SpendStream(double& Budget, double& Content, double Earned, TFunctionRef<FLotPour(double)> Spend)
{
	Budget = FMath::Min(Budget + Earned, Content);
	const FLotPour Pour = Spend(Budget);
	Content = FMath::Max(0.0, Content - Pour.LitresUsed);
	Budget = FMath::Max(0.0, Budget - Pour.LitresUsed);
	// A rest the grid could still pay for means the whole footprint is taken: the stream does not wait for it.
	if (Budget + StreamEpsilon >= FLotGrid::LitresPerTexel)
	{
		Budget = 0.0;
	}
	return Pour;
}

FLotEvents FLotGame::ApplyPour(const FLotPour& Pour)
{
	FLotEvents Events;
	Events.LitresPoured = Pour.LitresUsed;
	Events.CompletedCells = Pour.CompletedCells;
	Events.CreditsEarned = static_cast<double>(Pour.CompletedCells.Num()) * LotBalance::CreditsPerCell
		+ Pour.LitresUsed * LotBalance::CreditsPerLitre;
	TotalLitres += Pour.LitresUsed;
	Credits += Events.CreditsEarned;

	// The grid already holds the state after the pour; the state before it is the same minus the new cells.
	int32 NewInEntry[AsphaltLot::NumEntries] = {};
	for (int32 Cell : Pour.CompletedCells)
	{
		const int32 Entry = AsphaltLot::EntryOf(Cell);
		if (Entry != INDEX_NONE)
		{
			++NewInEntry[Entry];
		}
	}
	const int32 CellsBefore = GetCompletedCells() - Pour.CompletedCells.Num();
	for (int32 Entry = 0; Entry < AsphaltLot::NumEntries; ++Entry)
	{
		const bool bCompleteAfter = IsEntryComplete(Entry);
		const bool bCompleteBefore = bCompleteAfter && NewInEntry[Entry] == 0;
		if (bCompleteAfter && !bCompleteBefore)
		{
			Events.CompletedEntries.Add(Entry);
		}
		const bool bUnlockedAfter = IsUnlocked(static_cast<ELotUnlock>(Entry));
		const bool bUnlockedBefore = bCompleteBefore && CellsBefore >= LotBalance::UnlockCells[Entry];
		if (bUnlockedAfter && !bUnlockedBefore)
		{
			Events.Unlocked.Add(static_cast<ELotUnlock>(Entry));
			Events.bExitOpened = Events.bExitOpened || static_cast<ELotUnlock>(Entry) == ELotUnlock::Exit;
		}
	}
	return Events;
}

TArray<uint8> FLotGame::SaveToBytes() const
{
	TArray<uint8> Bytes;
	Bytes.Reserve(8192);
	LotSaveIO::FWriter W(Bytes);

	W.WriteU32(SaveMagic);
	W.WriteI32(SaveVersion);
	W.WriteI32(AsphaltLot::NumCells);
	W.WriteI32(FLotGrid::Size);

	W.WriteDouble(Credits);
	W.WriteDouble(PlaytimeSeconds);
	W.WriteDouble(TotalLitres);

	W.WriteI32(PlantLevel);
	W.WriteDouble(PlantStock);

	for (const bool bItem : bOwned)
	{
		W.WriteBool(bItem);
	}

	W.WriteI32(static_cast<int32>(Hand));
	for (int32 I = 0; I < NumLitreHands; ++I)
	{
		W.WriteDouble(HandLitres[I]);
	}
	W.WriteDouble(SwapRemaining);

	W.WriteDouble(MovableLitres[static_cast<int32>(ELotMovable::Trolley)]);
	W.WriteDouble(MovableLitres[static_cast<int32>(ELotMovable::Cart)]);
	W.WriteI32(static_cast<int32>(Held));

	for (const FLotPaver& Robot : Pavers)
	{
		W.WriteI32(Robot.Upgrade[static_cast<int32>(ELotPaverUpgrade::Speed)]);
		W.WriteI32(Robot.Upgrade[static_cast<int32>(ELotPaverUpgrade::Capacity)]);
		W.WriteI32(Robot.Sector);
		W.WriteI32(Robot.Step);
		W.WriteVector(Robot.Pos);
		W.WriteI32(static_cast<int32>(Robot.Phase));
		W.WriteDouble(Robot.Budget);
		W.WriteDouble(Robot.Tank);
	}
	W.WriteDouble(CarTank);

	W.WriteBool(bWon);

	W.WriteVector(WorldState.PlayerPos);
	W.WriteDouble(WorldState.PlayerYawDeg);
	for (const FLotMovable& Movable : WorldState.Movables)
	{
		W.WriteVector(Movable.Pos);
		W.WriteDouble(Movable.YawDeg);
	}
	W.WriteVector(WorldState.CarPos);
	W.WriteDouble(WorldState.CarYawDeg);
	W.WriteBool(WorldState.bPlayerInCar);
	W.WriteBool(bAutoPaverAuto);

	Grid.Write(Bytes);
	return Bytes;
}

bool FLotGame::LoadFromBytes(TConstArrayView<uint8> Bytes)
{
	LotSaveIO::FReader R(Bytes);
	uint32 Magic = 0;
	int32 Version = 0;
	int32 NumCells = 0;
	int32 Side = 0;
	if (!R.ReadU32(Magic) || Magic != SaveMagic || !R.ReadI32(Version) || (Version != SaveVersion && Version != 6 && Version != 5 && Version != 4)
		|| !R.ReadI32(NumCells) || NumCells != AsphaltLot::NumCells || !R.ReadI32(Side) || Side != FLotGrid::Size)
	{
		return false;
	}

	// Everything is parsed into a temporary game and swapped in only when the whole blob is valid.
	FLotGame Loaded;

	if (!R.ReadFinite(Loaded.Credits) || Loaded.Credits < 0.0
		|| !R.ReadFinite(Loaded.PlaytimeSeconds) || Loaded.PlaytimeSeconds < 0.0
		|| !R.ReadFinite(Loaded.TotalLitres) || Loaded.TotalLitres < 0.0)
	{
		return false;
	}

	if (!R.ReadI32(Loaded.PlantLevel) || Loaded.PlantLevel < 0 || Loaded.PlantLevel > MaxPlantLevel
		|| !R.ReadFinite(Loaded.PlantStock) || Loaded.PlantStock < 0.0 || Loaded.PlantStock > Loaded.GetPlantStorage() + LoadSlackL)
	{
		return false;
	}

	// Version 7 stores one flag per item. Before it the two speed items AutoPaverFast and AutoPaverFaster stood between AutoPaver and Car: version 6 stored
	// 15 flags in that order, version 5 stored 13 (no robots 2 and 3), version 4 stored 11 (the old wide bar is CarT2, CarT3 and CarT4 do not exist).
	// The old flags are mapped to the items by name; the two speed items were global, so they become speed level 1 or 2 of every robot that is owned.
	int32 OldSpeedLevel = 0;
	if (Version >= 7)
	{
		for (int32 I = 0; I < NumItems; ++I)
		{
			if (!R.ReadBool(Loaded.bOwned[I]))
			{
				return false;
			}
		}
	}
	else
	{
		const int32 NumStoredItems = Version == 6 ? OldItem::Num : (Version == 5 ? OldItem::Num - 2 : OldItem::Num - 4);
		bool bOldOwned[OldItem::Num] = {};
		for (int32 I = 0; I < NumStoredItems; ++I)
		{
			if (!R.ReadBool(bOldOwned[I]))
			{
				return false;
			}
			if (OldItem::Map(I) != ELotItem::Count)
			{
				Loaded.bOwned[static_cast<int32>(OldItem::Map(I))] = bOldOwned[I];
			}
		}
		// Each speed item needed the one before it.
		if ((bOldOwned[OldItem::AutoPaverFast] && !bOldOwned[OldItem::AutoPaver]) || (bOldOwned[OldItem::AutoPaverFaster] && !bOldOwned[OldItem::AutoPaverFast]))
		{
			return false;
		}
		OldSpeedLevel = bOldOwned[OldItem::AutoPaverFaster] ? 2 : (bOldOwned[OldItem::AutoPaverFast] ? 1 : 0);
	}
	for (int32 I = 0; I < NumItems; ++I)
	{
		const ELotItem Required = LotBalance::ItemRequires[I];
		if (Loaded.bOwned[I] && Required != ELotItem::Count && !Loaded.Owns(Required))
		{
			return false;
		}
	}

	int32 HandValue = 0;
	if (!R.ReadI32(HandValue) || HandValue < 0 || HandValue >= static_cast<int32>(ELotHand::Count))
	{
		return false;
	}
	Loaded.Hand = static_cast<ELotHand>(HandValue);
	if (!Loaded.CanSelectHand(Loaded.Hand))
	{
		return false;
	}
	for (int32 I = 0; I < NumLitreHands; ++I)
	{
		if (!R.ReadFinite(Loaded.HandLitres[I]) || Loaded.HandLitres[I] < 0.0 || Loaded.HandLitres[I] > LotBalance::HandCapacityL[I] + LoadSlackL)
		{
			return false;
		}
		// A vessel that is not owned has never been filled.
		if (Loaded.HandLitres[I] > 0.0 && !Loaded.CanSelectHand(static_cast<ELotHand>(I)))
		{
			return false;
		}
	}
	if (!R.ReadFinite(Loaded.SwapRemaining) || Loaded.SwapRemaining < 0.0 || Loaded.SwapRemaining > LotBalance::SwapSeconds)
	{
		return false;
	}

	for (const ELotMovable Vessel : {ELotMovable::Trolley, ELotMovable::Cart})
	{
		double& Litres = Loaded.MovableLitres[static_cast<int32>(Vessel)];
		if (!R.ReadFinite(Litres) || Litres < 0.0 || Litres > Loaded.GetMovableCapacity(Vessel) + LoadSlackL || (Litres > 0.0 && !Loaded.OwnsMovable(Vessel)))
		{
			return false;
		}
	}
	int32 HeldValue = 0;
	if (!R.ReadI32(HeldValue) || !IsValidHeld(HeldValue))
	{
		return false;
	}
	Loaded.Held = static_cast<ELotMovable>(HeldValue);

	// Versions 4 and 5 kept one robot, which is robot 0; the others are empty. Version 6 and later keep a block for each of them; from version 7 it starts with the two upgrade levels.
	const int32 NumStoredPavers = Version >= 6 ? MaxAutoPavers : 1;
	for (int32 Paver = 0; Paver < NumStoredPavers; ++Paver)
	{
		FLotPaver& Robot = Loaded.Pavers[Paver];
		int32 PhaseValue = 0;
		if (Version >= 7)
		{
			for (int32 Kind = 0; Kind < NumPaverUpgrades; ++Kind)
			{
				// The levels belong to a robot that is owned; any other robot is at level 0.
				if (!R.ReadI32(Robot.Upgrade[Kind]) || Robot.Upgrade[Kind] < 0 || Robot.Upgrade[Kind] > MaxPaverUpgradeLevel
					|| (Robot.Upgrade[Kind] > 0 && !Loaded.Owns(AutoPaverItem[Paver])))
				{
					return false;
				}
			}
		}
		else if (Loaded.Owns(AutoPaverItem[Paver]))
		{
			Robot.Upgrade[static_cast<int32>(ELotPaverUpgrade::Speed)] = OldSpeedLevel;
		}
		if (!R.ReadI32(Robot.Sector) || (Robot.Sector != INDEX_NONE && (Robot.Sector < 0 || Robot.Sector >= AsphaltLot::NumSectors))
			|| !R.ReadI32(Robot.Step) || Robot.Step < 0 || Robot.Step >= AsphaltLot::CellsPerSector
			|| !R.ReadVector(Robot.Pos)
			|| !R.ReadI32(PhaseValue) || PhaseValue < 0 || PhaseValue >= static_cast<int32>(ELotPaverPhase::Count)
			|| !R.ReadFinite(Robot.Budget) || Robot.Budget < 0.0 || Robot.Budget > LotBalance::AutoPaverBudgetCapL
			|| !R.ReadFinite(Robot.Tank) || Robot.Tank < 0.0 || Robot.Tank > TankCapacityAt(Robot.Upgrade[static_cast<int32>(ELotPaverUpgrade::Capacity)]) + LoadSlackL)
		{
			return false;
		}
		Robot.Phase = static_cast<ELotPaverPhase>(PhaseValue);
		// Without a sector the robot idles, and only an owned robot can have a sector or fuel. A sector has one robot.
		if (Robot.Sector == INDEX_NONE
			? (Robot.Step != 0 || Robot.Phase != ELotPaverPhase::Idle || Robot.Budget != 0.0)
			: (!Loaded.Owns(AutoPaverItem[Paver]) || Robot.Phase == ELotPaverPhase::Idle || Robot.Budget > Robot.Tank + LoadSlackL))
		{
			return false;
		}
		if (Robot.Tank > 0.0 && !Loaded.Owns(AutoPaverItem[Paver]))
		{
			return false;
		}
		for (int32 Other = 0; Other < Paver; ++Other)
		{
			if (Robot.Sector != INDEX_NONE && Loaded.Pavers[Other].Sector == Robot.Sector)
			{
				return false;
			}
		}
	}
	if (!R.ReadFinite(Loaded.CarTank) || Loaded.CarTank < 0.0 || Loaded.CarTank > Loaded.GetCarTankCapacity() + LoadSlackL)
	{
		return false;
	}

	if (!R.ReadBool(Loaded.bWon))
	{
		return false;
	}

	FLotWorldState& World = Loaded.WorldState;
	if (!R.ReadVector(World.PlayerPos) || !R.ReadFinite(World.PlayerYawDeg))
	{
		return false;
	}
	for (FLotMovable& Movable : World.Movables)
	{
		// The held flag is not stored; it follows from the held movable below.
		if (!R.ReadVector(Movable.Pos) || !R.ReadFinite(Movable.YawDeg))
		{
			return false;
		}
	}
	if (!R.ReadVector(World.CarPos) || !R.ReadFinite(World.CarYawDeg) || !R.ReadBool(World.bPlayerInCar))
	{
		return false;
	}
	if (!R.ReadBool(Loaded.bAutoPaverAuto))
	{
		return false;
	}
	if (World.bPlayerInCar && (!Loaded.Owns(ELotItem::Car) || Loaded.Held != ELotMovable::Count))
	{
		return false;
	}
	if (Loaded.Held != ELotMovable::Count)
	{
		if (!Loaded.OwnsMovable(Loaded.Held))
		{
			return false;
		}
		World.Movables[static_cast<int32>(Loaded.Held)].bHeld = true;
	}

	// The grid is the rest of the blob, to the last byte.
	TConstArrayView<uint8> GridBytes;
	int32 GridBytesRead = 0;
	if (!R.ReadBlock(R.Remaining(), GridBytes) || !Loaded.Grid.Read(GridBytes, GridBytesRead) || GridBytesRead != GridBytes.Num())
	{
		return false;
	}
	if (Loaded.bWon && !Loaded.IsExitOpen())
	{
		return false;
	}

	// What is saved is only the ground; whether the asphalt runs is the setting of the running game, not of the save.
	const bool bFluid = Grid.IsFluidEnabled();
	*this = MoveTemp(Loaded);
	Grid.SetFluidEnabled(bFluid);
	return true;
}
