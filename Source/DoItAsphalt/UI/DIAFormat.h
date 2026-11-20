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
	/** One decimal at most, rounded down: (3.24, 5) -> "3.2 / 5 l". */
	DOITASPHALT_API FText Litres(double Have, double Capacity);
	/** Percentage rounded down: (137, 30000) -> "137 / 30,000 · 0%". */
	DOITASPHALT_API FText Progress(int32 Done, int32 Total);
	/** Plain whole number with separators. */
	DOITASPHALT_API FText Count(int64 Value);
	/** Game time, whole seconds rounded down: 754 -> "12:34", 3725 -> "1:02:05". Never negative. */
	DOITASPHALT_API FText Duration(double Seconds);
	/** Whole litres, rounded down, with the unit: 12345.9 -> "12,345 l". */
	DOITASPHALT_API FText LitresPoured(double Litres);
	/** Items bought out of all of them: (8, 16) -> "8 / 16". */
	DOITASPHALT_API FText Items(int32 Bought, int32 Total);

	DOITASPHALT_API FText ItemName(ELotItem Item);
	DOITASPHALT_API FText UnlockName(ELotUnlock Unlock);
	DOITASPHALT_API FText HandName(ELotHand Hand);
	/** One short sentence on what an unlock step opens, for the toast; the entry boards show the name only. */
	DOITASPHALT_API FText UnlockEffect(ELotUnlock Unlock);

	/** What the player holds: the movable pulled or pushed along, else the item in the hand ("Bowl"). */
	DOITASPHALT_API FText HandLabel(const FLotGame& Game);
	/**
	 * The vessel in the hand: "1.3 / 2.5 l"; the suitcase or the cart while one is held (their content); empty for the trowel
	 * and for any other held movable.
	 */
	DOITASPHALT_API FText HandGauge(const FLotGame& Game);
	/** True if the player, or the suitcase or cart held by the player, stands where asphalt can be drawn by hand. */
	DOITASPHALT_API bool IsInRefillZone(const FLotGame& Game);
	/** The plant stock while in the refill zone: "42 / 600 l"; empty elsewhere. */
	DOITASPHALT_API FText RefillLine(const FLotGame& Game);
	/** The first step not yet unlocked: "Next: Sprint – entry 3 / 12, cells 57 / 100"; empty once every step is open. */
	DOITASPHALT_API FText NextUnlock(const FLotGame& Game);
	/** "Nearest unfinished: 42 m" (whole metres, rounded to the nearest). */
	DOITASPHALT_API FText NearestUnfinished(double DistanceCm);
	/** Clockwise angle in degrees (-180..180) from the view direction to the target; 0 = straight ahead, 90 = to the right. Yaw as the engine counts it. */
	DOITASPHALT_API double ArrowDegrees(FVector2D From, FVector2D To, double ViewYawDeg);

	/**
	 * The board of an entry segment (the step of the same number): "STRIP WORK", "Entry 3 / 12" and "Cells 120 / 350" on three lines,
	 * or the name and "UNLOCKED" once the step is open. Empty for an entry that does not exist. The cells never read above the threshold.
	 */
	DOITASPHALT_API FText EntryBoard(const FLotGame& Game, int32 Entry);

	/** "+30 credits", "+1 credit". */
	DOITASPHALT_API FText CreditsToast(double Credits);
	/** Entry is 0-based; the player sees it from 1. */
	DOITASPHALT_API FText EntryToast(int32 Entry);
	DOITASPHALT_API FText UnlockToast(ELotUnlock Unlock);
	DOITASPHALT_API FText ItemBoughtToast(ELotItem Item);
	DOITASPHALT_API FText PlantToast(int32 Level);
	DOITASPHALT_API FText ExitToast();

	/**
	 * The prompt of a suitcase, cart, station or finisher: "E – Pull" (suitcase), "E – Push" (cart), "E – Carry" (station) or "E – Guide" (finisher) while it
	 * stands, "E – Let go" while it is held ("E – Put down" for the station); a standing suitcase adds "F – Open flap" on a second line. Empty for an invalid kind.
	 */
	DOITASPHALT_API FText MovablePrompt(ELotMovable Movable, bool bHeld);
	/** The toast when a held movable cannot be set down because something is in the way. */
	DOITASPHALT_API FText MovableNoRoomToast();
}

/** What the HUD shows, already formatted. An empty text means the line is hidden. */
struct FDIAHudView
{
	FText Credits;
	FText Progress;
	float ProgressFraction = 0.f;
	/** What is in the hand or held ("Bowl", "Cart"). */
	FText HandLabel;
	/** The vessel in the hand ("1.3 / 2.5 l"). */
	FText HandGauge;
	/** The plant stock, in the refill zone only. */
	FText Refill;
	/** The next unlock step and how far it is. */
	FText Next;
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
		/** The item in the hand and the movable held (ELotHand / ELotMovable values). */
		int32 Hand = -1;
		int32 Held = -1;
		/** The hand gauge in tenths of litres and its capacity; -1 when nothing is shown. */
		int32 GaugeTenths = -1;
		int32 GaugeCapacityTenths = -1;
		/** The plant stock and storage in tenths while in the refill zone; -1 elsewhere. */
		int32 StockTenths = -1;
		int32 StorageTenths = -1;
		/** The next step (ELotUnlock::Count when none) and its entry progress. */
		int32 NextStep = -1;
		int32 NextEntryCells = -1;

		bool operator==(const FKey& Other) const = default;
	};

	FKey Key;
	FDIAHudView View;
	int32 RebuildCount = 0;
};

/** What the check view shows. */
struct FDIACheckView
{
	/** The view is held. */
	bool bActive = false;
	/** "Nearest unfinished: 42 m"; empty when the view is off or nothing is left to do. */
	FText Nearest;
	/** The centre of the nearest unfinished cell (cm); meaningful while Nearest is not empty. */
	FVector2D Target = FVector2D::ZeroVector;
};

/**
 * The nearest unfinished cell for the check view. The search scans every cell, so while the view is held it runs when the
 * view is switched on and then at most every AsphaltHud::CheckRefreshSeconds; while it is off nothing is searched.
 */
class DOITASPHALT_API FDIACheckModel
{
public:
	/** Advances by DeltaSeconds; Held is whether the check view is on. From is where the player actually is (the pawn's location in cm), read by the caller at this moment. */
	void Update(const FLotGame& Game, bool bHeld, float DeltaSeconds, const FVector2D& From);

	const FDIACheckView& GetView() const { return View; }
	/** How often the cells were scanned; for tests. */
	int32 GetSearchCount() const { return SearchCount; }

private:
	FDIACheckView View;
	float SinceSearch = 0.f;
	int32 SearchCount = 0;
};

/** The state of a shop row. */
enum class EDIAShopState : uint8
{
	/** Can be bought now. */
	Buy,
	/** On offer, but too expensive. */
	NoCredits,
	/** Not on offer yet; Reason says why. */
	Locked,
	/** Owned (the plant: at its highest level). */
	Owned
};

/** One row of the shop, already formatted. */
struct FDIAShopRow
{
	/** True for the plant's next level; otherwise Item is the row's item. */
	bool bPlant = false;
	ELotItem Item = ELotItem::Count;
	FText Name;
	/** What it gives: "150 → 300 l/min", "60 l". */
	FText Value;
	/** The price in credits; empty when owned. */
	FText Price;
	/** Why the row is locked: "350 cells + entry 3", "needs Auto Paver"; empty otherwise. */
	FText Reason;
	EDIAShopState State = EDIAShopState::Locked;
};

/**
 * What a purchase asks for, named by what is bought and not by the row it was shown in (the rows move when something is
 * bought). The plant is named by the level it expects to buy.
 */
struct FDIAShopTarget
{
	bool bValid = false;
	bool bPlant = false;
	ELotItem Item = ELotItem::Count;
	/** For the plant: the level the player has now, i.e. the level before the purchase. */
	int32 PlantLevel = -1;
};

/**
 * The rows of the shop: the plant's next level first, then the items in ELotItem order, with the owned rows at the end
 * (keeping their order). That order is fixed by Resort(), which the panel calls when it opens: while the shop is open a
 * purchase turns its row to Owned in place and no row moves, so a second click on the same spot can only meet the same row.
 * Like FDIAHudModel the model rebuilds the texts only when something shown changed: the state of each row, the reason a row
 * is locked and the plant level (the credits themselves are not shown); the subsystem reports a state change on every tick.
 */
class DOITASPHALT_API FDIAShopModel
{
public:
	/** The plant row and one row per item. */
	static constexpr int32 NumRows = 1 + static_cast<int32>(ELotItem::Count);

	/** Re-reads the game; true (and new texts, in the same rows) only if something shown changed. The first call also sorts. */
	bool Update(const FLotGame& Game);
	/** Puts the owned rows at the end, in their own order; call it when the shop opens (and on a new game), never while it is open. */
	void Resort();

	const FDIAShopRow& GetRow(int32 Index) const { return Rows[FMath::Clamp(Index, 0, NumRows - 1)]; }
	/** What a row offers, as a purchase request that names what is bought (and the plant level it expects); invalid for a row that cannot be bought. */
	FDIAShopTarget TargetOf(int32 Index, const FLotGame& Game) const;
	/**
	 * Buys the target through the world subsystem (which broadcasts it). It checks the game itself, not the rows: an item
	 * already owned, no longer affordable or no longer offered, or a plant that is not at the level the request expected, buys
	 * nothing.
	 */
	static bool Buy(const FDIAShopTarget& Target, class UDIAWorldSubsystem& Subsystem);
	/** How often the texts were rebuilt; for tests. */
	int32 GetRebuildCount() const { return RebuildCount; }

private:
	struct FKey
	{
		int32 PlantLevel = -1;
		/** EDIAShopState of the plant and of each item, in their own order. */
		int32 States[NumRows] = {};
		/** Why each item is locked (0 not, 1 its step is closed, 2 an item is missing): the reason text depends on it. */
		int32 Locks[static_cast<int32>(ELotItem::Count)] = {};

		bool operator==(const FKey& Other) const = default;
	};

	FKey Key;
	/** The rows in their natural order (the plant, then ELotItem order), and the shown rows: Rows[I] is Natural[Order[I]]. */
	FDIAShopRow Natural[NumRows];
	int32 Order[NumRows] = {};
	FDIAShopRow Rows[NumRows];
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
