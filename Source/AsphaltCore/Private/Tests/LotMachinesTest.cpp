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
DIA_TEST(FLotCarNothingWithoutCarTest, "DoItAsphalt.Lot.Car.NothingWithoutCar")
DIA_TEST(FLotAutoPaverEmptyTankFetchesTest, "DoItAsphalt.Lot.AutoPaver.EmptyTankFetches")
DIA_TEST(FLotAutoPaverRunsDryAndReturnsTest, "DoItAsphalt.Lot.AutoPaver.RunsDryAndReturns")
DIA_TEST(FLotAutoPaverNearestStationTest, "DoItAsphalt.Lot.AutoPaver.NearestStation")
DIA_TEST(FLotCarWidthTest, "DoItAsphalt.Lot.Car.Width")
DIA_TEST(FLotCarDrawsFromTankTest, "DoItAsphalt.Lot.Car.DrawsFromTank")
DIA_TEST(FLotCarLoadsAtStationTest, "DoItAsphalt.Lot.Car.LoadsAtStation")
DIA_TEST(FLotCarSpawnAndLevelTest, "DoItAsphalt.Lot.Car.SpawnAndLevel")
DIA_TEST(FLotMachinesLitresConservedTest, "DoItAsphalt.Lot.Machines.LitresConserved")

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

	/** Auto paver bought at the given level (0..2), plant at level 5 with a full stock. */
	void RichPaver(FLotGame& G, int32 Level)
	{
		G.DebugGrantItem(ELotItem::AutoPaver);
		if (Level >= 1)
		{
			G.DebugGrantItem(ELotItem::AutoPaverFast);
		}
		if (Level >= 2)
		{
			G.DebugGrantItem(ELotItem::AutoPaverFaster);
		}
		FullPlant(G);
		G.DebugFillTanks();
	}

	/** Sends the auto paver to the sector and ticks until it has arrived. */
	void SendAndArrive(FLotGame& G, int32 Sector)
	{
		G.AssignAutoPaverSector(Sector);
		for (int32 I = 0; I < 1000 && G.IsAutoPaverTravelling(); ++I)
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

bool FLotAutoPaverIdleWithoutSectorTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	const FVector2D Where = G.GetAutoPaverPos();
	const FLotEvents Events = G.Tick(600.0);
	TestEqual(TEXT("waiting"), G.GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no cells"), G.GetCompletedCells(), 0);
	TestEqual(TEXT("no litres"), Events.LitresPoured, 0.0);
	TestEqual(TEXT("stays put"), G.GetAutoPaverPos(), Where);
	TestFalse(TEXT("not travelling"), G.IsAutoPaverTravelling());
	return true;
}

bool FLotAutoPaverRejectsInvalidTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestFalse(TEXT("without the auto paver"), G.AssignAutoPaverSector(0));
	G.DebugGrantItem(ELotItem::AutoPaver);
	TestFalse(TEXT("sector -1"), G.AssignAutoPaverSector(-1));
	TestFalse(TEXT("sector 75"), G.AssignAutoPaverSector(AsphaltLot::NumSectors));
	TestEqual(TEXT("still waiting"), G.GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("sector 0"), G.AssignAutoPaverSector(0));
	TestTrue(TEXT("sector 74"), G.AssignAutoPaverSector(AsphaltLot::NumSectors - 1));
	TestEqual(TEXT("assigned"), G.GetAutoPaverSector(), AsphaltLot::NumSectors - 1);
	TestEqual(TEXT("at step 0"), G.GetAutoPaverStep(), 0);
	TestTrue(TEXT("travelling"), G.IsAutoPaverTravelling());
	return true;
}

bool FLotAutoPaverSpawnTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("level without"), G.GetAutoPaverLevel(), -1);
	TestEqual(TEXT("rate without"), G.GetAutoPaverRateCellsPerMin(), 0.0);
	G.DebugGrantItem(ELotItem::AutoPaverFast);
	TestEqual(TEXT("upgrade alone is no auto paver"), G.GetAutoPaverLevel(), -1);
	G.DebugGrantItem(ELotItem::AutoPaver);
	TestEqual(TEXT("level 1 with the fast upgrade"), G.GetAutoPaverLevel(), 1);
	TestEqual(TEXT("spawn"), G.GetAutoPaverPos(), LotLayout::ItemSpawn(ELotItem::AutoPaver));
	G.DebugGrantItem(ELotItem::AutoPaverFaster);
	TestEqual(TEXT("level 2"), G.GetAutoPaverLevel(), 2);
	TestEqual(TEXT("rate 2"), G.GetAutoPaverRateCellsPerMin(), LotBalance::AutoPaverRateCellsPerMin[2]);

	FLotGame B;
	B.DebugGive(1000000.0);
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		B.DebugCompleteCell(AsphaltLot::EntryCell(static_cast<int32>(ELotUnlock::AutoPaver), I));
	}
	B.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::AutoPaver)]);
	TestTrue(TEXT("bought"), B.BuyItem(ELotItem::AutoPaver));
	TestEqual(TEXT("level 0"), B.GetAutoPaverLevel(), 0);
	TestEqual(TEXT("rate 0"), B.GetAutoPaverRateCellsPerMin(), LotBalance::AutoPaverRateCellsPerMin[0]);
	TestEqual(TEXT("bought spawn"), B.GetAutoPaverPos(), LotLayout::ItemSpawn(ELotItem::AutoPaver));

	// A second grant of an owned machine does not move it again.
	FLotGame M;
	M.DebugGrantItem(ELotItem::AutoPaver);
	M.AssignAutoPaverSector(TestSector);
	M.Tick(5.0);
	const FVector2D Moved = M.GetAutoPaverPos();
	TestNotEqual(TEXT("it moved"), Moved, LotLayout::ItemSpawn(ELotItem::AutoPaver));
	M.DebugGrantItem(ELotItem::AutoPaver);
	TestEqual(TEXT("not moved back"), M.GetAutoPaverPos(), Moved);
	return true;
}

bool FLotAutoPaverTravelTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 0);
	G.AssignAutoPaverSector(TestSector);
	const FVector2D Start = G.GetAutoPaverPos();
	const FVector2D Target = AsphaltLot::CellCentre(AsphaltLot::SectorCell(TestSector, 0));
	G.Tick(1.0);
	TestNearlyEqual(TEXT("one second of travel"), FVector2D::Distance(Start, G.GetAutoPaverPos()), LotBalance::AutoPaverSpeedCmS, 1e-3);
	const double Along = FVector2D::DotProduct((G.GetAutoPaverPos() - Start).GetSafeNormal(), (Target - Start).GetSafeNormal());
	TestNearlyEqual(TEXT("in a straight line"), Along, 1.0, 1e-9);
	TestEqual(TEXT("no work while travelling"), G.GetCompletedCells(), 0);
	const double Expected = FVector2D::Distance(Start, Target) / LotBalance::AutoPaverSpeedCmS;
	G.Tick(Expected - 1.0 - 0.6);
	TestTrue(TEXT("still on the way"), G.IsAutoPaverTravelling());
	G.Tick(1.0);
	TestFalse(TEXT("arrived"), G.IsAutoPaverTravelling());
	TestEqual(TEXT("at the first cell"), G.GetAutoPaverPos(), Target);
	TestEqual(TEXT("sector kept"), G.GetAutoPaverSector(), TestSector);
	return true;
}

bool FLotAutoPaverRatePerMinuteTest::RunTest(const FString& Parameters)
{
	const double Expected[3] = {120.0, 170.0, 220.0};
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
	G.AssignAutoPaverSector(TestSector);
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
	G.AssignAutoPaverSector(TestSector);
	for (int32 I = 0; I < 300 * 4; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
	}
	TestEqual(TEXT("whole sector done"), CompleteCellsOfSector(G, TestSector), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("waiting again"), G.GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("step reset"), G.GetAutoPaverStep(), 0);
	const FVector2D Where = G.GetAutoPaverPos();
	const FLotEvents Later = G.Tick(60.0);
	TestEqual(TEXT("nothing more"), Later.LitresPoured, 0.0);
	TestEqual(TEXT("no more cells"), G.GetCompletedCells(), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("stays in the last cell"), G.GetAutoPaverPos(), Where);

	// A finished sector can be assigned again: it arrives and is done at once.
	TestTrue(TEXT("assign a finished sector"), G.AssignAutoPaverSector(TestSector));
	SendAndArrive(G, TestSector);
	G.Tick(0.25);
	TestEqual(TEXT("done again at once"), G.GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
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
	TestEqual(TEXT("waiting"), G.GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
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
	TestTrue(TEXT("reassign"), G.AssignAutoPaverSector(TestSector + 1));
	TestEqual(TEXT("new sector"), G.GetAutoPaverSector(), TestSector + 1);
	TestEqual(TEXT("restarts at step 0"), G.GetAutoPaverStep(), 0);
	TestTrue(TEXT("travelling"), G.IsAutoPaverTravelling());
	G.Tick(0.25);
	TestEqual(TEXT("old sector left as it is"), CompleteCellsOfSector(G, TestSector), Done);
	return true;
}

bool FLotAutoPaverEmptyTankFetchesTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	FullPlant(G);
	TestEqual(TEXT("the tank starts empty"), G.GetAutoPaverTank(), 0.0);
	TestTrue(TEXT("assign"), G.AssignAutoPaverSector(TestSector));
	TestTrue(TEXT("first to the station"), G.GetAutoPaverPhase() == ELotPaverPhase::ToStation);
	TestTrue(TEXT("travelling"), G.IsAutoPaverTravelling());

	bool bLoading = false;
	bool bLoaded = false;
	bool bWorking = false;
	double Previous = 0.0;
	for (int32 I = 0; I < 400 * 4 && !bWorking; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
		bLoading = bLoading || G.GetAutoPaverPhase() == ELotPaverPhase::Loading;
		bLoaded = bLoaded || (bLoading && G.GetAutoPaverPhase() == ELotPaverPhase::ToSector);
		bWorking = G.GetAutoPaverPhase() == ELotPaverPhase::Working;
		TestTrue(TEXT("the tank only grows before the work starts"), G.GetAutoPaverTank() >= Previous);
		Previous = G.GetAutoPaverTank();
		TestEqual(TEXT("nothing is laid before it works"), G.GetCompletedCells(), 0);
	}
	TestTrue(TEXT("it loaded at the station"), bLoading && bLoaded);
	TestTrue(TEXT("and reached the sector"), bWorking);
	TestEqual(TEXT("with a full tank"), G.GetAutoPaverTank(), LotBalance::AutoPaverTankL);
	TestEqual(TEXT("at the first cell"), G.GetAutoPaverPos(), AsphaltLot::CellCentre(AsphaltLot::SectorCell(TestSector, 0)));
	return true;
}

bool FLotAutoPaverRunsDryAndReturnsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	RichPaver(G, 2);
	G.AssignAutoPaverSector(TestSector);
	bool bWentBack = false;
	bool bCameBack = false;
	int32 StepAtDry = -1;
	for (int32 I = 0; I < 900 * 4 && G.GetAutoPaverSector() != INDEX_NONE; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
		if (G.GetAutoPaverPhase() == ELotPaverPhase::ToStation && !bWentBack)
		{
			bWentBack = true;
			StepAtDry = G.GetAutoPaverStep();
			TestTrue(TEXT("it ran dry"), G.GetAutoPaverTank() <= LotBalance::AutoPaverTankEmptyL);
		}
		if (bWentBack && G.GetAutoPaverPhase() == ELotPaverPhase::Working)
		{
			bCameBack = true;
			TestEqual(TEXT("it carries on where it stopped"), G.GetAutoPaverStep(), StepAtDry);
			TestTrue(TEXT("with a full tank"), G.GetAutoPaverTank() > LotBalance::AutoPaverTankL - 1.0);
			bWentBack = false;
		}
	}
	// 4000 l are 400 cells; a 2000 l tank is not enough for them.
	TestTrue(TEXT("it had to fetch asphalt"), bCameBack);
	TestEqual(TEXT("and finished the whole sector"), CompleteCellsOfSector(G, TestSector), AsphaltLot::CellsPerSector);
	TestEqual(TEXT("then waits"), G.GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
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
	TestEqual(TEXT("the fixed station is the nearest to the start"), G.FindNearestStation(G.GetAutoPaverPos()), 0);
	TestEqual(TEXT("the portable station is the nearest to the far sector"), G.FindNearestStation(FarCell), 1);
	G.AssignAutoPaverSector(FarSector);
	for (int32 I = 0; I < 900 * 4 && G.GetAutoPaverPhase() != ELotPaverPhase::ToStation; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
	}
	TestTrue(TEXT("ran dry"), G.GetAutoPaverPhase() == ELotPaverPhase::ToStation);
	for (int32 I = 0; I < 600 * 4 && G.GetAutoPaverPhase() == ELotPaverPhase::ToStation; ++I)
	{
		G.Tick(0.25);
		FullPlant(G);
	}
	TestTrue(TEXT("loading"), G.GetAutoPaverPhase() == ELotPaverPhase::Loading);
	TestEqual(TEXT("at the portable station, not the fixed one"), G.GetAutoPaverPos(), G.GetStationPos(1));
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

	FLotGame W;
	W.DebugGrantItem(ELotItem::Car);
	W.DebugGrantItem(ELotItem::CarWideBar);
	FullPlant(W);
	W.DebugFillTanks();
	TestEqual(TEXT("wide bar"), W.GetCarLevel(), 1);
	W.PourFromCar(A, B);
	TestEqual(TEXT("still three rows complete"), W.GetCompletedCells(), 30);
	for (int32 Col = 40; Col < 50; ++Col)
	{
		TestEqual(TEXT("25 cm above"), W.GetGrid().GetCellDone(MiddleCell(Col, 38)), 100);
		TestEqual(TEXT("25 cm below"), W.GetGrid().GetCellDone(MiddleCell(Col, 42)), 100);
		TestFalse(TEXT("not complete"), W.GetGrid().IsCellComplete(MiddleCell(Col, 38)));
	}
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
	G.DebugGrantItem(ELotItem::CarWideBar);
	TestEqual(TEXT("upgrade alone is no car"), G.GetCarLevel(), -1);
	G.DebugGrantItem(ELotItem::Car);
	TestEqual(TEXT("car spawn"), G.World().CarPos, LotLayout::ItemSpawn(ELotItem::Car));
	TestEqual(TEXT("wide bar"), G.GetCarLevel(), 1);
	G.World().CarPos = FVector2D(5.0, 6.0);
	G.DebugGrantItem(ELotItem::Car);
	TestEqual(TEXT("not moved again"), G.World().CarPos, FVector2D(5.0, 6.0));
	return true;
}

bool FLotMachinesLitresConservedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::AutoPaver);
	G.DebugGrantItem(ELotItem::Car);
	G.AssignAutoPaverSector(TestSector);
	const auto Total = [&G]() { return G.GetPlantStock() + G.GetTotalLitres() + G.GetAutoPaverTank() + G.GetCarTank(); };
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
	TestTrue(TEXT("the auto paver loaded"), G.GetAutoPaverTank() > 0.0);

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

#endif // WITH_DEV_AUTOMATION_TESTS
