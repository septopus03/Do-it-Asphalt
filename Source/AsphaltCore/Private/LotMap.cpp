// Copyright Do it Asphalt. All Rights Reserved.

#include "LotMap.h"

namespace
{
	/** Inclusive rectangle of cells (columns x rows) of one entry segment. */
	struct FEntryRect
	{
		int32 ColMin;
		int32 ColMax;
		int32 RowMin;
		int32 RowMax;
		FVector2D Centre;
		FVector2D Outward;
	};

	const FEntryRect Entries[AsphaltLot::NumEntries] = {
		{97, 102, 0, 1, FVector2D(0.0, -10000.0), FVector2D(0.0, -1.0)},
		{152, 157, 98, 99, FVector2D(5500.0, 0.0), FVector2D(0.0, 1.0)},
		{98, 99, 152, 157, FVector2D(0.0, 5500.0), FVector2D(1.0, 0.0)},
		{0, 1, 97, 102, FVector2D(-10000.0, 0.0), FVector2D(-1.0, 0.0)},
		{47, 52, 198, 199, FVector2D(-5000.0, 10000.0), FVector2D(0.0, 1.0)},
		{198, 199, 47, 52, FVector2D(10000.0, -5000.0), FVector2D(1.0, 0.0)},
	};
}

namespace AsphaltLot
{
	bool IsLot(int32 Col, int32 Row)
	{
		return Col >= 0 && Col < Side && Row >= 0 && Row < Side && !(Col >= Cut && Row >= Cut);
	}

	int32 CellIndex(int32 Col, int32 Row)
	{
		if (!IsLot(Col, Row))
		{
			return INDEX_NONE;
		}
		return Row < Cut ? Row * Side + Col : Cut * Side + (Row - Cut) * Cut + Col;
	}

	FIntPoint CellCoord(int32 Cell)
	{
		if (Cell < 0 || Cell >= NumCells)
		{
			return FIntPoint(INDEX_NONE, INDEX_NONE);
		}
		if (Cell < Cut * Side)
		{
			return FIntPoint(Cell % Side, Cell / Side);
		}
		const int32 Rest = Cell - Cut * Side;
		return FIntPoint(Rest % Cut, Cut + Rest / Cut);
	}

	int32 CellAt(FVector2D WorldCm)
	{
		// Range check first, so NaN and huge values never reach the float-to-int conversion.
		if (!(WorldCm.X >= -HalfExtentCm && WorldCm.X < HalfExtentCm && WorldCm.Y >= -HalfExtentCm && WorldCm.Y < HalfExtentCm))
		{
			return INDEX_NONE;
		}
		const int32 Col = FMath::FloorToInt32((WorldCm.X + HalfExtentCm) / CellCm);
		const int32 Row = FMath::FloorToInt32((WorldCm.Y + HalfExtentCm) / CellCm);
		return CellIndex(Col, Row);
	}

	FVector2D CellCentre(int32 Cell)
	{
		const FIntPoint Coord = CellCoord(Cell);
		if (Coord.X == INDEX_NONE)
		{
			return FVector2D::ZeroVector;
		}
		return FVector2D(-HalfExtentCm + (Coord.X + 0.5) * CellCm, -HalfExtentCm + (Coord.Y + 0.5) * CellCm);
	}

	int32 SectorOf(int32 Cell)
	{
		const FIntPoint Coord = CellCoord(Cell);
		if (Coord.X == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		const int32 SCol = Coord.X / SectorSide;
		const int32 SRow = Coord.Y / SectorSide;
		return SRow < Cut / SectorSide ? SRow * (Side / SectorSide) + SCol : 50 + (SRow - Cut / SectorSide) * (Cut / SectorSide) + SCol;
	}

	int32 SectorAt(FVector2D WorldCm)
	{
		return SectorOf(CellAt(WorldCm));
	}

	/** Grid position of the first (top-left) cell of a sector, or {INDEX_NONE, INDEX_NONE}. */
	static FIntPoint SectorOrigin(int32 Sector)
	{
		if (Sector < 0 || Sector >= NumSectors)
		{
			return FIntPoint(INDEX_NONE, INDEX_NONE);
		}
		const int32 WideSectors = (Side / SectorSide) * (Cut / SectorSide); // 50 sectors in the wide part
		if (Sector < WideSectors)
		{
			return FIntPoint((Sector % (Side / SectorSide)) * SectorSide, (Sector / (Side / SectorSide)) * SectorSide);
		}
		const int32 Rest = Sector - WideSectors;
		return FIntPoint((Rest % (Cut / SectorSide)) * SectorSide, Cut + (Rest / (Cut / SectorSide)) * SectorSide);
	}

	int32 SectorCell(int32 Sector, int32 Step)
	{
		const FIntPoint Origin = SectorOrigin(Sector);
		if (Origin.X == INDEX_NONE || Step < 0 || Step >= CellsPerSector)
		{
			return INDEX_NONE;
		}
		const int32 Row = Step / SectorSide;
		const int32 Offset = Step % SectorSide;
		const int32 Col = (Row % 2 == 0) ? Offset : SectorSide - 1 - Offset;
		return CellIndex(Origin.X + Col, Origin.Y + Row);
	}

	FVector2D SectorCentre(int32 Sector)
	{
		const FIntPoint Origin = SectorOrigin(Sector);
		if (Origin.X == INDEX_NONE)
		{
			return FVector2D::ZeroVector;
		}
		const double Half = SectorSide * 0.5 * CellCm;
		return FVector2D(-HalfExtentCm + Origin.X * CellCm + Half, -HalfExtentCm + Origin.Y * CellCm + Half);
	}

	int32 EntryCell(int32 Entry, int32 I)
	{
		if (Entry < 0 || Entry >= NumEntries || I < 0 || I >= CellsPerEntry)
		{
			return INDEX_NONE;
		}
		const FEntryRect& Rect = Entries[Entry];
		const int32 Width = Rect.ColMax - Rect.ColMin + 1;
		return CellIndex(Rect.ColMin + I % Width, Rect.RowMin + I / Width);
	}

	int32 EntryOf(int32 Cell)
	{
		const FIntPoint Coord = CellCoord(Cell);
		if (Coord.X == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		for (int32 Entry = 0; Entry < NumEntries; ++Entry)
		{
			const FEntryRect& Rect = Entries[Entry];
			if (Coord.X >= Rect.ColMin && Coord.X <= Rect.ColMax && Coord.Y >= Rect.RowMin && Coord.Y <= Rect.RowMax)
			{
				return Entry;
			}
		}
		return INDEX_NONE;
	}

	FVector2D EntryCentre(int32 Entry)
	{
		return (Entry >= 0 && Entry < NumEntries) ? Entries[Entry].Centre : FVector2D::ZeroVector;
	}

	FVector2D EntryOutward(int32 Entry)
	{
		return (Entry >= 0 && Entry < NumEntries) ? Entries[Entry].Outward : FVector2D::ZeroVector;
	}
}
