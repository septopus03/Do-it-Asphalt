// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotFluid.h"
#include "LotGame.h"
#include "LotGrid.h"
#include "LotMap.h"

DIA_TEST(FLotFluidOffByDefaultTest, "DoItAsphalt.Lot.Fluid.OffByDefault")
DIA_TEST(FLotFluidPaysAtPourTest, "DoItAsphalt.Lot.Fluid.PaysAtPour")
DIA_TEST(FLotFluidSpreadsTest, "DoItAsphalt.Lot.Fluid.Spreads")
DIA_TEST(FLotFluidVolumeNeverGrowsTest, "DoItAsphalt.Lot.Fluid.VolumeNeverGrows")
DIA_TEST(FLotFluidCoolsAndSetsTest, "DoItAsphalt.Lot.Fluid.CoolsAndSets")
DIA_TEST(FLotFluidColdRunsSlowerTest, "DoItAsphalt.Lot.Fluid.ColdRunsSlower")
DIA_TEST(FLotFluidWallsTest, "DoItAsphalt.Lot.Fluid.Walls")
DIA_TEST(FLotFluidTileBordersTest, "DoItAsphalt.Lot.Fluid.TileBorders")
DIA_TEST(FLotFluidDeterministicTest, "DoItAsphalt.Lot.Fluid.Deterministic")
DIA_TEST(FLotFluidRipplesTest, "DoItAsphalt.Lot.Fluid.Ripples")
DIA_TEST(FLotFluidCopyTest, "DoItAsphalt.Lot.Fluid.Copy")
DIA_TEST(FLotFluidGameBooksTest, "DoItAsphalt.Lot.Fluid.GameBooks")
DIA_TEST(FLotFluidGameSettleTest, "DoItAsphalt.Lot.Fluid.GameSettle")
DIA_TEST(FLotFluidPerformanceTest, "DoItAsphalt.Lot.Fluid.Performance")

namespace
{
	/** The strip that, with the stamp padding, covers exactly the given cell. */
	void CellStrip(int32 Cell, FVector2D& OutA, FVector2D& OutB)
	{
		const FVector2D Centre = AsphaltLot::CellCentre(Cell);
		OutA = Centre + FVector2D(-45.0, 0.0);
		OutB = Centre + FVector2D(45.0, 0.0);
	}

	/** A cell well inside the car park (col 50, row 50). */
	const int32 MidCell = 50 * AsphaltLot::Side + 50;

	int32 CountTexels(const FLotGrid& Grid, ELotTexel State = ELotTexel::Done)
	{
		int32 Count = 0;
		for (uint8 Texel : Grid.GetTexels())
		{
			Count += Texel == static_cast<uint8>(State) ? 1 : 0;
		}
		return Count;
	}

	int32 CountAll(const FLotGrid& Grid)
	{
		return CountTexels(Grid);
	}

	FLotPour PourOnMidCell(FLotGrid& Grid, double Litres, int32 Cell = MidCell)
	{
		FVector2D A, B;
		CellStrip(Cell, A, B);
		return Grid.Stamp(A, B, 100.0, Litres);
	}

	/** Steps the grid until nothing runs (or the seconds pass); returns the steps taken. */
	int32 RunUntilSettled(FLotGrid& Grid, double MaxSeconds)
	{
		int32 Steps = 0;
		const int32 MaxSteps = FMath::CeilToInt(MaxSeconds / LotBalance::FluidStepSeconds);
		while (Grid.IsFluidActive() && Steps < MaxSteps)
		{
			Grid.StepFluid();
			++Steps;
		}
		return Steps;
	}

	TArray<uint8> TexelsOf(const FLotGrid& Grid)
	{
		return TArray<uint8>(Grid.GetTexels().GetData(), Grid.GetTexels().Num());
	}
}

bool FLotFluidOffByDefaultTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	TestFalse(TEXT("off"), G.IsFluidEnabled());
	const FLotPour Pour = PourOnMidCell(G, 10.0);
	TestEqual(TEXT("the texels change at once"), CountTexels(G), 400);
	TestEqual(TEXT("and are paid (the cell completes at 95 %, the rest is free)"), Pour.TexelsChanged, 380);
	TestFalse(TEXT("nothing runs"), G.IsFluidActive());
	return true;
}

bool FLotFluidPaysAtPourTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	G.SetFluidEnabled(true);
	const FLotPour Pour = PourOnMidCell(G, 5.0);
	TestNearlyEqual(TEXT("the litres are paid when poured"), Pour.LitresUsed, 5.0, 1e-9);
	TestEqual(TEXT("no ground yet"), CountAll(G), 0);
	TestTrue(TEXT("the asphalt runs"), G.IsFluidActive());
	TestNearlyEqual(TEXT("the heaps hold the paid volume"), G.GetFluid().GetTotalUnits(), 200.0, 1e-3);
	TestEqual(TEXT("no cell completed by the pour"), Pour.CompletedCells.Num(), 0);
	return true;
}

bool FLotFluidSpreadsTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	G.SetFluidEnabled(true);
	PourOnMidCell(G, 10.0);
	const int32 Heaps = G.GetFluid().GetNumTiles();
	TestTrue(TEXT("at least one tile"), Heaps >= 1);
	const int32 Steps = RunUntilSettled(G, 60.0);
	const int32 Covered = CountAll(G);
	AddInfo(FString::Printf(TEXT("spread: covered %d of 400 paid texels in %d steps"), Covered, Steps));
	TestFalse(TEXT("settles within the limit"), G.IsFluidActive());
	TestTrue(TEXT("it covered most of what was paid for"), Covered >= 300);
	TestTrue(TEXT("and no more than it can hold"), Covered <= 600);
	// The asphalt ran out of the 20 x 20 texel cell it was poured on.
	const FIntPoint Coord = AsphaltLot::CellCoord(MidCell);
	int32 Outside = 0;
	for (int32 Y = (Coord.Y - 1) * 20; Y < (Coord.Y + 2) * 20; ++Y)
	{
		for (int32 X = (Coord.X - 1) * 20; X < (Coord.X + 2) * 20; ++X)
		{
			const bool bInside = X >= Coord.X * 20 && X < (Coord.X + 1) * 20 && Y >= Coord.Y * 20 && Y < (Coord.Y + 1) * 20;
			Outside += (!bInside && G.GetTexel(X, Y) != ELotTexel::Free) ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("spread: %d texels beyond the cell"), Outside));
	TestTrue(TEXT("it ran past the cell it was poured on"), Outside > 0);
	return true;
}

bool FLotFluidVolumeNeverGrowsTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	G.SetFluidEnabled(true);
	PourOnMidCell(G, 10.0);
	double Last = G.GetFluid().GetTotalUnits();
	const double Start = Last;
	bool bGrew = false;
	bool bNegative = false;
	// The ground a texel sets as is paid for by the heap, so the fluid alone may only lose volume (cooling and the thin edge).
	for (int32 Step = 0; Step < 600 && G.IsFluidActive(); ++Step)
	{
		G.StepFluid();
		const double Now = G.GetFluid().GetTotalUnits();
		bGrew = bGrew || Now > Last + 1e-2;
		bNegative = bNegative || Now < 0.0;
		Last = Now;
	}
	TestFalse(TEXT("the volume never grows"), bGrew);
	TestFalse(TEXT("and never goes below zero"), bNegative);
	TestTrue(TEXT("something was lost to cooling"), Last < Start);
	return true;
}

bool FLotFluidCoolsAndSetsTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	G.SetFluidEnabled(true);
	PourOnMidCell(G, 10.0);
	const int32 Steps = RunUntilSettled(G, LotBalance::FluidSettleSeconds);
	const double Seconds = Steps * LotBalance::FluidStepSeconds;
	AddInfo(FString::Printf(TEXT("cools: settled after %.1f s"), Seconds));
	TestFalse(TEXT("nothing runs any more"), G.IsFluidActive());
	TestEqual(TEXT("no tile is kept for nothing"), G.GetFluid().GetNumTiles(), 0);
	// A single hot texel needs ln(1 / FreezeHeat) cooling times to freeze; the settle limit covers it.
	TestTrue(TEXT("it took its time"), Seconds > 1.0);
	TestTrue(TEXT("but less than the settle limit"), Seconds < LotBalance::FluidSettleSeconds);
	TestTrue(TEXT("the ground is set"), CountAll(G) > 0);
	return true;
}

bool FLotFluidColdRunsSlowerTest::RunTest(const FString& Parameters)
{
	// All asphalt poured at once cools together: after FluidCoolSeconds its heat is a third (1 / e), and a cold puddle barely moves.
	FLotFluid F;
	TArray<uint8> Ground;
	Ground.SetNumZeroed(FLotFluid::GridSide * FLotFluid::GridSide);
	F.Add(1000, 1000, 400.f);
	const auto NoSet = [](int32, int32) {};
	TestNearlyEqual(TEXT("fresh asphalt is at full heat"), F.GetHeat(1000, 1000), 1.f, 1e-4f);
	const int32 SecondsSteps = FMath::RoundToInt(LotBalance::FluidCoolSeconds / LotBalance::FluidStepSeconds);
	for (int32 Step = 0; Step < SecondsSteps; ++Step)
	{
		F.Step(Ground, NoSet);
	}
	float HeatMax = 0.f;
	int32 Wet = 0;
	for (int32 Dy = -60; Dy <= 60; ++Dy)
	{
		for (int32 Dx = -60; Dx <= 60; ++Dx)
		{
			HeatMax = FMath::Max(HeatMax, F.GetHeat(1000 + Dx, 1000 + Dy));
			Wet += F.GetUnits(1000 + Dx, 1000 + Dy) > 0.f ? 1 : 0;
		}
	}
	TestNearlyEqual(TEXT("after the cooling time the heat is a third"), HeatMax, FMath::Exp(-1.f), 0.02f);
	TestTrue(TEXT("the heap ran out over many texels"), Wet >= 100);
	return true;
}

bool FLotFluidWallsTest::RunTest(const FString& Parameters)
{
	FLotFluid F;
	TArray<uint8> Ground;
	Ground.SetNumZeroed(FLotFluid::GridSide * FLotFluid::GridSide);
	// A heap next to the shopping-centre quadrant (X >= 2000 and Y >= 2000 is not car park) and one at the grid edge.
	F.Add(1999, 2500, 200.f);
	F.Add(2500, 1999, 200.f);
	F.Add(0, 3999, 200.f);
	bool bBadCallback = false;
	for (int32 Step = 0; Step < 400 && F.IsActive(); ++Step)
	{
		F.Step(Ground, [&](int32 X, int32 Y)
		{
			bBadCallback = bBadCallback || !FLotFluid::IsPourable(X, Y);
		});
	}
	TestFalse(TEXT("nothing was set in the quadrant or outside"), bBadCallback);
	TestEqual(TEXT("the quadrant holds no asphalt"), F.GetUnits(2000, 2500) + F.GetUnits(2500, 2000) + F.GetUnits(3000, 3000), 0.f);
	TestFalse(TEXT("add refuses the quadrant"), F.Add(3000, 3000, 1.f));
	TestFalse(TEXT("add refuses outside"), F.Add(-1, 5, 1.f));
	TestFalse(TEXT("add refuses nothing"), F.Add(10, 10, 0.f));
	TestFalse(TEXT("add refuses a non-number"), F.Add(10, 10, std::numeric_limits<float>::quiet_NaN()));
	return true;
}

bool FLotFluidTileBordersTest::RunTest(const FString& Parameters)
{
	FLotFluid F;
	TArray<uint8> Ground;
	Ground.SetNumZeroed(FLotFluid::GridSide * FLotFluid::GridSide);
	// On the corner of four tiles, so the asphalt has to cross tile borders in both directions.
	F.Add(31, 31, 40.f);
	F.Add(32, 32, 40.f);
	const double Start = F.GetTotalUnits();
	TArray<TPair<int32, int32>> Set;
	for (int32 Step = 0; Step < 60; ++Step)
	{
		F.Step(Ground, [&](int32 X, int32 Y)
		{
			Set.Add({X, Y});
			Ground[Y * FLotFluid::GridSide + X] = 1;
		});
	}
	TestTrue(TEXT("it crossed into the four tiles around the corner"), F.GetNumTiles() >= 4 || F.GetNumTiles() == 0);
	int32 Left = 0, Right = 0, Below = 0, Above = 0;
	for (const TPair<int32, int32>& P : Set)
	{
		Left += P.Key < 31 ? 1 : 0;
		Right += P.Key > 32 ? 1 : 0;
		Below += P.Value < 31 ? 1 : 0;
		Above += P.Value > 32 ? 1 : 0;
	}
	TestTrue(TEXT("it spread in all four directions"), Left > 0 && Right > 0 && Below > 0 && Above > 0);
	// The same on both sides: the heap is symmetric about the corner, so the spread must not favour a direction.
	TestTrue(TEXT("and symmetrically (left/right)"), FMath::Abs(Left - Right) <= 2);
	TestTrue(TEXT("and symmetrically (below/above)"), FMath::Abs(Below - Above) <= 2);
	TestTrue(TEXT("volume did not grow"), F.GetTotalUnits() <= Start + 1e-3);
	return true;
}

bool FLotFluidDeterministicTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Results[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FLotGrid G;
		G.SetFluidEnabled(true);
		PourOnMidCell(G, 10.0);
		for (int32 Step = 0; Step < 40; ++Step)
		{
			G.StepFluid();
			if (Step == 10)
			{
				PourOnMidCell(G, 3.0, MidCell + 1);
			}
		}
		RunUntilSettled(G, 60.0);
		Results[Run] = TexelsOf(G);
	}
	TestTrue(TEXT("the same pours give the same ground"), Results[0] == Results[1]);
	return true;
}

bool FLotFluidRipplesTest::RunTest(const FString& Parameters)
{
	// A heap on flat ground sloshes: the asphalt does not just creep to a smooth puddle, some texel beyond the front
	// rises above the film after the front has passed it (the overshoot that makes it livelier than a real fluid).
	FLotFluid F;
	TArray<uint8> Ground;
	Ground.SetNumZeroed(FLotFluid::GridSide * FLotFluid::GridSide);
	F.Add(500, 500, 60.f);
	float PeakAtRing = 0.f;
	bool bFell = false;
	float Previous = 0.f;
	for (int32 Step = 0; Step < 90; ++Step)
	{
		F.Step(Ground, [](int32, int32) {});
		const float Here = F.GetUnits(504, 500);
		PeakAtRing = FMath::Max(PeakAtRing, Here);
		bFell = bFell || (Step > 3 && Here < Previous - 1e-3f);
		Previous = Here;
	}
	AddInfo(FString::Printf(TEXT("ripples: peak at 4 texels from the heap %.2f"), PeakAtRing));
	TestTrue(TEXT("the asphalt reached a texel away from the heap"), PeakAtRing > 0.1f);
	TestTrue(TEXT("and it rose and fell there"), bFell);
	return true;
}

bool FLotFluidCopyTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	G.SetFluidEnabled(true);
	PourOnMidCell(G, 10.0);
	for (int32 Step = 0; Step < 10; ++Step)
	{
		G.StepFluid();
	}
	FLotGrid Copy = G;
	TestTrue(TEXT("the copy runs too"), Copy.IsFluidActive());
	TestNearlyEqual(TEXT("with the same volume"), Copy.GetFluid().GetTotalUnits(), G.GetFluid().GetTotalUnits(), 1e-6);
	RunUntilSettled(Copy, 60.0);
	TestTrue(TEXT("settling the copy leaves the original running"), G.IsFluidActive());
	RunUntilSettled(G, 60.0);
	TestTrue(TEXT("and both end the same"), TexelsOf(G) == TexelsOf(Copy));
	return true;
}

bool FLotFluidGameBooksTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	Game.SetFluidEnabled(true);
	TestTrue(TEXT("on"), Game.IsFluidEnabled());
	Game.ScoopFromTrough(20.0);
	const FVector2D Aim = AsphaltLot::CellCentre(MidCell);
	const FLotEvents Poured = Game.PourHand(Aim, Aim + FVector2D(150.0, 0.0), 1.0);
	TestTrue(TEXT("litres were poured"), Poured.LitresPoured > 0.0);
	TestNearlyEqual(TEXT("a dollar a litre at once"), Poured.CreditsEarned, Poured.LitresPoured, 1e-9);
	TestEqual(TEXT("no ground yet"), CountAll(Game.GetGrid()), 0);
	TestTrue(TEXT("the asphalt runs"), Game.IsFluidActive());
	double Credits = Game.GetCredits();
	int32 Cells = 0;
	for (int32 Tick = 0; Tick < 60 * 30 && Game.IsFluidActive(); ++Tick)
	{
		const FLotEvents E = Game.Tick(1.0 / 30.0);
		Cells += E.CompletedCells.Num();
		Credits += E.CreditsEarned;
		TestEqual(TEXT("the ground pays nothing but the cells"), E.LitresPoured, 0.0);
	}
	AddInfo(FString::Printf(TEXT("books: poured %.2f L, %d texels of ground, %d cells completed"), Poured.LitresPoured, CountAll(Game.GetGrid()), Cells));
	TestFalse(TEXT("it settles"), Game.IsFluidActive());
	TestTrue(TEXT("it set some ground"), CountAll(Game.GetGrid()) > 0);
	TestNearlyEqual(TEXT("credits are what the events booked"), Game.GetCredits(), Credits, 1e-6);
	TestNearlyEqual(TEXT("litres are counted once"), Game.GetTotalLitres(), Poured.LitresPoured, 1e-9);
	return true;
}

bool FLotFluidGameSettleTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	Game.SetFluidEnabled(true);
	Game.ScoopFromTrough(20.0);
	const FVector2D Aim = AsphaltLot::CellCentre(MidCell);
	Game.PourHand(Aim, Aim + FVector2D(150.0, 0.0), 1.0);
	Game.Tick(0.2);
	TestTrue(TEXT("running"), Game.IsFluidActive());

	// A save is of a copy that has been let to settle; the running game goes on.
	FLotGame Copy = Game;
	Copy.SettleFluid();
	TestFalse(TEXT("the copy has settled"), Copy.IsFluidActive());
	TestTrue(TEXT("the game has not"), Game.IsFluidActive());
	const TArray<uint8> Blob = Copy.SaveToBytes();

	FLotGame Loaded;
	Loaded.SetFluidEnabled(true);
	TestTrue(TEXT("the save loads"), Loaded.LoadFromBytes(Blob));
	TestTrue(TEXT("loading keeps the fluid on"), Loaded.IsFluidEnabled());
	TestFalse(TEXT("and nothing runs after it"), Loaded.IsFluidActive());
	TestTrue(TEXT("the ground is the settled one"), TexelsOf(Loaded.GetGrid()) == TexelsOf(Copy.GetGrid()));
	TestNearlyEqual(TEXT("with the same credits"), Loaded.GetCredits(), Copy.GetCredits(), 1e-9);

	FLotGame Fresh;
	TestTrue(TEXT("a save loads into a game with the fluid off"), Fresh.LoadFromBytes(Blob));
	TestFalse(TEXT("and leaves it off"), Fresh.IsFluidEnabled());
	return true;
}

bool FLotFluidPerformanceTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	G.SetFluidEnabled(true);
	// Ten cells poured next to each other at once: the worst a player does with the biggest tools.
	for (int32 I = 0; I < 10; ++I)
	{
		FVector2D A, B;
		CellStrip(MidCell + I, A, B);
		G.Stamp(A, B, 100.0, 10.0);
	}
	const int32 Tiles = G.GetFluid().GetNumTiles();
	const double Start = FPlatformTime::Seconds();
	int32 Steps = 0;
	while (G.IsFluidActive() && Steps < 60)
	{
		G.StepFluid();
		++Steps;
	}
	const double PerStepMs = (FPlatformTime::Seconds() - Start) * 1000.0 / FMath::Max(Steps, 1);
	AddInfo(FString::Printf(TEXT("performance: %d tiles at the start, %.2f ms per step over %d steps"), Tiles, PerStepMs, Steps));
	TestTrue(TEXT("a step is cheap enough for a frame"), PerStepMs < 12.0);
	return true;
}

#endif
