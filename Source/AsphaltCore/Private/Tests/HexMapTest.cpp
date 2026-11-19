// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltLayout.h"
#include "HexMap.h"

DIA_TEST(FMapCellsTest, "DoItAsphalt.Core.Map.Cells")
DIA_TEST(FMapCellOrderTest, "DoItAsphalt.Core.Map.CellOrder")
DIA_TEST(FMapLookupTest, "DoItAsphalt.Core.Map.Lookup")
DIA_TEST(FMapRoadsTest, "DoItAsphalt.Core.Map.Roads")
DIA_TEST(FMapLayoutTest, "DoItAsphalt.Core.Map.Layout")

bool FMapCellsTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	TestEqual(TEXT("num"), Map.Num(), 499);
	TestTrue(TEXT("center"), Map.GetCell(0).Coord == FHexCoord{0, 0});

	TSet<FHexCoord> Seen;
	int32 BadLots = 0;
	int32 BadRoads = 0;
	for (int32 Index = 0; Index < Map.Num(); ++Index)
	{
		const FAsphaltCell& Cell = Map.GetCell(Index);
		Seen.Add(Cell.Coord);
		if (Index < FHexMap::NumLotCells)
		{
			if (Cell.Kind != EAsphaltCellKind::Lot || Cell.RoadIndex != INDEX_NONE
				|| AsphaltHex::Distance(Cell.Coord, FHexCoord{0, 0}) > FHexMap::LotRadius)
			{
				++BadLots;
			}
		}
		else if (Cell.Kind != EAsphaltCellKind::Road || Cell.RoadIndex != (Index - FHexMap::NumLotCells) / FHexMap::CellsPerRoad)
		{
			++BadRoads;
		}
	}
	TestEqual(TEXT("bad lot cells"), BadLots, 0);
	TestEqual(TEXT("bad road cells"), BadRoads, 0);
	TestEqual(TEXT("unique coords"), Seen.Num(), 499);
	return true;
}

bool FMapCellOrderTest::RunTest(const FString& Parameters)
{
	// Saved games store cell indices, so the order of the cells is part of the file format.
	FHexMap Map;
	TestTrue(TEXT("cell 0"), Map.GetCell(0).Coord == FHexCoord{0, 0});
	TestTrue(TEXT("cell 1 is Directions[4]"), Map.GetCell(1).Coord == AsphaltHex::Directions[4]);
	TestTrue(TEXT("cell 1 literal"), Map.GetCell(1).Coord == FHexCoord{-1, 1});
	// Ring 1 walks Directions[4] -> +Directions[0] -> ... and ends one step before its start.
	TestTrue(TEXT("cell 2"), Map.GetCell(2).Coord == FHexCoord{0, 1});
	TestTrue(TEXT("cell 6 is the last of ring 1"), Map.GetCell(6).Coord == FHexCoord{-1, 0});
	TestTrue(TEXT("cell 7 starts ring 2"), Map.GetCell(7).Coord == FHexCoord{-2, 2});
	TestTrue(TEXT("cell 468 is the last lot cell"), Map.GetCell(468).Coord == FHexCoord{-12, 11});
	TestTrue(TEXT("cell 469 is the first road cell"), Map.GetCell(469).Coord == FHexCoord{13, -6});
	TestTrue(TEXT("cell 498 is the last road cell"), Map.GetCell(498).Kind == EAsphaltCellKind::Road && Map.GetCell(498).RoadIndex == 5);
	return true;
}

bool FMapLookupTest::RunTest(const FString& Parameters)
{
	FHexMap Map;
	TestTrue(TEXT("edge is lot"), Map.FindCell({12, -6}) >= 0 && Map.FindCell({12, -6}) < 469);
	TestEqual(TEXT("road0 first"), Map.FindCell({13, -6}), 469);
	TestEqual(TEXT("off map"), Map.FindCell({13, 0}), (int32)INDEX_NONE);

	int32 BadLookups = 0;
	for (int32 Index = 0; Index < Map.Num(); ++Index)
	{
		if (Map.FindCell(Map.GetCell(Index).Coord) != Index)
		{
			++BadLookups;
		}
	}
	TestEqual(TEXT("FindCell inverts GetCell"), BadLookups, 0);
	return true;
}

bool FMapRoadsTest::RunTest(const FString& Parameters)
{
	FHexMap Map;

	const FHexCoord Road0[5] = {{13, -6}, {13, -7}, {14, -7}, {14, -6}, {14, -8}};
	const TConstArrayView<int32> Cells0 = Map.GetRoadCells(0);
	TestEqual(TEXT("road0 count"), Cells0.Num(), 5);
	for (int32 J = 0; J < Cells0.Num() && J < 5; ++J)
	{
		TestTrue(*FString::Printf(TEXT("road0 cell %d"), J), Map.GetCell(Cells0[J]).Coord == Road0[J]);
	}

	const FHexCoord Road2[5] = {{-6, -7}, {-7, -6}, {-7, -7}, {-6, -8}, {-8, -6}};
	const TConstArrayView<int32> Cells2 = Map.GetRoadCells(2);
	TestEqual(TEXT("road2 count"), Cells2.Num(), 5);
	for (int32 J = 0; J < Cells2.Num() && J < 5; ++J)
	{
		TestTrue(*FString::Printf(TEXT("road2 cell %d"), J), Map.GetCell(Cells2[J]).Coord == Road2[J]);
	}

	TestTrue(TEXT("dir0"), FHexMap::RoadDirection(0).Equals(FVector2D(0.8660, -0.5), 0.001));
	for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
	{
		TestTrue(*FString::Printf(TEXT("dir %d is unit"), Road), FMath::IsNearlyEqual(FHexMap::RoadDirection(Road).Size(), 1.0, 0.001));
		TestTrue(*FString::Printf(TEXT("anchor %d is outside lot"), Road), AsphaltHex::Distance(FHexMap::RoadAnchor(Road), FHexCoord{0, 0}) == 12);
		for (int32 CellIndex : Map.GetRoadCells(Road))
		{
			TestEqual(*FString::Printf(TEXT("road %d cell kind"), Road), Map.GetCell(CellIndex).RoadIndex, Road);
		}
	}
	return true;
}

bool FMapLayoutTest::RunTest(const FString& Parameters)
{
	FHexMap Map;

	// 16 points on the refill circle must be off the map (the map is far from the plant side).
	for (int32 I = 0; I < 16; ++I)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * I / 16.0;
		const FVector2D P = AsphaltLayout::RefillCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * AsphaltLayout::RefillRadiusCm;
		TestEqual(*FString::Printf(TEXT("refill point %d off map"), I), Map.FindCell(AsphaltHex::FromWorld(P)), (int32)INDEX_NONE);
	}

	for (int32 I = 0; I < FHexMap::NumRoads; ++I)
	{
		TestEqual(*FString::Printf(TEXT("kiosk %d off map"), I), Map.FindCell(AsphaltHex::FromWorld(AsphaltLayout::KioskPos(I))), (int32)INDEX_NONE);
		TestEqual(*FString::Printf(TEXT("gate %d off map"), I), Map.FindCell(AsphaltHex::FromWorld(AsphaltLayout::GatePos(I))), (int32)INDEX_NONE);
		TestTrue(*FString::Printf(TEXT("kiosk %d inside fence"), I), AsphaltLayout::KioskPos(I).Size() < AsphaltLayout::SiteRadiusCm);
		TestTrue(*FString::Printf(TEXT("spawn %d inside fence"), I), AsphaltLayout::SpecialSpawn(I).Size() < AsphaltLayout::SiteRadiusCm);
		TestEqual(*FString::Printf(TEXT("gate %d on fence"), I), AsphaltLayout::GatePos(I).Size(), AsphaltLayout::SiteRadiusCm, 0.01);

		const int32 SpawnCell = Map.FindCell(AsphaltHex::FromWorld(AsphaltLayout::SpecialSpawn(I)));
		TestTrue(*FString::Printf(TEXT("spawn %d is a lot cell"), I), SpawnCell != INDEX_NONE && Map.GetCell(SpawnCell).Kind == EAsphaltCellKind::Lot);
	}

	TestEqual(TEXT("exit off map"), Map.FindCell(AsphaltHex::FromWorld(AsphaltLayout::ExitTrigger())), (int32)INDEX_NONE);
	TestEqual(TEXT("exit distance"), AsphaltLayout::ExitTrigger().Size(), 2750.0, 0.01);

	TestTrue(TEXT("kiosk3"), AsphaltLayout::KioskPos(3).Equals(FVector2D(-1337, 518), 1.0));
	TestTrue(TEXT("gate5"), AsphaltLayout::GatePos(5).Equals(FVector2D(2252, 1300), 1.0));
	TestTrue(TEXT("spawn0"), AsphaltLayout::SpecialSpawn(0).Equals(FVector2D(707, -408), 1.0));
	return true;
}
