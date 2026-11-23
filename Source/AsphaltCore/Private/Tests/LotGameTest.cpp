// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotMap.h"

DIA_TEST(FLotGameCreditsOnlyOnCompletionTest, "DoItAsphalt.Lot.Game.CreditsOnlyOnCompletion")
DIA_TEST(FLotGameCellPaysOnceTest, "DoItAsphalt.Lot.Game.CellPaysOnce")
DIA_TEST(FLotGameNearestIncompleteTest, "DoItAsphalt.Lot.Game.NearestIncomplete")
DIA_TEST(FLotGameTickIsRobustTest, "DoItAsphalt.Lot.Game.TickIsRobust")
DIA_TEST(FLotPlantStartAndProduceTest, "DoItAsphalt.Lot.Plant.StartAndProduce")
DIA_TEST(FLotPlantLevelsTest, "DoItAsphalt.Lot.Plant.Levels")
DIA_TEST(FLotShopPricesTest, "DoItAsphalt.Lot.Shop.Prices")
DIA_TEST(FLotShopLockedUntilUnlockTest, "DoItAsphalt.Lot.Shop.LockedUntilUnlock")
DIA_TEST(FLotShopPrerequisitesTest, "DoItAsphalt.Lot.Shop.Prerequisites")
DIA_TEST(FLotShopBuyRejectedTest, "DoItAsphalt.Lot.Shop.BuyRejected")
DIA_TEST(FLotUnlockEventsTest, "DoItAsphalt.Lot.Unlock.Events")
DIA_TEST(FLotExitOpensAtAllTest, "DoItAsphalt.Lot.Exit.OpensAtAll")

namespace
{
	constexpr double Tolerance = 1e-6;

	/** The first Count cells by index that belong to no entry segment. */
	TArray<int32> NonEntryCells(int32 Count)
	{
		TArray<int32> Cells;
		for (int32 Cell = 0; Cell < AsphaltLot::NumCells && Cells.Num() < Count; ++Cell)
		{
			if (AsphaltLot::EntryOf(Cell) == INDEX_NONE)
			{
				Cells.Add(Cell);
			}
		}
		return Cells;
	}

	/** The cells of an entry segment. */
	TArray<int32> EntryCells(int32 Entry)
	{
		TArray<int32> Cells;
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			Cells.Add(AsphaltLot::EntryCell(Entry, I));
		}
		return Cells;
	}

	int32 CountOf(const TArray<ELotUnlock>& Array, ELotUnlock Unlock)
	{
		int32 N = 0;
		for (ELotUnlock U : Array)
		{
			N += U == Unlock ? 1 : 0;
		}
		return N;
	}

	/** Completes the cells one by one, returns how many events reported the unlock and the zero-based event index of the last such event. */
	void CompleteTracking(FLotGame& G, const TArray<int32>& Cells, ELotUnlock Unlock, int32& OutReports, int32& OutLastEvent, int32& InOutEventCounter)
	{
		for (int32 Cell : Cells)
		{
			const FLotEvents E = G.DebugCompleteCell(Cell);
			if (E.Unlocked.Contains(Unlock))
			{
				++OutReports;
				OutLastEvent = InOutEventCounter;
			}
			++InOutEventCounter;
		}
	}
}

bool FLotGameCreditsOnlyOnCompletionTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("new game credits"), G.GetCredits(), 0.0);
	TestEqual(TEXT("new game cells"), G.GetCompletedCells(), 0);
	TestEqual(TEXT("new game playtime"), G.GetPlaytimeSeconds(), 0.0);
	TestEqual(TEXT("new game litres"), G.GetTotalLitres(), 0.0);
	const FLotEvents E = G.DebugCompleteCell(0);
	TestNearlyEqual(TEXT("credits earned: ten for the cell, one a litre"), E.CreditsEarned, 10.0 + 10.0, Tolerance);
	TestNearlyEqual(TEXT("credits"), G.GetCredits(), 10.0 + 10.0, Tolerance);
	TestEqual(TEXT("completed cells"), G.GetCompletedCells(), 1);
	TestTrue(TEXT("event cells"), E.CompletedCells == TArray<int32>({0}));
	TestNearlyEqual(TEXT("litres poured"), E.LitresPoured, 10.0, Tolerance);
	TestNearlyEqual(TEXT("total litres"), G.GetTotalLitres(), 10.0, Tolerance);
	return true;
}

bool FLotGameCellPaysOnceTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugCompleteCell(0);
	const FLotEvents E = G.DebugCompleteCell(0);
	TestEqual(TEXT("second credits"), E.CreditsEarned, 0.0);
	TestTrue(TEXT("second cells"), E.CompletedCells.IsEmpty());
	TestNearlyEqual(TEXT("credits"), G.GetCredits(), 10.0 + 10.0, Tolerance);
	TestEqual(TEXT("cells"), G.GetCompletedCells(), 1);
	const FLotEvents Invalid = G.DebugCompleteCell(INDEX_NONE);
	TestTrue(TEXT("invalid cell"), Invalid.CompletedCells.IsEmpty());
	return true;
}

bool FLotGameNearestIncompleteTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("new game"), G.FindNearestIncomplete(AsphaltLot::CellCentre(0)), 0);
	G.DebugCompleteCell(0);
	TestEqual(TEXT("tie goes to the lower index"), G.FindNearestIncomplete(AsphaltLot::CellCentre(0)), 1);
	TestEqual(TEXT("far point"), G.FindNearestIncomplete(AsphaltLot::CellCentre(12345)), 12345);
	G.DebugCompleteCells(AsphaltLot::NumCells);
	TestEqual(TEXT("all complete"), G.FindNearestIncomplete(AsphaltLot::CellCentre(0)), (int32)INDEX_NONE);
	return true;
}

bool FLotGameTickIsRobustTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	const double Stock = G.GetPlantStock();
	G.Tick(0.0);
	G.Tick(-5.0);
	G.Tick(std::numeric_limits<double>::quiet_NaN());
	G.Tick(std::numeric_limits<double>::infinity());
	TestEqual(TEXT("bad Dt ignored: stock"), G.GetPlantStock(), Stock);
	TestEqual(TEXT("bad Dt ignored: playtime"), G.GetPlaytimeSeconds(), 0.0);
	const FLotEvents E = G.Tick(1.0);
	TestNearlyEqual(TEXT("playtime"), G.GetPlaytimeSeconds(), 1.0, Tolerance);
	TestTrue(TEXT("tick without work reports nothing"), E.CompletedCells.IsEmpty() && E.CreditsEarned == 0.0 && !E.bExitOpened);
	G.Tick(1.0e15);
	TestNearlyEqual(TEXT("clipped to one day"), G.GetPlaytimeSeconds(), 1.0 + 86400.0, 1e-3);
	return true;
}

bool FLotPlantStartAndProduceTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("level"), G.GetPlantLevel(), 0);
	TestNearlyEqual(TEXT("start stock"), G.GetPlantStock(), 80.0, Tolerance);
	TestNearlyEqual(TEXT("rate"), G.GetPlantRatePerMin(), 150.0, Tolerance);
	TestNearlyEqual(TEXT("storage"), G.GetPlantStorage(), 600.0, Tolerance);
	G.Tick(60.0);
	TestNearlyEqual(TEXT("after a minute"), G.GetPlantStock(), 230.0, Tolerance);
	G.Tick(600.0);
	TestNearlyEqual(TEXT("capped at storage"), G.GetPlantStock(), 600.0, Tolerance);
	G.DebugSetPlantStock(-3.0);
	TestEqual(TEXT("stock clipped below"), G.GetPlantStock(), 0.0);
	G.DebugSetPlantStock(1.0e9);
	TestNearlyEqual(TEXT("stock clipped above"), G.GetPlantStock(), 600.0, Tolerance);
	return true;
}

bool FLotPlantLevelsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGive(99.0);
	TestFalse(TEXT("99 credits cannot buy"), G.CanBuyPlant());
	TestFalse(TEXT("99 credits do not buy"), G.BuyPlant());
	TestEqual(TEXT("level unchanged"), G.GetPlantLevel(), 0);
	TestNearlyEqual(TEXT("credits unchanged"), G.GetCredits(), 99.0, Tolerance);
	G.DebugGive(1.0);
	TestNearlyEqual(TEXT("next price"), G.GetPlantNextPrice(), 100.0, Tolerance);
	TestTrue(TEXT("100 credits buy"), G.BuyPlant());
	TestEqual(TEXT("level"), G.GetPlantLevel(), 1);
	TestNearlyEqual(TEXT("rate"), G.GetPlantRatePerMin(), 300.0, Tolerance);
	TestNearlyEqual(TEXT("credits spent"), G.GetCredits(), 0.0, Tolerance);
	TestNearlyEqual(TEXT("next price"), G.GetPlantNextPrice(), 400.0, Tolerance);

	G.DebugSetPlantLevel(5);
	TestEqual(TEXT("level 5"), G.GetPlantLevel(), 5);
	TestEqual(TEXT("no next price"), G.GetPlantNextPrice(), -1.0);
	G.DebugGive(1.0e6);
	TestFalse(TEXT("cannot buy at maximum"), G.CanBuyPlant());
	TestFalse(TEXT("no buy at maximum"), G.BuyPlant());
	TestEqual(TEXT("level stays"), G.GetPlantLevel(), 5);
	G.DebugSetPlantLevel(6);
	TestEqual(TEXT("level 6 ignored"), G.GetPlantLevel(), 5);
	G.DebugSetPlantLevel(-1);
	TestEqual(TEXT("level -1 ignored"), G.GetPlantLevel(), 5);
	TestNearlyEqual(TEXT("rate at 5"), G.GetPlantRatePerMin(), 5200.0, Tolerance);

	G.DebugSetPlantStock(G.GetPlantStorage());
	G.DebugSetPlantLevel(0);
	TestNearlyEqual(TEXT("stock follows lower storage"), G.GetPlantStock(), 600.0, Tolerance);
	return true;
}

bool FLotShopPricesTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	const double Expected[11] = {35, 70, 160, 110, 340, 900, 3000, 9000, 22000, 12000, 24000};
	for (int32 I = 0; I < 11; ++I)
	{
		TestNearlyEqual(FString::Printf(TEXT("price %d"), I), G.GetItemPrice(static_cast<ELotItem>(I)), Expected[I], Tolerance);
	}
	TestEqual(TEXT("invalid item"), G.GetItemPrice(ELotItem::Count), -1.0);
	TestFalse(TEXT("invalid item not owned"), G.Owns(ELotItem::Count));
	TestFalse(TEXT("invalid item not offered"), G.IsItemOffered(ELotItem::Count));
	TestFalse(TEXT("invalid item not bought"), G.BuyItem(ELotItem::Count));
	return true;
}

bool FLotShopLockedUntilUnlockTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGive(1000000.0);
	for (int32 Cell : NonEntryCells(350))
	{
		G.DebugCompleteCell(Cell);
	}
	G.DebugGive(1000000.0 - G.GetCredits());
	TestEqual(TEXT("350 cells"), G.GetCompletedCells(), 350);
	TestFalse(TEXT("step locked without the segment"), G.IsUnlocked(ELotUnlock::StripWork));
	TestFalse(TEXT("station not offered"), G.IsItemOffered(ELotItem::Station));
	TestFalse(TEXT("station not bought"), G.BuyItem(ELotItem::Station));
	for (int32 Cell : EntryCells(2))
	{
		TestFalse(TEXT("still locked while the segment is incomplete"), G.IsUnlocked(ELotUnlock::StripWork));
		G.DebugCompleteCell(Cell);
	}
	TestTrue(TEXT("segment complete"), G.IsEntryComplete(2));
	TestTrue(TEXT("step unlocked"), G.IsUnlocked(ELotUnlock::StripWork));
	TestTrue(TEXT("station offered"), G.IsItemOffered(ELotItem::Station));
	TestTrue(TEXT("station bought"), G.BuyItem(ELotItem::Station));
	TestTrue(TEXT("owned"), G.Owns(ELotItem::Station));
	TestFalse(TEXT("no longer offered"), G.IsItemOffered(ELotItem::Station));
	return true;
}

bool FLotShopPrerequisitesTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGive(1.0e7);
	G.DebugCompleteCells(AsphaltLot::NumCells);
	G.DebugGive(1.0e7 - G.GetCredits());
	TestTrue(TEXT("all steps unlocked"), G.IsUnlocked(ELotUnlock::Car));
	TestFalse(TEXT("fast needs the paver"), G.BuyItem(ELotItem::AutoPaverFast));
	TestFalse(TEXT("faster needs the fast"), G.BuyItem(ELotItem::AutoPaverFaster));
	TestFalse(TEXT("wide bar needs the car"), G.BuyItem(ELotItem::CarWideBar));
	TestTrue(TEXT("paver"), G.BuyItem(ELotItem::AutoPaver));
	TestFalse(TEXT("faster still needs the fast"), G.BuyItem(ELotItem::AutoPaverFaster));
	TestTrue(TEXT("fast"), G.BuyItem(ELotItem::AutoPaverFast));
	TestTrue(TEXT("faster"), G.BuyItem(ELotItem::AutoPaverFaster));
	TestTrue(TEXT("car"), G.BuyItem(ELotItem::Car));
	TestTrue(TEXT("wide bar"), G.BuyItem(ELotItem::CarWideBar));
	TestEqual(TEXT("owned count"), G.GetNumItemsOwned(), 5);
	G.DebugGrantItem(ELotItem::Canister);
	G.DebugGrantItem(ELotItem::Count);
	TestEqual(TEXT("granted count"), G.GetNumItemsOwned(), 6);
	return true;
}

bool FLotShopBuyRejectedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGive(9.0);
	TestFalse(TEXT("cannot afford"), G.CanBuyItem(ELotItem::Bucket));
	TestFalse(TEXT("bucket rejected"), G.BuyItem(ELotItem::Bucket));
	TestFalse(TEXT("not owned"), G.Owns(ELotItem::Bucket));
	TestNearlyEqual(TEXT("credits unchanged"), G.GetCredits(), 9.0, Tolerance);
	G.DebugGive(26.0);
	TestTrue(TEXT("bucket"), G.BuyItem(ELotItem::Bucket));
	TestNearlyEqual(TEXT("credits after"), G.GetCredits(), 0.0, Tolerance);
	TestFalse(TEXT("second purchase rejected"), G.BuyItem(ELotItem::Bucket));
	TestNearlyEqual(TEXT("credits not charged twice"), G.GetCredits(), 0.0, Tolerance);
	TestEqual(TEXT("owned count"), G.GetNumItemsOwned(), 1);
	return true;
}

bool FLotUnlockEventsTest::RunTest(const FString& Parameters)
{
	// Segment 0 alone: QuickSwap opens exactly at the segment's last cell (12 cells >= 10).
	{
		FLotGame G;
		int32 Reports = 0, LastEvent = -1, Counter = 0, EntryReports = 0, EntryEvent = -1;
		const TArray<int32> Cells = EntryCells(0);
		for (int32 Cell : Cells)
		{
			const FLotEvents E = G.DebugCompleteCell(Cell);
			if (E.Unlocked.Contains(ELotUnlock::QuickSwap))
			{
				++Reports;
				LastEvent = Counter;
			}
			if (E.CompletedEntries.Contains(0))
			{
				++EntryReports;
				EntryEvent = Counter;
			}
			TestEqual(TEXT("progress"), G.GetEntryProgress(0), Counter + 1);
			++Counter;
		}
		TestEqual(TEXT("QuickSwap reported once"), Reports, 1);
		TestEqual(TEXT("at the segment's last cell"), LastEvent, 11);
		TestEqual(TEXT("segment reported once"), EntryReports, 1);
		TestEqual(TEXT("at the twelfth cell"), EntryEvent, 11);
		TestTrue(TEXT("unlocked"), G.IsUnlocked(ELotUnlock::QuickSwap));
		TestTrue(TEXT("segment complete"), G.IsEntryComplete(0));
		// Afterwards nothing is reported again.
		const FLotEvents Later = G.DebugCompleteCells(100);
		TestFalse(TEXT("never reported again"), Later.Unlocked.Contains(ELotUnlock::QuickSwap));
		TestTrue(TEXT("segment never reported again"), Later.CompletedEntries.IsEmpty());
	}

	// Ten other cells first, then the segment: still exactly one report, at the segment's last cell.
	{
		FLotGame G;
		int32 Reports = 0, LastEvent = -1, Counter = 0;
		CompleteTracking(G, NonEntryCells(10), ELotUnlock::QuickSwap, Reports, LastEvent, Counter);
		TestEqual(TEXT("not before the segment"), Reports, 0);
		TestFalse(TEXT("cell count alone does not unlock"), G.IsUnlocked(ELotUnlock::QuickSwap));
		const int32 Base = Counter;
		CompleteTracking(G, EntryCells(0), ELotUnlock::QuickSwap, Reports, LastEvent, Counter);
		TestEqual(TEXT("reported once"), Reports, 1);
		TestEqual(TEXT("at the segment's last cell"), LastEvent, Base + 11);
	}

	// Sprint (100 cells): segment 1 first, then the cells: the 100th cell opens the step.
	{
		FLotGame G;
		int32 Reports = 0, LastEvent = -1, Counter = 0;
		CompleteTracking(G, EntryCells(1), ELotUnlock::Sprint, Reports, LastEvent, Counter);
		TestEqual(TEXT("segment alone does not unlock"), Reports, 0);
		TestFalse(TEXT("locked"), G.IsUnlocked(ELotUnlock::Sprint));
		CompleteTracking(G, NonEntryCells(87), ELotUnlock::Sprint, Reports, LastEvent, Counter);
		TestEqual(TEXT("99 cells: not yet"), Reports, 0);
		CompleteTracking(G, TArray<int32>({NonEntryCells(88).Last()}), ELotUnlock::Sprint, Reports, LastEvent, Counter);
		TestEqual(TEXT("100th cell reports"), Reports, 1);
		TestEqual(TEXT("100 cells"), G.GetCompletedCells(), 100);
		TestEqual(TEXT("at the 100th event"), LastEvent, Counter - 1);
		CompleteTracking(G, NonEntryCells(200), ELotUnlock::Sprint, Reports, LastEvent, Counter);
		TestEqual(TEXT("reported only once"), Reports, 1);
	}

	// Sprint: cells first, then the segment: the segment's last cell opens the step.
	{
		FLotGame G;
		int32 Reports = 0, LastEvent = -1, Counter = 0;
		CompleteTracking(G, NonEntryCells(100), ELotUnlock::Sprint, Reports, LastEvent, Counter);
		TestEqual(TEXT("cells alone do not unlock"), Reports, 0);
		const int32 Base = Counter;
		CompleteTracking(G, EntryCells(1), ELotUnlock::Sprint, Reports, LastEvent, Counter);
		TestEqual(TEXT("reported once"), Reports, 1);
		TestEqual(TEXT("at the segment's last cell"), LastEvent, Base + 11);
	}

	// A batch that opens several steps at once reports each once.
	{
		FLotGame G;
		const FLotEvents E = G.DebugCompleteCells(AsphaltLot::NumCells);
		for (int32 U = 0; U < static_cast<int32>(ELotUnlock::Count); ++U)
		{
			TestEqual(*FString::Printf(TEXT("step %d reported once"), U), CountOf(E.Unlocked, static_cast<ELotUnlock>(U)), 1);
		}
		TestEqual(TEXT("six segments"), E.CompletedEntries.Num(), 6);
	}
	return true;
}

bool FLotExitOpensAtAllTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.SetWon();
	TestFalse(TEXT("SetWon ignored before the exit"), G.HasWon());
	TestFalse(TEXT("exit closed"), G.IsExitOpen());
	const FLotEvents Almost = G.DebugCompleteCells(AsphaltLot::NumCells - 1);
	TestFalse(TEXT("not opened one cell short"), Almost.bExitOpened);
	TestFalse(TEXT("exit still closed"), G.IsExitOpen());
	G.SetWon();
	TestFalse(TEXT("still not won"), G.HasWon());
	const FLotEvents Last = G.DebugCompleteCells(1);
	TestTrue(TEXT("opened by the last cell"), Last.bExitOpened);
	TestTrue(TEXT("exit open"), G.IsExitOpen());
	TestEqual(TEXT("exit equals unlock"), G.IsExitOpen(), G.IsUnlocked(ELotUnlock::Exit));
	TestNearlyEqual(TEXT("credits: ten a cell and ten litres of each"), G.GetCredits(), 600000.0, Tolerance);
	TestEqual(TEXT("all complete"), G.GetCompletedCells(), AsphaltLot::NumCells);
	TestFalse(TEXT("never again"), G.DebugCompleteCells(10).bExitOpened);
	G.SetWon();
	TestTrue(TEXT("won"), G.HasWon());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
