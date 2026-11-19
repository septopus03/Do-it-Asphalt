// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexCoord.h"

enum class EAsphaltCellKind : uint8
{
	Lot,
	Road,
};

/** One cell of the map: either part of the asphalt lot or part of a road leading to it. */
struct FAsphaltCell
{
	FHexCoord Coord;
	EAsphaltCellKind Kind = EAsphaltCellKind::Lot;
	/** Index of the road (0..5) for road cells, INDEX_NONE for lot cells. */
	int32 RoadIndex = INDEX_NONE;
};

/**
 * The fixed map of 499 cells: a hexagonal lot of radius 12 (469 cells) plus six roads of five cells each.
 * Cell order is binding because saved games store cell indices:
 *  - index 0 is {0,0}; then rings k = 1..12, each starting at Directions[4] * k and walking k steps
 *    in Directions[0..5] in turn;
 *  - then the roads, index 469 + 5 * i + j.
 */
class ASPHALTCORE_API FHexMap
{
public:
	static constexpr int32 LotRadius = 12;
	static constexpr int32 NumLotCells = 469;
	static constexpr int32 NumRoads = 6;
	static constexpr int32 CellsPerRoad = 5;
	static constexpr int32 NumCells = 499;

	FHexMap();

	int32 Num() const;
	const FAsphaltCell& GetCell(int32 Index) const;

	/** Index of the cell at the coordinate, or INDEX_NONE if the coordinate is not on the map. */
	int32 FindCell(FHexCoord Coord) const;

	/** The five cell indices of a road, in map order. */
	TConstArrayView<int32> GetRoadCells(int32 Road) const;

	/** Lot-edge cell that the road is attached to: (Dir[i] + Dir[(i+1)%6]) * 6. */
	static FHexCoord RoadAnchor(int32 Road);

	/** Unit vector pointing from the lot centre out through the road. */
	static FVector2D RoadDirection(int32 Road);

private:
	TArray<FAsphaltCell> Cells;
	TMap<FHexCoord, int32> CellIndexByCoord;
	TArray<int32> RoadCellIndices;
};
