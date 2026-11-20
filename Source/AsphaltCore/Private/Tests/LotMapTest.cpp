// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotMap.h"

DIA_TEST(FLotMapIndexingTest, "DoItAsphalt.Lot.Map.Indexing")
DIA_TEST(FLotMapWorldTest, "DoItAsphalt.Lot.Map.World")
DIA_TEST(FLotMapSectorsTest, "DoItAsphalt.Lot.Map.Sectors")
DIA_TEST(FLotMapEntriesTest, "DoItAsphalt.Lot.Map.Entries")

bool FLotMapIndexingTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltLot;

	TestEqual(TEXT("first"), CellIndex(0, 0), 0);
	TestEqual(TEXT("end of wide part"), CellIndex(199, 99), 19999);
	TestEqual(TEXT("start of narrow part"), CellIndex(0, 100), 20000);
	TestEqual(TEXT("last"), CellIndex(99, 199), 29999);
	TestEqual(TEXT("shopping centre"), CellIndex(100, 100), (int32)INDEX_NONE);
	TestEqual(TEXT("left of grid"), CellIndex(-1, 0), (int32)INDEX_NONE);
	TestEqual(TEXT("right of grid"), CellIndex(200, 0), (int32)INDEX_NONE);
	TestEqual(TEXT("below grid"), CellIndex(0, 200), (int32)INDEX_NONE);

	int32 Bad = 0;
	for (int32 Index = 0; Index < NumCells; ++Index)
	{
		const FIntPoint Coord = CellCoord(Index);
		if (CellIndex(Coord.X, Coord.Y) != Index)
		{
			++Bad;
		}
	}
	TestEqual(TEXT("round trip"), Bad, 0);
	TestEqual(TEXT("bad cell index"), CellCoord(NumCells), FIntPoint(INDEX_NONE, INDEX_NONE));
	return true;
}

bool FLotMapWorldTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltLot;

	TestEqual(TEXT("corner cell"), CellAt(FVector2D(-9950.0, -9950.0)), 0);
	TestEqual(TEXT("shopping centre"), CellAt(FVector2D(50.0, 50.0)), (int32)INDEX_NONE);
	TestEqual(TEXT("outside left"), CellAt(FVector2D(-10001.0, 0.0)), (int32)INDEX_NONE);
	TestEqual(TEXT("outside below"), CellAt(FVector2D(0.0, -10500.0)), (int32)INDEX_NONE);
	TestTrue(TEXT("centre of cell 0"), CellCentre(0).Equals(FVector2D(-9950.0, -9950.0)));
	TestTrue(TEXT("bad cell has zero centre"), CellCentre(INDEX_NONE).IsZero());

	int32 Bad = 0;
	for (int32 Index = 0; Index < NumCells; ++Index)
	{
		if (CellAt(CellCentre(Index)) != Index)
		{
			++Bad;
		}
	}
	TestEqual(TEXT("centre round trip"), Bad, 0);
	return true;
}

bool FLotMapSectorsTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltLot;

	TArray<int32> Counts;
	Counts.SetNumZeroed(NumSectors);
	int32 BadSector = 0;
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		const int32 Sector = SectorOf(Cell);
		if (Sector < 0 || Sector >= NumSectors)
		{
			++BadSector;
			continue;
		}
		++Counts[Sector];
	}
	TestEqual(TEXT("every cell has a sector"), BadSector, 0);

	int32 BadCount = 0;
	int32 BadWalk = 0;
	for (int32 Sector = 0; Sector < NumSectors; ++Sector)
	{
		if (Counts[Sector] != CellsPerSector)
		{
			++BadCount;
		}
		TSet<int32> Seen;
		for (int32 Step = 0; Step < CellsPerSector; ++Step)
		{
			const int32 Cell = SectorCell(Sector, Step);
			if (Cell == INDEX_NONE || SectorOf(Cell) != Sector)
			{
				++BadWalk;
			}
			Seen.Add(Cell);
		}
		if (Seen.Num() != CellsPerSector)
		{
			++BadWalk;
		}
		if (SectorAt(SectorCentre(Sector)) != Sector)
		{
			++BadWalk;
		}
	}
	TestEqual(TEXT("400 cells per sector"), BadCount, 0);
	TestEqual(TEXT("sector walk covers each cell once"), BadWalk, 0);

	TestEqual(TEXT("step 0"), SectorCell(0, 0), CellIndex(0, 0));
	TestEqual(TEXT("end of row 0"), SectorCell(0, 19), CellIndex(19, 0));
	TestEqual(TEXT("row 1 walks back"), SectorCell(0, 20), CellIndex(19, 1));
	TestEqual(TEXT("last step"), SectorCell(0, 399), CellIndex(0, 19));
	TestEqual(TEXT("step out of range"), SectorCell(0, 400), (int32)INDEX_NONE);
	TestEqual(TEXT("sector out of range"), SectorCell(75, 0), (int32)INDEX_NONE);
	TestEqual(TEXT("negative sector"), SectorCell(-1, 0), (int32)INDEX_NONE);
	TestTrue(TEXT("bad sector has zero centre"), SectorCentre(75).IsZero());
	TestEqual(TEXT("sector of bad cell"), SectorOf(NumCells), (int32)INDEX_NONE);
	TestEqual(TEXT("sector outside"), SectorAt(FVector2D(50.0, 50.0)), (int32)INDEX_NONE);
	return true;
}

bool FLotMapEntriesTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltLot;

	TSet<int32> All;
	int32 Bad = 0;
	for (int32 Entry = 0; Entry < NumEntries; ++Entry)
	{
		for (int32 I = 0; I < CellsPerEntry; ++I)
		{
			const int32 Cell = EntryCell(Entry, I);
			if (Cell == INDEX_NONE || EntryOf(Cell) != Entry)
			{
				++Bad;
			}
			All.Add(Cell);
		}
	}
	TestEqual(TEXT("entry cells valid"), Bad, 0);
	TestEqual(TEXT("72 distinct entry cells"), All.Num(), NumEntries * CellsPerEntry);

	TestEqual(TEXT("cell (99,0) is in entry 0"), EntryOf(CellIndex(99, 0)), 0);
	TestEqual(TEXT("cell (0,0) is in no entry"), EntryOf(CellIndex(0, 0)), (int32)INDEX_NONE);
	TestEqual(TEXT("bad cell is in no entry"), EntryOf(INDEX_NONE), (int32)INDEX_NONE);
	TestEqual(TEXT("entry 0 first cell"), EntryCell(0, 0), CellIndex(97, 0));
	TestEqual(TEXT("entry 0 row-major"), EntryCell(0, 6), CellIndex(97, 1));
	TestEqual(TEXT("entry out of range"), EntryCell(6, 0), (int32)INDEX_NONE);
	TestEqual(TEXT("index out of range"), EntryCell(0, 12), (int32)INDEX_NONE);

	const FVector2D Centres[NumEntries] = {{0, -10000}, {5500, 0}, {0, 5500}, {-10000, 0}, {-5000, 10000}, {10000, -5000}};
	const FVector2D Outward[NumEntries] = {{0, -1}, {0, 1}, {1, 0}, {-1, 0}, {0, 1}, {1, 0}};
	for (int32 Entry = 0; Entry < NumEntries; ++Entry)
	{
		TestTrue(*FString::Printf(TEXT("centre %d"), Entry), EntryCentre(Entry).Equals(Centres[Entry]));
		TestTrue(*FString::Printf(TEXT("outward %d"), Entry), EntryOutward(Entry).Equals(Outward[Entry]));
	}
	TestTrue(TEXT("bad entry centre"), EntryCentre(6).IsZero());
	TestTrue(TEXT("bad entry outward"), EntryOutward(-1).IsZero());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
