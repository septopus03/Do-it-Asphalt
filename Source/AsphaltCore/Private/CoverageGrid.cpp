// Copyright Do it Asphalt. All Rights Reserved.

#include "CoverageGrid.h"
#include "AsphaltBalance.h"

namespace
{
	constexpr int32 NumTexels = FCoverageGrid::Size * FCoverageGrid::Size;
	constexpr int32 NumBytes = NumTexels / 8;
	/** Tolerance that keeps a budget of exactly N texel prices from losing the last texel to rounding. */
	constexpr double LitreEpsilon = 1e-9;

	int32 TexelIndexOf(int32 X, int32 Y)
	{
		return Y * FCoverageGrid::Size + X;
	}

	int32 ThresholdTexelsFor(int32 TexelCount)
	{
		return FMath::Clamp(FMath::CeilToInt(AsphaltBalance::CompletionThreshold * TexelCount - 1e-9), 1, TexelCount);
	}
}

FCoverageGrid::FCoverageGrid(const FHexMap& Map)
	: NumCells(Map.Num())
{
	TexelCells.Init(-1, NumTexels);
	Texels.SetNumZeroed(NumTexels);
	CellTexelCount.SetNumZeroed(NumCells);
	CellCovered.SetNumZeroed(NumCells);
	CellThresholdTexels.SetNumZeroed(NumCells);
	CellComplete.SetNumZeroed(NumCells);
	CellBounds.Init(FIntRect(Size, Size, 0, 0), NumCells);

	for (int32 Y = 0; Y < Size; ++Y)
	{
		const double Wy = -HalfExtentCm + (Y + 0.5) * TexelCm;
		for (int32 X = 0; X < Size; ++X)
		{
			const double Wx = -HalfExtentCm + (X + 0.5) * TexelCm;
			const int32 Cell = Map.FindCell(AsphaltHex::FromWorld(FVector2D(Wx, Wy)));
			if (Cell == INDEX_NONE)
			{
				continue;
			}
			TexelCells[TexelIndexOf(X, Y)] = static_cast<int16>(Cell);
			++CellTexelCount[Cell];
			FIntRect& Bounds = CellBounds[Cell];
			Bounds.Min.X = FMath::Min(Bounds.Min.X, X);
			Bounds.Min.Y = FMath::Min(Bounds.Min.Y, Y);
			Bounds.Max.X = FMath::Max(Bounds.Max.X, X + 1);
			Bounds.Max.Y = FMath::Max(Bounds.Max.Y, Y + 1);
		}
	}

	CellTexelStart.SetNumZeroed(NumCells + 1);
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		CellTexelStart[Cell + 1] = CellTexelStart[Cell] + CellTexelCount[Cell];
		CellThresholdTexels[Cell] = ThresholdTexelsFor(CellTexelCount[Cell]);
	}

	// Texel indices grouped by cell; scanning in index order keeps every group in row-major order.
	CellTexelIndices.SetNumUninitialized(CellTexelStart[NumCells]);
	TArray<int32> Fill;
	Fill.SetNumZeroed(NumCells);
	for (int32 Index = 0; Index < NumTexels; ++Index)
	{
		const int32 Cell = TexelCells[Index];
		if (Cell >= 0)
		{
			CellTexelIndices[CellTexelStart[Cell] + Fill[Cell]++] = Index;
		}
	}
}

double FCoverageGrid::TexelCost(int32 Cell) const
{
	return AsphaltBalance::LitresPerCell / CellTexelCount[Cell];
}

void FCoverageGrid::ExpandDirty(int32 MinX, int32 MinY, int32 MaxXInclusive, int32 MaxYInclusive)
{
	if (!bDirty)
	{
		Dirty = FIntRect(MinX, MinY, MaxXInclusive + 1, MaxYInclusive + 1);
		bDirty = true;
		return;
	}
	Dirty.Min.X = FMath::Min(Dirty.Min.X, MinX);
	Dirty.Min.Y = FMath::Min(Dirty.Min.Y, MinY);
	Dirty.Max.X = FMath::Max(Dirty.Max.X, MaxXInclusive + 1);
	Dirty.Max.Y = FMath::Max(Dirty.Max.Y, MaxYInclusive + 1);
}

bool FCoverageGrid::PaintTexel(int32 TexelIndex, int32 Cell)
{
	Texels[TexelIndex] = 255;
	++CellCovered[Cell];
	const int32 X = TexelIndex % Size;
	const int32 Y = TexelIndex / Size;
	ExpandDirty(X, Y, X, Y);
	return CellCovered[Cell] >= CellThresholdTexels[Cell];
}

void FCoverageGrid::FinishCell(int32 Cell, FPourResult& Result)
{
	for (int32 I = CellTexelStart[Cell]; I < CellTexelStart[Cell + 1]; ++I)
	{
		Texels[CellTexelIndices[I]] = 255;
	}
	CellCovered[Cell] = CellTexelCount[Cell];
	CellComplete[Cell] = 1;
	++CompleteCount;
	const FIntRect& Bounds = CellBounds[Cell];
	ExpandDirty(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X - 1, Bounds.Max.Y - 1);
	Result.CompletedCells.Add(Cell);
}

FPourResult FCoverageGrid::Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres)
{
	FPourResult Result;
	if (!(MaxLitres > 0.0) || !(WidthCm >= 0.0))
	{
		return Result;
	}

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
	const double Pad = AsphaltBalance::StampPadCm;
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
		return FMath::Floor((World + HalfExtentCm) / TexelCm);
	};
	const double RawMinX = ToTexel(Min.X);
	const double RawMaxX = ToTexel(Max.X);
	const double RawMinY = ToTexel(Min.Y);
	const double RawMaxY = ToTexel(Max.Y);
	if (RawMaxX < 0.0 || RawMinX > Size - 1 || RawMaxY < 0.0 || RawMinY > Size - 1)
	{
		return Result;
	}
	const int32 X0 = static_cast<int32>(FMath::Max(RawMinX, 0.0));
	const int32 X1 = static_cast<int32>(FMath::Min(RawMaxX, (double)(Size - 1)));
	const int32 Y0 = static_cast<int32>(FMath::Max(RawMinY, 0.0));
	const int32 Y1 = static_cast<int32>(FMath::Min(RawMaxY, (double)(Size - 1)));

	const double AxisLength = Length;
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		const double Wy = -HalfExtentCm + (Y + 0.5) * TexelCm;
		for (int32 X = X0; X <= X1; ++X)
		{
			const int32 Index = TexelIndexOf(X, Y);
			const int32 Cell = TexelCells[Index];
			if (Cell < 0 || Texels[Index] != 0)
			{
				continue; // off the map or already covered: free
			}
			const FVector2D Offset(-HalfExtentCm + (X + 0.5) * TexelCm - A.X, Wy - A.Y);
			const double Along = FVector2D::DotProduct(Offset, Axis);
			const double Across = FVector2D::DotProduct(Offset, Normal);
			if (Along < -Pad || Along > AxisLength + Pad || FMath::Abs(Across) > HalfWidth)
			{
				continue;
			}

			const double Cost = TexelCost(Cell);
			if (MaxLitres - Result.LitresUsed + LitreEpsilon < Cost)
			{
				Result.LitresUsed = FMath::Min(Result.LitresUsed, MaxLitres);
				return Result;
			}
			Result.LitresUsed += Cost;
			if (PaintTexel(Index, Cell))
			{
				FinishCell(Cell, Result);
			}
		}
	}
	Result.LitresUsed = FMath::Min(Result.LitresUsed, MaxLitres);
	return Result;
}

FPourResult FCoverageGrid::DepositIntoCell(int32 Cell, double MaxLitres)
{
	FPourResult Result;
	if (Cell < 0 || Cell >= NumCells || CellComplete[Cell] || !(MaxLitres > 0.0))
	{
		return Result;
	}

	const double Cost = TexelCost(Cell);
	for (int32 I = CellTexelStart[Cell]; I < CellTexelStart[Cell + 1]; ++I)
	{
		const int32 Index = CellTexelIndices[I];
		if (Texels[Index] != 0)
		{
			continue;
		}
		if (MaxLitres - Result.LitresUsed + LitreEpsilon < Cost)
		{
			break;
		}
		Result.LitresUsed += Cost;
		if (PaintTexel(Index, Cell))
		{
			FinishCell(Cell, Result);
			break;
		}
	}
	Result.LitresUsed = FMath::Min(Result.LitresUsed, MaxLitres);
	return Result;
}

FPourResult FCoverageGrid::CompleteCell(int32 Cell)
{
	FPourResult Result;
	if (Cell < 0 || Cell >= NumCells || CellComplete[Cell])
	{
		return Result;
	}
	Result.LitresUsed = (CellTexelCount[Cell] - CellCovered[Cell]) * TexelCost(Cell);
	FinishCell(Cell, Result);
	return Result;
}

int32 FCoverageGrid::GetCellTexelCount(int32 Cell) const
{
	return (Cell >= 0 && Cell < NumCells) ? CellTexelCount[Cell] : 0;
}

double FCoverageGrid::GetCellFill(int32 Cell) const
{
	if (Cell < 0 || Cell >= NumCells || CellTexelCount[Cell] == 0)
	{
		return 0.0;
	}
	return static_cast<double>(CellCovered[Cell]) / CellTexelCount[Cell];
}

bool FCoverageGrid::IsCellComplete(int32 Cell) const
{
	return Cell >= 0 && Cell < NumCells && CellComplete[Cell] != 0;
}

int32 FCoverageGrid::NumCompleteCells() const
{
	return CompleteCount;
}

int32 FCoverageGrid::GetTexelCell(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= Size || Y >= Size)
	{
		return INDEX_NONE;
	}
	const int32 Cell = TexelCells[TexelIndexOf(X, Y)];
	return Cell >= 0 ? Cell : INDEX_NONE;
}

TConstArrayView<uint8> FCoverageGrid::GetTexels() const
{
	return Texels;
}

bool FCoverageGrid::ConsumeDirtyRect(FIntRect& Out)
{
	if (!bDirty)
	{
		return false;
	}
	Out = Dirty;
	bDirty = false;
	return true;
}

void FCoverageGrid::WriteBits(TArray<uint8>& Out) const
{
	Out.Reset();
	Out.SetNumZeroed(NumBytes);
	for (int32 Index = 0; Index < NumTexels; ++Index)
	{
		if (Texels[Index] != 0)
		{
			Out[Index >> 3] |= static_cast<uint8>(1u << (Index & 7));
		}
	}
}

bool FCoverageGrid::ReadBits(TConstArrayView<uint8> In)
{
	if (In.Num() != NumBytes)
	{
		return false;
	}

	FMemory::Memzero(Texels.GetData(), Texels.Num());
	FMemory::Memzero(CellCovered.GetData(), CellCovered.Num() * sizeof(int32));
	FMemory::Memzero(CellComplete.GetData(), CellComplete.Num());
	CompleteCount = 0;

	// Bits on texels that belong to no cell are ignored.
	for (int32 Index = 0; Index < NumTexels; ++Index)
	{
		const int32 Cell = TexelCells[Index];
		if (Cell >= 0 && (In[Index >> 3] & (1u << (Index & 7))) != 0)
		{
			Texels[Index] = 255;
			++CellCovered[Cell];
		}
	}

	// A cell that is at or above the threshold is complete, so its remainder must be filled.
	FPourResult Ignored;
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		if (CellCovered[Cell] >= CellThresholdTexels[Cell])
		{
			FinishCell(Cell, Ignored);
		}
	}

	bDirty = true;
	Dirty = FIntRect(0, 0, Size, Size);
	return true;
}
