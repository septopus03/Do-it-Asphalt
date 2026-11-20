// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAFormat.h"
#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "UI/DIAHudStyle.h"

#define LOCTEXT_NAMESPACE "DIAHud"

namespace
{
	/** Clamps an upgrade level to the table it indexes: 0 .. last entry (the same bound as FAsphaltGame::GetMaxLevel). */
	template <typename TableType, SIZE_T TableSize>
	int32 DIAFormatClampToTable(int32 Level, const TableType (&)[TableSize])
	{
		return FMath::Clamp(Level, 0, static_cast<int32>(TableSize) - 1);
	}

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
	return FText::Format(LOCTEXT("LitresPoured", "{0} L"), Count(WholeMoney(Litres)));
}

FText DIAFormat::Upgrades(int32 Bought, int32 Total)
{
	return FText::Format(LOCTEXT("Upgrades", "{0} / {1}"), Count(Bought), Count(Total));
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

FText DIAFormat::UpgradeName(EAsphaltUpgrade Upgrade)
{
	switch (Upgrade)
	{
	case EAsphaltUpgrade::Backpack: return LOCTEXT("UpgradeBackpack", "Backpack");
	case EAsphaltUpgrade::Tool: return LOCTEXT("UpgradeTool", "Sprayer");
	case EAsphaltUpgrade::Plant: return LOCTEXT("UpgradePlant", "Plant");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::UpgradeValue(EAsphaltUpgrade Upgrade, int32 Level)
{
	switch (Upgrade)
	{
	case EAsphaltUpgrade::Backpack:
		return FText::Format(LOCTEXT("ValueBackpack", "{0} L"), Count(FMath::RoundToInt64(AsphaltBalance::BackpackCapacityL[DIAFormatClampToTable(Level, AsphaltBalance::BackpackCapacityL)])));
	case EAsphaltUpgrade::Tool:
		// One decimal always, so that the widths line up: "0.2 m", "1.0 m", "2.0 m".
		return FText::Format(LOCTEXT("ValueTool", "{0} m"), FText::AsCultureInvariant(FString::Printf(TEXT("%.1f"), AsphaltBalance::ToolWidthM[DIAFormatClampToTable(Level, AsphaltBalance::ToolWidthM)])));
	case EAsphaltUpgrade::Plant:
		return FText::Format(LOCTEXT("ValuePlant", "{0} L/min"), Count(FMath::RoundToInt64(AsphaltBalance::PlantRateLPerMin[DIAFormatClampToTable(Level, AsphaltBalance::PlantRateLPerMin)])));
	default:
		return FText::GetEmpty();
	}
}

FText DIAFormat::ShardsToast(int32 Shards)
{
	return FText::Format(Shards == 1 ? LOCTEXT("ToastShard", "+{0} shard") : LOCTEXT("ToastShards", "+{0} shards"), Count(Shards));
}

FText DIAFormat::RoadToast(int32 Road)
{
	return FText::Format(LOCTEXT("ToastRoad", "Road {0} complete"), Count(Road + 1));
}

FText DIAFormat::ExitSign()
{
	return LOCTEXT("ExitSign", "EXIT");
}

FText DIAFormat::SpecialName(EAsphaltSpecial Special)
{
	switch (Special)
	{
	case EAsphaltSpecial::Sprint: return LOCTEXT("SpecialSprint", "Sprint");
	case EAsphaltSpecial::TwoHands: return LOCTEXT("SpecialTwoHands", "Two Hands");
	case EAsphaltSpecial::PortableStation: return LOCTEXT("SpecialStation", "Portable Station");
	case EAsphaltSpecial::AutoPaver: return LOCTEXT("SpecialAutoPaver", "Auto Paver");
	case EAsphaltSpecial::PaverCar: return LOCTEXT("SpecialPaverCar", "Paver Truck");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::SpecialToast(EAsphaltSpecial Special)
{
	return FText::Format(LOCTEXT("ToastSpecial", "{0} unlocked"), SpecialName(Special));
}

FText DIAFormat::KioskPrompt(const FAsphaltGame& Game, EAsphaltSpecial Special)
{
	const FText Name = SpecialName(Special);
	const int32 Road = Game.GetSpecialRoad(Special);
	if (!Game.IsRoadComplete(Road))
	{
		return FText::Format(LOCTEXT("PromptRoad", "Finish the road ({0} / {1})"), Count(Game.GetRoadProgress(Road)), Count(FHexMap::CellsPerRoad));
	}
	if (Game.HasSpecial(Special))
	{
		return FText::Format(LOCTEXT("PromptOwned", "{0} \u2013 owned"), Name);
	}
	const FText Price = Count(FMath::RoundToInt64(Game.GetSpecialPrice(Special)));
	if (Game.CanBuySpecial(Special))
	{
		return FText::Format(LOCTEXT("PromptBuy", "E \u2013 Buy {0} ({1})"), Name, Price);
	}
	return FText::Format(LOCTEXT("PromptNeed", "{0} \u2013 need {1}"), Name, Price);
}

FText DIAFormat::CarPrompt(bool bInCar)
{
	return bInCar ? LOCTEXT("PromptCarGetOut", "E \u2013 Get out") : LOCTEXT("PromptCarGetIn", "E \u2013 Get in");
}

FText DIAFormat::StationPrompt(bool bCarried)
{
	return bCarried ? LOCTEXT("PromptStationPutDown", "E \u2013 Put down") : LOCTEXT("PromptStationPickUp", "E \u2013 Pick up");
}

FText DIAFormat::StationNoRoomToast()
{
	return LOCTEXT("ToastStationNoRoom", "No room here");
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

EAsphaltUpgrade FDIAShopModel::RowUpgrade(int32 Index)
{
	static constexpr EAsphaltUpgrade Order[NumRows] = {EAsphaltUpgrade::Backpack, EAsphaltUpgrade::Tool, EAsphaltUpgrade::Plant};
	return Order[FMath::Clamp(Index, 0, NumRows - 1)];
}

bool FDIAShopModel::Update(const FAsphaltGame& Game)
{
	FKey NewKey;
	for (int32 I = 0; I < NumRows; ++I)
	{
		const EAsphaltUpgrade Upgrade = RowUpgrade(I);
		NewKey.Level[I] = Game.GetLevel(Upgrade);
		NewKey.bCanBuy[I] = Game.CanBuy(Upgrade);
	}
	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	for (int32 I = 0; I < NumRows; ++I)
	{
		const EAsphaltUpgrade Upgrade = RowUpgrade(I);
		const int32 Level = NewKey.Level[I];
		const bool bMax = Level >= Game.GetMaxLevel(Upgrade);
		FDIAShopRow& Row = Rows[I];
		Row.Upgrade = Upgrade;
		Row.Name = DIAFormat::UpgradeName(Upgrade);
		Row.bMax = bMax;
		Row.bCanBuy = NewKey.bCanBuy[I];
		Row.Change = bMax ? DIAFormat::UpgradeValue(Upgrade, Level)
			: FText::Format(LOCTEXT("ShopChange", "{0} → {1}"), DIAFormat::UpgradeValue(Upgrade, Level), DIAFormat::UpgradeValue(Upgrade, Level + 1));
		Row.Price = DIAFormat::Price(Game.GetNextPrice(Upgrade), Upgrade == EAsphaltUpgrade::Backpack);
	}
	return true;
}

int32 FDIAWinModel::UpgradesBought(const FAsphaltGame& Game)
{
	int32 Bought = 0;
	for (const EAsphaltUpgrade Upgrade : {EAsphaltUpgrade::Backpack, EAsphaltUpgrade::Tool, EAsphaltUpgrade::Plant})
	{
		Bought += Game.GetLevel(Upgrade);
	}
	for (int32 Special = 0; Special < static_cast<int32>(EAsphaltSpecial::Count); ++Special)
	{
		Bought += Game.HasSpecial(static_cast<EAsphaltSpecial>(Special)) ? 1 : 0;
	}
	return Bought;
}

int32 FDIAWinModel::UpgradesTotal(const FAsphaltGame& Game)
{
	int32 Total = static_cast<int32>(EAsphaltSpecial::Count);
	for (const EAsphaltUpgrade Upgrade : {EAsphaltUpgrade::Backpack, EAsphaltUpgrade::Tool, EAsphaltUpgrade::Plant})
	{
		Total += Game.GetMaxLevel(Upgrade);
	}
	return Total;
}

bool FDIAWinModel::Update(const FAsphaltGame& Game)
{
	const double Poured = Game.GetTotalLitresPoured();
	FKey NewKey;
	NewKey.Seconds = FMath::Max<int64>(0, FMath::FloorToInt64(Game.GetPlaytimeSeconds() + 1e-9));
	NewKey.Litres = WholeMoney(Poured);
	NewKey.Bought = UpgradesBought(Game);
	NewKey.Total = UpgradesTotal(Game);
	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	View.Time = DIAFormat::Duration(Game.GetPlaytimeSeconds());
	View.Poured = DIAFormat::LitresPoured(Poured);
	// Every coin comes from pouring, so what was earned is what the litres paid; the purse is smaller after the shopping.
	View.Money = DIAFormat::Money(Poured * AsphaltBalance::MoneyPerLitre);
	View.Upgrades = DIAFormat::Upgrades(NewKey.Bought, NewKey.Total);
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
