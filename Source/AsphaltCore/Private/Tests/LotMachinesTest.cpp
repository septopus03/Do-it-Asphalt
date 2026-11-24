// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotLayout.h"
#include "LotMap.h"

DIA_TEST(FLotAutoPaverIdleWithoutSectorTest, "DoItAsphalt.Lot.AutoPaver.IdleWithoutSector")
DIA_TEST(FLotAutoPaverRejectsInvalidTest, "DoItAsphalt.Lot.AutoPaver.RejectsInvalid")
DIA_TEST(FLotAutoPaverSpawnTest, "DoItAsphalt.Lot.AutoPaver.SpawnAndLevel")
DIA_TEST(FLotAutoPaverTravelTest, "DoItAsphalt.Lot.AutoPaver.Travel")
DIA_TEST(FLotAutoPaverRatePerMinuteTest, "DoItAsphalt.Lot.AutoPaver.RatePerMinute")
DIA_TEST(FLotAutoPaverStarvedByPlantTest, "DoItAsphalt.Lot.AutoPaver.StarvedByPlant")
DIA_TEST(FLotAutoPaverFinishesSectorTest, "DoItAsphalt.Lot.AutoPaver.FinishesSector")
DIA_TEST(FLotAutoPaverSkipsCompletedTest, "DoItAsphalt.Lot.AutoPaver.SkipsCompleted")
DIA_TEST(FLotAutoPaverReassignTest, "DoItAsphalt.Lot.AutoPaver.Reassign")
DIA_TEST(FLotAutoPaverAutoPicksSectorTest, "DoItAsphalt.Lot.AutoPaver.AutoPicksSector")
DIA_TEST(FLotAutoPaverSmallPlantLoadsTest, "DoItAsphalt.Lot.AutoPaver.SmallPlantLoads")
DIA_TEST(FLotAutoPaverStarvedFlagTest, "DoItAsphalt.Lot.AutoPaver.StarvedFlag")
DIA_TEST(FLotCarNothingWithoutCarTest, "DoItAsphalt.Lot.Car.NothingWithoutCar")
DIA_TEST(FLotAutoPaverEmptyTankFetchesTest, "DoItAsphalt.Lot.AutoPaver.EmptyTankFetches")
DIA_TEST(FLotAutoPaverRunsDryAndReturnsTest, "DoItAsphalt.Lot.AutoPaver.RunsDryAndReturns")
DIA_TEST(FLotAutoPaverNearestStationTest, "DoItAsphalt.Lot.AutoPaver.NearestStation")
DIA_TEST(FLotCarWidthTest, "DoItAsphalt.Lot.Car.Width")
DIA_TEST(FLotCarTurnLeavesNoGapTest, "DoItAsphalt.Lot.Car.TurnLeavesNoGap")
DIA_TEST(FLotCarDrawsFromTankTest, "DoItAsphalt.Lot.Car.DrawsFromTank")
DIA_TEST(FLotCarLoadsAtStationTest, "DoItAsphalt.Lot.Car.LoadsAtStation")
DIA_TEST(FLotCarSpawnAndLevelTest, "DoItAsphalt.Lot.Car.SpawnAndLevel")
DIA_TEST(FLotMachinesLitresConservedTest, "DoItAsphalt.Lot.Machines.LitresConserved")
DIA_TEST(FLotAutoPaverSecondNeedsFirstTest, "DoItAsphalt.Lot.AutoPaver.SecondNeedsFirst")
DIA_TEST(FLotAutoPaverSpawnInDocksTest, "DoItAsphalt.Lot.AutoPaver.SpawnInDocks")
DIA_TEST(FLotAutoPaverAutoPicksDifferentSectorsTest, "DoItAsphalt.Lot.AutoPaver.AutoPicksDifferentSectors")
DIA_TEST(FLotAutoPaverAssignStealsTest, "DoItAsphalt.Lot.AutoPaver.AssignSteals")
DIA_TEST(FLotAutoPaverFewerSectorsThanPaversTest, "DoItAsphalt.Lot.AutoPaver.FewerSectorsThanPavers")
DIA_TEST(FLotAutoPaverSharedStockTest, "DoItAsphalt.Lot.AutoPaver.SharedStock")
DIA_TEST(FLotAutoPaverPerRobotSpeedTest, "DoItAsphalt.Lot.AutoPaver.PerRobotSpeed")
DIA_TEST(FLotAutoPaverUpgradePurchaseTest, "DoItAsphalt.Lot.AutoPaver.UpgradePurchase")
DIA_TEST(FLotAutoPaverUpgradeEffectsTest, "DoItAsphalt.Lot.AutoPaver.UpgradeEffects")
DIA_TEST(FLotAutoPaverUpgradesIndependentTest, "DoItAsphalt.Lot.AutoPaver.UpgradesIndependent")
DIA_TEST(FLotAutoPaverBigTankLoadsTest, "DoItAsphalt.Lot.AutoPaver.BigTankLoads")
DIA_TEST(FLotAutoPaverDebugUpgradeTest, "DoItAsphalt.Lot.AutoPaver.DebugUpgrade")
DIA_TEST(FLotAutoPaverCapacitySavesStationTimeTest, "DoItAsphalt.Lot.AutoPaver.CapacitySavesStationTime")

namespace
{
	constexpr double Tolerance = 1e-6;
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	/** A sector in the middle of the lot. */
	constexpr int32 TestSector = 12;

	int32 MiddleCell(int32 Col = 40, int32 Row = 40)
	{
		return AsphaltLot::CellIndex(Col, Row);
	}

	/** Axis of a strip over Count cells of one row, starting at column Col; the stamp's end pads reach the cell edges exactly. */
	void RowStrip(int32 Col, int32 Row, int32 Count, FVector2D& A, FVector2D& B)
	{
		A = AsphaltLot::CellCentre(MiddleCell(Col, Row)) + FVector2D(-AsphaltLot::CellCm * 0.5 + LotBalance::StampPadCm, 0.0);
		B = AsphaltLot::CellCentre(MiddleCell(Col + Count - 1, Row)) + FVector2D(AsphaltLot::CellCm * 0.5 - LotBalance::StampPadCm, 0.0);
	}

	/** Game with the plant at level 5 and a full stock. */
	void FullPlant(FLotGame& G)
	{
		G.DebugSetPlantLevel(5);
		G.DebugSetPlantStock(G.GetPlantStorage());
	}

	/** Auto paver bought with its speed at the given level (0..2), plant at level 5 with a full stock. */
	void RichPaver(FLotGame& G, int32 Level)
	{
		G.DebugGrantItem(ELotItem::AutoPaver);
		G.SetAutoPaverAuto(false);
		G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, Level);
		FullPlant(G);
		G.DebugFillTanks();
	}

	/** Sends the auto paver to the sector and ticks until it has arrived. */
	void SendAndArrive(FLotGame& G, int32 Sector)
	{
		G.AssignAutoPaverSector(0, Sector);
		for (int32 I = 0; I < 1000 && G.IsAutoPaverTravelling(0); ++I)
		{
			G.Tick(0.25);
			FullPlant(G);
			G.DebugFillTanks();
		}
	}

	/** Ticks for Seconds with a full plant and full tanks, so that neither the stock nor a trip to the station limits the machines. */
	void RunFed(FLotGame& G, double Seconds)
	{
		for (double T = 0.0; T < Seconds; T += 0.25)
		{
			G.Tick(0.25);
			FullPlant(G);
			G.DebugFillTanks();
		}
	}

	int32 CompleteCellsOfSector(const FLotGame& G, int32 Sector)
	{
		int32 Count = 0;
		for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
		{
			Count += G.GetGrid().IsCellComplete(AsphaltLot::SectorCell(Sector, Step)) ? 1 : 0;
		}
		return Count;
	}
}

bool FLotAutoPaverAutoPicksSectorTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	TestTrue(TEXT("a bought paver is automatic"), G.IsAutoPaverAuto());
	TestEqual(TEXT("no sector yet"), G.GetAutoPaverSector(0), INDEX_NONE);
	FullPlant(G);
	G.DebugFillTanks();
	RunFed(G, 6.0);
	TestTrue(TEXT("it picked a sector on its own"), G.GetAutoPaverSector(0) != INDEX_NONE);
	RunFed(G, 600.0);
	TestTrue(TEXT("and paved something"), G.GetCompletedCells() > 0);

	// Switched off, it finishes and then stays put.
	G.SetAutoPaverAuto(false);
	TestFalse(TEXT("switched off"), G.IsAutoPaverAuto());
	for (int32 I = 0; I < 4000 && G.GetAutoPaverSector(0) != INDEX_NONE; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
		G.DebugFillTanks();
	}
	RunFed(G, 10.0);
	TestEqual(TEXT("it waits for the player"), G.GetAutoPaverSector(0), INDEX_NONE);
	return true;
}

bool FLotAutoPaverSmallPlantLoadsTest::RunTest(const FString& Parameters)
{
	// The tank (2000 l) is bigger than what a level-0 plant stores: the paver must still leave with what is there.
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.DebugSetPlantStock(G.GetPlantStorage());
	TestTrue(TEXT("the plant stores less than the tank takes"), G.GetPlantStorage() < G.GetAutoPaverTankCapacity(0));
	bool bLeft = false;
	for (int32 I = 0; I < 4 * 600 && !bLeft; ++I)
	{
		G.Tick(0.25);
		bLeft = G.GetAutoPaverPhase(0) == ELotPaverPhase::ToSector || G.GetAutoPaverPhase(0) == ELotPaverPhase::Working;
	}
	TestTrue(TEXT("it left the station with a part of a tank"), bLeft);
	TestTrue(TEXT("it carries at least the minimum load or has laid some"), G.GetAutoPaverTank(0) >= LotBalance::AutoPaverMinLoadL - 1.0 || G.GetCompletedCells() > 0 || G.GetAutoPaverStep(0) > 0);
	return true;
}

bool FLotAutoPaverStarvedFlagTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);
	TestTrue(TEXT("assign"), G.AssignAutoPaverSector(0, TestSector));
	bool bLoading = false;
	for (int32 I = 0; I < 4 * 600 && !bLoading; ++I)
	{
		G.Tick(0.25);
		G.DebugSetPlantStock(0.0);
		bLoading = G.GetAutoPaverPhase(0) == ELotPaverPhase::Loading;
	}
	TestTrue(TEXT("it waits at the station"), bLoading);
	TestTrue(TEXT("with no asphalt in the plant it is starved"), G.IsAutoPaverStarved(0));
	G.DebugSetPlantStock(500.0);
	TestFalse(TEXT("with asphalt it is not"), G.IsAutoPaverStarved(0));
	return true;
}

bool FLotAutoPaverIdleWithoutSectorTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	const FVector2D Where = G.GetAutoPaverPos(0);
	const FLotEvents Events = G.Tick(600.0);
	TestEqual(TEXT("waiting"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no cells"), G.GetCompletedCells(), 0);
	TestEqual(TEXT("no litres"), Events.LitresPoured, 0.0);
	TestEqual(TEXT("stays put"), G.GetAutoPaverPos(0), Where);
	TestFalse(TEXT("not travelling"), G.IsAutoPaverTravelling(0));
	return true;
}

bool FLotAutoPaverRejectsInvalidTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestFalse(TEXT("without the auto paver"), G.AssignAutoPaverSector(0, 0));
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);
	TestFalse(TEXT("sector -1"), G.AssignAutoPaverSector(0, -1));
	TestFalse(TEXT("sector 75"), G.AssignAutoPaverSector(0, AsphaltLot::NumSectors));
	TestEqual(TEXT("still waiting"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("sector 0"), G.AssignAutoPaverSector(0, 0));
	TestTrue(TEXT("sector 74"), G.AssignAutoPaverSector(0, AsphaltLot::NumSectors - 1));
	TestEqual(TEXT("assigned"), G.GetAutoPaverSector(0), AsphaltLot::NumSectors - 1);
	TestEqual(TEXT("at step 0"), G.GetAutoPaverStep(0), 0);
	TestTrue(TEXT("travelling"), G.IsAutoPaverTravelling(0));
	return true;
}

bool FLotAutoPaverSpawnTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("level without"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), -1);
	TestEqual(TEXT("rate without"), G.GetAutoPaverRateCellsPerMin(0), 0.0);
	TestEqual(TEXT("tank size without"), G.GetAutoPaverTankCapacity(0), 0.0);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 1);
	TestEqual(TEXT("an upgrade alone is no auto paver"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), -1);
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);
	TestEqual(TEXT("a new robot starts at level 0"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	TestEqual(TEXT("spawn"), G.GetAutoPaverPos(0), LotLayout::ItemSpawn(ELotItem::AutoPaver));
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 2);
	TestEqual(TEXT("level 2"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 2);
	TestEqual(TEXT("rate 2"), G.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[2]);

	FLotGame B;
	B.DebugGive(1000000.0);
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		B.DebugCompleteCell(AsphaltLot::EntryCell(static_cast<int32>(ELotUnlock::AutoPaver), I));
	}
	B.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::AutoPaver)]);
	TestTrue(TEXT("bought"), B.BuyItem(ELotItem::AutoPaver));
	TestEqual(TEXT("level 0"), B.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	TestEqual(TEXT("rate 0"), B.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[0]);
	TestEqual(TEXT("tank size 0"), B.GetAutoPaverTankCapacity(0), LotBalance::AutoPaverTankLByLevel[0]);
	TestEqual(TEXT("bought spawn"), B.GetAutoPaverPos(0), LotLayout::ItemSpawn(ELotItem::AutoPaver));

	// A second grant of an owned machine does not move it again.
	FLotGame M;
	M.DebugGrantItem(ELotItem::AutoPaver);
	M.SetAutoPaverAuto(false);
	M.AssignAutoPaverSector(0, TestSector);
	M.Tick(5.0);
	const FVector2D Moved = M.GetAutoPaverPos(0);
	TestNotEqual(TEXT("it moved"), Moved, LotLayout::ItemSpawn(ELotItem::AutoPaver));
	M.DebugGrantItem(ELotItem::AutoPaver);
	M.SetAutoPaverAuto(false);
	TestEqual(TEXT("not moved back"), M.GetAutoPaverPos(0), Moved);
	return true;
}

bool FLotAutoPaverTravelTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	G.AssignAutoPaverSector(0, TestSector);
	const FVector2D Start = G.GetAutoPaverPos(0);
	const FVector2D Target = AsphaltLot::CellCentre(AsphaltLot::SectorCell(TestSector, 0));
	G.Tick(1.0);
	TestNearlyEqual(TEXT("one second of travel"), FVector2D::Distance(Start, G.GetAutoPaverPos(0)), LotBalance::AutoPaverSpeedCmS, 1e-3);
	const double Along = FVector2D::DotProduct((G.GetAutoPaverPos(0) - Start).GetSafeNormal(), (Target - Start).GetSafeNormal());
	TestNearlyEqual(TEXT("in a straight line"), Along, 1.0, 1e-9);
	TestEqual(TEXT("no work while travelling"), G.GetCompletedCells(), 0);
	const double Expected = FVector2D::Distance(Start, Target) / LotBalance::AutoPaverSpeedCmS;
	G.Tick(Expected - 1.0 - 0.6);
	TestTrue(TEXT("still on the way"), G.IsAutoPaverTravelling(0));
	G.Tick(1.0);
	TestFalse(TEXT("arrived"), G.IsAutoPaverTravelling(0));
	TestEqual(TEXT("at the first cell"), G.GetAutoPaverPos(0), Target);
	TestEqual(TEXT("sector kept"), G.GetAutoPaverSector(0), TestSector);
	return true;
}

bool FLotAutoPaverRatePerMinuteTest::RunTest(const FString& Parameters)
{
	const double Expected[3] = {120.0, 150.0, 180.0};
	for (int32 Level = 0; Level < 3; ++Level)
	{
		FLotGame G;
		RichPaver(G, Level);
		SendAndArrive(G, TestSector);
		const double CreditsBefore = G.GetCredits();
		RunFed(G, 60.0);
		const int32 Cells = G.GetCompletedCells();
		TestTrue(*FString::Printf(TEXT("level %d lays %d cells"), Level, Cells), FMath::Abs(Cells - Expected[Level]) <= 2.0);
		TestNearlyEqual(*FString::Printf(TEXT("level %d credits"), Level), G.GetCredits() - CreditsBefore,
			Cells * static_cast<double>(LotBalance::CreditsPerCell) + G.GetTotalLitres() * LotBalance::CreditsPerLitre, Tolerance);
	}
	return true;
}

bool FLotAutoPaverStarvedByPlantTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);
	G.AssignAutoPaverSector(0, TestSector);
	const double Supply0 = G.GetPlantStock();
	double Time = 0.0;
	for (int32 I = 0; I < 1800 * 4; ++I)
	{
		G.Tick(0.25);
		Time += 0.25;
		if (G.GetPlantStock() < 0.0)
		{
			AddError(TEXT("plant stock went negative"));
			break;
		}
	}
	// The paver waits at the station until its tank is full, so a weak plant keeps it waiting; it never lays more than the plant made.
	const int32 Cells = G.GetCompletedCells();
	const double Made = Supply0 + G.GetPlantRatePerMin() / 60.0 * Time;
	TestTrue(*FString::Printf(TEXT("%d cells from a level 0 plant"), Cells), Cells >= 150 && Cells * 9.0 <= Made);
	return true;
}

bool FLotAutoPaverFinishesSectorTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	G.AssignAutoPaverSector(0, TestSector);
	for (int32 I = 0; I < 300 * 4; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
	}
	TestEqual(TEXT("whole sector done"), CompleteCellsOfSector(G, TestSector), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("waiting again"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("step reset"), G.GetAutoPaverStep(0), 0);
	const FVector2D Where = G.GetAutoPaverPos(0);
	const FLotEvents Later = G.Tick(60.0);
	TestEqual(TEXT("nothing more"), Later.LitresPoured, 0.0);
	TestEqual(TEXT("no more cells"), G.GetCompletedCells(), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("stays in the last cell"), G.GetAutoPaverPos(0), Where);

	// A finished sector can be assigned again: it arrives and is done at once.
	TestTrue(TEXT("assign a finished sector"), G.AssignAutoPaverSector(0, TestSector));
	SendAndArrive(G, TestSector);
	G.Tick(0.25);
	TestEqual(TEXT("done again at once"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FLotAutoPaverSkipsCompletedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	for (int32 Step = 0; Step < 200; ++Step)
	{
		G.DebugCompleteCell(AsphaltLot::SectorCell(TestSector, Step));
	}
	SendAndArrive(G, TestSector);
	RunFed(G, 110.0);
	TestEqual(TEXT("rest done"), CompleteCellsOfSector(G, TestSector), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("waiting"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FLotAutoPaverReassignTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	SendAndArrive(G, TestSector);
	RunFed(G, 10.0);
	const int32 Done = CompleteCellsOfSector(G, TestSector);
	TestTrue(TEXT("some work done"), Done > 0 && Done < AsphaltLot::CellsPerSector);
	TestTrue(TEXT("reassign"), G.AssignAutoPaverSector(0, TestSector + 1));
	TestEqual(TEXT("new sector"), G.GetAutoPaverSector(0), TestSector + 1);
	TestEqual(TEXT("restarts at step 0"), G.GetAutoPaverStep(0), 0);
	TestTrue(TEXT("travelling"), G.IsAutoPaverTravelling(0));
	G.Tick(0.25);
	TestEqual(TEXT("old sector left as it is"), CompleteCellsOfSector(G, TestSector), Done);
	return true;
}

bool FLotAutoPaverEmptyTankFetchesTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);
	FullPlant(G);
	TestEqual(TEXT("the tank starts empty"), G.GetAutoPaverTank(0), 0.0);
	TestTrue(TEXT("assign"), G.AssignAutoPaverSector(0, TestSector));
	TestTrue(TEXT("first to the station"), G.GetAutoPaverPhase(0) == ELotPaverPhase::ToStation);
	TestTrue(TEXT("travelling"), G.IsAutoPaverTravelling(0));

	bool bLoading = false;
	bool bLoaded = false;
	bool bWorking = false;
	double Previous = 0.0;
	for (int32 I = 0; I < 400 * 4 && !bWorking; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
		bLoading = bLoading || G.GetAutoPaverPhase(0) == ELotPaverPhase::Loading;
		bLoaded = bLoaded || (bLoading && G.GetAutoPaverPhase(0) == ELotPaverPhase::ToSector);
		bWorking = G.GetAutoPaverPhase(0) == ELotPaverPhase::Working;
		TestTrue(TEXT("the tank only grows before the work starts"), G.GetAutoPaverTank(0) >= Previous);
		Previous = G.GetAutoPaverTank(0);
		TestEqual(TEXT("nothing is laid before it works"), G.GetCompletedCells(), 0);
	}
	TestTrue(TEXT("it loaded at the station"), bLoading && bLoaded);
	TestTrue(TEXT("and reached the sector"), bWorking);
	TestEqual(TEXT("with a full tank"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[0]);
	TestEqual(TEXT("at the first cell"), G.GetAutoPaverPos(0), AsphaltLot::CellCentre(AsphaltLot::SectorCell(TestSector, 0)));
	return true;
}

bool FLotAutoPaverRunsDryAndReturnsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 2);
	G.AssignAutoPaverSector(0, TestSector);
	bool bWentBack = false;
	bool bCameBack = false;
	int32 StepAtDry = -1;
	for (int32 I = 0; I < 900 * 4 && G.GetAutoPaverSector(0) != INDEX_NONE; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
		if (G.GetAutoPaverPhase(0) == ELotPaverPhase::ToStation && !bWentBack)
		{
			bWentBack = true;
			StepAtDry = G.GetAutoPaverStep(0);
			TestTrue(TEXT("it ran dry"), G.GetAutoPaverTank(0) <= LotBalance::AutoPaverTankEmptyL);
		}
		if (bWentBack && G.GetAutoPaverPhase(0) == ELotPaverPhase::Working)
		{
			bCameBack = true;
			TestEqual(TEXT("it carries on where it stopped"), G.GetAutoPaverStep(0), StepAtDry);
			TestTrue(TEXT("with a full tank"), G.GetAutoPaverTank(0) > LotBalance::AutoPaverTankLByLevel[0] - 1.0);
			bWentBack = false;
		}
	}
	// 4000 l are 400 cells; a 2000 l tank is not enough for them.
	TestTrue(TEXT("it had to fetch asphalt"), bCameBack);
	TestEqual(TEXT("and finished the whole sector"), CompleteCellsOfSector(G, TestSector), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("then waits"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FLotAutoPaverNearestStationTest::RunTest(const FString& Parameters)
{
	// The portable station is set down beside a sector far from the fixed one: a paver that runs dry there goes to it.
	const int32 FarSector = 40;
	const FVector2D FarCell = AsphaltLot::CellCentre(AsphaltLot::SectorCell(FarSector, 0));
	FLotGame G;
	RichPaver(G, 2);
	G.DebugGrantItem(ELotItem::Station);
	TestTrue(TEXT("grab"), G.Grab(ELotMovable::Station));
	G.Release(FarCell + FVector2D(0.0, 300.0), 0.0);
	TestEqual(TEXT("the fixed station is the nearest to the start"), G.FindNearestStation(G.GetAutoPaverPos(0)), 0);
	TestEqual(TEXT("the portable station is the nearest to the far sector"), G.FindNearestStation(FarCell), 1);
	G.AssignAutoPaverSector(0, FarSector);
	for (int32 I = 0; I < 900 * 4 && G.GetAutoPaverPhase(0) != ELotPaverPhase::ToStation; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
	}
	TestTrue(TEXT("ran dry"), G.GetAutoPaverPhase(0) == ELotPaverPhase::ToStation);
	for (int32 I = 0; I < 600 * 4 && G.GetAutoPaverPhase(0) == ELotPaverPhase::ToStation; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
	}
	TestTrue(TEXT("loading"), G.GetAutoPaverPhase(0) == ELotPaverPhase::Loading);
	TestEqual(TEXT("at the portable station, not the fixed one"), G.GetAutoPaverPos(0), G.GetStationPos(1));
	return true;
}

bool FLotCarNothingWithoutCarTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	FullPlant(G);
	TestEqual(TEXT("car level without"), G.GetCarLevel(), -1);
	FVector2D A, B;
	RowStrip(40, 40, 10, A, B);
	const FLotEvents Events = G.PourFromCar(A, B);
	TestEqual(TEXT("no litres"), Events.LitresPoured, 0.0);
	TestEqual(TEXT("no cells"), Events.CompletedCells.Num(), 0);
	TestEqual(TEXT("nothing on the ground"), G.GetTotalLitres(), 0.0);
	TestEqual(TEXT("stock untouched"), G.GetPlantStock(), G.GetPlantStorage());
	return true;
}

bool FLotCarWidthTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::Car);
	FullPlant(G);
	G.DebugFillTanks();
	TestEqual(TEXT("plain bar"), G.GetCarLevel(), 0);
	FVector2D A, B;
	RowStrip(40, 40, 10, A, B);
	const FLotEvents Events = G.PourFromCar(A, B);
	TestEqual(TEXT("three rows of ten"), G.GetCompletedCells(), 30);
	TestEqual(TEXT("reported"), Events.CompletedCells.Num(), 30);
	TestNearlyEqual(TEXT("credits: ten a cell and one a litre"), Events.CreditsEarned, 300.0 + Events.LitresPoured * LotBalance::CreditsPerLitre, Tolerance);
	TestTrue(TEXT("row above"), G.GetGrid().IsCellComplete(MiddleCell(45, 39)));
	TestTrue(TEXT("row below"), G.GetGrid().IsCellComplete(MiddleCell(45, 41)));
	TestEqual(TEXT("no fourth row"), G.GetGrid().GetCellDone(MiddleCell(45, 42)), 0);
	TestEqual(TEXT("start of the strip"), G.GetGrid().GetCellDone(MiddleCell(39, 40)), 0);

	// Every tier lays a strip of its bar's width: the three rows of the plain bar are complete, what is wider is laid beside them,
	// whole rows complete and the rest of the next row part covered (one texel row is 5 cm, 20 texels of a cell).
	for (int32 Level = 1; Level < LotBalance::NumCarLevels; ++Level)
	{
		FLotGame W;
		for (int32 Tier = 0; Tier <= Level; ++Tier)
		{
			W.DebugGrantItem(static_cast<ELotItem>(static_cast<int32>(ELotItem::Car) + Tier));
		}
		FullPlant(W);
		W.DebugFillTanks();
		TestEqual(TEXT("tier level"), W.GetCarLevel(), Level);
		W.PourFromCar(A, B);
		const double Extra = (LotBalance::CarWidthCm[Level] - LotBalance::CarWidthCm[0]) * 0.5;
		const int32 ExtraRows = FMath::FloorToInt(Extra / AsphaltLot::CellCm);
		const int32 PartTexels = FMath::RoundToInt((Extra - ExtraRows * AsphaltLot::CellCm) / 5.0) * 20;
		TestEqual(*FString::Printf(TEXT("tier %d: rows complete"), Level + 1), W.GetCompletedCells(), 10 * (3 + 2 * ExtraRows));
		for (int32 Col = 40; Col < 50; ++Col)
		{
			TestTrue(TEXT("whole row above"), ExtraRows == 0 || W.GetGrid().IsCellComplete(MiddleCell(Col, 39 - ExtraRows)));
			TestTrue(TEXT("whole row below"), ExtraRows == 0 || W.GetGrid().IsCellComplete(MiddleCell(Col, 41 + ExtraRows)));
			TestEqual(*FString::Printf(TEXT("tier %d: part of the row above"), Level + 1), W.GetGrid().GetCellDone(MiddleCell(Col, 38 - ExtraRows)), PartTexels);
			TestEqual(*FString::Printf(TEXT("tier %d: part of the row below"), Level + 1), W.GetGrid().GetCellDone(MiddleCell(Col, 42 + ExtraRows)), PartTexels);
			TestEqual(*FString::Printf(TEXT("tier %d: nothing beyond"), Level + 1), W.GetGrid().GetCellDone(MiddleCell(Col, 37 - ExtraRows)), 0);
		}
	}
	return true;
}

namespace
{
	/** Texels that are Done in a square window of the grid, as a set of indexes. */
	TSet<int32> DoneTexels(const FLotGrid& Grid, FVector2D Centre, double HalfCm)
	{
		TSet<int32> Result;
		const auto ToTexel = [](double World) { return static_cast<int32>(FMath::Floor((World + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm)); };
		for (int32 Y = ToTexel(Centre.Y - HalfCm); Y <= ToTexel(Centre.Y + HalfCm); ++Y)
		{
			for (int32 X = ToTexel(Centre.X - HalfCm); X <= ToTexel(Centre.X + HalfCm); ++X)
			{
				if (Grid.GetTexel(X, Y) == ELotTexel::Done)
				{
					Result.Add(Y * FLotGrid::Size + X);
				}
			}
		}
		return Result;
	}

	/** The car drives a half circle of the given radius; the bar is behind it. Steps of StepCm along the path, with or without the headings. */
	void DriveCurve(FLotGame& G, FVector2D Centre, double Radius, double StepCm, bool bWithHeadings)
	{
		const double Arc = UE_DOUBLE_PI * Radius;
		const int32 Steps = FMath::CeilToInt(Arc / StepCm);
		FVector2D PreviousBar;
		FVector2D PreviousForward;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const double Phi = UE_DOUBLE_PI * I / Steps;
			const FVector2D Forward(-FMath::Sin(Phi), FMath::Cos(Phi));
			const FVector2D Bar = Centre + FVector2D(FMath::Cos(Phi), FMath::Sin(Phi)) * Radius - Forward * LotBalance::CarBarBehindCm[0];
			if (I > 0)
			{
				if (bWithHeadings)
				{
					G.PourFromCar(PreviousBar, Bar, PreviousForward, Forward);
				}
				else
				{
					G.PourFromCar(PreviousBar, Bar);
				}
			}
			PreviousBar = Bar;
			PreviousForward = Forward;
		}
	}
}

bool FLotCarTurnLeavesNoGapTest::RunTest(const FString& Parameters)
{
	const FVector2D Centre = AsphaltLot::CellCentre(MiddleCell(40, 40));
	const double Radius = 450.0;
	const double Window = 1400.0;

	// The reference: the same turn driven in tiny steps (the bar's path is smooth there).
	FLotGame Reference;
	Reference.DebugGrantItem(ELotItem::Car);
	Reference.DebugFillTanks();
	DriveCurve(Reference, Centre, Radius, 1.0, true);
	const TSet<int32> Wanted = DoneTexels(Reference.GetGrid(), Centre, Window);
	TestTrue(TEXT("the reference laid a real strip"), Wanted.Num() > 5000);

	// The same turn at the length of a frame (10 cm) with the headings: no more than a texel of fringe may be missing.
	FLotGame Coarse;
	Coarse.DebugGrantItem(ELotItem::Car);
	Coarse.DebugFillTanks();
	DriveCurve(Coarse, Centre, Radius, 10.0, true);
	const TSet<int32> Got = DoneTexels(Coarse.GetGrid(), Centre, Window);
	int32 Missing = 0;
	for (const int32 Index : Wanted)
	{
		Missing += Got.Contains(Index) ? 0 : 1;
	}
	TestTrue(*FString::Printf(TEXT("the turn is unbroken (%d of %d texels missing)"), Missing, Wanted.Num()), Missing <= Wanted.Num() / 200);

	// Without the headings the bar is taken across its way of travel, which on a turn is not across the car: the gaps and the fringe.
	FLotGame Old;
	Old.DebugGrantItem(ELotItem::Car);
	Old.DebugFillTanks();
	DriveCurve(Old, Centre, Radius, 10.0, false);
	const TSet<int32> OldGot = DoneTexels(Old.GetGrid(), Centre, Window);
	int32 OldMissing = 0;
	for (const int32 Index : Wanted)
	{
		OldMissing += OldGot.Contains(Index) ? 0 : 1;
	}
	TestTrue(*FString::Printf(TEXT("the headings fix something (%d missing without them, %d with)"), OldMissing, Missing), OldMissing > Missing);

	// A straight line gives the same strip with and without the headings.
	FLotGame Straight;
	Straight.DebugGrantItem(ELotItem::Car);
	Straight.DebugFillTanks();
	FLotGame Plain;
	Plain.DebugGrantItem(ELotItem::Car);
	Plain.DebugFillTanks();
	const FVector2D P(-300.0, 40.0);
	const FVector2D Q(700.0, 140.0);
	const FVector2D Dir = (Q - P).GetSafeNormal();
	Straight.PourFromCar(Centre + P, Centre + Q, Dir, Dir);
	Plain.PourFromCar(Centre + P, Centre + Q);
	const TSet<int32> StraightTexels = DoneTexels(Straight.GetGrid(), Centre, Window);
	const TSet<int32> PlainTexels = DoneTexels(Plain.GetGrid(), Centre, Window);
	TestTrue(TEXT("a straight run is the same strip"), StraightTexels.Num() == PlainTexels.Num() && StraightTexels.Difference(PlainTexels).Num() == 0);
	return true;
}

bool FLotCarDrawsFromTankTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::Car);
	FullPlant(G);
	const double Stock = G.GetPlantStock();
	TestEqual(TEXT("the tank starts empty"), G.GetCarTank(), 0.0);
	FVector2D A, B;
	RowStrip(40, 40, 60, A, B);
	TestEqual(TEXT("an empty tank lays nothing"), G.PourFromCar(A, B).LitresPoured, 0.0);
	TestEqual(TEXT("and the stock is left alone"), G.GetPlantStock(), Stock);

	G.DebugFillTanks();
	const double Tank = G.GetCarTank();
	TestEqual(TEXT("full tank"), Tank, LotBalance::CarTankL[0]);
	// 60 cells of three rows need about 1700 litres; a 6000 l tank holds them.
	const FLotEvents Events = G.PourFromCar(A, B);
	TestTrue(TEXT("poured something"), Events.LitresPoured > 0.0);
	TestNearlyEqual(TEXT("tank paid exactly"), G.GetCarTank(), Tank - Events.LitresPoured, Tolerance);
	TestEqual(TEXT("the plant stock is not touched by the bar"), G.GetPlantStock(), Stock);
	TestNearlyEqual(TEXT("booked on the ground"), G.GetTotalLitres(), Events.LitresPoured, Tolerance);

	// A long strip empties the tank and stops there.
	FVector2D C, D;
	RowStrip(0, 40, 200, C, D);
	FLotGame H;
	H.DebugGrantItem(ELotItem::Car);
	H.DebugFillTanks();
	// One strip of 200 cells takes about 5700 litres, so two strips next to each other are more than the tank holds.
	FVector2D E, F;
	RowStrip(0, 44, 200, E, F);
	const FLotEvents Long = H.PourFromCar(C, D);
	const FLotEvents Longer = H.PourFromCar(E, F);
	TestTrue(TEXT("tank used up"), H.GetCarTank() < FLotGrid::LitresPerTexel);
	TestTrue(TEXT("at most the tank"), Long.LitresPoured + Longer.LitresPoured <= LotBalance::CarTankL[0] + Tolerance);
	TestEqual(TEXT("second call pours nothing"), H.PourFromCar(E, F).LitresPoured, 0.0);
	TestTrue(TEXT("bad input is ignored"), G.PourFromCar(FVector2D(NaN, 0.0), B).LitresPoured == 0.0);
	return true;
}

bool FLotCarLoadsAtStationTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("no car, no tank"), G.GetCarTankCapacity(), 0.0);
	G.DebugGrantItem(ELotItem::Car);
	FullPlant(G);
	TestEqual(TEXT("tank size"), G.GetCarTankCapacity(), LotBalance::CarTankL[0]);
	G.World().CarPos = LotLayout::ItemSpawn(ELotItem::Car);
	TestFalse(TEXT("the spawn is not at the station"), G.IsAtStation(G.World().CarPos));
	TestEqual(TEXT("nothing is loaded away from a station"), G.LoadCar(1.0), 0.0);

	G.World().CarPos = LotLayout::VehicleStationPos + FVector2D(LotBalance::VehicleStationRadiusCm - 10.0, 0.0);
	TestTrue(TEXT("at the station"), G.IsAtStation(G.World().CarPos));
	const double Stock = G.GetPlantStock();
	const double Moved = G.LoadCar(1.0);
	TestNearlyEqual(TEXT("a second of loading"), Moved, LotBalance::VehicleLoadRateLPerS, Tolerance);
	TestNearlyEqual(TEXT("tank has it"), G.GetCarTank(), Moved, Tolerance);
	TestNearlyEqual(TEXT("stock paid"), G.GetPlantStock(), Stock - Moved, Tolerance);
	for (int32 I = 0; I < 20; ++I)
	{
		G.LoadCar(1.0);
	}
	TestEqual(TEXT("full, not more"), G.GetCarTank(), G.GetCarTankCapacity());
	TestEqual(TEXT("bad time"), G.LoadCar(NaN), 0.0);

	// A poor plant stock limits it.
	FLotGame P;
	P.DebugGrantItem(ELotItem::Car);
	P.World().CarPos = LotLayout::VehicleStationPos;
	P.DebugSetPlantStock(30.0);
	P.LoadCar(10.0);
	TestNearlyEqual(TEXT("only the stock"), P.GetCarTank(), 30.0, Tolerance);
	TestEqual(TEXT("stock empty"), P.GetPlantStock(), 0.0);

	// The portable station is a second one once it is bought and set down; held, it does not count.
	FLotGame S;
	TestEqual(TEXT("one station"), S.GetNumStations(), 1);
	S.DebugGrantItem(ELotItem::Station);
	TestEqual(TEXT("two stations"), S.GetNumStations(), 2);
	TestEqual(TEXT("its place"), S.GetStationPos(1), LotLayout::ItemSpawn(ELotItem::Station));
	TestTrue(TEXT("grab it"), S.Grab(ELotMovable::Station));
	TestEqual(TEXT("held: one station"), S.GetNumStations(), 1);
	S.Release(FVector2D(3000.0, 3000.0), 0.0);
	TestEqual(TEXT("set down"), S.GetNumStations(), 2);
	TestEqual(TEXT("nearest of two"), S.FindNearestStation(FVector2D(3100.0, 3000.0)), 1);
	TestEqual(TEXT("nearest of two, other side"), S.FindNearestStation(FVector2D(-3000.0, -9000.0)), 0);
	return true;
}

bool FLotCarSpawnAndLevelTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::CarT2);
	TestEqual(TEXT("upgrade alone is no car"), G.GetCarLevel(), -1);
	G.DebugGrantItem(ELotItem::Car);
	TestEqual(TEXT("car spawn"), G.World().CarPos, LotLayout::ItemSpawn(ELotItem::Car));
	TestEqual(TEXT("car II"), G.GetCarLevel(), 1);
	G.World().CarPos = FVector2D(5.0, 6.0);
	G.DebugGrantItem(ELotItem::Car);
	TestEqual(TEXT("not moved again"), G.World().CarPos, FVector2D(5.0, 6.0));

	// The level is the highest tier owned, 0..3, and the tank follows it.
	FLotGame L;
	TestEqual(TEXT("no car"), L.GetCarLevel(), -1);
	for (int32 Level = 0; Level < LotBalance::NumCarLevels; ++Level)
	{
		L.DebugGrantItem(static_cast<ELotItem>(static_cast<int32>(ELotItem::Car) + Level));
		TestEqual(*FString::Printf(TEXT("tier %d"), Level + 1), L.GetCarLevel(), Level);
		TestEqual(*FString::Printf(TEXT("tier %d tank size"), Level + 1), L.GetCarTankCapacity(), LotBalance::CarTankL[Level]);
		L.DebugFillTanks();
		TestEqual(*FString::Printf(TEXT("tier %d full tank"), Level + 1), L.GetCarTank(), LotBalance::CarTankL[Level]);
	}
	// A tier above a missing one does not count (the shop never lets it happen).
	FLotGame Gap;
	Gap.DebugGrantItem(ELotItem::Car);
	Gap.DebugGrantItem(ELotItem::CarT3);
	TestEqual(TEXT("a gap stops the level"), Gap.GetCarLevel(), 0);
	return true;
}

bool FLotMachinesLitresConservedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);
	G.DebugGrantItem(ELotItem::Car);
	G.AssignAutoPaverSector(0, TestSector);
	const auto Total = [&G]() { return G.GetPlantStock() + G.GetTotalLitres() + G.GetAutoPaverTank(0) + G.GetCarTank(); };
	const double Sum0 = Total();
	const double Rate = G.GetPlantRatePerMin() / 60.0;
	double Time = 0.0;
	for (int32 I = 0; I < 400; ++I)
	{
		G.Tick(0.25);
		Time += 0.25;
		TestTrue(TEXT("stock below the storage, so nothing is clipped"), G.GetPlantStock() < G.GetPlantStorage());
		if (!TestNearlyEqual(TEXT("auto paver conserves litres"), Total(), Sum0 + Rate * Time, 1e-6))
		{
			break;
		}
	}
	TestTrue(TEXT("the auto paver loaded"), G.GetAutoPaverTank(0) > 0.0);

	G.DebugSetPlantStock(300.0);
	G.DebugFillTanks();
	const double Sum1 = Total();
	FVector2D A, B;
	RowStrip(40, 40, 30, A, B);
	const FLotEvents Events = G.PourFromCar(A, B);
	TestTrue(TEXT("the car poured"), Events.LitresPoured > 0.0);
	TestNearlyEqual(TEXT("car conserves litres"), Total(), Sum1, 1e-6);
	// Non-finite litres never reach the books.
	G.PourFromCar(FVector2D(Infinity, 0.0), B);
	TestNearlyEqual(TEXT("bad input conserves litres"), Total(), Sum1, 1e-6);
	return true;
}

namespace
{
	/** A game in which the unlock step of the robots is open and rich enough for all of them. */
	void OpenRobotShop(FLotGame& G)
	{
		G.DebugGive(1000000.0);
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			G.DebugCompleteCell(AsphaltLot::EntryCell(static_cast<int32>(ELotUnlock::AutoPaver), I));
		}
		G.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::AutoPaver)]);
	}

	/** Grants the first Count robots, in order; they stay under orders (AUTO off). */
	void GrantPavers(FLotGame& G, int32 Count)
	{
		for (int32 P = 0; P < Count; ++P)
		{
			G.DebugGrantItem(AutoPaverItem[P]);
		}
		G.SetAutoPaverAuto(false);
	}
}

bool FLotAutoPaverSecondNeedsFirstTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	OpenRobotShop(G);
	TestEqual(TEXT("price of robot 2"), G.GetItemPrice(ELotItem::AutoPaver2), 40000.0);
	TestEqual(TEXT("price of robot 3"), G.GetItemPrice(ELotItem::AutoPaver3), 80000.0);
	TestTrue(TEXT("robot 2 is sold by the robot building"), LotBalance::ItemShop[static_cast<int32>(ELotItem::AutoPaver2)] == ELotShop::Robots);
	TestTrue(TEXT("robot 3 is sold by the robot building"), LotBalance::ItemShop[static_cast<int32>(ELotItem::AutoPaver3)] == ELotShop::Robots);
	TestEqual(TEXT("no robot yet"), G.GetNumAutoPavers(), 0);

	TestFalse(TEXT("robot 2 is not offered without robot 1"), G.IsItemOffered(ELotItem::AutoPaver2));
	TestFalse(TEXT("robot 3 is not offered without robot 1"), G.IsItemOffered(ELotItem::AutoPaver3));
	TestFalse(TEXT("robot 2 cannot be bought without robot 1"), G.BuyItem(ELotItem::AutoPaver2));
	TestTrue(TEXT("robot 1"), G.BuyItem(ELotItem::AutoPaver));
	TestEqual(TEXT("one robot"), G.GetNumAutoPavers(), 1);
	TestTrue(TEXT("robot 2 is offered now"), G.IsItemOffered(ELotItem::AutoPaver2));
	TestFalse(TEXT("robot 3 is not offered without robot 2"), G.IsItemOffered(ELotItem::AutoPaver3));
	TestFalse(TEXT("robot 3 cannot be bought without robot 2"), G.BuyItem(ELotItem::AutoPaver3));
	const double Credits = G.GetCredits();
	TestTrue(TEXT("robot 2"), G.BuyItem(ELotItem::AutoPaver2));
	TestEqual(TEXT("robot 2 cost 40000"), Credits - G.GetCredits(), 40000.0);
	TestTrue(TEXT("robot 3 is offered now"), G.IsItemOffered(ELotItem::AutoPaver3));
	TestTrue(TEXT("robot 3"), G.BuyItem(ELotItem::AutoPaver3));
	TestEqual(TEXT("three robots"), G.GetNumAutoPavers(), 3);
	TestFalse(TEXT("a bought robot is not offered again"), G.IsItemOffered(ELotItem::AutoPaver3));

	// Every robot has its upgrades from the moment it is bought.
	for (int32 P = 0; P < MaxAutoPavers; ++P)
	{
		TestTrue(*FString::Printf(TEXT("robot %d can be upgraded"), P), G.CanBuyPaverUpgrade(P, ELotPaverUpgrade::Speed) && G.CanBuyPaverUpgrade(P, ELotPaverUpgrade::Capacity));
	}
	return true;
}

bool FLotAutoPaverSpawnInDocksTest::RunTest(const FString& Parameters)
{
	constexpr double Tol = 0.01;
	FLotGame G;
	OpenRobotShop(G);
	for (int32 P = 0; P < MaxAutoPavers; ++P)
	{
		TestEqual(*FString::Printf(TEXT("robot %d is nowhere before it is bought"), P), G.GetAutoPaverPos(P), FVector2D::ZeroVector);
		TestTrue(*FString::Printf(TEXT("robot %d bought"), P), G.BuyItem(AutoPaverItem[P]));
		TestEqual(*FString::Printf(TEXT("%d robots"), P + 1), G.GetNumAutoPavers(), P + 1);
		TestTrue(*FString::Printf(TEXT("robot %d stands in dock %d"), P, P), G.GetAutoPaverPos(P).Equals(LotLayout::PaverDock(P), Tol));
	}
	for (int32 P = 0; P < MaxAutoPavers; ++P)
	{
		TestTrue(*FString::Printf(TEXT("robot %d still in its dock"), P), G.GetAutoPaverPos(P).Equals(LotLayout::PaverDock(P), Tol));
	}
	TestTrue(TEXT("the first robot spawns where the item spawn table says"), LotLayout::ItemSpawn(ELotItem::AutoPaver).Equals(LotLayout::PaverDock(0), Tol));

	// A robot outside the owned ones is the empty one.
	TestEqual(TEXT("robot 3 does not exist"), G.GetAutoPaverPos(MaxAutoPavers), FVector2D::ZeroVector);
	TestEqual(TEXT("robot -1 does not exist"), G.GetAutoPaverPos(-1), FVector2D::ZeroVector);
	TestEqual(TEXT("its sector"), G.GetAutoPaverSector(MaxAutoPavers), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("its tank"), G.GetAutoPaverTank(-1), 0.0);
	TestFalse(TEXT("it cannot be sent"), G.AssignAutoPaverSector(MaxAutoPavers, 3));
	return true;
}

bool FLotAutoPaverAutoPicksDifferentSectorsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.DebugGrantItem(ELotItem::AutoPaver2);
	TestTrue(TEXT("automatic"), G.IsAutoPaverAuto());
	FullPlant(G);
	G.DebugFillTanks();
	RunFed(G, 5.0);
	const int32 First = G.GetAutoPaverSector(0);
	const int32 Second = G.GetAutoPaverSector(1);
	TestTrue(TEXT("robot 1 has a sector"), First != INDEX_NONE);
	TestTrue(TEXT("robot 2 has a sector"), Second != INDEX_NONE);
	TestNotEqual(TEXT("not the same one"), First, Second);
	TestEqual(TEXT("the sector knows its robot (1)"), G.FindAutoPaverInSector(First), 0);
	TestEqual(TEXT("the sector knows its robot (2)"), G.FindAutoPaverInSector(Second), 1);

	// And so it stays for as long as they work.
	for (int32 I = 0; I < 200; ++I)
	{
		RunFed(G, 1.0);
		if (G.GetAutoPaverSector(0) != INDEX_NONE && G.GetAutoPaverSector(0) == G.GetAutoPaverSector(1))
		{
			AddError(TEXT("the robots share a sector"));
			break;
		}
	}
	return true;
}

bool FLotAutoPaverAssignStealsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	GrantPavers(G, 2);
	FullPlant(G);
	G.DebugFillTanks();
	TestFalse(TEXT("a robot that is not bought cannot be sent"), G.AssignAutoPaverSector(2, 7));
	TestTrue(TEXT("robot 0 to sector 7"), G.AssignAutoPaverSector(0, 7));
	RunFed(G, 5.0);
	TestEqual(TEXT("robot 0 works there"), G.GetAutoPaverSector(0), 7);
	TestEqual(TEXT("found"), G.FindAutoPaverInSector(7), 0);
	TestEqual(TEXT("nobody in another sector"), G.FindAutoPaverInSector(8), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("nobody in an invalid sector"), G.FindAutoPaverInSector(AsphaltLot::NumSectors), static_cast<int32>(INDEX_NONE));
	const FVector2D Where = G.GetAutoPaverPos(0);
	const double Tank = G.GetAutoPaverTank(0);

	TestTrue(TEXT("robot 1 takes the sector"), G.AssignAutoPaverSector(1, 7));
	TestEqual(TEXT("robot 1 has it"), G.GetAutoPaverSector(1), 7);
	TestEqual(TEXT("robot 1 starts from the first cell"), G.GetAutoPaverStep(1), 0);
	TestEqual(TEXT("robot 0 lost it"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("robot 0 step"), G.GetAutoPaverStep(0), 0);
	TestTrue(TEXT("robot 0 is idle"), G.GetAutoPaverPhase(0) == ELotPaverPhase::Idle);
	TestFalse(TEXT("robot 0 does not travel"), G.IsAutoPaverTravelling(0));
	TestEqual(TEXT("robot 0 stays where it was"), G.GetAutoPaverPos(0), Where);
	TestEqual(TEXT("robot 0 keeps its tank"), G.GetAutoPaverTank(0), Tank);
	TestEqual(TEXT("the sector now belongs to robot 1"), G.FindAutoPaverInSector(7), 1);

	// A robot that has lost its sector, with AUTO off, waits for orders.
	RunFed(G, 5.0);
	TestEqual(TEXT("robot 0 still waits"), G.GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and robot 1 still has the sector"), G.GetAutoPaverSector(1), 7);

	// Sending a robot to its own sector restarts it and takes nothing from the other.
	TestTrue(TEXT("robot 1 to its own sector"), G.AssignAutoPaverSector(1, 7));
	TestEqual(TEXT("the same sector"), G.GetAutoPaverSector(1), 7);
	TestEqual(TEXT("it restarts"), G.GetAutoPaverStep(1), 0);
	return true;
}

bool FLotAutoPaverFewerSectorsThanPaversTest::RunTest(const FString& Parameters)
{
	constexpr int32 LastSector = 3;
	FLotGame G;
	for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
	{
		if (Sector == LastSector)
		{
			continue;
		}
		for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
		{
			G.DebugCompleteCell(AsphaltLot::SectorCell(Sector, Step));
		}
	}
	TestEqual(TEXT("only one sector is left"), CompleteCellsOfSector(G, LastSector), 0);
	GrantPavers(G, 3);
	G.SetAutoPaverAuto(true);
	FullPlant(G);
	G.DebugFillTanks();

	int32 MostOnTheLastSector = 0;
	for (double T = 0.0; T < 60.0; T += 0.25)
	{
		G.Tick(0.25);
		FullPlant(G);
		G.DebugFillTanks();
		int32 OnLast = 0;
		for (int32 P = 0; P < 3; ++P)
		{
			if (G.GetAutoPaverSector(P) == LastSector)
			{
				++OnLast;
			}
			else if (G.GetAutoPaverSector(P) != INDEX_NONE || G.GetAutoPaverPhase(P) != ELotPaverPhase::Idle)
			{
				AddError(*FString::Printf(TEXT("robot %d is not idle without a sector at %.2f s"), P, T));
				return false;
			}
		}
		if (T >= 1.0 && !TestEqual(TEXT("exactly one robot has the last sector"), OnLast, 1))
		{
			return false;
		}
		MostOnTheLastSector = FMath::Max(MostOnTheLastSector, OnLast);
	}
	TestEqual(TEXT("never two on a sector"), MostOnTheLastSector, 1);
	TestTrue(TEXT("and it laid something"), CompleteCellsOfSector(G, LastSector) > 0);

	// A robot can still be sent over finished ground: it drives there, passes the cells and lays nothing.
	const int32 Holder = G.FindAutoPaverInSector(LastSector);
	const int32 Idle = Holder == 1 ? 0 : 1; // one of the two that wait
	const int32 DoneSector = LastSector == 0 ? 1 : 0;
	const double TankBefore = G.GetAutoPaverTank(Idle);
	TestTrue(TEXT("sent to a finished sector"), G.AssignAutoPaverSector(Idle, DoneSector));
	TestEqual(TEXT("the finished sector is its"), G.FindAutoPaverInSector(DoneSector), Idle);
	TestEqual(TEXT("the other robot still has the last sector"), G.FindAutoPaverInSector(LastSector), Holder);
	bool bPassed = false;
	for (int32 I = 0; I < 2000 && !bPassed; ++I)
	{
		G.Tick(0.25);
		bPassed = G.GetAutoPaverSector(Idle) == INDEX_NONE;
	}
	TestTrue(TEXT("it passed the whole sector"), bPassed);
	TestEqual(TEXT("without a litre laid by it"), G.GetAutoPaverTank(Idle), TankBefore);
	return true;
}

bool FLotAutoPaverSharedStockTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	GrantPavers(G, 2);
	TestTrue(TEXT("robot 0 sent"), G.AssignAutoPaverSector(0, 12));
	TestTrue(TEXT("robot 1 sent"), G.AssignAutoPaverSector(1, 13));
	// Both drive to the station with empty tanks; the plant stock is kept at nothing, so that they only wait there.
	bool bBothThere = false;
	for (int32 I = 0; I < 2000 && !bBothThere; ++I)
	{
		G.DebugSetPlantStock(0.0);
		G.Tick(0.25);
		bBothThere = G.GetAutoPaverPhase(0) == ELotPaverPhase::Loading && G.GetAutoPaverPhase(1) == ELotPaverPhase::Loading;
	}
	if (!TestTrue(TEXT("both robots wait at the station"), bBothThere))
	{
		return false;
	}
	G.DebugSetPlantStock(100.0);
	const double TanksBefore = G.GetAutoPaverTank(0) + G.GetAutoPaverTank(1);
	const double Production = G.GetPlantRatePerMin() / 60.0 * 0.25;
	G.Tick(0.25);
	const double Moved = G.GetAutoPaverTank(0) + G.GetAutoPaverTank(1) - TanksBefore;
	TestTrue(TEXT("the tanks together took no more than the stock held"), Moved <= 100.0 + Production + 1e-6);
	TestTrue(TEXT("the stock never goes below nothing"), G.GetPlantStock() >= 0.0);
	TestNearlyEqual(TEXT("what the tanks took is what the stock lost"), G.GetPlantStock(), 100.0 + Production - Moved, 1e-6);
	TestTrue(TEXT("and it was the whole stock"), G.GetPlantStock() <= 1e-6);
	// The first robot in the order is served first.
	TestTrue(TEXT("robot 0 got the most"), G.GetAutoPaverTank(0) >= G.GetAutoPaverTank(1));
	return true;
}

bool FLotAutoPaverPerRobotSpeedTest::RunTest(const FString& Parameters)
{
	// Two robots at different speed levels lay cells at their own rates.
	FLotGame G;
	GrantPavers(G, 2);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 1);
	FullPlant(G);
	G.DebugFillTanks();
	constexpr int32 SectorA = 12;
	constexpr int32 SectorB = 40;
	G.AssignAutoPaverSector(0, SectorA);
	G.AssignAutoPaverSector(1, SectorB);
	for (int32 I = 0; I < 2000 && (G.IsAutoPaverTravelling(0) || G.IsAutoPaverTravelling(1)); ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
		G.DebugFillTanks();
	}
	TestFalse(TEXT("robot 0 arrived"), G.IsAutoPaverTravelling(0));
	TestFalse(TEXT("robot 1 arrived"), G.IsAutoPaverTravelling(1));
	const int32 BaseA = CompleteCellsOfSector(G, SectorA);
	const int32 BaseB = CompleteCellsOfSector(G, SectorB);
	RunFed(G, 60.0);
	const int32 CellsA = CompleteCellsOfSector(G, SectorA) - BaseA;
	const int32 CellsB = CompleteCellsOfSector(G, SectorB) - BaseB;
	TestTrue(*FString::Printf(TEXT("robot 0 (speed 1) laid %d cells in a minute"), CellsA), FMath::Abs(CellsA - LotBalance::AutoPaverRateCellsPerMin[1]) <= 2.0);
	TestTrue(*FString::Printf(TEXT("robot 1 (speed 0) laid %d cells in a minute"), CellsB), FMath::Abs(CellsB - LotBalance::AutoPaverRateCellsPerMin[0]) <= 2.0);
	return true;
}

bool FLotAutoPaverUpgradePurchaseTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("no robot: level"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), -1);
	TestEqual(TEXT("no robot: price"), G.GetPaverUpgradePrice(0, ELotPaverUpgrade::Speed), -1.0);
	TestFalse(TEXT("no robot: cannot buy"), G.CanBuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	G.DebugGive(1.0e6);
	TestFalse(TEXT("no robot: not bought"), G.BuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	TestEqual(TEXT("no robot: nothing paid"), G.GetCredits(), 1.0e6);

	G.DebugGrantItem(ELotItem::AutoPaver);
	for (const ELotPaverUpgrade Kind : {ELotPaverUpgrade::Speed, ELotPaverUpgrade::Capacity})
	{
		TestEqual(TEXT("robot 1 is not owned"), G.GetPaverUpgradeLevel(1, Kind), -1);
		TestFalse(TEXT("robot 1 cannot be upgraded"), G.BuyPaverUpgrade(1, Kind));
		TestFalse(TEXT("robot -1 cannot be upgraded"), G.BuyPaverUpgrade(-1, Kind));
		TestFalse(TEXT("robot 3 cannot be upgraded"), G.BuyPaverUpgrade(MaxAutoPavers, Kind));
	}
	TestFalse(TEXT("an invalid upgrade is not bought"), G.BuyPaverUpgrade(0, ELotPaverUpgrade::Count));
	TestEqual(TEXT("an invalid upgrade has no level"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Count), -1);
	TestEqual(TEXT("an invalid upgrade has no price"), G.GetPaverUpgradePrice(0, ELotPaverUpgrade::Count), -1.0);
	TestEqual(TEXT("nothing paid by the refused ones"), G.GetCredits(), 1.0e6);

	// The levels come in order at the table's prices.
	for (const ELotPaverUpgrade Kind : {ELotPaverUpgrade::Speed, ELotPaverUpgrade::Capacity})
	{
		const int32 Row = static_cast<int32>(Kind);
		TestEqual(TEXT("level 0 at the start"), G.GetPaverUpgradeLevel(0, Kind), 0);
		for (int32 Level = 0; Level < MaxPaverUpgradeLevel; ++Level)
		{
			const double Price = LotBalance::PaverUpgradePrice[Row][Level];
			TestEqual(TEXT("the price of the next level"), G.GetPaverUpgradePrice(0, Kind), Price);
			const double Before = G.GetCredits();
			TestTrue(TEXT("it can be bought"), G.CanBuyPaverUpgrade(0, Kind));
			TestTrue(TEXT("and is"), G.BuyPaverUpgrade(0, Kind));
			TestEqual(TEXT("it cost the price"), Before - G.GetCredits(), Price);
			TestEqual(TEXT("one level more"), G.GetPaverUpgradeLevel(0, Kind), Level + 1);
		}
		TestEqual(TEXT("no price at the maximum"), G.GetPaverUpgradePrice(0, Kind), -1.0);
		TestFalse(TEXT("cannot be bought at the maximum"), G.CanBuyPaverUpgrade(0, Kind));
		const double Before = G.GetCredits();
		TestFalse(TEXT("is not bought at the maximum"), G.BuyPaverUpgrade(0, Kind));
		TestEqual(TEXT("nothing paid at the maximum"), G.GetCredits(), Before);
		TestEqual(TEXT("the level stays at the maximum"), G.GetPaverUpgradeLevel(0, Kind), MaxPaverUpgradeLevel);
	}
	TestEqual(TEXT("the whole bill of one robot"), 1.0e6 - G.GetCredits(), 20000.0 + 60000.0 + 5000.0 + 15000.0);

	// Credits decide: one credit short buys nothing, the exact sum does.
	FLotGame Poor;
	Poor.DebugGrantItem(ELotItem::AutoPaver);
	Poor.DebugGive(LotBalance::PaverUpgradePrice[0][0] - 1.0);
	TestFalse(TEXT("one credit short"), Poor.CanBuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	TestFalse(TEXT("one credit short is not bought"), Poor.BuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	TestEqual(TEXT("level unchanged"), Poor.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	Poor.DebugGive(1.0);
	TestTrue(TEXT("the exact price"), Poor.BuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	TestEqual(TEXT("nothing left"), Poor.GetCredits(), 0.0);
	return true;
}

bool FLotAutoPaverUpgradeEffectsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGive(1.0e6);
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.SetAutoPaverAuto(false);

	// A speed level raises the rate at once and leaves the tank alone.
	TestEqual(TEXT("rate 0"), G.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[0]);
	for (int32 Level = 1; Level <= MaxPaverUpgradeLevel; ++Level)
	{
		TestTrue(TEXT("speed bought"), G.BuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
		TestEqual(TEXT("the rate follows the speed level"), G.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[Level]);
		TestEqual(TEXT("the tank size does not"), G.GetAutoPaverTankCapacity(0), LotBalance::AutoPaverTankLByLevel[0]);
	}

	// A capacity level raises the size of the tank; the tank is not filled by it.
	G.DebugFillTanks();
	TestEqual(TEXT("a full tank"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[0]);
	for (int32 Level = 1; Level <= MaxPaverUpgradeLevel; ++Level)
	{
		TestTrue(TEXT("capacity bought"), G.BuyPaverUpgrade(0, ELotPaverUpgrade::Capacity));
		TestEqual(TEXT("the tank size follows the capacity level"), G.GetAutoPaverTankCapacity(0), LotBalance::AutoPaverTankLByLevel[Level]);
		TestEqual(TEXT("the tank holds what it held"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[0]);
		TestEqual(TEXT("the rate does not follow the capacity level"), G.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[MaxPaverUpgradeLevel]);
	}
	G.DebugFillTanks();
	TestEqual(TEXT("filling fills the bigger tank"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[MaxPaverUpgradeLevel]);

	// A bigger tank lasts longer: 4500 l are 450 cells, more than the 400 of a sector, so a full big tank does not run dry in one.
	FLotGame B;
	B.DebugGrantItem(ELotItem::AutoPaver);
	B.SetAutoPaverAuto(false);
	B.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 2);
	B.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 2);
	FullPlant(B);
	B.DebugFillTanks();
	B.AssignAutoPaverSector(0, TestSector);
	bool bWentToStation = false;
	for (int32 I = 0; I < 4 * 600 && B.GetAutoPaverSector(0) != INDEX_NONE; ++I)
	{
		B.Tick(0.25);
		FullPlant(B);
		bWentToStation = bWentToStation || B.GetAutoPaverPhase(0) == ELotPaverPhase::ToStation || B.GetAutoPaverPhase(0) == ELotPaverPhase::Loading;
	}
	TestEqual(TEXT("the sector is done"), CompleteCellsOfSector(B, TestSector), AsphaltLot::CellsPerSector);
	TestFalse(TEXT("without a trip to the station"), bWentToStation);
	return true;
}

bool FLotAutoPaverUpgradesIndependentTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	OpenRobotShop(G);
	GrantPavers(G, 3);
	TestTrue(TEXT("robot 1 speed"), G.BuyPaverUpgrade(1, ELotPaverUpgrade::Speed));
	TestTrue(TEXT("robot 1 capacity"), G.BuyPaverUpgrade(1, ELotPaverUpgrade::Capacity));
	TestTrue(TEXT("robot 1 capacity again"), G.BuyPaverUpgrade(1, ELotPaverUpgrade::Capacity));

	TestEqual(TEXT("robot 0 speed is untouched"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	TestEqual(TEXT("robot 0 capacity is untouched"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Capacity), 0);
	TestEqual(TEXT("robot 2 speed is untouched"), G.GetPaverUpgradeLevel(2, ELotPaverUpgrade::Speed), 0);
	TestEqual(TEXT("robot 2 capacity is untouched"), G.GetPaverUpgradeLevel(2, ELotPaverUpgrade::Capacity), 0);
	TestEqual(TEXT("robot 1 speed"), G.GetPaverUpgradeLevel(1, ELotPaverUpgrade::Speed), 1);
	TestEqual(TEXT("robot 1 capacity"), G.GetPaverUpgradeLevel(1, ELotPaverUpgrade::Capacity), 2);
	TestEqual(TEXT("robot 0 rate"), G.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[0]);
	TestEqual(TEXT("robot 1 rate"), G.GetAutoPaverRateCellsPerMin(1), LotBalance::AutoPaverRateCellsPerMin[1]);
	TestEqual(TEXT("robot 2 rate"), G.GetAutoPaverRateCellsPerMin(2), LotBalance::AutoPaverRateCellsPerMin[0]);
	TestEqual(TEXT("robot 0 tank size"), G.GetAutoPaverTankCapacity(0), LotBalance::AutoPaverTankLByLevel[0]);
	TestEqual(TEXT("robot 1 tank size"), G.GetAutoPaverTankCapacity(1), LotBalance::AutoPaverTankLByLevel[2]);
	TestEqual(TEXT("robot 2 tank size"), G.GetAutoPaverTankCapacity(2), LotBalance::AutoPaverTankLByLevel[0]);
	TestEqual(TEXT("robot 1 price of the next capacity level: none"), G.GetPaverUpgradePrice(1, ELotPaverUpgrade::Capacity), -1.0);
	TestEqual(TEXT("robot 0 price of capacity is still the first"), G.GetPaverUpgradePrice(0, ELotPaverUpgrade::Capacity), static_cast<double>(LotBalance::PaverUpgradePrice[1][0]));

	// Every tank is filled to its own size.
	G.DebugFillTanks();
	TestEqual(TEXT("robot 0 tank"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[0]);
	TestEqual(TEXT("robot 1 tank"), G.GetAutoPaverTank(1), LotBalance::AutoPaverTankLByLevel[2]);
	TestEqual(TEXT("robot 2 tank"), G.GetAutoPaverTank(2), LotBalance::AutoPaverTankLByLevel[0]);

	// A robot cannot be bought twice, so the upgrade levels are per robot: a robot bought later starts at level 0 while the ones before it keep theirs.
	FLotGame H;
	OpenRobotShop(H);
	TestTrue(TEXT("robot 1"), H.BuyItem(ELotItem::AutoPaver));
	TestTrue(TEXT("its speed"), H.BuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	TestTrue(TEXT("robot 2"), H.BuyItem(ELotItem::AutoPaver2));
	TestEqual(TEXT("the new robot starts at speed 0"), H.GetPaverUpgradeLevel(1, ELotPaverUpgrade::Speed), 0);
	TestEqual(TEXT("the first keeps its speed"), H.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 1);
	TestEqual(TEXT("robot 3 is not owned yet"), H.GetPaverUpgradeLevel(2, ELotPaverUpgrade::Speed), -1);
	return true;
}

bool FLotAutoPaverBigTankLoadsTest::RunTest(const FString& Parameters)
{
	// At a station the robot fills the tank of its own level, and leaves when it is full.
	for (const int32 Level : {0, 1, 2})
	{
		FLotGame G;
		GrantPavers(G, 1);
		G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, Level);
		FullPlant(G);
		G.AssignAutoPaverSector(0, TestSector);
		bool bLoading = false;
		bool bWorking = false;
		for (int32 I = 0; I < 4 * 600 && !bWorking; ++I)
		{
			G.Tick(0.25);
			FullPlant(G);
			bLoading = bLoading || G.GetAutoPaverPhase(0) == ELotPaverPhase::Loading;
			bWorking = G.GetAutoPaverPhase(0) == ELotPaverPhase::Working;
		}
		TestTrue(*FString::Printf(TEXT("capacity %d: it loaded and went to work"), Level), bLoading && bWorking);
		TestEqual(*FString::Printf(TEXT("capacity %d: it left with a full tank of its size"), Level), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[Level]);
	}
	return true;
}

bool FLotAutoPaverDebugUpgradeTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 2);
	TestEqual(TEXT("a robot that is not owned is left alone"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), -1);
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.DebugSetPaverUpgrade(1, ELotPaverUpgrade::Speed, 2);
	TestEqual(TEXT("robot 1 is still not owned"), G.GetPaverUpgradeLevel(1, ELotPaverUpgrade::Speed), -1);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 9);
	TestEqual(TEXT("a level above the maximum is clipped"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), MaxPaverUpgradeLevel);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, -4);
	TestEqual(TEXT("a negative level is clipped"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Count, 1);
	TestEqual(TEXT("an invalid kind does nothing"), G.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 2);
	G.DebugFillTanks();
	TestEqual(TEXT("a big tank"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[2]);
	G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 0);
	TestEqual(TEXT("a smaller tank cuts what it holds"), G.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[0]);

	// A robot in the middle of its work with a big tank: after the capacity is lowered the game is still one the loader accepts.
	FLotGame W;
	W.DebugGrantItem(ELotItem::AutoPaver);
	W.SetAutoPaverAuto(false);
	W.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 2);
	W.DebugSetPlantLevel(5);
	W.DebugSetPlantStock(W.GetPlantStorage());
	W.DebugFillTanks();
	W.AssignAutoPaverSector(0, TestSector);
	for (int32 I = 0; I < 1000 && W.GetAutoPaverStep(0) < 3; ++I)
	{
		W.Tick(0.25);
	}
	TestTrue(TEXT("the robot works"), W.GetAutoPaverStep(0) >= 3);
	W.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 0);
	TestTrue(TEXT("the tank fits the new capacity"), W.GetAutoPaverTank(0) <= LotBalance::AutoPaverTankLByLevel[0]);
	FLotGame Loaded;
	TestTrue(TEXT("the loader accepts the state after the capacity was lowered"), Loaded.LoadFromBytes(W.SaveToBytes()));
	return true;
}

bool FLotAutoPaverCapacitySavesStationTimeTest::RunTest(const FString& Parameters)
{
	// The same robot (speed level 0) works on its own for the same game minutes with the tank of capacity level 0 and of level 2: the big tank
	// means fewer trips to the station, so it spends less time driving there and loading, and it paves more cells.
	constexpr double TestMinutes = 20.0;
	double StationSeconds[2] = {0.0, 0.0};
	int32 Cells[2] = {0, 0};
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FLotGame G;
		GrantPavers(G, 1);
		G.SetAutoPaverAuto(true);
		G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, Run == 0 ? 0 : MaxPaverUpgradeLevel);
		FullPlant(G);
		G.DebugFillTanks();
		for (double T = 0.0; T < TestMinutes * 60.0; T += 0.25)
		{
			G.Tick(0.25);
			FullPlant(G);
			const ELotPaverPhase Phase = G.GetAutoPaverPhase(0);
			StationSeconds[Run] += (Phase == ELotPaverPhase::ToStation || Phase == ELotPaverPhase::Loading) ? 0.25 : 0.0;
		}
		Cells[Run] = G.GetCompletedCells();
	}
	AddInfo(FString::Printf(TEXT("%.0f min: capacity 0: %.0f s at the station, %d cells; capacity 2: %.0f s, %d cells"), TestMinutes, StationSeconds[0], Cells[0], StationSeconds[1], Cells[1]));
	TestTrue(TEXT("the small tank makes trips to the station"), StationSeconds[0] > 0.0);
	TestTrue(TEXT("the big tank spends clearly less time at the station"), StationSeconds[1] < StationSeconds[0] * 0.75);
	TestTrue(TEXT("and paves more cells"), Cells[1] > Cells[0]);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
