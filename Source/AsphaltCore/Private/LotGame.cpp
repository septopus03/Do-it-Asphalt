// Copyright Do it Asphalt. All Rights Reserved.

#include "LotGame.h"

namespace
{
	/**
	 * Longest span a single Tick call simulates (one day). Only a guard against absurd Dt values such as 1e15,
	 * which would otherwise keep the loop running for ever; real callers pass a frame time.
	 */
	constexpr double MaxTickSeconds = 86400.0;
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

	/** Only the suitcase and the cart hold asphalt. */
	bool HoldsLitres(ELotMovable Movable)
	{
		return Movable == ELotMovable::Suitcase || Movable == ELotMovable::Cart;
	}

	/** The shop item behind a movable. */
	ELotItem MovableItem(ELotMovable Movable)
	{
		switch (Movable)
		{
		case ELotMovable::Suitcase: return ELotItem::Suitcase;
		case ELotMovable::Cart: return ELotItem::Cart;
		case ELotMovable::Station: return ELotItem::Station;
		default: return ELotItem::Finisher;
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
		TickAutoPaver(Step, Events);
		Remaining -= Step;
	}
	return Events;
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

int32 FLotGame::GetAutoPaverLevel() const
{
	if (!Owns(ELotItem::AutoPaver))
	{
		return -1;
	}
	return Owns(ELotItem::AutoPaverFaster) ? 2 : (Owns(ELotItem::AutoPaverFast) ? 1 : 0);
}

double FLotGame::GetAutoPaverRateCellsPerMin() const
{
	const int32 Level = GetAutoPaverLevel();
	return Level >= 0 ? LotBalance::AutoPaverRateCellsPerMin[Level] : 0.0;
}

bool FLotGame::AssignAutoPaverSector(int32 Sector)
{
	if (!Owns(ELotItem::AutoPaver) || Sector < 0 || Sector >= AsphaltLot::NumSectors)
	{
		return false;
	}
	AutoPaverSector = Sector;
	AutoPaverStep = 0;
	AutoPaverBudget = 0.0;
	bAutoPaverTravelling = true;
	return true;
}

int32 FLotGame::GetCarLevel() const
{
	if (!Owns(ELotItem::Car))
	{
		return -1;
	}
	return Owns(ELotItem::CarWideBar) ? 1 : 0;
}

FLotEvents FLotGame::PourFromCar(FVector2D A, FVector2D B)
{
	const int32 Level = GetCarLevel();
	if (Level < 0 || !IsFinitePoint(A) || !IsFinitePoint(B))
	{
		return FLotEvents();
	}
	const double Width = LotBalance::CarWidthCm[Level];
	FLotEvents Events = ApplyPour(Grid.FinishRaw(A, B, Width));
	const FLotPour Pour = Grid.Stamp(A, B, Width, PlantStock, ELotTexel::Done);
	PlantStock = FMath::Max(0.0, PlantStock - Pour.LitresUsed);
	Events.Append(ApplyPour(Pour));
	return Events;
}

void FLotGame::TickAutoPaver(double Step, FLotEvents& Events)
{
	if (AutoPaverSector == INDEX_NONE)
	{
		return;
	}
	if (bAutoPaverTravelling)
	{
		const FVector2D Target = AsphaltLot::CellCentre(AsphaltLot::SectorCell(AutoPaverSector, 0));
		const double Distance = FVector2D::Distance(AutoPaverPos, Target);
		const double Reach = LotBalance::AutoPaverSpeedCmS * Step;
		if (Reach >= Distance)
		{
			AutoPaverPos = Target;
			bAutoPaverTravelling = false;
		}
		else
		{
			AutoPaverPos += (Target - AutoPaverPos) * (Reach / Distance);
		}
		return;
	}

	const double Earned = GetAutoPaverRateCellsPerMin() * LotBalance::LitresPerCell * LotBalance::CompletionThreshold / 60.0 * Step;
	AutoPaverBudget = FMath::Min3(AutoPaverBudget + Earned, LotBalance::AutoPaverBudgetCapL, PlantStock);
	while (AutoPaverStep < AsphaltLot::CellsPerSector)
	{
		const int32 Cell = AsphaltLot::SectorCell(AutoPaverSector, AutoPaverStep);
		const FVector2D Centre = AsphaltLot::CellCentre(Cell);
		// The walk starts where the machine stands and continues from the centre of the cell before.
		const FVector2D From = AutoPaverStep > 0 ? AsphaltLot::CellCentre(AsphaltLot::SectorCell(AutoPaverSector, AutoPaverStep - 1)) : AutoPaverPos;
		AutoPaverPos = Centre;
		if (!Grid.IsCellComplete(Cell))
		{
			if (Grid.GetCellRaw(Cell) > 0)
			{
				const double Half = AsphaltLot::CellCm * 0.5 - LotBalance::StampPadCm;
				Events.Append(ApplyPour(Grid.FinishRaw(Centre - FVector2D(Half, 0.0), Centre + FVector2D(Half, 0.0), AsphaltLot::CellCm)));
			}
			if (!Grid.IsCellComplete(Cell) && AutoPaverBudget > 0.0)
			{
				const FLotPour Pour = Grid.DepositIntoCell(Cell, AutoPaverBudget, From, ELotTexel::Done);
				AutoPaverBudget = FMath::Max(0.0, AutoPaverBudget - Pour.LitresUsed);
				PlantStock = FMath::Max(0.0, PlantStock - Pour.LitresUsed);
				Events.Append(ApplyPour(Pour));
			}
			if (!Grid.IsCellComplete(Cell))
			{
				return;
			}
		}
		++AutoPaverStep;
	}
	AutoPaverSector = INDEX_NONE;
	AutoPaverStep = 0;
	AutoPaverBudget = 0.0;
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
		if (!bWasOwned || (Item != ELotItem::AutoPaver && Item != ELotItem::Car))
		{
			PlaceBoughtItem(Item);
		}
	}
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

bool FLotGame::CanSelectHand(ELotHand NewHand) const
{
	switch (NewHand)
	{
	case ELotHand::Bowl:
	case ELotHand::Trowel:
		return true;
	case ELotHand::Scoop:
		return Owns(ELotItem::Scoop);
	case ELotHand::Bucket:
		return Owns(ELotItem::Bucket);
	default:
		return false;
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
	const bool bVessel = Hand == ELotHand::Bowl || Hand == ELotHand::Scoop;
	// The scoop in hand lifts a full scoop; the bowl is filled with the spoon, or with the scoop once it is owned.
	const double Stroke = (Hand == ELotHand::Scoop || Owns(ELotItem::Scoop)) ? LotBalance::ScoopLitres : LotBalance::SpoonLitres;
	double& Content = HandLitres[static_cast<int32>(Hand)];
	if (!bVessel || !HandsFree() || Content >= GetHandCapacity(Hand) || !(PlantStock > 0.0))
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

double FLotGame::FillBucket(ELotSource Source, double Dt)
{
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt) || !Owns(ELotItem::Bucket) || Hand == ELotHand::Bucket || !HandsFree())
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
	case ELotSource::Suitcase:
	case ELotSource::Cart:
	{
		const ELotMovable Movable = Source == ELotSource::Suitcase ? ELotMovable::Suitcase : ELotMovable::Cart;
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
	double& Content = HandLitres[static_cast<int32>(ELotHand::Bucket)];
	const double Moved = FMath::Max(0.0, FMath::Min3(Rate * FMath::Min(Dt, MaxTickSeconds), GetHandCapacity(ELotHand::Bucket) - Content, *SourceLitres));
	Content += Moved;
	*SourceLitres -= Moved;
	return Moved;
}

FLotEvents FLotGame::PourHand(FVector2D Aim, FVector2D From, double Dt)
{
	const double Radius = (Hand == ELotHand::Bowl || Hand == ELotHand::Scoop) ? LotBalance::PourRadiusCm[static_cast<int32>(Hand)] : 0.0;
	if (!(Radius > 0.0) || !HandsFree() || !(Dt > 0.0) || !FMath::IsFinite(Dt) || !IsFinitePoint(Aim) || !IsFinitePoint(From))
	{
		return FLotEvents();
	}
	double& Content = HandLitres[static_cast<int32>(Hand)];
	const FLotPour Pour = SpendStream(StreamBudget, Content, LotBalance::HandPourRateLPerS * FMath::Min(Dt, MaxTickSeconds), [&](double Budget)
	{
		return Grid.PourDisc(Aim, Radius, Budget, From);
	});
	return ApplyPour(Pour);
}

FLotEvents FLotGame::DumpBucket(FVector2D Aim, FVector2D From)
{
	double& Content = HandLitres[static_cast<int32>(ELotHand::Bucket)];
	const int32 Cell = IsFinitePoint(Aim) ? AsphaltLot::CellAt(Aim) : INDEX_NONE;
	if (Hand != ELotHand::Bucket || !HandsFree() || Cell == INDEX_NONE || !(Content > 0.0))
	{
		return FLotEvents();
	}
	const FLotPour Pour = Grid.DepositIntoCell(Cell, Content, From, ELotTexel::Done);
	Content = FMath::Max(0.0, Content - Pour.LitresUsed);
	return ApplyPour(Pour);
}

FLotEvents FLotGame::Trowel(FVector2D A, FVector2D B)
{
	if (Hand != ELotHand::Trowel || !HandsFree())
	{
		return FLotEvents();
	}
	return ApplyPour(Grid.FinishRaw(A, B, LotBalance::TrowelWidthCm));
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
	DoseBudget = 0.0;
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
	case ELotMovable::Suitcase: return LotBalance::SuitcaseCapacityL;
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

FLotEvents FLotGame::DoseSuitcase(FVector2D Mouth, FVector2D From, double Dt)
{
	const ELotMovable Suitcase = ELotMovable::Suitcase;
	if (!OwnsMovable(Suitcase) || Held == Suitcase || !(Dt > 0.0) || !FMath::IsFinite(Dt) || !IsFinitePoint(Mouth) || !IsFinitePoint(From))
	{
		return FLotEvents();
	}
	const int32 Cell = AsphaltLot::CellAt(Mouth);
	if (Cell == INDEX_NONE || Grid.IsCellComplete(Cell) || Grid.GetCellDone(Cell) + Grid.GetCellRaw(Cell) >= FLotGrid::TexelsPerCell)
	{
		// Nothing can be laid here: the flap closes and the stream starts anew elsewhere.
		DoseBudget = 0.0;
		return FLotEvents();
	}
	double& Content = MovableLitres[static_cast<int32>(Suitcase)];
	const double Earned = LotBalance::SuitcaseDoseRateLPerS * FMath::Min(Dt, MaxTickSeconds);
	const FLotPour Pour = SpendStream(DoseBudget, Content, Earned, [&](double Budget)
	{
		return Grid.DepositIntoCell(Cell, Budget, From, ELotTexel::Raw);
	});
	return ApplyPour(Pour);
}

FLotEvents FLotGame::LayCartStrip(FVector2D A, FVector2D B)
{
	if (Held != ELotMovable::Cart)
	{
		return FLotEvents();
	}
	double& Content = MovableLitres[static_cast<int32>(ELotMovable::Cart)];
	const FLotPour Pour = Grid.Stamp(A, B, LotBalance::CartStripWidthCm, Content, ELotTexel::Raw);
	Content = FMath::Max(0.0, Content - Pour.LitresUsed);
	return ApplyPour(Pour);
}

FLotEvents FLotGame::FinisherPass(FVector2D A, FVector2D B)
{
	if (Held != ELotMovable::Finisher)
	{
		return FLotEvents();
	}
	return ApplyPour(Grid.FinishRaw(A, B, LotBalance::FinisherWidthCm));
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
	if (Item == ELotItem::AutoPaver)
	{
		AutoPaverPos = LotLayout::ItemSpawn(Item);
	}
	else if (Item == ELotItem::Car)
	{
		WorldState.CarPos = LotLayout::ItemSpawn(Item);
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
	Events.CreditsEarned = static_cast<double>(Pour.CompletedCells.Num()) * LotBalance::CreditsPerCell;
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
