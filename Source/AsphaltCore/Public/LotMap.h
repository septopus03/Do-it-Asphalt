// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The fixed geometry of the L-shaped car park: a 200 x 200 grid of 1 x 1 m cells minus the shopping-centre
 * quadrant (Col >= 100 && Row >= 100), 30 000 cells in all. Index and sector orders are binding because
 * saved games store them:
 *  - cell index: Row < 100 -> Row * 200 + Col; otherwise 20000 + (Row - 100) * 100 + Col;
 *  - world: X, Y in [-10000, 10000] cm, Col = floor((X + 10000) / 100), Row likewise from Y;
 *  - sector: 20 x 20 cells; SRow < 5 -> SRow * 10 + SCol, otherwise 50 + (SRow - 5) * 5 + SCol;
 *  - six entry segments of 12 cells each, where the fleet can enter the car park.
 * Every function returns INDEX_NONE (or a zero vector) for arguments outside the car park.
 */
namespace AsphaltLot
{
	inline constexpr int32 Side = 200;
	inline constexpr int32 Cut = 100;
	inline constexpr int32 NumCells = 30000;
	inline constexpr double CellCm = 100.0;
	inline constexpr double HalfExtentCm = 10000.0;
	inline constexpr int32 SectorSide = 20;
	inline constexpr int32 NumSectors = 75;
	inline constexpr int32 CellsPerSector = 400;
	inline constexpr int32 NumEntries = 6;
	inline constexpr int32 CellsPerEntry = 12;

	/** True if the grid position is a car-park cell: inside 0..199 and not in the shopping-centre quadrant. */
	ASPHALTCORE_API bool IsLot(int32 Col, int32 Row);

	/** Index of the cell, or INDEX_NONE outside the car park. */
	ASPHALTCORE_API int32 CellIndex(int32 Col, int32 Row);

	/** Grid position (Col, Row) of a cell index; {INDEX_NONE, INDEX_NONE} for an invalid index. */
	ASPHALTCORE_API FIntPoint CellCoord(int32 Cell);

	/** Cell containing the world point (cm), or INDEX_NONE outside the car park. */
	ASPHALTCORE_API int32 CellAt(FVector2D WorldCm);

	/** World position (cm) of the cell centre. */
	ASPHALTCORE_API FVector2D CellCentre(int32 Cell);

	/** Sector (0..74) the cell belongs to, or INDEX_NONE for an invalid cell. */
	ASPHALTCORE_API int32 SectorOf(int32 Cell);

	/** Sector containing the world point, or INDEX_NONE outside the car park. */
	ASPHALTCORE_API int32 SectorAt(FVector2D WorldCm);

	/** The Step-th cell (0..399) of a sector, row by row, every second row walked the opposite way. */
	ASPHALTCORE_API int32 SectorCell(int32 Sector, int32 Step);

	/** World position (cm) of the sector centre. */
	ASPHALTCORE_API FVector2D SectorCentre(int32 Sector);

	/** The I-th cell (0..11) of an entry segment, row by row inside its rectangle. */
	ASPHALTCORE_API int32 EntryCell(int32 Entry, int32 I);

	/** Entry segment the cell belongs to, or INDEX_NONE if it belongs to none. */
	ASPHALTCORE_API int32 EntryOf(int32 Cell);

	/** Point on the car-park outline (cm) where the entry segment lies. */
	ASPHALTCORE_API FVector2D EntryCentre(int32 Entry);

	/** Unit vector pointing out of the car park through the entry segment. */
	ASPHALTCORE_API FVector2D EntryOutward(int32 Entry);
}
