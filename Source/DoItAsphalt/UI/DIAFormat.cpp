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

	/** Below this fraction of its tank the car's HUD asks for a station. */
	constexpr double CarLowTankFraction = 0.1;

	/** The vessel in the hand, or the trolley or cart that is held: how much it holds and how much fits. False when there is none. */
	bool GaugeOf(const FLotGame& Game, double& OutHave, double& OutCapacity)
	{
		const ELotMovable Held = Game.GetHeld();
		// Behind the wheel the gauge is the car's tank.
		if (Game.World().bPlayerInCar)
		{
			if (Game.GetCarLevel() < 0)
			{
				return false;
			}
			OutHave = Game.GetCarTank();
			OutCapacity = Game.GetCarTankCapacity();
			return true;
		}
		if (Held == ELotMovable::Trolley || Held == ELotMovable::Cart)
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
		case ELotItem::Bucket: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)])));
		case ELotItem::Canister: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Canister)])));
		case ELotItem::Tub: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Tub)])));
		case ELotItem::Trolley: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::TrolleyCapacityL)));
		case ELotItem::Cart: return FText::Format(NSLOCTEXT("DIAShop", "Litres", "{0} l"), TenthsText(Tenths(LotBalance::CartCapacityL)));
		case ELotItem::Station: return NSLOCTEXT("DIAShop", "StationValue", "Refill anywhere");
		case ELotItem::Car: return FText::Format(NSLOCTEXT("DIAShop", "Rate", "{0} {1}"), Whole(LotBalance::CarRateCellsPerMin[0]), PerMinuteCells());
		case ELotItem::CarT2: return Change(LotBalance::CarRateCellsPerMin[0], LotBalance::CarRateCellsPerMin[1], PerMinuteCells());
		case ELotItem::CarT3: return Change(LotBalance::CarRateCellsPerMin[1], LotBalance::CarRateCellsPerMin[2], PerMinuteCells());
		case ELotItem::CarT4: return Change(LotBalance::CarRateCellsPerMin[2], LotBalance::CarRateCellsPerMin[3], PerMinuteCells());
		default: return FText::GetEmpty();
		}
	}

	/** A robot card's value: the rate and the tank at the given upgrade levels ("150 cells/min · 3,000 l tank"; the number, the "l" and "tank" are tied with no-break spaces so that a wrap leaves the whole tank size on the second line). A level below 0 reads as level 0. */
	FText RobotValue(int32 SpeedLevel, int32 CapacityLevel)
	{
		const int32 Speed = FMath::Clamp(SpeedLevel, 0, MaxPaverUpgradeLevel);
		const int32 Capacity = FMath::Clamp(CapacityLevel, 0, MaxPaverUpgradeLevel);
		return FText::Format(NSLOCTEXT("DIAShop", "RobotStats", "{0} {1} · {2}\u00A0l\u00A0tank"), Whole(LotBalance::AutoPaverRateCellsPerMin[Speed]), PerMinuteCells(), Whole(LotBalance::AutoPaverTankLByLevel[Capacity]));
	}

	/** An upgrade row's value: "120 → 150 cells/min" for the next level, only the value that is reached once the last level is bought. */
	FText UpgradeValue(ELotPaverUpgrade Kind, int32 Level)
	{
		const int32 From = FMath::Clamp(Level, 0, MaxPaverUpgradeLevel);
		const bool bSpeed = Kind == ELotPaverUpgrade::Speed;
		const double* Table = bSpeed ? LotBalance::AutoPaverRateCellsPerMin : LotBalance::AutoPaverTankLByLevel;
		const FText Unit = bSpeed ? PerMinuteCells() : NSLOCTEXT("DIAShop", "LitresTank", "l tank");
		if (From >= MaxPaverUpgradeLevel)
		{
			return FText::Format(NSLOCTEXT("DIAShop", "UpgradeReached", "{0} {1}"), Whole(Table[From]), Unit);
		}
		return Change(Table[From], Table[From + 1], Unit);
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

	/** A robot's name: just "Paver" while one robot is owned, "Paver 2" and so on once there are more. */
	FText PaverName(const FLotGame& Game, int32 Paver)
	{
		return Game.GetNumAutoPavers() > 1 ? FText::Format(LOCTEXT("PaverNameN", "Paver {0}"), DIAFormat::Count(Paver + 1)) : LOCTEXT("PaverName", "Paver");
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
	case ELotItem::Bucket: return LOCTEXT("ItemBucket", "Bucket");
	case ELotItem::Canister: return LOCTEXT("ItemCanister", "Canister");
	case ELotItem::Tub: return LOCTEXT("ItemTub", "Tub");
	case ELotItem::Trolley: return LOCTEXT("ItemTrolley", "Trolley");
	case ELotItem::Cart: return LOCTEXT("ItemCart", "Cart");
	case ELotItem::Station: return LOCTEXT("ItemStation", "Portable Station");
	case ELotItem::AutoPaver: return LOCTEXT("ItemAutoPaver", "Auto Paver 1");
	case ELotItem::Car: return LOCTEXT("ItemCar", "Paver Car");
	case ELotItem::CarT2: return LOCTEXT("ItemCarT2", "Paver Car II");
	case ELotItem::CarT3: return LOCTEXT("ItemCarT3", "Paver Car III");
	case ELotItem::CarT4: return LOCTEXT("ItemCarT4", "Paver Car IV");
	case ELotItem::AutoPaver2: return LOCTEXT("ItemAutoPaver2", "Auto Paver 2");
	case ELotItem::AutoPaver3: return LOCTEXT("ItemAutoPaver3", "Auto Paver 3");
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
	case ELotHand::Bucket: return LOCTEXT("HandBucket", "Bucket");
	case ELotHand::Canister: return LOCTEXT("HandCanister", "Canister");
	case ELotHand::Tub: return LOCTEXT("HandTub", "Tub");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::UnlockEffect(ELotUnlock Unlock)
{
	switch (Unlock)
	{
	case ELotUnlock::QuickSwap: return LOCTEXT("EffectQuickSwap", "Hand tools change at once.");
	case ELotUnlock::Sprint: return LOCTEXT("EffectSprint", "Hold Shift to run between the work and the trough.");
	case ELotUnlock::StripWork: return LOCTEXT("EffectStripWork", "The portable station is on offer: refill anywhere on the lot.");
	case ELotUnlock::AutoPaver: return LOCTEXT("EffectAutoPaver", "The auto paver is on offer: mark a sector and it paves on its own.");
	case ELotUnlock::Car: return LOCTEXT("EffectCar", "The paver car is on offer: pave the main aisles from the cab.");
	case ELotUnlock::Exit: return LOCTEXT("EffectExit", "The exit gate is open: drive through it to finish.");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::HandLabel(const FLotGame& Game)
{
	if (Game.World().bPlayerInCar)
	{
		return LOCTEXT("HandCar", "Car");
	}
	switch (Game.GetHeld())
	{
	case ELotMovable::Trolley: return ItemName(ELotItem::Trolley);
	case ELotMovable::Cart: return ItemName(ELotItem::Cart);
	case ELotMovable::Station: return ItemName(ELotItem::Station);
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
	// Only the player's own position counts. A trolley or cart fills where it stands (DIAMovable), never while it is held,
	// and the core knows its position only from the last time it was put down.
	return DIARefill::IsInZone(Game, Game.World().PlayerPos);
}

FText DIAFormat::RefillLine(const FLotGame& Game)
{
	if (Game.World().bPlayerInCar && Game.GetCarLevel() >= 0)
	{
		// Behind the wheel the line is about the tank: at a station what is there to load, elsewhere where to go when it runs low.
		if (Game.IsAtStation(Game.World().PlayerPos))
		{
			return FText::Format(LOCTEXT("CarLoading", "Station: loading · plant stock {0}"), Litres(Game.GetPlantStock(), Game.GetPlantStorage()));
		}
		if (Game.GetCarTank() < Game.GetCarTankCapacity() * CarLowTankFraction)
		{
			return LOCTEXT("CarTankLow", "Tank low – drive to a station");
		}
		return FText::GetEmpty();
	}
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

FText DIAFormat::ShopName(ELotShop Shop)
{
	switch (Shop)
	{
	case ELotShop::Tools: return LOCTEXT("ShopTools", "Tools");
	case ELotShop::Robots: return LOCTEXT("ShopRobots", "Robots");
	case ELotShop::Garage: return LOCTEXT("ShopGarage", "Garage");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::ShopPrompt(ELotShop Shop)
{
	switch (Shop)
	{
	case ELotShop::Tools: return LOCTEXT("PromptShopTools", "E – Tools");
	case ELotShop::Robots: return LOCTEXT("PromptShopRobots", "E – Robots");
	case ELotShop::Garage: return LOCTEXT("PromptShopGarage", "E – Garage");
	default: return FText::GetEmpty();
	}
}

FText DIAFormat::ShopSubtitle(ELotShop Shop)
{
	switch (Shop)
	{
	case ELotShop::Tools: return LOCTEXT("ShopToolsSubtitle", "Hand tools and the plant");
	case ELotShop::Robots: return LOCTEXT("ShopRobotsSubtitle", "Robots and the portable station");
	case ELotShop::Garage: return LOCTEXT("ShopGarageSubtitle", "Trucks");
	default: return FText::GetEmpty();
	}
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
	if (Item == ELotItem::AutoPaver)
	{
		return LOCTEXT("ToastPaverBought", "Auto Paver bought – it paves on its own (M = map)");
	}
	return FText::Format(LOCTEXT("ToastItemBought", "{0} bought"), ItemName(Item));
}

FText DIAFormat::PaverUpgradeToast(const FLotGame& Game, int32 Paver, ELotPaverUpgrade Upgrade)
{
	const FText Name = PaverName(Game, Paver);
	if (Upgrade == ELotPaverUpgrade::Speed)
	{
		return FText::Format(LOCTEXT("ToastPaverSpeed", "{0}: speed {1} {2}"), Name, Whole(Game.GetAutoPaverRateCellsPerMin(Paver)), PerMinuteCells());
	}
	return FText::Format(LOCTEXT("ToastPaverTank", "{0}: tank {1} l"), Name, Whole(Game.GetAutoPaverTankCapacity(Paver)));
}

FText DIAFormat::PlantToast(int32 Level)
{
	return FText::Format(LOCTEXT("ToastPlant", "Plant level {0}"), Count(Level));
}

FText DIAFormat::ExitToast()
{
	return LOCTEXT("ToastExit", "The exit is open!");
}

FText DIAFormat::CarPrompt(bool bInCar)
{
	return bInCar ? LOCTEXT("PromptGetOut", "E – Get out") : LOCTEXT("PromptGetIn", "E – Get in");
}

FText DIAFormat::CarNoExitToast()
{
	return LOCTEXT("ToastCarNoExit", "No room to get out here.");
}

FText DIAFormat::ExitSign()
{
	return LOCTEXT("ExitSign", "EXIT");
}

FText DIAFormat::SectorNumber(int32 Sector)
{
	return Count(Sector + 1);
}

FText DIAFormat::PaverSentToast(int32 Paver, int32 NumPavers, int32 Sector)
{
	if (NumPavers <= 1)
	{
		return FText::Format(LOCTEXT("ToastPaverSent", "Paver → sector {0}"), SectorNumber(Sector));
	}
	return FText::Format(LOCTEXT("ToastPaverSentN", "Paver {0} → sector {1}"), Count(Paver + 1), SectorNumber(Sector));
}

FText DIAFormat::PaverNoneToast()
{
	return LOCTEXT("ToastPaverNone", "No paver");
}

FText DIAFormat::PaverOffLotToast()
{
	return LOCTEXT("ToastPaverOffLot", "Not on the lot");
}

FText DIAFormat::PaverAlreadyHereToast(int32 Paver, int32 NumPavers)
{
	if (NumPavers <= 1)
	{
		return LOCTEXT("ToastPaverHere", "Paver is already here");
	}
	return FText::Format(LOCTEXT("ToastPaverHereN", "Paver {0} is already here"), Count(Paver + 1));
}

FText DIAFormat::PaverAutoToast(bool bAuto)
{
	return bAuto ? LOCTEXT("ToastPaverAutoOn", "Paver: AUTO on") : LOCTEXT("ToastPaverAutoOff", "Paver: AUTO off");
}

FText DIAFormat::PaverLine(const FLotGame& Game, int32 Paver)
{
	if (Paver < 0 || Paver >= Game.GetNumAutoPavers())
	{
		return FText::GetEmpty();
	}
	const FText Name = PaverName(Game, Paver);
	const int32 Sector = Game.GetAutoPaverSector(Paver);
	if (Sector == INDEX_NONE)
	{
		return Game.IsAutoPaverAuto() ? FText::Format(LOCTEXT("PaverLooking", "{0}: AUTO – no unpaved sector to go to"), Name)
			: FText::Format(LOCTEXT("PaverWaiting", "{0}: waiting – press M for the map"), Name);
	}
	const FText Tank = Litres(Game.GetAutoPaverTank(Paver), Game.GetAutoPaverTankCapacity(Paver));
	switch (Game.GetAutoPaverPhase(Paver))
	{
	case ELotPaverPhase::ToStation: return FText::Format(LOCTEXT("PaverToStation", "{0} → station (tank empty) · sector {1}"), Name, SectorNumber(Sector));
	case ELotPaverPhase::Loading:
		if (Game.IsAutoPaverStarved(Paver))
		{
			return FText::Format(LOCTEXT("PaverStarved", "{0}: waiting at the station – the plant has no asphalt"), Name);
		}
		return FText::Format(LOCTEXT("PaverLoading", "{0}: loading at the station · {1}"), Name, Tank);
	case ELotPaverPhase::ToSector: return FText::Format(LOCTEXT("PaverToSector", "{0} → sector {1} · {2}"), Name, SectorNumber(Sector), Tank);
	default: break;
	}
	return FText::Format(LOCTEXT("PaverWorking", "{0}: sector {1} · {2} / {3} · tank {4}"), Name, SectorNumber(Sector), Count(Game.GetAutoPaverStep(Paver)), Count(AsphaltLot::CellsPerSector), Tank);
}

FText DIAFormat::MovablePrompt(ELotMovable Movable, bool bHeld)
{
	if (bHeld)
	{
		return Movable == ELotMovable::Station ? LOCTEXT("PromptPutDown", "E – Put down") : LOCTEXT("PromptLetGo", "E – Let go");
	}
	switch (Movable)
	{
	case ELotMovable::Trolley: return LOCTEXT("PromptPushTrolley", "E – Push");
	case ELotMovable::Station: return LOCTEXT("PromptCarry", "E – Carry");
	case ELotMovable::Cart: return LOCTEXT("PromptPush", "E – Push");
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
	NewKey.InCar = Game.World().bPlayerInCar ? 1 : 0;
	double Have = 0.0;
	double Capacity = 0.0;
	if (GaugeOf(Game, Have, Capacity))
	{
		NewKey.GaugeTenths = Tenths(Have);
		NewKey.GaugeCapacityTenths = Tenths(Capacity);
	}
	// The stock moves all the time, so it only counts while it is shown.
	const bool bCarLine = Game.World().bPlayerInCar && Game.GetCarLevel() >= 0;
	if (bCarLine)
	{
		NewKey.CarLine = Game.IsAtStation(Game.World().PlayerPos) ? 2 : (Game.GetCarTank() < Game.GetCarTankCapacity() * CarLowTankFraction ? 1 : 0);
	}
	if (NewKey.CarLine == 2 || (!bCarLine && DIAFormat::IsInRefillZone(Game)))
	{
		NewKey.StockTenths = Tenths(Game.GetPlantStock());
		NewKey.StorageTenths = Tenths(Game.GetPlantStorage());
	}
	const ELotUnlock Step = NextStep(Game);
	NewKey.NextStep = static_cast<int32>(Step);
	NewKey.NextEntryCells = Step == ELotUnlock::Count ? 0 : Game.GetEntryProgress(static_cast<int32>(Step));
	// A robot's line shows the sector and the step; while it waits (or is not owned) the step is not shown.
	NewKey.NumPavers = Game.GetNumAutoPavers();
	for (int32 Paver = 0; Paver < NewKey.NumPavers; ++Paver)
	{
		NewKey.PaverSector[Paver] = Game.GetAutoPaverSector(Paver);
		NewKey.PaverStep[Paver] = NewKey.PaverSector[Paver] == INDEX_NONE ? 0 : Game.GetAutoPaverStep(Paver);
		NewKey.PaverPhase[Paver] = static_cast<int32>(Game.GetAutoPaverPhase(Paver));
		NewKey.PaverFlags[Paver] = (Game.IsAutoPaverAuto() ? 1 : 0) | (Game.IsAutoPaverStarved(Paver) ? 2 : 0);
		NewKey.PaverTankTenths[Paver] = Tenths(Game.GetAutoPaverTank(Paver));
	}

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
	View.RefillCaption = bCarLine ? LOCTEXT("RefillCaptionCar", "Car") : LOCTEXT("RefillCaptionPlant", "Plant stock");
	View.Next = DIAFormat::NextUnlock(Game);
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		View.Pavers[Paver] = DIAFormat::PaverLine(Game, Paver);
	}
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
	constexpr int32 NumUpgrades = static_cast<int32>(ELotPaverUpgrade::Count);
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
	// A robot's upgrades: not on offer (Locked) until the robot is owned, Owned at the last level, else bought by the credits.
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (int32 K = 0; K < NumUpgrades; ++K)
		{
			const ELotPaverUpgrade Kind = static_cast<ELotPaverUpgrade>(K);
			const int32 Level = Game.GetPaverUpgradeLevel(Paver, Kind);
			NewKey.Levels[Paver][K] = Level;
			EDIAShopState State = EDIAShopState::Locked;
			if (Level >= MaxPaverUpgradeLevel)
			{
				State = EDIAShopState::Owned;
			}
			else if (Level >= 0)
			{
				State = Game.CanBuyPaverUpgrade(Paver, Kind) ? EDIAShopState::Buy : EDIAShopState::NoCredits;
			}
			NewKey.States[FirstUpgradeRow + Paver * NumUpgrades + K] = static_cast<int32>(State);
		}
	}

	if (RebuildCount > 0 && NewKey == Key)
	{
		return false;
	}
	Key = NewKey;
	++RebuildCount;

	// The rows in their natural order: the plant, the items, then the upgrade rows.
	{
		FDIAShopRow& Row = Natural[0];
		Row = FDIAShopRow();
		Row.bPlant = true;
		Row.Shop = LotBalance::PlantShop;
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
		Row.Shop = LotBalance::ItemShop[I];
		Row.Name = DIAFormat::ItemName(Item);
		Row.Value = ItemValue(Item);
		// A robot's card shows what the robot has now (level 0 while it is not owned).
		for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
		{
			if (Item == AutoPaverItem[Paver])
			{
				Row.Value = RobotValue(NewKey.Levels[Paver][static_cast<int32>(ELotPaverUpgrade::Speed)], NewKey.Levels[Paver][static_cast<int32>(ELotPaverUpgrade::Capacity)]);
			}
		}
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

	// Then the upgrades, robot by robot.
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (int32 K = 0; K < NumUpgrades; ++K)
		{
			const ELotPaverUpgrade Kind = static_cast<ELotPaverUpgrade>(K);
			const int32 Level = NewKey.Levels[Paver][K];
			FDIAShopRow& Row = Natural[FirstUpgradeRow + Paver * NumUpgrades + K];
			Row = FDIAShopRow();
			Row.bPaverUpgrade = true;
			Row.Paver = Paver;
			Row.Upgrade = Kind;
			Row.Shop = ELotShop::Robots;
			Row.Name = Kind == ELotPaverUpgrade::Speed ? LOCTEXT("ShopUpgradeSpeed", "Speed") : LOCTEXT("ShopUpgradeCapacity", "Capacity");
			Row.Value = UpgradeValue(Kind, Level);
			Row.State = static_cast<EDIAShopState>(NewKey.States[FirstUpgradeRow + Paver * NumUpgrades + K]);
			if (Row.State != EDIAShopState::Owned)
			{
				// A robot that is not owned shows the price of its first level.
				Row.Price = DIAFormat::Credits(Level >= 0 ? Game.GetPaverUpgradePrice(Paver, Kind) : LotBalance::PaverUpgradePrice[K][0]);
			}
			if (Row.State == EDIAShopState::Locked)
			{
				Row.Reason = FText::Format(NSLOCTEXT("DIAShop", "LockedItem", "needs {0}"), DIAFormat::ItemName(AutoPaverItem[Paver]));
			}
		}
	}

	// The rows never move: what is bought stays where it was and only turns to Owned.
	for (int32 I = 0; I < NumRows; ++I)
	{
		Rows[I] = Natural[I];
	}
	return true;
}

TArray<int32> FDIAShopModel::RowsOf(ELotShop Shop) const
{
	TArray<int32> Result;
	for (int32 I = 0; I < NumRows; ++I)
	{
		if (Rows[I].Shop == Shop)
		{
			Result.Add(I);
		}
	}
	return Result;
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
	if (Rows[Index].bPaverUpgrade)
	{
		Target.bPaverUpgrade = true;
		Target.Paver = Rows[Index].Paver;
		Target.Upgrade = Rows[Index].Upgrade;
		Target.UpgradeLevel = Game.GetPaverUpgradeLevel(Target.Paver, Target.Upgrade);
	}
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
	if (Target.bPaverUpgrade)
	{
		// The level the click expected must still be the level the robot has: a second click on a spot that moved on buys nothing.
		return Game.GetPaverUpgradeLevel(Target.Paver, Target.Upgrade) == Target.UpgradeLevel && Subsystem.BuyPaverUpgrade(Target.Paver, Target.Upgrade);
	}
	return Target.bPlant ? (Game.GetPlantLevel() == Target.PlantLevel && Subsystem.BuyPlant()) : Subsystem.BuyItem(Target.Item);
}

int32 FDIAWinModel::ItemsBought(const FLotGame& Game)
{
	int32 Bought = Game.GetNumItemsOwned() + Game.GetPlantLevel();
	// The upgrade levels of the robots that are owned (a robot that is not owned has none).
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (int32 Kind = 0; Kind < static_cast<int32>(ELotPaverUpgrade::Count); ++Kind)
		{
			Bought += FMath::Max(0, Game.GetPaverUpgradeLevel(Paver, static_cast<ELotPaverUpgrade>(Kind)));
		}
	}
	return Bought;
}

int32 FDIAWinModel::ItemsTotal()
{
	return static_cast<int32>(ELotItem::Count) + static_cast<int32>(std::size(LotBalance::PlantPrice))
		+ MaxAutoPavers * static_cast<int32>(ELotPaverUpgrade::Count) * MaxPaverUpgradeLevel;
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
	View.Cells = DIAFormat::Count(NewKey.Cells);
	View.Credits = DIAFormat::Credits(NewKey.Cells * LotBalance::CreditsPerCell);
	View.Items = DIAFormat::Items(NewKey.Bought, ItemsTotal());
	return true;
}

void FDIAToasts::Add(const FText& Text)
{
	Items.Add({Text, 0.f});
	while (Items.Num() > AsphaltHud::MaxToasts)
	{
		const int32 Credits = Items.IndexOfByPredicate([](const FItem& Item) { return Item.bCredits; });
		Items.RemoveAt(Credits != INDEX_NONE ? Credits : 0);
	}
}

void FDIAToasts::AddCredits(double Credits)
{
	if (FItem* Shown = Items.FindByPredicate([](const FItem& Item) { return Item.bCredits; }))
	{
		Shown->Credits += Credits;
		Shown->Text = DIAFormat::CreditsToast(Shown->Credits);
		Shown->Age = 0.f;
		return;
	}
	if (Items.Num() >= AsphaltHud::MaxToasts)
	{
		return;
	}
	FItem Item;
	Item.Text = DIAFormat::CreditsToast(Credits);
	Item.bCredits = true;
	Item.Credits = Credits;
	Items.Add(Item);
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
