// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "Internationalization/Text.h"

/** Text of the HUD as pure functions (no widgets), so that it can be tested. English, ready for localization. */
namespace DIAFormat
{
	/** Whole credits, rounded down, with thousands separators: 1234.7 -> "1,234". Never negative. */
	DOITASPHALT_API FText Credits(double Value);
	/** One decimal at most, rounded down: (3.24, 5) -> "3.2 / 5 L". */
	DOITASPHALT_API FText Litres(double Have, double Capacity);
	/** Percentage rounded down: (137, 30000) -> "137 / 30,000 · 0%". */
	DOITASPHALT_API FText Progress(int32 Done, int32 Total);
	/** Plain whole number with separators. */
	DOITASPHALT_API FText Count(int64 Value);
	/** Game time, whole seconds rounded down: 754 -> "12:34", 3725 -> "1:02:05". Never negative. */
	DOITASPHALT_API FText Duration(double Seconds);
	/** Whole litres, rounded down, with the unit: 12345.9 -> "12,345 L". */
	DOITASPHALT_API FText LitresPoured(double Litres);
	/** Items bought out of all of them: (8, 16) -> "8 / 16". */
	DOITASPHALT_API FText Items(int32 Bought, int32 Total);

	DOITASPHALT_API FText ItemName(ELotItem Item);
	DOITASPHALT_API FText UnlockName(ELotUnlock Unlock);

	/** "+30 credits", "+1 credit". */
	DOITASPHALT_API FText CreditsToast(double Credits);
	/** Entry is 0-based; the player sees it from 1. */
	DOITASPHALT_API FText EntryToast(int32 Entry);
	DOITASPHALT_API FText UnlockToast(ELotUnlock Unlock);
	DOITASPHALT_API FText ItemBoughtToast(ELotItem Item);
	DOITASPHALT_API FText PlantToast(int32 Level);
	DOITASPHALT_API FText ExitToast();
}

/** What the HUD shows, already formatted. */
struct FDIAHudView
{
	FText Credits;
	FText Progress;
	float ProgressFraction = 0.f;
};

/**
 * Turns the game into a FDIAHudView, but only builds the texts when what they show changed: the key holds the shown
 * numbers at the precision of their text (credits in whole units), and the world subsystem reports a state change on
 * every tick.
 */
class DOITASPHALT_API FDIAHudModel
{
public:
	/** Re-reads the game; true (and a new view) only if something shown changed. */
	bool Update(const FLotGame& Game);

	const FDIAHudView& GetView() const { return View; }
	/** How often the view was rebuilt; for tests. */
	int32 GetRebuildCount() const { return RebuildCount; }

private:
	struct FKey
	{
		int64 Credits = -1;
		int32 CellsDone = -1;
		int32 CellsTotal = -1;

		bool operator==(const FKey& Other) const = default;
	};

	FKey Key;
	FDIAHudView View;
	int32 RebuildCount = 0;
};

/** What the win screen shows, already formatted. */
struct FDIAWinView
{
	FText Time;
	FText Poured;
	FText Credits;
	FText Items;
};

/**
 * The statistics of the win screen: game time, litres poured, credits earned and items bought. Like the other models it
 * rebuilds the texts only when something shown changed. The credits earned are what the completed cells paid
 * (LotBalance::CreditsPerCell), not the credits left in the purse; the items are those owned plus the plant levels, out
 * of everything that can be bought.
 */
class DOITASPHALT_API FDIAWinModel
{
public:
	/** Re-reads the game; true (and a new view) only if something shown changed. */
	bool Update(const FLotGame& Game);

	const FDIAWinView& GetView() const { return View; }
	/** How often the view was rebuilt; for tests. */
	int32 GetRebuildCount() const { return RebuildCount; }

	/** The number of items owned plus the plant level. */
	static int32 ItemsBought(const FLotGame& Game);
	/** Every item plus every plant level. */
	static int32 ItemsTotal();

private:
	struct FKey
	{
		int64 Seconds = -1;
		int64 Litres = -1;
		int32 Cells = -1;
		int32 Bought = -1;

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
