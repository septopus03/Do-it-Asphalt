// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAFormat.h"
#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
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

	int64 WholeMoney(double Value)
	{
		return FMath::Max<int64>(0, FMath::FloorToInt64(Value + 1e-9));
	}

	float Fraction(double Have, double Capacity)
	{
		return Capacity > 0.0 ? static_cast<float>(FMath::Clamp(Have / Capacity, 0.0, 1.0)) : 0.f;
	}
}

FText DIAFormat::Count(int64 Value)
{
	return FText::AsCultureInvariant(Grouped(Value));
}

FText DIAFormat::Money(double Value)
{
	return Count(WholeMoney(Value));
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

FText DIAFormat::Price(double Price, bool bShards)
{
	if (Price < 0.0)
	{
		return LOCTEXT("PriceMax", "MAX");
	}
	const int64 Whole = FMath::RoundToInt64(Price);
	if (!bShards)
	{
		return Count(Whole);
	}
	return FText::Format(Whole == 1 ? LOCTEXT("PriceShard", "{0} shard") : LOCTEXT("PriceShards", "{0} shards"), Count(Whole));
}

FText DIAFormat::ShardsToast(int32 Shards)
{
	return FText::Format(Shards == 1 ? LOCTEXT("ToastShard", "+{0} shard") : LOCTEXT("ToastShards", "+{0} shards"), Count(Shards));
}

FText DIAFormat::RoadToast(int32 Road)
{
	return FText::Format(LOCTEXT("ToastRoad", "Road {0} complete"), Count(Road + 1));
}

FText DIAFormat::SpecialName(EAsphaltSpecial Special)
{
	switch (Special)
	{
	case EAsphaltSpecial::Sprint: return LOCTEXT("SpecialSprint", "Sprint");
	case EAsphaltSpecial::TwoHands: return LOCTEXT("SpecialTwoHands", "Two hands");
	case EAsphaltSpecial::PortableStation: return LOCTEXT("SpecialStation", "Portable station");
	case EAsphaltSpecial::AutoPaver: return LOCTEXT("SpecialAutoPaver", "Auto paver");
	case EAsphaltSpecial::PaverCar: return LOCTEXT("SpecialPaverCar", "Paver car");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::SpecialToast(EAsphaltSpecial Special)
{
	return FText::Format(LOCTEXT("ToastSpecial", "{0} unlocked"), SpecialName(Special));
}

FText DIAFormat::ExitToast()
{
	return LOCTEXT("ToastExit", "The exit is open!");
}

bool FDIAHudModel::IsNearRefillPoint(const FAsphaltGame& Game)
{
	const FVector2D Player = Game.World().PlayerPos;
	if (FVector2D::Distance(Player, AsphaltLayout::RefillCenter) <= AsphaltHud::StockRadiusCm)
	{
		return true;
	}
	return Game.HasSpecial(EAsphaltSpecial::PortableStation) && !Game.World().bStationCarried &&
		FVector2D::Distance(Player, Game.World().StationPos) <= AsphaltHud::StockRadiusCm;
}

bool FDIAHudModel::Update(const FAsphaltGame& Game)
{
	const bool bCar = Game.World().bPlayerInCar && Game.HasSpecial(EAsphaltSpecial::PaverCar);
	const double BagLitres = bCar ? Game.GetCarLitres() : Game.GetBackpackLitres();
	const double BagCapacity = bCar ? AsphaltBalance::CarTankL : Game.GetBackpackCapacity();
	const bool bStock = IsNearRefillPoint(Game);

	FKey NewKey;
	NewKey.Money = WholeMoney(Game.GetMoney());
	NewKey.Shards = Game.GetShards();
	NewKey.CellsDone = Game.GetGrid().NumCompleteCells();
	NewKey.CellsTotal = Game.GetMap().Num();
	NewKey.BagTenths = Tenths(BagLitres);
	NewKey.BagCapacity = Tenths(BagCapacity);
	NewKey.bCar = bCar;
	NewKey.bStock = bStock;
	// What is not shown is not tracked: far from the plant its production must not cause rebuilds.
	NewKey.StockTenths = bStock ? Tenths(Game.GetPlantStock()) : -1;
	NewKey.StockCapacity = bStock ? Tenths(Game.GetPlantStorage()) : -1;

	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	View.Money = DIAFormat::Money(Game.GetMoney());
	View.Shards = DIAFormat::Count(NewKey.Shards);
	View.Progress = DIAFormat::Progress(NewKey.CellsDone, NewKey.CellsTotal);
	View.ProgressFraction = Fraction(NewKey.CellsDone, NewKey.CellsTotal);
	View.Backpack = DIAFormat::Litres(BagLitres, BagCapacity);
	View.BackpackCaption = bCar ? LOCTEXT("CarTank", "CAR TANK") : LOCTEXT("Backpack", "BACKPACK");
	View.BackpackFraction = Fraction(BagLitres, BagCapacity);
	View.bShowStock = bStock;
	View.Stock = bStock ? DIAFormat::Litres(Game.GetPlantStock(), Game.GetPlantStorage()) : FText::GetEmpty();
	View.StockFraction = bStock ? Fraction(Game.GetPlantStock(), Game.GetPlantStorage()) : 0.f;
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
