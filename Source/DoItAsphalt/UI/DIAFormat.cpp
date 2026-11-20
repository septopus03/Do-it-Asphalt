// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAFormat.h"
#include "LotBalance.h"
#include "LotMap.h"
#include "Game/DIARefill.h"
#include "Game/DIAWorldSubsystem.h"
#include "UI/DIAHudStyle.h"

#define LOCTEXT_NAMESPACE "DIAHud"

namespace
{
	/** Digits with "," between thousands. Written by hand: the texts must be the same on every machine and culture. */
	FString Grouped(int64 Value)
	{
		FString Digits = FString::Printf(TEXT("%lld"), Value < 0 ? -Value : Value);
		FString Out;
		for (int32 I = 0; I < Digits.Len(); ++I)
		{
			if (I > 0 && (Digits.Len() - I) % 3 == 0)
			{
				Out.AppendChar(TEXT(','));
			}
			Out.AppendChar(Digits[I]);
		}
		return Value < 0 ? TEXT("-") + Out : Out;
	}

	/** A number of tenths as "3.2", or "3" when there is no fraction. */
	FText TenthsText(int32 Tenths)
	{
		const FString Whole = Grouped(Tenths / 10);
		return FText::AsCultureInvariant(Tenths % 10 == 0 ? Whole : FString::Printf(TEXT("%s.%d"), *Whole, Tenths % 10));
	}

	/** Litres in whole tenths, rounded down (the epsilon keeps 3.2 from becoming 3.1999 -> 3.1). */
	int32 Tenths(double Litres)
	{
		return FMath::Max(0, FMath::FloorToInt(Litres * 10.0 + 1e-6));
	}

	int64 WholeCredits(double Value)
	{
		return FMath::Max<int64>(0, FMath::FloorToInt64(Value + 1e-9));
	}

	/** A rate or capacity from the balance tables as a whole number: 5200 -> "5,200". */
	FText Whole(double Value)
	{
		return FText::AsCultureInvariant(Grouped(FMath::RoundToInt64(Value)));
	}

	/** "150 → 300" followed by the unit. */
	FText Change(double From, double To, const FText& Unit)
	{
		return FText::Format(NSLOCTEXT("DIAShop", "Change", "{0} → {1} {2}"), Whole(From), Whole(To), Unit);
	}

	FText PerMinuteLitres() { return NSLOCTEXT("DIAShop", "LitresPerMinute", "l/min"); }
	FText PerMinuteCells() { return NSLOCTEXT("DIAShop", "CellsPerMinute", "cells/min"); }

	/** The vessel in the hand, or the suitcase or cart that is held: how much it holds and how much fits. False when there is none. */
	bool GaugeOf(const FLotGame& Game, double& OutHave, double& OutCapacity)
	{
		const ELotMovable Held = Game.GetHeld();
		if (Held == ELotMovable::Suitcase || Held == ELotMovable::Cart)
		{
			OutHave = Game.GetMovableLitres(Held);
			OutCapacity = Game.GetMovableCapacity(Held);
			return true;
		}
		if (Held != ELotMovable::Count)
		{
			return false;
		}
		OutHave = Game.GetHandLitres(Game.GetHand());
		OutCapacity = Game.GetHandCapacity(Game.GetHand());
		return OutCapacity > 0.0;
	}

	/** The first step that is not open yet; ELotUnlock::Count when all are. */
	ELotUnlock NextStep(const FLotGame& Game)
	{
		for (int32 I = 0; I < static_cast<int32>(ELotUnlock::Count); ++I)
		{
			if (!Game.IsUnlocked(static_cast<ELotUnlock>(I)))
			{
				return static_cast<ELotUnlock>(I);
			}
		}
		return ELotUnlock::Count;
	}

	/** What the item gives, from the balance tables: its size, its rate, or "from -> to" for an upgrade. */
	FText ItemValue(ELotItem Item)
	{
		switch (Item)
		{
		case ELotItem::Scoop: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::ScoopLitres)));
		case ELotItem::Bucket: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)])));
		case ELotItem::Suitcase: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::SuitcaseCapacityL)));
		case ELotItem::Cart: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::CartCapacityL)));
		case ELotItem::Station: return NSLOCTEXT("DIAShop", "StationValue", "Refill anywhere");
		case ELotItem::Finisher: return FText::Format(NSLOCTEXT("DIAShop", "Rate", "{0} {1}"), Whole(LotBalance::FinisherRateCellsPerMin), PerMinuteCells());
		case ELotItem::AutoPaver: return FText::Format(NSLOCTEXT("DIAShop", "Rate", "{0} {1}"), Whole(LotBalance::AutoPaverRateCellsPerMin[0]), PerMinuteCells());
		case ELotItem::AutoPaverFast: return Change(LotBalance::AutoPaverRateCellsPerMin[0], LotBalance::AutoPaverRateCellsPerMin[1], PerMinuteCells());
		case ELotItem::AutoPaverFaster: return Change(LotBalance::AutoPaverRateCellsPerMin[1], LotBalance::AutoPaverRateCellsPerMin[2], PerMinuteCells());
		case ELotItem::Car: return FText::Format(NSLOCTEXT("DIAShop", "Rate", "{0} {1}"), Whole(LotBalance::CarRateCellsPerMin[0]), PerMinuteCells());
		case ELotItem::CarWideBar: return Change(LotBalance::CarRateCellsPerMin[0], LotBalance::CarRateCellsPerMin[1], PerMinuteCells());
		default: return FText::GetEmpty();
		}
	}

	/** Why an item is not on offer, as a cheap number: 0 nothing, 1 its step is closed, 2 an item it upgrades is missing. */
	int32 LockKind(const FLotGame& Game, ELotItem Item)
	{
		const int32 Index = static_cast<int32>(Item);
		const ELotUnlock Step = LotBalance::ItemUnlock[Index];
		if (Step != ELotUnlock::Count && !Game.IsUnlocked(Step))
		{
			return 1;
		}
		const ELotItem Required = LotBalance::ItemRequires[Index];
		return Required != ELotItem::Count && !Game.Owns(Required) ? 2 : 0;
	}

	/** The text for it: "350 cells + entry 3" or "needs Auto Paver". */
	FText LockReason(const FLotGame& Game, ELotItem Item)
	{
		const int32 Index = static_cast<int32>(Item);
		const ELotUnlock Step = LotBalance::ItemUnlock[Index];
		if (LockKind(Game, Item) == 1)
		{
			const int32 StepIndex = static_cast<int32>(Step);
			// The entry is numbered from 1, as in the entry toast.
			return FText::Format(NSLOCTEXT("DIAShop", "LockedStep", "{0} cells + entry {1}"), DIAFormat::Count(LotBalance::UnlockCells[StepIndex]), DIAFormat::Count(StepIndex + 1));
		}
		const ELotItem Required = LotBalance::ItemRequires[Index];
		if (LockKind(Game, Item) == 2)
		{
			return FText::Format(NSLOCTEXT("DIAShop", "LockedItem", "needs {0}"), DIAFormat::ItemName(Required));
		}
		return FText::GetEmpty();
	}

	/** The plant's rate at a level, from the balance tables. */
	double PlantRate(int32 Level)
	{
		return LotBalance::PlantRateLPerMin[FMath::Clamp(Level, 0, static_cast<int32>(std::size(LotBalance::PlantRateLPerMin)) - 1)];
	}
}

FText DIAFormat::Count(int64 Value)
{
	return FText::AsCultureInvariant(Grouped(Value));
}

FText DIAFormat::Credits(double Value)
{
	return Count(WholeCredits(Value));
}

FText DIAFormat::Duration(double Seconds)
{
	const int64 Whole = FMath::Max<int64>(0, FMath::FloorToInt64(Seconds + 1e-9));
	const int64 Hours = Whole / 3600;
	const int64 Minutes = Whole / 60 % 60;
	const int64 Secs = Whole % 60;
	return FText::AsCultureInvariant(Hours > 0 ? FString::Printf(TEXT("%lld:%02lld:%02lld"), Hours, Minutes, Secs)
											   : FString::Printf(TEXT("%lld:%02lld"), Minutes, Secs));
}

FText DIAFormat::LitresPoured(double Litres)
{
	return FText::Format(LOCTEXT("LitresPoured", "{0} l"), Count(WholeCredits(Litres)));
}

FText DIAFormat::Items(int32 Bought, int32 Total)
{
	return FText::Format(LOCTEXT("Items", "{0} / {1}"), Count(Bought), Count(Total));
}

FText DIAFormat::Litres(double Have, double Capacity)
{
	return FText::Format(LOCTEXT("Litres", "{0} / {1} l"), TenthsText(Tenths(Have)), TenthsText(Tenths(Capacity)));
}

FText DIAFormat::Progress(int32 Done, int32 Total)
{
	const int32 Percent = Total > 0 ? FMath::Clamp(static_cast<int32>(static_cast<int64>(Done) * 100 / Total), 0, 100) : 0;
	return FText::Format(LOCTEXT("Progress", "{0} / {1} · {2}%"), Count(Done), Count(Total), Count(Percent));
}

FText DIAFormat::ItemName(ELotItem Item)
{
	switch (Item)
	{
	case ELotItem::Scoop: return LOCTEXT("ItemScoop", "Scoop");
	case ELotItem::Bucket: return LOCTEXT("ItemBucket", "Bucket");
	case ELotItem::Suitcase: return LOCTEXT("ItemSuitcase", "Suitcase");
	case ELotItem::Cart: return LOCTEXT("ItemCart", "Cart");
	case ELotItem::Station: return LOCTEXT("ItemStation", "Portable Station");
	case ELotItem::Finisher: return LOCTEXT("ItemFinisher", "Finisher");
	case ELotItem::AutoPaver: return LOCTEXT("ItemAutoPaver", "Auto Paver");
	case ELotItem::AutoPaverFast: return LOCTEXT("ItemAutoPaverFast", "Fast Auto Paver");
	case ELotItem::AutoPaverFaster: return LOCTEXT("ItemAutoPaverFaster", "Faster Auto Paver");
	case ELotItem::Car: return LOCTEXT("ItemCar", "Paver Car");
	case ELotItem::CarWideBar: return LOCTEXT("ItemCarWideBar", "Wide Bar");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::UnlockName(ELotUnlock Unlock)
{
	switch (Unlock)
	{
	case ELotUnlock::QuickSwap: return LOCTEXT("UnlockQuickSwap", "Quick Swap");
	case ELotUnlock::Sprint: return LOCTEXT("UnlockSprint", "Sprint");
	case ELotUnlock::StripWork: return LOCTEXT("UnlockStripWork", "Strip Work");
	case ELotUnlock::AutoPaver: return LOCTEXT("UnlockAutoPaver", "Auto Paver");
	case ELotUnlock::Car: return LOCTEXT("UnlockCar", "Paver Car");
	case ELotUnlock::Exit: return LOCTEXT("UnlockExit", "Exit");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::HandName(ELotHand Hand)
{
	switch (Hand)
	{
	case ELotHand::Bowl: return LOCTEXT("HandBowl", "Bowl");
	case ELotHand::Scoop: return LOCTEXT("HandScoop", "Scoop");
	case ELotHand::Bucket: return LOCTEXT("HandBucket", "Bucket");
	case ELotHand::Trowel: return LOCTEXT("HandTrowel", "Trowel");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::UnlockEffect(ELotUnlock Unlock)
{
	switch (Unlock)
	{
	case ELotUnlock::QuickSwap: return LOCTEXT("EffectQuickSwap", "Hand tools change at once.");
	case ELotUnlock::Sprint: return LOCTEXT("EffectSprint", "Hold Shift to run between the work and the trough.");
	case ELotUnlock::StripWork: return LOCTEXT("EffectStripWork", "The portable station, the cart and the finisher are on offer: work in strips.");
	case ELotUnlock::AutoPaver: return LOCTEXT("EffectAutoPaver", "The auto paver is on offer: mark a sector and it paves on its own.");
	case ELotUnlock::Car: return LOCTEXT("EffectCar", "The paver car is on offer: pave the main aisles from the cab.");
	case ELotUnlock::Exit: return LOCTEXT("EffectExit", "The exit gate is open: drive through it to finish.");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::HandLabel(const FLotGame& Game)
{
	switch (Game.GetHeld())
	{
	case ELotMovable::Suitcase: return ItemName(ELotItem::Suitcase);
	case ELotMovable::Cart: return ItemName(ELotItem::Cart);
	case ELotMovable::Station: return ItemName(ELotItem::Station);
	case ELotMovable::Finisher: return ItemName(ELotItem::Finisher);
	default: return HandName(Game.GetHand());
	}
}

FText DIAFormat::HandGauge(const FLotGame& Game)
{
	double Have = 0.0;
	double Capacity = 0.0;
	return GaugeOf(Game, Have, Capacity) ? Litres(Have, Capacity) : FText::GetEmpty();
}

bool DIAFormat::IsInRefillZone(const FLotGame& Game)
{
	if (DIARefill::IsInZone(Game, Game.World().PlayerPos))
	{
		return true;
	}
	// The suitcase and the cart fill where they stand, also while the player pulls them.
	const ELotMovable Held = Game.GetHeld();
	return (Held == ELotMovable::Suitcase || Held == ELotMovable::Cart) && DIARefill::IsInZone(Game, Game.World().Movables[static_cast<int32>(Held)].Pos);
}

FText DIAFormat::RefillLine(const FLotGame& Game)
{
	return IsInRefillZone(Game) ? Litres(Game.GetPlantStock(), Game.GetPlantStorage()) : FText::GetEmpty();
}

FText DIAFormat::NextUnlock(const FLotGame& Game)
{
	const ELotUnlock Step = NextStep(Game);
	if (Step == ELotUnlock::Count)
	{
		return FText::GetEmpty();
	}
	const int32 Index = static_cast<int32>(Step);
	const int32 Threshold = LotBalance::UnlockCells[Index];
	return FText::Format(LOCTEXT("NextUnlock", "Next: {0} – entry {1} / {2}, cells {3} / {4}"), UnlockName(Step),
		Count(Game.GetEntryProgress(Index)), Count(AsphaltLot::CellsPerEntry), Count(FMath::Min(Game.GetCompletedCells(), Threshold)), Count(Threshold));
}

FText DIAFormat::NearestUnfinished(double DistanceCm)
{
	return FText::Format(LOCTEXT("NearestUnfinished", "Nearest unfinished: {0} m"), Count(FMath::RoundToInt64(DistanceCm / 100.0)));
}

double DIAFormat::ArrowDegrees(FVector2D From, FVector2D To, double ViewYawDeg)
{
	const FVector2D Delta = To - From;
	const double Bearing = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	return FMath::FindDeltaAngleDegrees(ViewYawDeg, Bearing);
}

FText DIAFormat::EntryBoard(const FLotGame& Game, int32 Entry)
{
	if (Entry < 0 || Entry >= AsphaltLot::NumEntries)
	{
		return FText::GetEmpty();
	}
	const ELotUnlock Unlock = static_cast<ELotUnlock>(Entry);
	const FText Name = UnlockName(Unlock).ToUpper();
	if (Game.IsUnlocked(Unlock))
	{
		return FText::Format(LOCTEXT("BoardUnlocked", "{0}\nUNLOCKED"), Name);
	}
	const int32 Threshold = LotBalance::UnlockCells[Entry];
	return FText::Format(LOCTEXT("BoardLocked", "{0}\nEntry {1} / {2}\nCells {3} / {4}"), Name,
		Count(Game.GetEntryProgress(Entry)), Count(AsphaltLot::CellsPerEntry),
		Count(FMath::Min(Game.GetCompletedCells(), Threshold)), Count(Threshold));
}

FText DIAFormat::CreditsToast(double Credits)
{
	const int64 Whole = WholeCredits(Credits);
	return FText::Format(Whole == 1 ? LOCTEXT("ToastCredit", "+{0} credit") : LOCTEXT("ToastCredits", "+{0} credits"), Count(Whole));
}

FText DIAFormat::EntryToast(int32 Entry)
{
	return FText::Format(LOCTEXT("ToastEntry", "Entry {0} complete"), Count(Entry + 1));
}

FText DIAFormat::UnlockToast(ELotUnlock Unlock)
{
	return FText::Format(LOCTEXT("ToastUnlock", "Unlocked: {0}\n{1}"), UnlockName(Unlock), UnlockEffect(Unlock));
}

FText DIAFormat::ItemBoughtToast(ELotItem Item)
{
	return FText::Format(LOCTEXT("ToastItemBought", "{0} bought"), ItemName(Item));
}

FText DIAFormat::PlantToast(int32 Level)
{
	return FText::Format(LOCTEXT("ToastPlant", "Plant level {0}"), Count(Level));
}

FText DIAFormat::ExitToast()
{
	return LOCTEXT("ToastExit", "The exit is open!");
}

FText DIAFormat::MovablePrompt(ELotMovable Movable, bool bHeld)
{
	if (bHeld)
	{
		return LOCTEXT("PromptLetGo", "E – Let go");
	}
	switch (Movable)
	{
	case ELotMovable::Suitcase: return LOCTEXT("PromptPullFlap", "E – Pull\nF – Open flap");
	case ELotMovable::Station: return LOCTEXT("PromptPull", "E – Pull");
	case ELotMovable::Cart:
	case ELotMovable::Finisher: return LOCTEXT("PromptPush", "E – Push");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::MovableNoRoomToast()
{
	return LOCTEXT("ToastNoRoom", "No room to put it down here.");
}

bool FDIAHudModel::Update(const FLotGame& Game)
{
	FKey NewKey;
	NewKey.Credits = WholeCredits(Game.GetCredits());
	NewKey.CellsDone = Game.GetCompletedCells();
	NewKey.CellsTotal = AsphaltLot::NumCells;
	NewKey.Hand = static_cast<int32>(Game.GetHand());
	NewKey.Held = static_cast<int32>(Game.GetHeld());
	double Have = 0.0;
	double Capacity = 0.0;
	if (GaugeOf(Game, Have, Capacity))
	{
		NewKey.GaugeTenths = Tenths(Have);
		NewKey.GaugeCapacityTenths = Tenths(Capacity);
	}
	// The stock moves all the time, so it only counts while it is shown.
	if (DIAFormat::IsInRefillZone(Game))
	{
		NewKey.StockTenths = Tenths(Game.GetPlantStock());
		NewKey.StorageTenths = Tenths(Game.GetPlantStorage());
	}
	const ELotUnlock Step = NextStep(Game);
	NewKey.NextStep = static_cast<int32>(Step);
	NewKey.NextEntryCells = Step == ELotUnlock::Count ? 0 : Game.GetEntryProgress(static_cast<int32>(Step));

	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	View.Credits = DIAFormat::Credits(Game.GetCredits());
	View.Progress = DIAFormat::Progress(NewKey.CellsDone, NewKey.CellsTotal);
	View.ProgressFraction = NewKey.CellsTotal > 0 ? static_cast<float>(FMath::Clamp(static_cast<double>(NewKey.CellsDone) / NewKey.CellsTotal, 0.0, 1.0)) : 0.f;
	View.HandLabel = DIAFormat::HandLabel(Game);
	View.HandGauge = DIAFormat::HandGauge(Game);
	View.Refill = DIAFormat::RefillLine(Game);
	View.Next = DIAFormat::NextUnlock(Game);
	return true;
}

void FDIACheckModel::Update(const FLotGame& Game, bool bHeld, float DeltaSeconds, const FVector2D& From)
{
	if (!bHeld)
	{
		View = FDIACheckView();
		SinceSearch = 0.f;
		return;
	}
	const bool bSwitchedOn = !View.bActive;
	View.bActive = true;
	SinceSearch += DeltaSeconds;
	if (!bSwitchedOn && SinceSearch < AsphaltHud::CheckRefreshSeconds)
	{
		return;
	}
	SinceSearch = 0.f;
	++SearchCount;

	const int32 Cell = Game.FindNearestIncomplete(From);
	if (Cell == INDEX_NONE)
	{
		View.Nearest = FText::GetEmpty();
		View.Target = FVector2D::ZeroVector;
		return;
	}
	View.Target = AsphaltLot::CellCentre(Cell);
	View.Nearest = DIAFormat::NearestUnfinished(FVector2D::Distance(From, View.Target));
}

bool FDIAShopModel::Update(const FLotGame& Game)
{
	constexpr int32 NumItems = static_cast<int32>(ELotItem::Count);
	constexpr int32 MaxPlantLevel = static_cast<int32>(std::size(LotBalance::PlantPrice));

	// The key first, from cheap queries: this runs on every tick, so nothing is formatted unless it changed.
	FKey NewKey;
	NewKey.PlantLevel = Game.GetPlantLevel();
	const bool bPlantMax = NewKey.PlantLevel >= MaxPlantLevel;
	NewKey.States[0] = static_cast<int32>(bPlantMax ? EDIAShopState::Owned : (Game.CanBuyPlant() ? EDIAShopState::Buy : EDIAShopState::NoCredits));
	for (int32 I = 0; I < NumItems; ++I)
	{
		const ELotItem Item = static_cast<ELotItem>(I);
		EDIAShopState State = EDIAShopState::Owned;
		if (!Game.Owns(Item))
		{
			if (Game.IsItemOffered(Item))
			{
				State = Game.CanBuyItem(Item) ? EDIAShopState::Buy : EDIAShopState::NoCredits;
			}
			else
			{
				State = EDIAShopState::Locked;
				NewKey.Locks[I] = LockKind(Game, Item);
			}
		}
		NewKey.States[1 + I] = static_cast<int32>(State);
	}

	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	// The rows in their natural order: the plant, then the items.
	{
		FDIAShopRow& Row = Natural[0];
		Row = FDIAShopRow();
		Row.bPlant = true;
		Row.Name = LOCTEXT("ShopPlant", "Plant");
		Row.Value = bPlantMax ? FText::Format(LOCTEXT("ShopRate", "{0} {1}"), Whole(PlantRate(NewKey.PlantLevel)), PerMinuteLitres())
							  : Change(PlantRate(NewKey.PlantLevel), PlantRate(NewKey.PlantLevel + 1), PerMinuteLitres());
		Row.State = static_cast<EDIAShopState>(NewKey.States[0]);
		Row.Price = bPlantMax ? FText::GetEmpty() : DIAFormat::Credits(Game.GetPlantNextPrice());
	}
	for (int32 I = 0; I < NumItems; ++I)
	{
		const ELotItem Item = static_cast<ELotItem>(I);
		FDIAShopRow& Row = Natural[1 + I];
		Row = FDIAShopRow();
		Row.Item = Item;
		Row.Name = DIAFormat::ItemName(Item);
		Row.Value = ItemValue(Item);
		Row.State = static_cast<EDIAShopState>(NewKey.States[1 + I]);
		if (Row.State != EDIAShopState::Owned)
		{
			Row.Price = DIAFormat::Credits(Game.GetItemPrice(Item));
		}
		if (Row.State == EDIAShopState::Locked)
		{
			Row.Reason = LockReason(Game, Item);
		}
	}

	// The shown rows keep their places; only the first pass sorts.
	if (RebuildCount == 1)
	{
		Resort();
	}
	else
	{
		for (int32 I = 0; I < NumRows; ++I)
		{
			Rows[I] = Natural[Order[I]];
		}
	}
	return true;
}

void FDIAShopModel::Resort()
{
	// What can be bought stays on top; the owned rows follow in their own order.
	int32 Next = 0;
	for (const bool bOwnedPass : {false, true})
	{
		for (int32 I = 0; I < NumRows; ++I)
		{
			if ((Natural[I].State == EDIAShopState::Owned) == bOwnedPass)
			{
				Order[Next] = I;
				Rows[Next] = Natural[I];
				++Next;
			}
		}
	}
}

FDIAShopTarget FDIAShopModel::TargetOf(int32 Index, const FLotGame& Game) const
{
	FDIAShopTarget Target;
	if (Index < 0 || Index >= NumRows || Rows[Index].State != EDIAShopState::Buy)
	{
		return Target;
	}
	Target.bValid = true;
	Target.bPlant = Rows[Index].bPlant;
	Target.Item = Rows[Index].Item;
	Target.PlantLevel = Game.GetPlantLevel();
	return Target;
}

bool FDIAShopModel::Buy(const FDIAShopTarget& Target, UDIAWorldSubsystem& Subsystem)
{
	if (!Target.bValid)
	{
		return false;
	}
	// The game decides, not the rows: they may be older than the click.
	const FLotGame& Game = Subsystem.GetGame();
	return Target.bPlant ? (Game.GetPlantLevel() == Target.PlantLevel && Subsystem.BuyPlant()) : Subsystem.BuyItem(Target.Item);
}

int32 FDIAWinModel::ItemsBought(const FLotGame& Game)
{
	return Game.GetNumItemsOwned() + Game.GetPlantLevel();
}

int32 FDIAWinModel::ItemsTotal()
{
	return static_cast<int32>(ELotItem::Count) + static_cast<int32>(std::size(LotBalance::PlantPrice));
}

bool FDIAWinModel::Update(const FLotGame& Game)
{
	FKey NewKey;
	NewKey.Seconds = FMath::Max<int64>(0, FMath::FloorToInt64(Game.GetPlaytimeSeconds() + 1e-9));
	NewKey.Litres = WholeCredits(Game.GetTotalLitres());
	NewKey.Cells = Game.GetCompletedCells();
	NewKey.Bought = ItemsBought(Game);
	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	View.Time = DIAFormat::Duration(Game.GetPlaytimeSeconds());
	View.Poured = DIAFormat::LitresPoured(Game.GetTotalLitres());
	// Every credit comes from completed cells, so what was earned is what they paid; the purse is smaller after the shopping.
	View.Credits = DIAFormat::Credits(NewKey.Cells * LotBalance::CreditsPerCell);
	View.Items = DIAFormat::Items(NewKey.Bought, ItemsTotal());
	return true;
}

void FDIAToasts::Add(const FText& Text)
{
	Items.Add({Text, 0.f});
	while (Items.Num() > AsphaltHud::MaxToasts)
	{
		Items.RemoveAt(0);
	}
}

void FDIAToasts::Advance(float DeltaSeconds)
{
	for (FItem& Item : Items)
	{
		Item.Age += DeltaSeconds;
	}
	Items.RemoveAll([](const FItem& Item) { return Item.Age > AsphaltHud::ToastHoldSeconds + AsphaltHud::ToastFadeSeconds; });
}

FText FDIAToasts::GetText(int32 Index) const
{
	return Items.IsValidIndex(Index) ? Items[Index].Text : FText::GetEmpty();
}

float FDIAToasts::GetAlpha(int32 Index) const
{
	if (!Items.IsValidIndex(Index))
	{
		return 0.f;
	}
	const float Faded = Items[Index].Age - AsphaltHud::ToastHoldSeconds;
	return FMath::Clamp(1.f - Faded / AsphaltHud::ToastFadeSeconds, 0.f, 1.f);
}

#undef LOCTEXT_NAMESPACE
