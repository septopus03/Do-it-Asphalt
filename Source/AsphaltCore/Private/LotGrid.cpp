// Copyright Do it Asphalt. All Rights Reserved.

#include "LotGrid.h"
#include "Algo/Sort.h"
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

	bool CandidateLess(double KeyA, int32 IndexA, double KeyB, int32 IndexB)
	{
		return KeyA < KeyB || (KeyA == KeyB && IndexA < IndexB);
	}
}

FLotGrid::FLotGrid()
{
	Texels.SetNumZeroed(Size * Size);
	CellDone.SetNumZeroed(AsphaltLot::NumCells);
	CellRaw.SetNumZeroed(AsphaltLot::NumCells);
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

void FLotGrid::ChangeTexel(int32 X, int32 Y, int32 Cell, ELotTexel To, FLotPour& Result)
{
	uint8& Texel = Texels[TexelIndexOf(X, Y)];
	if (Texel == static_cast<uint8>(ELotTexel::Raw))
	{
		--CellRaw[Cell];
	}
	Texel = static_cast<uint8>(To);
	if (To == ELotTexel::Raw)
	{
		++CellRaw[Cell];
	}
	else if (++CellDone[Cell] >= ThresholdTexels)
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
	CellRaw[Cell] = 0;
	++CompleteCount;
	ExpandDirty(SectorOfTexel(X0, Y0), X0, Y0, X0 + TexelsPerCellSide, Y0 + TexelsPerCellSide);
	Result.CompletedCells.Add(Cell);
}

void FLotGrid::SortCandidates()
{
	Algo::Sort(Candidates, [](const FCandidate& L, const FCandidate& R)
	{
		return CandidateLess(L.Key, L.Index, R.Key, R.Index);
	});
}

FLotPour FLotGrid::ApplyCandidates(int32 Budget, ELotTexel State)
{
	FLotPour Result;
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
		ChangeTexel(X, Y, CellOfTexel(X, Y), State, Result);
		++Result.TexelsChanged;
	}
	Result.LitresUsed = Result.TexelsChanged * LitresPerTexel;
	return Result;
}

void FLotGrid::CollectStampCandidates(FVector2D A, FVector2D B, double WidthCm, ELotTexel WantedState)
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
			if (Texels[Index] != static_cast<uint8>(WantedState))
			{
				continue;
			}
			if (WantedState == ELotTexel::Free && CellOfTexel(X, Y) == INDEX_NONE)
			{
				continue; // outside the car park: never changes
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

FLotPour FLotGrid::Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres, ELotTexel State)
{
	if (State == ELotTexel::Free || !FMath::IsFinite(MaxLitres) || !(MaxLitres > 0.0) || !FMath::IsFinite(WidthCm) || !(WidthCm >= 0.0) || !IsFinite(A) || !IsFinite(B))
	{
		return FLotPour();
	}
	CollectStampCandidates(A, B, WidthCm, ELotTexel::Free);
	SortCandidates();
	return ApplyCandidates(BudgetTexels(MaxLitres), State);
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
			Candidates.Add({FMath::Square(Wx - From.X) + FMath::Square(Wy - From.Y), Index});
		}
	}
	SortCandidates();
	return ApplyCandidates(BudgetTexels(MaxLitres), ELotTexel::Done);
}

FLotPour FLotGrid::DepositIntoCell(int32 Cell, double MaxLitres, FVector2D From, ELotTexel State)
{
	if (State == ELotTexel::Free || Cell < 0 || Cell >= AsphaltLot::NumCells || IsCellComplete(Cell) || !FMath::IsFinite(MaxLitres) || !(MaxLitres > 0.0) || !IsFinite(From))
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
	int32 Take = FMath::Min(BudgetTexels(MaxLitres), Candidates.Num());
	if (State == ELotTexel::Done)
	{
		Take = FMath::Min(Take, ThresholdTexels - static_cast<int32>(CellDone[Cell]));
	}
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
	return ApplyCandidates(Take, State);
}

FLotPour FLotGrid::FinishRaw(FVector2D A, FVector2D B, double WidthCm)
{
	FLotPour Result;
	if (!FMath::IsFinite(WidthCm) || !(WidthCm >= 0.0) || !IsFinite(A) || !IsFinite(B))
	{
		return Result;
	}
	CollectStampCandidates(A, B, WidthCm, ELotTexel::Raw);
	for (const FCandidate& Candidate : Candidates)
	{
		if (Texels[Candidate.Index] != static_cast<uint8>(ELotTexel::Raw))
		{
			continue; // finished meanwhile by the completion of its cell
		}
		const int32 X = Candidate.Index % Size;
		const int32 Y = Candidate.Index / Size;
		ChangeTexel(X, Y, CellOfTexel(X, Y), ELotTexel::Done, Result);
		++Result.TexelsChanged;
	}
	return Result;
}

FLotPour FLotGrid::CompleteCell(int32 Cell)
{
	FLotPour Result;
	if (Cell < 0 || Cell >= AsphaltLot::NumCells || IsCellComplete(Cell))
	{
		return Result;
	}
	Result.TexelsChanged = TexelsPerCell - CellDone[Cell] - CellRaw[Cell];
	Result.LitresUsed = Result.TexelsChanged * LitresPerTexel;
	FinishCell(Cell, Result);
	return Result;
}

int32 FLotGrid::GetCellDone(int32 Cell) const
{
	return (Cell >= 0 && Cell < AsphaltLot::NumCells) ? CellDone[Cell] : 0;
}

int32 FLotGrid::GetCellRaw(int32 Cell) const
{
	return (Cell >= 0 && Cell < AsphaltLot::NumCells) ? CellRaw[Cell] : 0;
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
