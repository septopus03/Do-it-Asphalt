// Copyright Do it Asphalt. All Rights Reserved.

#include "HexMap.h"

FHexMap::FHexMap()
{
	using namespace AsphaltHex;

	Cells.Reserve(NumCells);
	CellIndexByCoord.Reserve(NumCells);

	const auto AddCell = [this](FHexCoord Coord, EAsphaltCellKind Kind, int32 RoadIndex)
	{
		CellIndexByCoord.Add(Coord, Cells.Num());
		Cells.Add(FAsphaltCell{Coord, Kind, RoadIndex});
	};

	// Lot: the centre, then ring after ring. Each ring starts at Directions[4] * k and walks k steps in every direction.
	AddCell(FHexCoord{0, 0}, EAsphaltCellKind::Lot, INDEX_NONE);
	for (int32 K = 1; K <= LotRadius; ++K)
	{
		FHexCoord Cursor = Directions[4] * K;
		for (int32 Side = 0; Side < 6; ++Side)
		{
			for (int32 Step = 0; Step < K; ++Step)
			{
				AddCell(Cursor, EAsphaltCellKind::Lot, INDEX_NONE);
				Cursor = Cursor + Directions[Side];
			}
		}
	}
	check(Cells.Num() == NumLotCells);

	// Roads: five cells each, attached just outside the lot edge at the anchor.
	RoadCellIndices.Reserve(NumRoads * CellsPerRoad);
	for (int32 Road = 0; Road < NumRoads; ++Road)
	{
		const FHexCoord A = Directions[Road];
		const FHexCoord B = Directions[(Road + 1) % 6];
		const FHexCoord M = RoadAnchor(Road);
		const FHexCoord RoadCoords[CellsPerRoad] = {M + A, M + B, M + A + B, M + A * 2, M + B * 2};
		for (const FHexCoord& Coord : RoadCoords)
		{
			RoadCellIndices.Add(Cells.Num());
			AddCell(Coord, EAsphaltCellKind::Road, Road);
		}
	}
	check(Cells.Num() == NumCells);
}

int32 FHexMap::Num() const
{
	return Cells.Num();
}

const FAsphaltCell& FHexMap::GetCell(int32 Index) const
{
	return Cells[Index];
}

int32 FHexMap::FindCell(FHexCoord Coord) const
{
	const int32* Found = CellIndexByCoord.Find(Coord);
	return Found ? *Found : INDEX_NONE;
}

TConstArrayView<int32> FHexMap::GetRoadCells(int32 Road) const
{
	check(Road >= 0 && Road < NumRoads);
	return TConstArrayView<int32>(RoadCellIndices.GetData() + Road * CellsPerRoad, CellsPerRoad);
}

FHexCoord FHexMap::RoadAnchor(int32 Road)
{
	return (AsphaltHex::Directions[Road] + AsphaltHex::Directions[(Road + 1) % 6]) * 6;
}

FVector2D FHexMap::RoadDirection(int32 Road)
{
	return AsphaltHex::ToWorld(RoadAnchor(Road)).GetSafeNormal();
}
