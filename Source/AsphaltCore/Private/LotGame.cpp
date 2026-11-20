// Copyright Do it Asphalt. All Rights Reserved.

#include "LotGame.h"

namespace
{
	/**
	 * Longest span a single Tick call simulates (one day). Only a guard against absurd Dt values such as 1e15,
	 * which would otherwise keep the loop running for ever; real callers pass a frame time.
	 */
	constexpr double MaxTickSeconds = 86400.0;
	constexpr int32 NumItems = static_cast<int32>(ELotItem::Count);
	constexpr int32 NumUnlocks = static_cast<int32>(ELotUnlock::Count);
	constexpr int32 MaxPlantLevel = static_cast<int32>(UE_ARRAY_COUNT(LotBalance::PlantRateLPerMin)) - 1;

	static_assert(NumUnlocks == AsphaltLot::NumEntries, "unlock step i belongs to entry segment i");
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(LotBalance::PlantPrice)) == MaxPlantLevel, "one plant price per upgrade");

	bool IsValidItem(ELotItem Item)
	{
		return static_cast<int32>(Item) >= 0 && static_cast<int32>(Item) < NumItems;
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

void FLotGame::DebugGive(double CreditsToAdd)
{
	Credits += CreditsToAdd;
}

void FLotGame::DebugGrantItem(ELotItem Item)
{
	if (IsValidItem(Item))
	{
		bOwned[static_cast<int32>(Item)] = true;
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
