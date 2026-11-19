// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "Internationalization/Text.h"

/** Text of the HUD as pure functions (no widgets), so that it can be tested. English, ready for localization. */
namespace DIAFormat
{
	/** Whole units, rounded down, with thousands separators: 1234.7 -> "1,234". Never negative. */
	DOITASPHALT_API FText Money(double Value);
	/** One decimal at most, rounded down: (3.24, 5) -> "3.2 / 5 L". */
	DOITASPHALT_API FText Litres(double Have, double Capacity);
	/** Percentage rounded down: (137, 499) -> "137 / 499 · 27%". */
	DOITASPHALT_API FText Progress(int32 Done, int32 Total);
	/** Price of an upgrade: (25, false) -> "25", (4, true) -> "4 shards", (1, true) -> "1 shard", negative -> "MAX". */
	DOITASPHALT_API FText Price(double Price, bool bShards);
	/** Plain whole number with separators. */
	DOITASPHALT_API FText Count(int64 Value);
	/** Game time, whole seconds rounded down: 754 -> "12:34", 3725 -> "1:02:05". Never negative. */
	DOITASPHALT_API FText Duration(double Seconds);
	/** Whole litres, rounded down, with the unit: 12345.9 -> "12,345 L". */
	DOITASPHALT_API FText LitresPoured(double Litres);
	/** Upgrades bought out of all of them: (8, 35) -> "8 / 35". */
	DOITASPHALT_API FText Upgrades(int32 Bought, int32 Total);

	/** Row names of the shop: Backpack, Sprayer (the tool), Plant. */
	DOITASPHALT_API FText UpgradeName(EAsphaltUpgrade Upgrade);
	/** The value of an upgrade at a level, from the balance tables: "5 L", "0.2 m", "15 L/min". The level is clamped. */
	DOITASPHALT_API FText UpgradeValue(EAsphaltUpgrade Upgrade, int32 Level);

	DOITASPHALT_API FText ShardsToast(int32 Shards);
	/** Road is 0-based; the player sees it from 1. */
	DOITASPHALT_API FText RoadToast(int32 Road);
	DOITASPHALT_API FText SpecialName(EAsphaltSpecial Special);

	/** The word on the sign of the exit gate. */
	DOITASPHALT_API FText ExitSign();
	DOITASPHALT_API FText SpecialToast(EAsphaltSpecial Special);
	DOITASPHALT_API FText ExitToast();

	/**
	 * The prompt at the kiosk of a special: "Finish the road (3 / 5)" until its road is complete, then "E – Buy Sprint (100)",
	 * "Sprint – need 100" when the money does not suffice, and "Sprint – owned" afterwards.
	 */
	DOITASPHALT_API FText KioskPrompt(const FAsphaltGame& Game, EAsphaltSpecial Special);

	/** The prompt at the portable station: "E – Pick up" while it stands, "E – Put down" while it is carried. */
	DOITASPHALT_API FText StationPrompt(bool bCarried);
	/** The prompt at the paver truck: "E – Get in" beside it, "E – Get out" while driving it. */
	DOITASPHALT_API FText CarPrompt(bool bInCar);
	/** Shown when the station cannot be put down because something is in the way. */
	DOITASPHALT_API FText StationNoRoomToast();
}

/** What the HUD shows, already formatted. */
struct FDIAHudView
{
	FText Money;
	FText Shards;
	FText Progress;
	/** Backpack litres, or the car tank while the player is in the car. */
	FText Backpack;
	FText BackpackCaption;
	FText Stock;
	bool bShowStock = false;
	float ProgressFraction = 0.f;
	float BackpackFraction = 0.f;
	float StockFraction = 0.f;
};

/**
 * Turns the game into a FDIAHudView, but only builds the texts when what they show changed: the key holds the shown
 * numbers at the precision of their text (money in whole units, litres in tenths), and the world subsystem reports a
 * state change on every tick because the plant keeps producing.
 */
class DOITASPHALT_API FDIAHudModel
{
public:
	/** Re-reads the game; true (and a new view) only if something shown changed. */
	bool Update(const FAsphaltGame& Game);

	const FDIAHudView& GetView() const { return View; }
	/** How often the view was rebuilt; for tests. */
	int32 GetRebuildCount() const { return RebuildCount; }

	/** Whether the player stands within AsphaltHud::StockRadiusCm of a refill point (the plant, or the placed portable station). */
	static bool IsNearRefillPoint(const FAsphaltGame& Game);

private:
	struct FKey
	{
		int64 Money = -1;
		int32 Shards = -1;
		int32 CellsDone = -1;
		int32 CellsTotal = -1;
		int32 BagTenths = -1;
		int32 BagCapacity = -1;
		bool bCar = false;
		bool bStock = false;
		int32 StockTenths = -1;
		int32 StockCapacity = -1;

		bool operator==(const FKey& Other) const = default;
	};

	FKey Key;
	FDIAHudView View;
	int32 RebuildCount = 0;
};

/** One row of the shop, already formatted. */
struct FDIAShopRow
{
	EAsphaltUpgrade Upgrade = EAsphaltUpgrade::Backpack;
	FText Name;
	/** "5 L → 7 L"; at the maximum level only the value ("200 L"). */
	FText Change;
	/** DIAFormat::Price of the next level ("1 shard", "25"); "MAX" at the maximum level. */
	FText Price;
	/** The BUY button is active only when this is true: affordable and not at the maximum. */
	bool bCanBuy = false;
	bool bMax = false;
};

/**
 * The three rows of the shop (Backpack, Sprayer, Plant). Like FDIAHudModel it rebuilds the texts only when something
 * shown changed: a row depends on the level of its upgrade and on whether it can be bought (the money itself is not
 * shown), and the subsystem reports a state change every tick.
 */
class DOITASPHALT_API FDIAShopModel
{
public:
	static constexpr int32 NumRows = 3;

	/** Re-reads the game; true (and new rows) only if something shown changed. */
	bool Update(const FAsphaltGame& Game);

	const FDIAShopRow& GetRow(int32 Index) const { return Rows[FMath::Clamp(Index, 0, NumRows - 1)]; }
	/** The upgrade shown in a row: Backpack, Tool, Plant. */
	static EAsphaltUpgrade RowUpgrade(int32 Index);
	/** How often the rows were rebuilt; for tests. */
	int32 GetRebuildCount() const { return RebuildCount; }

private:
	struct FKey
	{
		int32 Level[NumRows] = {-1, -1, -1};
		bool bCanBuy[NumRows] = {false, false, false};

		bool operator==(const FKey& Other) const
		{
			for (int32 I = 0; I < NumRows; ++I)
			{
				if (Level[I] != Other.Level[I] || bCanBuy[I] != Other.bCanBuy[I])
				{
					return false;
				}
			}
			return true;
		}
	};

	FKey Key;
	FDIAShopRow Rows[NumRows];
	int32 RebuildCount = 0;
};

/** What the win screen shows, already formatted. */
struct FDIAWinView
{
	FText Time;
	FText Poured;
	FText Money;
	FText Upgrades;
};

/**
 * The statistics of the win screen (GDD chapter 9): game time, litres poured, money earned and upgrades bought. Like the other
 * models it rebuilds the texts only when something shown changed. The money earned is what the poured litres paid
 * (AsphaltBalance::MoneyPerLitre), not the money left in the purse; the upgrades are the levels of the three upgrades plus the
 * specials, out of everything that can be bought.
 */
class DOITASPHALT_API FDIAWinModel
{
public:
	/** Re-reads the game; true (and a new view) only if something shown changed. */
	bool Update(const FAsphaltGame& Game);

	const FDIAWinView& GetView() const { return View; }
	/** How often the view was rebuilt; for tests. */
	int32 GetRebuildCount() const { return RebuildCount; }

	/** The sum of the three levels and the number of specials owned. */
	static int32 UpgradesBought(const FAsphaltGame& Game);
	/** The sum of the three maximum levels and the number of specials: 35. */
	static int32 UpgradesTotal(const FAsphaltGame& Game);

private:
	struct FKey
	{
		int64 Seconds = -1;
		int64 Litres = -1;
		int32 Bought = -1;
		int32 Total = -1;

		bool operator==(const FKey& Other) const = default;
	};

	FKey Key;
	FDIAWinView View;
	int32 RebuildCount = 0;
};

/** The short messages on screen: each stays fully visible for ToastHoldSeconds, then fades for ToastFadeSeconds. */
class DOITASPHALT_API FDIAToasts
{
public:
	/** Adds a toast at the end; beyond MaxToasts the oldest is dropped. */
	void Add(const FText& Text);
	void Advance(float DeltaSeconds);

	int32 Num() const { return Items.Num(); }
	/** Oldest first; empty text out of range. */
	FText GetText(int32 Index) const;
	/** 1 while fully visible, falling to 0 during the fade; 0 out of range. */
	float GetAlpha(int32 Index) const;

private:
	struct FItem
	{
		FText Text;
		float Age = 0.f;
	};
	TArray<FItem> Items;
};
