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

	DOITASPHALT_API FText ShardsToast(int32 Shards);
	/** Road is 0-based; the player sees it from 1. */
	DOITASPHALT_API FText RoadToast(int32 Road);
	DOITASPHALT_API FText SpecialName(EAsphaltSpecial Special);
	DOITASPHALT_API FText SpecialToast(EAsphaltSpecial Special);
	DOITASPHALT_API FText ExitToast();
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
