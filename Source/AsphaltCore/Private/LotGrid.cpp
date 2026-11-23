// Copyright Do it Asphalt. All Rights Reserved.

#include "LotGrid.h"
#include "Algo/Sort.h"
#include "LotSaveIO.h"
#include <algorithm>

namespace
{
	constexpr int32 TexelsPerCellSide = 20;
	constexpr int32 TexelsPerSectorSide = TexelsPerCellSide * AsphaltLot::SectorSide;
	/** Tolerance that keeps a budget of exactly N texel prices from losing the last texel to rounding. */
	constexpr double LitreEpsilon = 1e-9;

	constexpr double AbsDifference(double A, double B)
	{
		return A > B ? A - B : B - A;
	}

	// The constants of FLotGrid are the representation of numbers owned by LotBalance and AsphaltLot; they may not drift.
	static_assert(FLotGrid::TexelsPerCell == TexelsPerCellSide * TexelsPerCellSide, "a cell is 20 x 20 texels");
	static_assert(AsphaltLot::CellCm == TexelsPerCellSide * FLotGrid::TexelCm, "a cell is 20 texels wide");
	static_assert(FLotGrid::Size * FLotGrid::TexelCm == 2.0 * AsphaltLot::HalfExtentCm, "the grid spans the whole car-park square");
	static_assert(FLotGrid::Size == AsphaltLot::Side * TexelsPerCellSide, "the grid has 20 texels per cell");
	static_assert(AbsDifference(FLotGrid::TexelsPerCell * FLotGrid::LitresPerTexel, LotBalance::LitresPerCell) < 1e-12, "a cell costs LitresPerCell");
	static_assert(AbsDifference(LotBalance::CompletionThreshold * FLotGrid::TexelsPerCell, FLotGrid::ThresholdTexels) < 1e-9, "the threshold is CompletionThreshold of a cell");
	static_assert(AsphaltLot::CellsPerSector == AsphaltLot::SectorSide * AsphaltLot::SectorSide, "sectors are 20 x 20 cells");

	int32 TexelIndexOf(int32 X, int32 Y)
	{
		return Y * FLotGrid::Size + X;
	}

	/** Centre of texel column or row N, in cm. */
	double TexelCentre(int32 N)
	{
		return -AsphaltLot::HalfExtentCm + (N + 0.5) * FLotGrid::TexelCm;
	}

	/** Cell that owns the texel (found by division), INDEX_NONE in the shopping-centre quadrant. */
	int32 CellOfTexel(int32 X, int32 Y)
	{
		const int32 Col = X / TexelsPerCellSide;
		const int32 Row = Y / TexelsPerCellSide;
		if (Row < AsphaltLot::Cut)
		{
			return Row * AsphaltLot::Side + Col;
		}
		return Col < AsphaltLot::Cut ? AsphaltLot::Side * AsphaltLot::Cut + (Row - AsphaltLot::Cut) * AsphaltLot::Cut + Col : INDEX_NONE;
	}

	/** Sector (same numbering as AsphaltLot::SectorOf) of a texel inside the car park. */
	int32 SectorOfTexel(int32 X, int32 Y)
	{
		const int32 SCol = X / TexelsPerSectorSide;
		const int32 SRow = Y / TexelsPerSectorSide;
		return SRow < 5 ? SRow * 10 + SCol : 50 + (SRow - 5) * 5 + SCol;
	}

	/** Number of texels the litres can pay for. */
	int32 BudgetTexels(double MaxLitres)
	{
		return static_cast<int32>(FMath::Min(FMath::FloorToDouble((MaxLitres + LitreEpsilon) / FLotGrid::LitresPerTexel), (double)TNumericLimits<int32>::Max()));
	}

	bool IsFinite(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}

	/** Bytes of the complete-cell bitset of a save: one bit per cell, 30 000 cells fill 3 750 bytes exactly. */
	constexpr int32 SaveCompleteBytes = AsphaltLot::NumCells / 8;
	/** Bytes of the texels of one in-progress cell: 400 texels at 1 bit fill 50 bytes exactly. */
	constexpr int32 SaveCellBytes = FLotGrid::TexelsPerCell / 8;
	/** An in-progress record: the cell index and its packed texels. */
	constexpr int32 SaveRecordBytes = 4 + SaveCellBytes;
	static_assert(AsphaltLot::NumCells % 8 == 0, "the complete-cell bitset has no padding bits");
	static_assert(FLotGrid::TexelsPerCell % 8 == 0, "the packed texels of a cell have no padding bits");

	/** Gives every sector (numbered as AsphaltLot::SectorOf) the rectangle of its texels. */
	void MakeAllSectorsDirty(TArray<FIntRect>& Dirty)
	{
		for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
		{
			const int32 Row = Sector < 50 ? Sector / 10 : 5 + (Sector - 50) / 5;
			const int32 Col = Sector < 50 ? Sector % 10 : (Sector - 50) % 5;
			Dirty[Sector] = FIntRect(Col * TexelsPerSectorSide, Row * TexelsPerSectorSide, (Col + 1) * TexelsPerSectorSide, (Row + 1) * TexelsPerSectorSide);
		}
	}

	bool CandidateLess(double KeyA, int32 IndexA, double KeyB, int32 IndexB)
	{
		return KeyA < KeyB || (KeyA == KeyB && IndexA < IndexB);
	}
}

FLotGrid::FLotGrid()
{
	Texels.SetNumZeroed(Size * Size);
	CellDone.SetNumZeroed(AsphaltLot::NumCells);
	Dirty.Init(FIntRect(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max(), TNumericLimits<int32>::Lowest(), TNumericLimits<int32>::Lowest()), AsphaltLot::NumSectors);
}

void FLotGrid::ExpandDirty(int32 Sector, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY)
{
	FIntRect& Rect = Dirty[Sector];
	Rect.Min.X = FMath::Min(Rect.Min.X, MinX);
	Rect.Min.Y = FMath::Min(Rect.Min.Y, MinY);
	Rect.Max.X = FMath::Max(Rect.Max.X, MaxX);
	Rect.Max.Y = FMath::Max(Rect.Max.Y, MaxY);
}

void FLotGrid::CoverTexel(int32 X, int32 Y, int32 Cell, FLotPour& Result)
{
	Texels[TexelIndexOf(X, Y)] = static_cast<uint8>(ELotTexel::Done);
	if (++CellDone[Cell] >= ThresholdTexels)
	{
		FinishCell(Cell, Result);
		return;
	}
	ExpandDirty(SectorOfTexel(X, Y), X, Y, X + 1, Y + 1);
}

void FLotGrid::FinishCell(int32 Cell, FLotPour& Result)
{
	const FIntPoint Coord = AsphaltLot::CellCoord(Cell);
	const int32 X0 = Coord.X * TexelsPerCellSide;
	const int32 Y0 = Coord.Y * TexelsPerCellSide;
	for (int32 Y = Y0; Y < Y0 + TexelsPerCellSide; ++Y)
	{
		FMemory::Memset(&Texels[TexelIndexOf(X0, Y)], static_cast<uint8>(ELotTexel::Done), TexelsPerCellSide);
	}
	CellDone[Cell] = TexelsPerCell;
	++CompleteCount;
	ExpandDirty(SectorOfTexel(X0, Y0), X0, Y0, X0 + TexelsPerCellSide, Y0 + TexelsPerCellSide);
	Result.CompletedCells.Add(Cell);
}

void FLotGrid::SetFluidEnabled(bool bEnabled)
{
	if (!bEnabled)
	{
		Fluid.Clear();
	}
	bFluidEnabled = bEnabled;
}

FLotPour FLotGrid::StepFluid()
{
	FLotPour Result;
	if (!IsFluidActive())
	{
		return Result;
	}
	Fluid.Step(Texels, [this, &Result](int32 X, int32 Y)
	{
		const int32 Cell = CellOfTexel(X, Y);
		if (Cell == INDEX_NONE || Texels[TexelIndexOf(X, Y)] != static_cast<uint8>(ELotTexel::Free))
		{
			return;
		}
		CoverTexel(X, Y, Cell, Result);
		++Result.TexelsChanged;
	});
	return Result;
}

FLotPour FLotGrid::SettleFluid(double MaxSeconds)
{
	FLotPour Total;
	const int32 MaxSteps = FMath::CeilToInt(MaxSeconds / LotBalance::FluidStepSeconds);
	for (int32 Step = 0; Step < MaxSteps && IsFluidActive(); ++Step)
	{
		const FLotPour Pour = StepFluid();
		Total.TexelsChanged += Pour.TexelsChanged;
		Total.CompletedCells.Append(Pour.CompletedCells);
	}
	return Total;
}

void FLotGrid::SortCandidates()
{
	Algo::Sort(Candidates, [](const FCandidate& L, const FCandidate& R)
	{
		return CandidateLess(L.Key, L.Index, R.Key, R.Index);
	});
}

FLotPour FLotGrid::ApplyCandidates(int32 Budget)
{
	FLotPour Result;
	if (bFluidEnabled && !bInstantPour)
	{
		// Pay for the texels, but lay their asphalt as a few hot heaps on the first of them (the nearest to the source): the heaps run.
		TArray<int32, TInlineAllocator<64>> Taken;
		for (const FCandidate& Candidate : Candidates)
		{
			if (Taken.Num() >= Budget)
			{
				break;
			}
			if (Texels[Candidate.Index] == static_cast<uint8>(ELotTexel::Free))
			{
				Taken.Add(Candidate.Index);
			}
		}
		if (Taken.Num() > 0)
		{
			const int32 Heaps = FMath::Max(1, FMath::FloorToInt(Taken.Num() / LotBalance::FluidPileUnits));
			const float Units = FMath::Min(static_cast<float>(Taken.Num()) / Heaps, static_cast<float>(LotBalance::FluidMaxPileUnits));
			for (int32 Heap = 0; Heap < Heaps; ++Heap)
			{
				Fluid.Add(Taken[Heap] % Size, Taken[Heap] / Size, Units);
			}
		}
		Result.TexelsChanged = Taken.Num();
		Result.LitresUsed = Taken.Num() * LitresPerTexel;
		return Result;
	}
	for (const FCandidate& Candidate : Candidates)
	{
		if (Texels[Candidate.Index] != static_cast<uint8>(ELotTexel::Free))
		{
			continue; // taken meanwhile by the completion of its cell: free
		}
		if (Result.TexelsChanged >= Budget)
		{
			break;
		}
		const int32 X = Candidate.Index % Size;
		const int32 Y = Candidate.Index / Size;
		CoverTexel(X, Y, CellOfTexel(X, Y), Result);
		++Result.TexelsChanged;
	}
	Result.LitresUsed = Result.TexelsChanged * LitresPerTexel;
	return Result;
}

void FLotGrid::CollectStampCandidates(FVector2D A, FVector2D B, double WidthCm)
{
	Candidates.Reset();

	FVector2D Axis = B - A;
	const double Length = Axis.Size();
	if (Length > UE_DOUBLE_SMALL_NUMBER)
	{
		Axis /= Length;
	}
	else
	{
		Axis = FVector2D(1.0, 0.0);
	}
	const FVector2D Normal(-Axis.Y, Axis.X);
	const double Pad = LotBalance::StampPadCm;
	const double HalfWidth = WidthCm * 0.5;

	// Bounding box of the rectangle in texel space; work stays proportional to it.
	const FVector2D Start = A - Axis * Pad;
	const FVector2D End = B + Axis * Pad;
	const FVector2D Side = Normal * HalfWidth;
	const FVector2D Corners[4] = {Start + Side, Start - Side, End + Side, End - Side};
	FVector2D Min = Corners[0];
	FVector2D Max = Corners[0];
	for (const FVector2D& Corner : Corners)
	{
		Min.X = FMath::Min(Min.X, Corner.X);
		Min.Y = FMath::Min(Min.Y, Corner.Y);
		Max.X = FMath::Max(Max.X, Corner.X);
		Max.Y = FMath::Max(Max.Y, Corner.Y);
	}
	const auto ToTexel = [](double World)
	{
		return FMath::Floor((World + AsphaltLot::HalfExtentCm) / TexelCm);
	};
	const double RawMinX = ToTexel(Min.X);
	const double RawMaxX = ToTexel(Max.X);
	const double RawMinY = ToTexel(Min.Y);
	const double RawMaxY = ToTexel(Max.Y);
	if (RawMaxX < 0.0 || RawMinX > Size - 1 || RawMaxY < 0.0 || RawMinY > Size - 1)
	{
		return;
	}
	const int32 X0 = static_cast<int32>(FMath::Max(RawMinX, 0.0));
	const int32 X1 = static_cast<int32>(FMath::Min(RawMaxX, (double)(Size - 1)));
	const int32 Y0 = static_cast<int32>(FMath::Max(RawMinY, 0.0));
	const int32 Y1 = static_cast<int32>(FMath::Min(RawMaxY, (double)(Size - 1)));

	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		const double Wy = TexelCentre(Y);
		for (int32 X = X0; X <= X1; ++X)
		{
			const int32 Index = TexelIndexOf(X, Y);
			if (Texels[Index] != static_cast<uint8>(ELotTexel::Free) || CellOfTexel(X, Y) == INDEX_NONE)
			{
				continue; // taken already, or outside the car park: never changes
			}
			const FVector2D Offset(TexelCentre(X) - A.X, Wy - A.Y);
			const double Along = FVector2D::DotProduct(Offset, Axis);
			const double Across = FVector2D::DotProduct(Offset, Normal);
			if (Along < -Pad || Along > Length + Pad || FMath::Abs(Across) > HalfWidth)
			{
				continue;
			}
			Candidates.Add({Along, Index});
		}
	}
}

FLotPour FLotGrid::Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres)
{
	if (!FMath::IsFinite(MaxLitres) || !(MaxLitres > 0.0) || !FMath::IsFinite(WidthCm) || !(WidthCm >= 0.0) || !IsFinite(A) || !IsFinite(B))
	{
		return FLotPour();
	}
	CollectStampCandidates(A, B, WidthCm);
	SortCandidates();
	return ApplyCandidates(BudgetTexels(MaxLitres));
}

FLotPour FLotGrid::PourDisc(FVector2D Centre, double RadiusCm, double MaxLitres, FVector2D From)
{
	if (!FMath::IsFinite(MaxLitres) || !(MaxLitres > 0.0) || !FMath::IsFinite(RadiusCm) || !(RadiusCm >= 0.0) || !IsFinite(Centre) || !IsFinite(From))
	{
		return FLotPour();
	}
	Candidates.Reset();

	const auto ToTexel = [](double World)
	{
		return FMath::Floor((World + AsphaltLot::HalfExtentCm) / TexelCm);
	};
	const double RawMinX = ToTexel(Centre.X - RadiusCm);
	const double RawMaxX = ToTexel(Centre.X + RadiusCm);
	const double RawMinY = ToTexel(Centre.Y - RadiusCm);
	const double RawMaxY = ToTexel(Centre.Y + RadiusCm);
	if (RawMaxX < 0.0 || RawMinX > Size - 1 || RawMaxY < 0.0 || RawMinY > Size - 1)
	{
		return FLotPour();
	}
	const int32 X0 = static_cast<int32>(FMath::Max(RawMinX, 0.0));
	const int32 X1 = static_cast<int32>(FMath::Min(RawMaxX, (double)(Size - 1)));
	const int32 Y0 = static_cast<int32>(FMath::Max(RawMinY, 0.0));
	const int32 Y1 = static_cast<int32>(FMath::Min(RawMaxY, (double)(Size - 1)));

	const double RadiusSquared = RadiusCm * RadiusCm;
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		const double Wy = TexelCentre(Y);
		for (int32 X = X0; X <= X1; ++X)
		{
			const int32 Index = TexelIndexOf(X, Y);
			if (Texels[Index] != static_cast<uint8>(ELotTexel::Free) || CellOfTexel(X, Y) == INDEX_NONE)
			{
				continue;
			}
			const double Wx = TexelCentre(X);
			if (FMath::Square(Wx - Centre.X) + FMath::Square(Wy - Centre.Y) > RadiusSquared)
			{
				continue;
			}
			// A fluid pour lands where it is aimed (the heaps lie on the first texels), a pour that sets at once fills from the source.
			const FVector2D Origin = (bFluidEnabled && !bInstantPour) ? Centre : From;
			Candidates.Add({FMath::Square(Wx - Origin.X) + FMath::Square(Wy - Origin.Y), Index});
		}
	}
	SortCandidates();
	return ApplyCandidates(BudgetTexels(MaxLitres));
}

FLotPour FLotGrid::DepositIntoCell(int32 Cell, double MaxLitres, FVector2D From)
{
	if (Cell < 0 || Cell >= AsphaltLot::NumCells || IsCellComplete(Cell) || !FMath::IsFinite(MaxLitres) || !(MaxLitres > 0.0) || !IsFinite(From))
	{
		return FLotPour();
	}

	const FIntPoint Coord = AsphaltLot::CellCoord(Cell);
	const int32 X0 = Coord.X * TexelsPerCellSide;
	const int32 Y0 = Coord.Y * TexelsPerCellSide;
	Candidates.Reset();
	for (int32 Y = Y0; Y < Y0 + TexelsPerCellSide; ++Y)
	{
		const double Wy = TexelCentre(Y);
		for (int32 X = X0; X < X0 + TexelsPerCellSide; ++X)
		{
			const int32 Index = TexelIndexOf(X, Y);
			if (Texels[Index] == static_cast<uint8>(ELotTexel::Free))
			{
				Candidates.Add({FMath::Square(TexelCentre(X) - From.X) + FMath::Square(Wy - From.Y), Index});
			}
		}
	}

	// Every texel costs the same and finishing the cell frees the rest, so only the first Take texels in order
	// can ever be paid for; select them instead of sorting the whole cell.
	const int32 Take = FMath::Min3(BudgetTexels(MaxLitres), Candidates.Num(), ThresholdTexels - static_cast<int32>(CellDone[Cell]));
	if (Take <= 0)
	{
		return FLotPour();
	}
	if (Take < Candidates.Num())
	{
		std::nth_element(Candidates.GetData(), Candidates.GetData() + Take, Candidates.GetData() + Candidates.Num(),
			[](const FCandidate& L, const FCandidate& R) { return CandidateLess(L.Key, L.Index, R.Key, R.Index); });
		Candidates.SetNum(Take, EAllowShrinking::No);
	}
	return ApplyCandidates(Take);
}

FLotPour FLotGrid::CompleteCell(int32 Cell)
{
	FLotPour Result;
	if (Cell < 0 || Cell >= AsphaltLot::NumCells || IsCellComplete(Cell))
	{
		return Result;
	}
	Result.TexelsChanged = TexelsPerCell - CellDone[Cell];
	Result.LitresUsed = Result.TexelsChanged * LitresPerTexel;
	FinishCell(Cell, Result);
	return Result;
}

int32 FLotGrid::GetCellDone(int32 Cell) const
{
	return (Cell >= 0 && Cell < AsphaltLot::NumCells) ? CellDone[Cell] : 0;
}

bool FLotGrid::IsCellComplete(int32 Cell) const
{
	return Cell >= 0 && Cell < AsphaltLot::NumCells && CellDone[Cell] >= ThresholdTexels;
}

int32 FLotGrid::NumCompleteCells() const
{
	return CompleteCount;
}

ELotTexel FLotGrid::GetTexel(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= Size || Y >= Size)
	{
		return ELotTexel::Free;
	}
	return static_cast<ELotTexel>(Texels[TexelIndexOf(X, Y)]);
}

TConstArrayView<uint8> FLotGrid::GetTexels() const
{
	return Texels;
}

void FLotGrid::ConsumeDirtyRects(TArray<FIntRect>& Out)
{
	Out.Reset();
	for (FIntRect& Rect : Dirty)
	{
		if (Rect.Max.X > Rect.Min.X)
		{
			Out.Add(Rect);
			Rect = FIntRect(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max(), TNumericLimits<int32>::Lowest(), TNumericLimits<int32>::Lowest());
		}
	}
}

void FLotGrid::Write(TArray<uint8>& Out) const
{
	TArray<int32> InProgress;
	const int32 BitsAt = Out.AddZeroed(SaveCompleteBytes);
	for (int32 Cell = 0; Cell < AsphaltLot::NumCells; ++Cell)
	{
		if (IsCellComplete(Cell))
		{
			Out[BitsAt + Cell / 8] |= static_cast<uint8>(1 << (Cell % 8));
		}
		else if (CellDone[Cell] > 0)
		{
			InProgress.Add(Cell);
		}
	}

	Out.Reserve(Out.Num() + 4 + InProgress.Num() * SaveRecordBytes);
	LotSaveIO::FWriter Writer(Out);
	Writer.WriteI32(InProgress.Num());
	for (const int32 Cell : InProgress)
	{
		Writer.WriteI32(Cell);
		const FIntPoint Coord = AsphaltLot::CellCoord(Cell);
		const int32 PackedAt = Out.AddZeroed(SaveCellBytes);
		for (int32 I = 0; I < TexelsPerCell; ++I)
		{
			const int32 Texel = Texels[TexelIndexOf(Coord.X * TexelsPerCellSide + I % TexelsPerCellSide, Coord.Y * TexelsPerCellSide + I / TexelsPerCellSide)];
			Out[PackedAt + I / 8] |= static_cast<uint8>(Texel << (I % 8));
		}
	}
}

bool FLotGrid::Read(TConstArrayView<uint8> In, int32& OutBytesRead)
{
	LotSaveIO::FReader Reader(In);
	TConstArrayView<uint8> Bits;
	int32 InProgressCount = 0;
	if (!Reader.ReadBlock(SaveCompleteBytes, Bits) || !Reader.ReadI32(InProgressCount))
	{
		return false;
	}
	// The count may not drive anything beyond what the remaining bytes can hold.
	if (InProgressCount < 0 || InProgressCount > AsphaltLot::NumCells || static_cast<int64>(InProgressCount) * SaveRecordBytes > Reader.Remaining())
	{
		return false;
	}

	// Everything is built on the side and taken over only when the whole grid is valid.
	TArray<uint8> NewTexels;
	NewTexels.SetNumZeroed(Size * Size);
	TArray<uint16> NewDone;
	NewDone.SetNumZeroed(AsphaltLot::NumCells);
	int32 NewComplete = 0;

	for (int32 Cell = 0; Cell < AsphaltLot::NumCells; ++Cell)
	{
		if ((Bits[Cell / 8] >> (Cell % 8)) & 1)
		{
			const FIntPoint Coord = AsphaltLot::CellCoord(Cell);
			for (int32 Y = Coord.Y * TexelsPerCellSide; Y < (Coord.Y + 1) * TexelsPerCellSide; ++Y)
			{
				FMemory::Memset(&NewTexels[TexelIndexOf(Coord.X * TexelsPerCellSide, Y)], static_cast<uint8>(ELotTexel::Done), TexelsPerCellSide);
			}
			NewDone[Cell] = TexelsPerCell;
			++NewComplete;
		}
	}

	int32 Previous = INDEX_NONE;
	for (int32 Record = 0; Record < InProgressCount; ++Record)
	{
		int32 Cell = 0;
		TConstArrayView<uint8> Packed;
		if (!Reader.ReadI32(Cell) || !Reader.ReadBlock(SaveCellBytes, Packed))
		{
			return false;
		}
		// Ascending, inside the car park and not at the same time complete.
		if (Cell <= Previous || Cell >= AsphaltLot::NumCells || NewDone[Cell] != 0)
		{
			return false;
		}
		Previous = Cell;

		const FIntPoint Coord = AsphaltLot::CellCoord(Cell);
		int32 Done = 0;
		for (int32 I = 0; I < TexelsPerCell; ++I)
		{
			const uint8 Texel = (Packed[I / 8] >> (I % 8)) & 1;
			Done += Texel;
			NewTexels[TexelIndexOf(Coord.X * TexelsPerCellSide + I % TexelsPerCellSide, Coord.Y * TexelsPerCellSide + I / TexelsPerCellSide)] = Texel;
		}
		// A cell at the threshold should have been complete; an untouched cell is not written at all.
		if (Done >= ThresholdTexels || Done == 0)
		{
			return false;
		}
		NewDone[Cell] = static_cast<uint16>(Done);
	}

	Texels = MoveTemp(NewTexels);
	CellDone = MoveTemp(NewDone);
	CompleteCount = NewComplete;
	Fluid.Clear();
	MakeAllSectorsDirty(Dirty);
	OutBytesRead = Reader.GetPosition();
	return true;
}
