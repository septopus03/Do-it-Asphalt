// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltBalance.h"
#include "CoverageGrid.h"
#include "HexMap.h"

DIA_TEST(FGridTexelCountsTest, "DoItAsphalt.Core.Grid.TexelCounts")
DIA_TEST(FStampExactLitresTest, "DoItAsphalt.Core.Stamp.ExactLitres")
DIA_TEST(FStampRepaintIsFreeTest, "DoItAsphalt.Core.Stamp.RepaintIsFree")
DIA_TEST(FStampOutsideMapIsFreeTest, "DoItAsphalt.Core.Stamp.OutsideMapIsFree")
DIA_TEST(FStampStopsWhenLitresRunOutTest, "DoItAsphalt.Core.Stamp.StopsWhenLitresRunOut")
DIA_TEST(FStampZeroLengthSegmentTest, "DoItAsphalt.Core.Stamp.ZeroLengthSegment")
DIA_TEST(FStampCompletesCellTest, "DoItAsphalt.Core.Stamp.CompletesCell")
DIA_TEST(FStampNeverOverspendsTest, "DoItAsphalt.Core.Stamp.NeverOverspends")
DIA_TEST(FStampCompletionMidStampTest, "DoItAsphalt.Core.Stamp.CompletionMidStamp")
DIA_TEST(FGridDepositIntoCellTest, "DoItAsphalt.Core.Grid.DepositIntoCell")
DIA_TEST(FGridThresholdIsFreeTest, "DoItAsphalt.Core.Grid.ThresholdIsFree")
DIA_TEST(FGridCompleteCellTest, "DoItAsphalt.Core.Grid.CompleteCell")
DIA_TEST(FGridDirtyRectTest, "DoItAsphalt.Core.Grid.DirtyRect")
DIA_TEST(FGridBitsTest, "DoItAsphalt.Core.Grid.Bits")

namespace
{
	int32 CountCovered(const FCoverageGrid& Grid)
	{
		int32 Count = 0;
		for (uint8 Texel : Grid.GetTexels())
		{
			Count += Texel != 0 ? 1 : 0;
		}
		return Count;
	}
}

bool FGridTexelCountsTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);

	int32 OutOfRange = 0;
	int32 Total = 0;
	for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
	{
		const int32 Count = G.GetCellTexelCount(Cell);
		Total += Count;
		if (Count < 385 || Count > 415)
		{
			++OutOfRange;
		}
	}
	TestEqual(TEXT("cells with implausible texel count"), OutOfRange, 0);
	TestEqual(TEXT("cell 0"), G.GetCellTexelCount(0), 408);

	int32 Owned = 0;
	for (int32 Y = 0; Y < FCoverageGrid::Size; ++Y)
	{
		for (int32 X = 0; X < FCoverageGrid::Size; ++X)
		{
			Owned += G.GetTexelCell(X, Y) != INDEX_NONE ? 1 : 0;
		}
	}
	TestEqual(TEXT("owned texels match the counts"), Owned, Total);
	TestEqual(TEXT("out of range texel"), G.GetTexelCell(-1, 0), (int32)INDEX_NONE);
	TestEqual(TEXT("centre texel belongs to cell 0"), G.GetTexelCell(320, 320), 0);
	TestEqual(TEXT("corner texel belongs to nothing"), G.GetTexelCell(0, 0), (int32)INDEX_NONE);
	TestEqual(TEXT("fresh grid has no completed cells"), G.NumCompleteCells(), 0);
	TestEqual(TEXT("fresh grid is bare"), CountCovered(G), 0);
	return true;
}

bool FStampExactLitresTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	const double PerTexel = 20.0 / G.GetCellTexelCount(0);
	FPourResult R = G.Stamp({-20, 0}, {20, 0}, 20.0, 100.0); // 50 x 20 cm -> 40 texels
	TestEqual(TEXT("litres"), R.LitresUsed, 40 * PerTexel, 1e-9);
	TestEqual(TEXT("fill"), G.GetCellFill(0), 40.0 / G.GetCellTexelCount(0), 1e-9);
	TestEqual(TEXT("covered texels"), CountCovered(G), 40);
	TestEqual(TEXT("nothing completed"), R.CompletedCells.Num(), 0);
	return true;
}

bool FStampRepaintIsFreeTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	G.Stamp({-20, 0}, {20, 0}, 20.0, 100.0);
	TestEqual(TEXT("again"), G.Stamp({-20, 0}, {20, 0}, 20.0, 100.0).LitresUsed, 0.0);
	return true;
}

bool FStampOutsideMapIsFreeTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	TestEqual(TEXT("outside"), G.Stamp({1500, 1500}, {1550, 1550}, 200.0, 100.0).LitresUsed, 0.0);
	TestEqual(TEXT("outside the grid"), G.Stamp({5000, 5000}, {6000, 6000}, 200.0, 100.0).LitresUsed, 0.0);
	TestEqual(TEXT("covered texels"), CountCovered(G), 0);
	return true;
}

bool FStampStopsWhenLitresRunOutTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G2(Map);
	const double PerTexel = 20.0 / G2.GetCellTexelCount(0);
	TestEqual(TEXT("limited"), G2.Stamp({-20, 0}, {20, 0}, 20.0, 10.5 * PerTexel).LitresUsed, 10 * PerTexel, 1e-9);
	TestEqual(TEXT("exactly ten texels"), CountCovered(G2), 10);
	return true;
}

bool FStampZeroLengthSegmentTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	const double PerTexel = 20.0 / G.GetCellTexelCount(0);
	const FPourResult R = G.Stamp({0, 0}, {0, 0}, 20.0, 100.0);
	TestEqual(TEXT("covers 2 x 4 texels"), CountCovered(G), 8);
	TestEqual(TEXT("litres"), R.LitresUsed, 8 * PerTexel, 1e-9);
	return true;
}

bool FStampCompletesCellTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	int32 Reports = 0;
	for (int32 Y = -65; Y <= 65; Y += 10)
	{
		const FPourResult R = G.Stamp({-70, (double)Y}, {70, (double)Y}, 10.0, 1000.0);
		for (int32 Cell : R.CompletedCells)
		{
			Reports += Cell == 0 ? 1 : 0;
		}
	}
	TestEqual(TEXT("cell 0 reported exactly once"), Reports, 1);
	TestTrue(TEXT("complete"), G.IsCellComplete(0));
	TestEqual(TEXT("fill"), G.GetCellFill(0), 1.0, 1e-12);
	return true;
}

bool FStampNeverOverspendsTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	const double PerTexel = 20.0 / G.GetCellTexelCount(0);

	// The stamp (x in [-35, 35], y in [-10, 10]) lies inside cell 0. A budget that is not a multiple of a texel
	// price must never be exceeded and never buys a partial texel.
	const double Budget = 33.7 * PerTexel;
	const FPourResult R = G.Stamp({-30, 0}, {30, 0}, 20.0, Budget);
	TestTrue(TEXT("not over budget"), R.LitresUsed <= Budget);
	TestEqual(TEXT("whole texels only"), R.LitresUsed, 33 * PerTexel, 1e-9);

	// Zero or negative budget paints nothing.
	TestEqual(TEXT("zero budget"), G.Stamp({-30, 0}, {30, 0}, 20.0, 0.0).LitresUsed, 0.0);
	TestEqual(TEXT("negative budget"), G.Stamp({-30, 0}, {30, 0}, 20.0, -5.0).LitresUsed, 0.0);
	TestEqual(TEXT("covered texels"), CountCovered(G), 33);
	return true;
}

bool FStampCompletionMidStampTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);

	// One big stamp: x in [-205, 205], y in [-200, 200]. Cell 0 and many neighbours reach their threshold in the
	// middle of the row-major pass. Within a cell the texels are paid in ascending order, so a cell that receives
	// S texels from the stamp pays min(S, Threshold) of them and the rest of a completed cell is free.
	TArray<int32> Stamped;
	Stamped.SetNumZeroed(Map.Num());
	for (int32 Y = 0; Y < FCoverageGrid::Size; ++Y)
	{
		for (int32 X = 0; X < FCoverageGrid::Size; ++X)
		{
			const double Wx = -FCoverageGrid::HalfExtentCm + (X + 0.5) * FCoverageGrid::TexelCm;
			const double Wy = -FCoverageGrid::HalfExtentCm + (Y + 0.5) * FCoverageGrid::TexelCm;
			const int32 Cell = G.GetTexelCell(X, Y);
			if (Cell != INDEX_NONE && FMath::Abs(Wx) <= 205.0 && FMath::Abs(Wy) <= 200.0)
			{
				++Stamped[Cell];
			}
		}
	}

	double ExpectedLitres = 0.0;
	TSet<int32> ExpectedCompleted;
	for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
	{
		const int32 Count = G.GetCellTexelCount(Cell);
		const int32 Threshold = FMath::CeilToInt(AsphaltBalance::CompletionThreshold * Count - 1e-9);
		ExpectedLitres += FMath::Min(Stamped[Cell], Threshold) * (20.0 / Count);
		if (Stamped[Cell] >= Threshold)
		{
			ExpectedCompleted.Add(Cell);
		}
	}
	TestTrue(TEXT("cell 0 is expected to complete"), ExpectedCompleted.Contains(0));

	FCoverageGrid Limited(Map);
	const FPourResult R = G.Stamp({-200, 0}, {200, 0}, 400.0, 100000.0);
	TestEqual(TEXT("litres"), R.LitresUsed, ExpectedLitres, 1e-6);
	TestEqual(TEXT("completed count"), R.CompletedCells.Num(), ExpectedCompleted.Num());
	TSet<int32> Seen;
	for (int32 Cell : R.CompletedCells)
	{
		TestTrue(*FString::Printf(TEXT("cell %d expected"), Cell), ExpectedCompleted.Contains(Cell));
		TestFalse(*FString::Printf(TEXT("cell %d reported twice"), Cell), Seen.Contains(Cell));
		TestTrue(*FString::Printf(TEXT("cell %d complete"), Cell), G.IsCellComplete(Cell));
		TestEqual(*FString::Printf(TEXT("cell %d fill"), Cell), G.GetCellFill(Cell), 1.0, 1e-12);
		Seen.Add(Cell);
	}
	TestEqual(TEXT("complete count matches"), G.NumCompleteCells(), R.CompletedCells.Num());
	TestEqual(TEXT("repaint is free"), G.Stamp({-200, 0}, {200, 0}, 400.0, 100000.0).LitresUsed, 0.0);

	// Running out of litres somewhere in the middle: never over budget, never a partly paid texel.
	const double Budget = ExpectedLitres * 0.5;
	const FPourResult Half = Limited.Stamp({-200, 0}, {200, 0}, 400.0, Budget);
	TestTrue(TEXT("half: within budget"), Half.LitresUsed <= Budget);
	TestTrue(TEXT("half: next texel was unaffordable"), Half.LitresUsed > Budget - 20.0 / 385.0);
	return true;
}

bool FGridDepositIntoCellTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	const double P5 = 20.0 / G.GetCellTexelCount(5);

	const FPourResult First = G.DepositIntoCell(5, 7.0);
	TestTrue(TEXT("first lower"), First.LitresUsed > 7.0 - P5);
	TestTrue(TEXT("first upper"), First.LitresUsed <= 7.0);
	TestEqual(TEXT("first completes nothing"), First.CompletedCells.Num(), 0);

	const FPourResult Second = G.DepositIntoCell(5, 7.0);
	TestTrue(TEXT("second lower"), Second.LitresUsed > 7.0 - P5);
	TestEqual(TEXT("second does not complete"), Second.CompletedCells.Num(), 0);
	TestFalse(TEXT("not complete yet"), G.IsCellComplete(5));

	const FPourResult Third = G.DepositIntoCell(5, 7.0);
	TestEqual(TEXT("third completes cell 5"), Third.CompletedCells.Num(), 1);
	if (Third.CompletedCells.Num() == 1)
	{
		TestEqual(TEXT("cell id"), Third.CompletedCells[0], 5);
	}

	TestEqual(TEXT("fourth is free"), G.DepositIntoCell(5, 7.0).LitresUsed, 0.0);
	TestEqual(TEXT("invalid cell"), G.DepositIntoCell(-1, 7.0).LitresUsed, 0.0);
	return true;
}

bool FGridThresholdIsFreeTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	const double P9 = 20.0 / G.GetCellTexelCount(9);

	const FPourResult R = G.DepositIntoCell(9, 100.0);
	TestTrue(TEXT("lower"), R.LitresUsed >= 19.4 - P9);
	TestTrue(TEXT("upper"), R.LitresUsed <= 19.4 + P9);
	TestTrue(TEXT("complete"), G.IsCellComplete(9));
	TestEqual(TEXT("fill"), G.GetCellFill(9), 1.0, 1e-12);
	return true;
}

bool FGridCompleteCellTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);

	const FPourResult R = G.CompleteCell(7);
	TestEqual(TEXT("litres"), R.LitresUsed, 20.0, 1e-6);
	TestEqual(TEXT("completed count"), R.CompletedCells.Num(), 1);
	if (R.CompletedCells.Num() == 1)
	{
		TestEqual(TEXT("cell id"), R.CompletedCells[0], 7);
	}
	TestTrue(TEXT("complete"), G.IsCellComplete(7));
	TestEqual(TEXT("fill"), G.GetCellFill(7), 1.0, 1e-12);

	const FPourResult Again = G.CompleteCell(7);
	TestEqual(TEXT("second litres"), Again.LitresUsed, 0.0);
	TestEqual(TEXT("second completed"), Again.CompletedCells.Num(), 0);

	// A partly painted cell only pays for the part that is still bare.
	const double P8 = 20.0 / G.GetCellTexelCount(8);
	G.DepositIntoCell(8, 5.0 * P8 + 0.5 * P8);
	TestEqual(TEXT("partial"), G.CompleteCell(8).LitresUsed, 20.0 - 5 * P8, 1e-6);
	return true;
}

bool FGridDirtyRectTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);

	FIntRect Rect;
	TestFalse(TEXT("fresh grid is clean"), G.ConsumeDirtyRect(Rect));

	G.Stamp({-20, 0}, {20, 0}, 20.0, 100.0);
	TestTrue(TEXT("dirty after stamp"), G.ConsumeDirtyRect(Rect));

	int32 Outside = 0;
	TConstArrayView<uint8> Texels = G.GetTexels();
	for (int32 Y = 0; Y < FCoverageGrid::Size; ++Y)
	{
		for (int32 X = 0; X < FCoverageGrid::Size; ++X)
		{
			if (Texels[Y * FCoverageGrid::Size + X] != 0 && (X < Rect.Min.X || X >= Rect.Max.X || Y < Rect.Min.Y || Y >= Rect.Max.Y))
			{
				++Outside;
			}
		}
	}
	TestEqual(TEXT("stamped texels outside the rect"), Outside, 0);
	TestTrue(TEXT("rect is small"), Rect.Width() <= 12 && Rect.Height() <= 6);
	TestFalse(TEXT("clean after consume"), G.ConsumeDirtyRect(Rect));

	// A repaint changes nothing.
	G.Stamp({-20, 0}, {20, 0}, 20.0, 100.0);
	TestFalse(TEXT("repaint is not dirty"), G.ConsumeDirtyRect(Rect));
	return true;
}

bool FGridBitsTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	FCoverageGrid G(Map);
	G.Stamp({-300, -100}, {300, 200}, 60.0, 1000.0);
	G.DepositIntoCell(3, 100.0);
	G.CompleteCell(11);

	TArray<uint8> Bits;
	G.WriteBits(Bits);
	TestEqual(TEXT("size"), Bits.Num(), 51200);

	FCoverageGrid H(Map);
	TestTrue(TEXT("read ok"), H.ReadBits(Bits));
	int32 FillMismatch = 0;
	for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
	{
		if (!FMath::IsNearlyEqual(G.GetCellFill(Cell), H.GetCellFill(Cell), 1e-12) || G.IsCellComplete(Cell) != H.IsCellComplete(Cell))
		{
			++FillMismatch;
		}
	}
	TestEqual(TEXT("cells that differ after the round trip"), FillMismatch, 0);
	TestEqual(TEXT("complete cells"), H.NumCompleteCells(), G.NumCompleteCells());
	TestTrue(TEXT("some cells complete"), G.NumCompleteCells() >= 2);
	TestEqual(TEXT("texels"), CountCovered(H), CountCovered(G));

	TArray<uint8> Bits2;
	H.WriteBits(Bits2);
	TestTrue(TEXT("bit identical"), Bits == Bits2);

	// A wrong size is rejected and leaves the grid untouched.
	TArray<uint8> Short;
	Short.SetNumZeroed(100);
	const int32 Before = CountCovered(H);
	TestFalse(TEXT("wrong size"), H.ReadBits(Short));
	TestEqual(TEXT("untouched after rejection"), CountCovered(H), Before);

	// Loading an empty mask clears everything.
	TArray<uint8> Empty;
	Empty.SetNumZeroed(51200);
	TestTrue(TEXT("read empty"), H.ReadBits(Empty));
	TestEqual(TEXT("empty texels"), CountCovered(H), 0);
	TestEqual(TEXT("empty complete"), H.NumCompleteCells(), 0);
	return true;
}
