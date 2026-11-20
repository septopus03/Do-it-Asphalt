// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAFormat.h"
#include "LotBalance.h"
#include "LotMap.h"
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
	return FText::Format(LOCTEXT("LitresPoured", "{0} L"), Count(WholeCredits(Litres)));
}

FText DIAFormat::Items(int32 Bought, int32 Total)
{
	return FText::Format(LOCTEXT("Items", "{0} / {1}"), Count(Bought), Count(Total));
}

FText DIAFormat::Litres(double Have, double Capacity)
{
	return FText::Format(LOCTEXT("Litres", "{0} / {1} L"), TenthsText(Tenths(Have)), TenthsText(Tenths(Capacity)));
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
	case ELotItem::Car: return LOCTEXT("ItemCar", "Paver Truck");
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
	case ELotUnlock::Car: return LOCTEXT("UnlockCar", "Paver Truck");
	case ELotUnlock::Exit: return LOCTEXT("UnlockExit", "Exit");
	default: return FText::GetEmpty();
	}
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
	return FText::Format(LOCTEXT("ToastUnlock", "{0} unlocked"), UnlockName(Unlock));
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

bool FDIAHudModel::Update(const FLotGame& Game)
{
	FKey NewKey;
	NewKey.Credits = WholeCredits(Game.GetCredits());
	NewKey.CellsDone = Game.GetCompletedCells();
	NewKey.CellsTotal = AsphaltLot::NumCells;

	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	View.Credits = DIAFormat::Credits(Game.GetCredits());
	View.Progress = DIAFormat::Progress(NewKey.CellsDone, NewKey.CellsTotal);
	View.ProgressFraction = NewKey.CellsTotal > 0 ? static_cast<float>(FMath::Clamp(static_cast<double>(NewKey.CellsDone) / NewKey.CellsTotal, 0.0, 1.0)) : 0.f;
	return true;
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
