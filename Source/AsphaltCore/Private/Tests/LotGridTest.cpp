// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGrid.h"
#include "LotMap.h"

DIA_TEST(FLotGridConstantsTest, "DoItAsphalt.Lot.Grid.Constants")
DIA_TEST(FLotStampDoneCompletesTest, "DoItAsphalt.Lot.Stamp.DoneCompletesAt95")
DIA_TEST(FLotStampRawDoesNotCompleteTest, "DoItAsphalt.Lot.Stamp.RawDoesNotComplete")
DIA_TEST(FLotStampOccupiedIsFreeTest, "DoItAsphalt.Lot.Stamp.OccupiedIsFree")
DIA_TEST(FLotStampOffLotIsFreeTest, "DoItAsphalt.Lot.Stamp.OffLotIsFree")
DIA_TEST(FLotStampStopsWhenLitresRunOutTest, "DoItAsphalt.Lot.Stamp.StopsWhenLitresRunOut")
DIA_TEST(FLotStampNeverOverspendsTest, "DoItAsphalt.Lot.Stamp.NeverOverspends")
DIA_TEST(FLotPourDiscNearestFirstTest, "DoItAsphalt.Lot.PourDisc.NearestFirst")
DIA_TEST(FLotDepositFromSideTest, "DoItAsphalt.Lot.Deposit.FromSide")
DIA_TEST(FLotDepositBucketFillsCellTest, "DoItAsphalt.Lot.Deposit.BucketFillsCell")
DIA_TEST(FLotFinishRawBecomesDoneTest, "DoItAsphalt.Lot.Finish.RawBecomesDone")
DIA_TEST(FLotFinishIgnoresFreeTest, "DoItAsphalt.Lot.Finish.IgnoresFree")
DIA_TEST(FLotCompleteRawRemainderTest, "DoItAsphalt.Lot.Complete.RawRemainderJoins")
DIA_TEST(FLotCompleteRawBlocksTest, "DoItAsphalt.Lot.Complete.RawBlocksThreshold")
DIA_TEST(FLotGridCompleteCellTest, "DoItAsphalt.Lot.Grid.CompleteCell")
DIA_TEST(FLotGridDirtyRectsTest, "DoItAsphalt.Lot.Grid.DirtyRects")
DIA_TEST(FLotStampMidCallCompletionTest, "DoItAsphalt.Lot.Stamp.MidCallCompletion")
DIA_TEST(FLotGridNonFiniteInputsTest, "DoItAsphalt.Lot.Grid.NonFiniteInputs")

namespace
{
	constexpr double Tolerance = 1e-9;

	/** The strip that, with the stamp padding, covers exactly cell 0. */
	struct FCellStrip
	{
		FVector2D A;
		FVector2D B;
		double Width = 100.0;
		FCellStrip(int32 Cell = 0)
		{
			const FVector2D Centre = AsphaltLot::CellCentre(Cell);
			A = Centre + FVector2D(-45.0, 0.0);
			B = Centre + FVector2D(45.0, 0.0);
		}
	};

	int32 CountState(const FLotGrid& Grid, ELotTexel State)
	{
		int32 Count = 0;
		for (uint8 Texel : Grid.GetTexels())
		{
			Count += Texel == static_cast<uint8>(State) ? 1 : 0;
		}
		return Count;
	}

	FVector2D TexelCentreOf(int32 X, int32 Y)
	{
		return FVector2D(-AsphaltLot::HalfExtentCm + (X + 0.5) * FLotGrid::TexelCm, -AsphaltLot::HalfExtentCm + (Y + 0.5) * FLotGrid::TexelCm);
	}

	/** Min and max of Metric over the texels of cell 0 that are (not) in the given state. */
	template <typename MetricType>
	void RangeOverCell0(const FLotGrid& Grid, bool bInState, ELotTexel State, MetricType Metric, double& OutMin, double& OutMax)
	{
		OutMin = TNumericLimits<double>::Max();
		OutMax = TNumericLimits<double>::Lowest();
		for (int32 Y = 0; Y < 20; ++Y)
		{
			for (int32 X = 0; X < 20; ++X)
			{
				if ((Grid.GetTexel(X, Y) == State) == bInState)
				{
					const double Value = Metric(TexelCentreOf(X, Y));
					OutMin = FMath::Min(OutMin, Value);
					OutMax = FMath::Max(OutMax, Value);
				}
			}
		}
	}
}

bool FLotGridConstantsTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	TestEqual(TEXT("Size"), FLotGrid::Size, 4000);
	TestEqual(TEXT("TexelCm"), FLotGrid::TexelCm, 5.0);
	TestEqual(TEXT("TexelsPerCell"), FLotGrid::TexelsPerCell, 400);
	TestNearlyEqual(TEXT("LitresPerTexel"), FLotGrid::LitresPerTexel, 0.025, Tolerance);
	TestEqual(TEXT("ThresholdTexels"), FLotGrid::ThresholdTexels, 380);
	TestEqual(TEXT("fresh grid has no completed cells"), G.NumCompleteCells(), 0);
	TestEqual(TEXT("fresh grid is free"), CountState(G, ELotTexel::Free), FLotGrid::Size * FLotGrid::Size);
	TestEqual(TEXT("texel array size"), G.GetTexels().Num(), FLotGrid::Size * FLotGrid::Size);
	TestEqual(TEXT("out of range texel"), (int32)G.GetTexel(-1, 0), (int32)ELotTexel::Free);
	TestEqual(TEXT("out of range texel high"), (int32)G.GetTexel(0, FLotGrid::Size), (int32)ELotTexel::Free);
	return true;
}

bool FLotStampDoneCompletesTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FCellStrip S;
	const FLotPour R = G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Done);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 9.5, Tolerance);
	TestEqual(TEXT("paid texels"), R.TexelsChanged, 380);
	TestTrue(TEXT("completed cells"), R.CompletedCells == TArray<int32>({0}));
	TestEqual(TEXT("done texels"), G.GetCellDone(0), 400);
	TestEqual(TEXT("raw texels"), G.GetCellRaw(0), 0);
	TestTrue(TEXT("complete"), G.IsCellComplete(0));
	TestEqual(TEXT("complete count"), G.NumCompleteCells(), 1);
	TestEqual(TEXT("nothing outside the cell"), CountState(G, ELotTexel::Done), 400);
	TestEqual(TEXT("texel"), (int32)G.GetTexel(19, 19), (int32)ELotTexel::Done);
	return true;
}

bool FLotStampRawDoesNotCompleteTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FCellStrip S;
	const FLotPour R = G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Raw);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 10.0, Tolerance);
	TestEqual(TEXT("no completed cell"), R.CompletedCells.Num(), 0);
	TestEqual(TEXT("raw texels"), G.GetCellRaw(0), 400);
	TestEqual(TEXT("done texels"), G.GetCellDone(0), 0);
	TestFalse(TEXT("not complete"), G.IsCellComplete(0));
	TestEqual(TEXT("raw in array"), CountState(G, ELotTexel::Raw), 400);
	return true;
}

bool FLotStampOccupiedIsFreeTest::RunTest(const FString& Parameters)
{
	{
		FLotGrid G;
		const FCellStrip S;
		G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Raw);
		const FLotPour R = G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Done);
		TestNearlyEqual(TEXT("done over raw: litres"), R.LitresUsed, 0.0, Tolerance);
		TestEqual(TEXT("done over raw: texels"), R.TexelsChanged, 0);
		TestEqual(TEXT("raw stays raw"), G.GetCellRaw(0), 400);
	}
	{
		FLotGrid G;
		const FCellStrip S(5);
		G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Done);
		const FLotPour R = G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Raw);
		TestNearlyEqual(TEXT("raw over done: litres"), R.LitresUsed, 0.0, Tolerance);
		TestEqual(TEXT("raw over done: texels"), R.TexelsChanged, 0);
		TestEqual(TEXT("done stays done"), G.GetCellDone(5), 400);
		TestEqual(TEXT("free state does nothing"), G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Free).TexelsChanged, 0);
	}
	return true;
}

bool FLotStampOffLotIsFreeTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FLotPour Mall = G.Stamp({4950, 5000}, {5050, 5000}, 100.0, 100.0, ELotTexel::Done);
	TestNearlyEqual(TEXT("mall quadrant: litres"), Mall.LitresUsed, 0.0, Tolerance);
	TestEqual(TEXT("mall quadrant: texels"), Mall.TexelsChanged, 0);
	const FLotPour Outside = G.Stamp({-50, -10500}, {50, -10500}, 100.0, 100.0, ELotTexel::Raw);
	TestNearlyEqual(TEXT("outside the square: litres"), Outside.LitresUsed, 0.0, Tolerance);
	TestEqual(TEXT("outside the square: texels"), Outside.TexelsChanged, 0);
	// Straddling the outline: only the car-park part is paid for.
	const FLotPour Edge = G.Stamp({-50, -10025}, {50, -10025}, 100.0, 1000.0, ELotTexel::Raw);
	TestEqual(TEXT("edge straddle pays only the lot part"), Edge.TexelsChanged, 110);
	TestEqual(TEXT("nothing in the mall"), (int32)G.GetTexel(3000, 3000), (int32)ELotTexel::Free);
	return true;
}

bool FLotStampStopsWhenLitresRunOutTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FCellStrip S;
	const FLotPour R = G.Stamp(S.A, S.B, S.Width, 1.0, ELotTexel::Done);
	TestEqual(TEXT("texels"), R.TexelsChanged, 40);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 1.0, Tolerance);
	TestEqual(TEXT("done texels"), G.GetCellDone(0), 40);

	const auto Projection = [&](const FVector2D& P) { return P.X - S.A.X; };
	double PouredMin, PouredMax, FreeMin, FreeMax;
	RangeOverCell0(G, true, ELotTexel::Done, Projection, PouredMin, PouredMax);
	RangeOverCell0(G, false, ELotTexel::Done, Projection, FreeMin, FreeMax);
	TestTrue(TEXT("poured part comes first along the axis"), PouredMax <= FreeMin);
	return true;
}

bool FLotStampNeverOverspendsTest::RunTest(const FString& Parameters)
{
	const FCellStrip S;
	{
		FLotGrid G;
		const FLotPour R = G.Stamp(S.A, S.B, S.Width, 0.03, ELotTexel::Done);
		TestEqual(TEXT("one texel"), R.TexelsChanged, 1);
		TestNearlyEqual(TEXT("litres"), R.LitresUsed, 0.025, Tolerance);
	}
	{
		FLotGrid G;
		const FLotPour R = G.Stamp(S.A, S.B, S.Width, 0.02, ELotTexel::Done);
		TestEqual(TEXT("no texel"), R.TexelsChanged, 0);
		TestNearlyEqual(TEXT("litres"), R.LitresUsed, 0.0, Tolerance);
	}
	return true;
}

bool FLotPourDiscNearestFirstTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	const FVector2D From = C0 + FVector2D(-100.0, 0.0);
	const FLotPour First = G.PourDisc(C0, 15.0, 0.25, From);
	TestEqual(TEXT("first call texels"), First.TexelsChanged, 10);
	TestNearlyEqual(TEXT("first call litres"), First.LitresUsed, 0.25, Tolerance);

	const auto Distance = [&](const FVector2D& P) { return FVector2D::Distance(P, From); };
	const auto InDisc = [&](int32 X, int32 Y) { return FVector2D::Distance(TexelCentreOf(X, Y), C0) <= 15.0; };
	double PouredMax = TNumericLimits<double>::Lowest();
	double RestMin = TNumericLimits<double>::Max();
	int32 DiscTexels = 0;
	for (int32 Y = 0; Y < 20; ++Y)
	{
		for (int32 X = 0; X < 20; ++X)
		{
			if (!InDisc(X, Y))
			{
				TestEqual(TEXT("outside the disc stays free"), (int32)G.GetTexel(X, Y), (int32)ELotTexel::Free);
				continue;
			}
			++DiscTexels;
			const double D = Distance(TexelCentreOf(X, Y));
			if (G.GetTexel(X, Y) == ELotTexel::Done)
			{
				PouredMax = FMath::Max(PouredMax, D);
			}
			else
			{
				RestMin = FMath::Min(RestMin, D);
			}
		}
	}
	TestTrue(TEXT("poured texels are the nearest"), PouredMax <= RestMin);

	const FLotPour Second = G.PourDisc(C0, 15.0, 10.0, From);
	TestEqual(TEXT("second call fills the rest of the disc"), Second.TexelsChanged, DiscTexels - 10);
	const FLotPour Third = G.PourDisc(C0, 15.0, 10.0, From);
	TestEqual(TEXT("third call"), Third.TexelsChanged, 0);
	TestNearlyEqual(TEXT("third call litres"), Third.LitresUsed, 0.0, Tolerance);
	return true;
}

bool FLotDepositFromSideTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	const FVector2D From = C0 + FVector2D(0.0, -150.0);
	const FLotPour R = G.DepositIntoCell(0, 2.5, From, ELotTexel::Done);
	TestEqual(TEXT("texels"), R.TexelsChanged, 100);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 2.5, Tolerance);
	TestEqual(TEXT("done"), G.GetCellDone(0), 100);

	const auto Distance = [&](const FVector2D& P) { return FVector2D::Distance(P, From); };
	double PouredMin, PouredMax, FreeMin, FreeMax;
	RangeOverCell0(G, true, ELotTexel::Done, Distance, PouredMin, PouredMax);
	RangeOverCell0(G, false, ELotTexel::Done, Distance, FreeMin, FreeMax);
	TestTrue(TEXT("poured texels are the nearest"), PouredMax <= FreeMin);
	return true;
}

bool FLotDepositBucketFillsCellTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	const FLotPour R = G.DepositIntoCell(0, 10.0, C0 + FVector2D(0.0, -150.0), ELotTexel::Done);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 9.5, Tolerance);
	TestTrue(TEXT("completed"), R.CompletedCells == TArray<int32>({0}));
	TestEqual(TEXT("done"), G.GetCellDone(0), 400);

	TestEqual(TEXT("negative cell"), G.DepositIntoCell(-1, 10.0, C0, ELotTexel::Done).TexelsChanged, 0);
	TestEqual(TEXT("cell past the end"), G.DepositIntoCell(30000, 10.0, C0, ELotTexel::Done).TexelsChanged, 0);
	TestEqual(TEXT("complete cell"), G.DepositIntoCell(0, 10.0, C0, ELotTexel::Done).TexelsChanged, 0);
	TestEqual(TEXT("free state"), G.DepositIntoCell(1, 10.0, C0, ELotTexel::Free).TexelsChanged, 0);
	return true;
}

bool FLotFinishRawBecomesDoneTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FCellStrip S;
	G.Stamp(S.A, S.B, S.Width, 100.0, ELotTexel::Raw);
	const FLotPour R = G.FinishRaw(S.A, S.B, S.Width);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 0.0, Tolerance);
	TestEqual(TEXT("converted texels"), R.TexelsChanged, 380);
	TestTrue(TEXT("completed"), R.CompletedCells == TArray<int32>({0}));
	TestEqual(TEXT("done"), G.GetCellDone(0), 400);
	TestEqual(TEXT("raw"), G.GetCellRaw(0), 0);
	TestEqual(TEXT("raw left in the array"), CountState(G, ELotTexel::Raw), 0);
	return true;
}

bool FLotFinishIgnoresFreeTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FCellStrip S;
	const FLotPour R = G.FinishRaw(S.A, S.B, S.Width);
	TestEqual(TEXT("texels"), R.TexelsChanged, 0);
	TestEqual(TEXT("completed"), R.CompletedCells.Num(), 0);
	TestEqual(TEXT("still free"), CountState(G, ELotTexel::Free), FLotGrid::Size * FLotGrid::Size);
	return true;
}

bool FLotCompleteRawRemainderTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	const FLotPour Raw = G.DepositIntoCell(0, 0.5, C0 + FVector2D(-150.0, 0.0), ELotTexel::Raw);
	TestEqual(TEXT("raw texels"), Raw.TexelsChanged, 20);
	TestEqual(TEXT("raw counted"), G.GetCellRaw(0), 20);

	const FLotPour R = G.DepositIntoCell(0, 100.0, C0 + FVector2D(150.0, 0.0), ELotTexel::Done);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 9.5, Tolerance);
	TestEqual(TEXT("paid texels"), R.TexelsChanged, 380);
	TestTrue(TEXT("completed once"), R.CompletedCells == TArray<int32>({0}));
	TestEqual(TEXT("raw absorbed"), G.GetCellRaw(0), 0);
	TestEqual(TEXT("done"), G.GetCellDone(0), 400);
	TestEqual(TEXT("raw left in the array"), CountState(G, ELotTexel::Raw), 0);
	return true;
}

bool FLotCompleteRawBlocksTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	G.DepositIntoCell(0, 2.5, C0 + FVector2D(-150.0, 0.0), ELotTexel::Raw);
	G.DepositIntoCell(0, 7.5, C0 + FVector2D(150.0, 0.0), ELotTexel::Done);
	TestEqual(TEXT("raw"), G.GetCellRaw(0), 100);
	TestEqual(TEXT("done"), G.GetCellDone(0), 300);
	TestFalse(TEXT("not complete"), G.IsCellComplete(0));
	return true;
}

bool FLotGridCompleteCellTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	G.DepositIntoCell(0, 2.5, C0 + FVector2D(0.0, -150.0), ELotTexel::Done);
	TestEqual(TEXT("done before"), G.GetCellDone(0), 100);
	const FLotPour R = G.CompleteCell(0);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 7.5, Tolerance);
	TestTrue(TEXT("completed"), R.CompletedCells == TArray<int32>({0}));
	TestEqual(TEXT("done"), G.GetCellDone(0), 400);
	const FLotPour Again = G.CompleteCell(0);
	TestNearlyEqual(TEXT("second call litres"), Again.LitresUsed, 0.0, Tolerance);
	TestEqual(TEXT("second call reports nothing"), Again.CompletedCells.Num(), 0);
	TestEqual(TEXT("complete count"), G.NumCompleteCells(), 1);
	TestEqual(TEXT("invalid cell"), G.CompleteCell(-1).CompletedCells.Num(), 0);
	return true;
}

bool FLotGridDirtyRectsTest::RunTest(const FString& Parameters)
{
	FLotGrid G;
	const FCellStrip S0;
	const int32 LastCell = AsphaltLot::SectorCell(74, 399);
	const FCellStrip S74(LastCell);
	G.Stamp(S0.A, S0.B, S0.Width, 3.0, ELotTexel::Done);
	G.Stamp(S74.A, S74.B, S74.Width, 3.0, ELotTexel::Raw);

	TArray<FIntRect> Rects;
	Rects.Add(FIntRect(1, 2, 3, 4)); // must be emptied first
	G.ConsumeDirtyRects(Rects);
	TestEqual(TEXT("two rectangles"), Rects.Num(), 2);

	const auto SectorBounds = [](int32 Sector)
	{
		const FVector2D Centre = AsphaltLot::SectorCentre(Sector);
		const int32 X = FMath::FloorToInt((Centre.X + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm) - 200;
		const int32 Y = FMath::FloorToInt((Centre.Y + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm) - 200;
		return FIntRect(X, Y, X + 400, Y + 400);
	};
	int32 InSector0 = 0;
	int32 InSector74 = 0;
	for (const FIntRect& Rect : Rects)
	{
		TestTrue(TEXT("rectangle is not empty"), Rect.Max.X > Rect.Min.X && Rect.Max.Y > Rect.Min.Y);
		const FIntRect B0 = SectorBounds(0);
		const FIntRect B74 = SectorBounds(74);
		const auto Inside = [&](const FIntRect& B) { return Rect.Min.X >= B.Min.X && Rect.Min.Y >= B.Min.Y && Rect.Max.X <= B.Max.X && Rect.Max.Y <= B.Max.Y; };
		InSector0 += Inside(B0) ? 1 : 0;
		InSector74 += Inside(B74) ? 1 : 0;
	}
	TestEqual(TEXT("one rectangle inside sector 0"), InSector0, 1);
	TestEqual(TEXT("one rectangle inside sector 74"), InSector74, 1);

	G.ConsumeDirtyRects(Rects);
	TestEqual(TEXT("second call is empty"), Rects.Num(), 0);

	// A completing cell marks its whole 20 x 20 texels.
	G.CompleteCell(0);
	G.ConsumeDirtyRects(Rects);
	TestEqual(TEXT("cell completion: one rectangle"), Rects.Num(), 1);
	if (Rects.Num() == 1)
	{
		TestTrue(TEXT("cell completion covers the cell"), Rects[0].Min == FIntPoint(0, 0) && Rects[0].Max == FIntPoint(20, 20));
	}
	return true;
}

bool FLotStampMidCallCompletionTest::RunTest(const FString& Parameters)
{
	// Two cells side by side; the stamp is wide enough for both. Cell 0 is almost complete beforehand, so it
	// completes in the middle of the call and its remaining candidates are skipped without charge.
	FLotGrid G;
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	G.DepositIntoCell(0, 9.0, C0 + FVector2D(-150.0, 0.0), ELotTexel::Done); // 360 texels
	const FLotPour R = G.Stamp(C0 + FVector2D(-45.0, 0.0), C0 + FVector2D(145.0, 0.0), 100.0, 100.0, ELotTexel::Done);
	TestEqual(TEXT("both cells complete"), R.CompletedCells.Num(), 2);
	TestEqual(TEXT("cell 0 done"), G.GetCellDone(0), 400);
	TestEqual(TEXT("cell 1 done"), G.GetCellDone(1), 400);
	// 20 texels to complete cell 0 (the free remainder is not paid) plus 380 for cell 1.
	TestEqual(TEXT("paid texels"), R.TexelsChanged, 20 + 380);
	TestNearlyEqual(TEXT("litres"), R.LitresUsed, 10.0, Tolerance);
	return true;
}

bool FLotGridNonFiniteInputsTest::RunTest(const FString& Parameters)
{
	const double Bad[3] = {NAN, INFINITY, -INFINITY};
	const FVector2D C0 = AsphaltLot::CellCentre(0);
	const FVector2D Good = C0 + FVector2D(10.0, 0.0);

	FLotGrid G;
	// Raw asphalt in cell 0 so that FinishRaw has something it must not touch.
	G.DepositIntoCell(0, 2.5, C0, ELotTexel::Raw);
	const int32 RawBefore = CountState(G, ELotTexel::Raw);

	int32 Calls = 0;
	const auto Check = [&](const TCHAR* What, const FLotPour& R)
	{
		++Calls;
		TestNearlyEqual(FString(What) + TEXT(": litres"), R.LitresUsed, 0.0, Tolerance);
		TestEqual(FString(What) + TEXT(": texels"), R.TexelsChanged, 0);
		TestEqual(FString(What) + TEXT(": completed"), R.CompletedCells.Num(), 0);
	};

	for (const double V : Bad)
	{
		const FVector2D BadX(V, 0.0);
		const FVector2D BadY(0.0, V);
		Check(TEXT("Stamp A"), G.Stamp(BadX, Good, 100.0, 100.0, ELotTexel::Done));
		Check(TEXT("Stamp B"), G.Stamp(Good, BadY, 100.0, 100.0, ELotTexel::Raw));
		Check(TEXT("Stamp width"), G.Stamp(C0, Good, V, 100.0, ELotTexel::Done));
		Check(TEXT("Stamp litres"), G.Stamp(C0, Good, 100.0, V, ELotTexel::Done));
		Check(TEXT("PourDisc centre"), G.PourDisc(BadY, 15.0, 100.0, C0));
		Check(TEXT("PourDisc radius"), G.PourDisc(C0, V, 100.0, C0));
		Check(TEXT("PourDisc from"), G.PourDisc(C0, 15.0, 100.0, BadX));
		Check(TEXT("PourDisc litres"), G.PourDisc(C0, 15.0, V, C0));
		Check(TEXT("Deposit from"), G.DepositIntoCell(1, 100.0, BadY, ELotTexel::Done));
		Check(TEXT("Deposit litres"), G.DepositIntoCell(1, V, C0, ELotTexel::Done));
		Check(TEXT("FinishRaw A"), G.FinishRaw(BadX, Good, 100.0));
		Check(TEXT("FinishRaw B"), G.FinishRaw(Good, BadY, 100.0));
		Check(TEXT("FinishRaw width"), G.FinishRaw(C0, Good, V));
	}
	TestEqual(TEXT("calls made"), Calls, 39);
	TestEqual(TEXT("raw asphalt untouched"), CountState(G, ELotTexel::Raw), RawBefore);
	TestEqual(TEXT("nothing finished"), CountState(G, ELotTexel::Done), 0);
	TestEqual(TEXT("no complete cell"), G.NumCompleteCells(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
