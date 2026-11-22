// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"

DIA_TEST(FLotBalanceTablesTest, "DoItAsphalt.Lot.Balance.Tables")
DIA_TEST(FLotBalanceDerivedTest, "DoItAsphalt.Lot.Balance.Derived")
DIA_TEST(FLotBalanceMachineSpeedsTest, "DoItAsphalt.Lot.Balance.MachineSpeeds")
DIA_TEST(FLotLayoutNothingFixedOnLotTest, "DoItAsphalt.Lot.Layout.NothingFixedOnLot")
DIA_TEST(FLotLayoutStartTest, "DoItAsphalt.Lot.Layout.Start")

bool FLotBalanceTablesTest::RunTest(const FString& Parameters)
{
	int32 ItemTotal = 0;
	for (const int32 Price : LotBalance::ItemPrice)
	{
		ItemTotal += Price;
	}
	TestEqual(TEXT("item prices"), ItemTotal, 71395);

	int32 PlantTotal = 0;
	for (const int32 Price : LotBalance::PlantPrice)
	{
		PlantTotal += Price;
	}
	TestEqual(TEXT("plant prices"), PlantTotal, 33900);

	for (int32 I = 1; I < (int32)ELotUnlock::Count; ++I)
	{
		TestTrue(TEXT("unlock cells grow"), LotBalance::UnlockCells[I] > LotBalance::UnlockCells[I - 1]);
	}

	for (int32 I = 0; I < (int32)ELotItem::Count; ++I)
	{
		ELotItem Expected = ELotItem::Count;
		if (I == (int32)ELotItem::AutoPaverFast) { Expected = ELotItem::AutoPaver; }
		if (I == (int32)ELotItem::AutoPaverFaster) { Expected = ELotItem::AutoPaverFast; }
		if (I == (int32)ELotItem::CarWideBar) { Expected = ELotItem::Car; }
		TestTrue(*FString::Printf(TEXT("requires %d"), I), LotBalance::ItemRequires[I] == Expected);
	}
	return true;
}

bool FLotBalanceDerivedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("car speed, plain bar"), LotBalance::CarPaveSpeedCmS(0), 144.444, 0.01);
	TestEqual(TEXT("car speed, wide bar"), LotBalance::CarPaveSpeedCmS(1), 142.857, 0.01);
	TestEqual(TEXT("dispense rate"), LotBalance::DispenseRateLPerS(0), 2.5);
	return true;
}

bool FLotBalanceMachineSpeedsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("station carry speed"), LotBalance::StationCarrySpeedCmS > 0.0);
	return true;
}

bool FLotLayoutNothingFixedOnLotTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltLot;

	TArray<TPair<FString, FVector2D>> Points;
	Points.Emplace(TEXT("trough"), LotLayout::TroughPos);
	Points.Emplace(TEXT("plant"), LotLayout::PlantPos);
	Points.Emplace(TEXT("gate"), LotLayout::GatePos());
	Points.Emplace(TEXT("exit trigger"), LotLayout::ExitTrigger());
	for (const int32 Entry : {0, 3, 4, 5})
	{
		Points.Emplace(FString::Printf(TEXT("board %d"), Entry), LotLayout::BoardPos(Entry));
	}

	for (const TPair<FString, FVector2D>& Point : Points)
	{
		TestEqual(*(Point.Key + TEXT(" is off the lot")), CellAt(Point.Value), (int32)INDEX_NONE);
		TestFalse(*(Point.Key + TEXT(" is outside the mall quadrant")), Point.Value.X >= 0.0 && Point.Value.Y >= 0.0);
	}
	return true;
}

bool FLotLayoutStartTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltLot;

	TestEqual(TEXT("player starts in entry segment 0"), EntryOf(CellAt(LotLayout::PlayerStart)), 0);
	TestEqual(TEXT("first cell is in entry segment 0"), EntryOf(LotLayout::FirstCell()), 0);
	TestTrue(TEXT("first cell is by the trough"), FVector2D::Distance(CellCentre(LotLayout::FirstCell()), LotLayout::TroughPos) <= 150.0);

	TArray<FVector2D> Spawns;
	for (const ELotItem Item : {ELotItem::Suitcase, ELotItem::Cart, ELotItem::Station, ELotItem::AutoPaver, ELotItem::Car})
	{
		const FVector2D Spawn = LotLayout::ItemSpawn(Item);
		TestTrue(TEXT("spawn is on the lot"), CellAt(Spawn) != INDEX_NONE);
		for (const FVector2D& Other : Spawns)
		{
			TestTrue(TEXT("spawns are apart"), FVector2D::Distance(Spawn, Other) >= 150.0);
		}
		Spawns.Add(Spawn);
	}
	TestEqual(TEXT("others spawn at the start"), LotLayout::ItemSpawn(ELotItem::Scoop), LotLayout::PlayerStart);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
