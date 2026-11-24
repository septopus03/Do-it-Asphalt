// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"

DIA_TEST(FLotBalanceTablesTest, "DoItAsphalt.Lot.Balance.Tables")
DIA_TEST(FLotBalanceItemShopTest, "DoItAsphalt.Lot.Balance.ItemShop")
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
	TestEqual(TEXT("item prices"), ItemTotal, 304615);

	int32 PlantTotal = 0;
	for (const int32 Price : LotBalance::PlantPrice)
	{
		PlantTotal += Price;
	}
	TestEqual(TEXT("plant prices"), PlantTotal, 33900);

	// The per-robot upgrades: two purchases of each kind, every level better than the one before, every price above the one before.
	TestEqual(TEXT("two levels bought of each kind"), MaxPaverUpgradeLevel, 2);
	for (int32 Level = 1; Level <= MaxPaverUpgradeLevel; ++Level)
	{
		TestTrue(*FString::Printf(TEXT("speed level %d is faster"), Level), LotBalance::AutoPaverRateCellsPerMin[Level] > LotBalance::AutoPaverRateCellsPerMin[Level - 1]);
		TestTrue(*FString::Printf(TEXT("capacity level %d holds more"), Level), LotBalance::AutoPaverTankLByLevel[Level] > LotBalance::AutoPaverTankLByLevel[Level - 1]);
	}
	for (int32 Kind = 0; Kind < (int32)ELotPaverUpgrade::Count; ++Kind)
	{
		for (int32 Level = 1; Level < MaxPaverUpgradeLevel; ++Level)
		{
			TestTrue(*FString::Printf(TEXT("upgrade %d: the next level costs more"), Kind), LotBalance::PaverUpgradePrice[Kind][Level] > LotBalance::PaverUpgradePrice[Kind][Level - 1]);
		}
	}
	TestEqual(TEXT("speed 1 price"), LotBalance::PaverUpgradePrice[(int32)ELotPaverUpgrade::Speed][0], 20000);
	TestEqual(TEXT("speed 2 price"), LotBalance::PaverUpgradePrice[(int32)ELotPaverUpgrade::Speed][1], 60000);
	TestEqual(TEXT("capacity 1 price"), LotBalance::PaverUpgradePrice[(int32)ELotPaverUpgrade::Capacity][0], 5000);
	TestEqual(TEXT("capacity 2 price"), LotBalance::PaverUpgradePrice[(int32)ELotPaverUpgrade::Capacity][1], 15000);
	TestTrue(TEXT("every robot costs more than the one before"), LotBalance::ItemPrice[(int32)ELotItem::AutoPaver] < LotBalance::ItemPrice[(int32)ELotItem::AutoPaver2]
		&& LotBalance::ItemPrice[(int32)ELotItem::AutoPaver2] < LotBalance::ItemPrice[(int32)ELotItem::AutoPaver3]);
	TestEqual(TEXT("robot 2 price"), LotBalance::ItemPrice[(int32)ELotItem::AutoPaver2], 40000);
	TestEqual(TEXT("robot 3 price"), LotBalance::ItemPrice[(int32)ELotItem::AutoPaver3], 80000);
	TestEqual(TEXT("tank at level 0"), LotBalance::AutoPaverTankLByLevel[0], 2000.0);
	TestEqual(TEXT("tank at level 2"), LotBalance::AutoPaverTankLByLevel[2], 4500.0);

	for (int32 I = 1; I < (int32)ELotUnlock::Count; ++I)
	{
		TestTrue(TEXT("unlock cells grow"), LotBalance::UnlockCells[I] > LotBalance::UnlockCells[I - 1]);
	}

	// The four car tiers: bought in order, each better than the one before, all behind the same unlock step.
	for (int32 Level = 0; Level < LotBalance::NumCarLevels; ++Level)
	{
		const int32 Item = (int32)ELotItem::Car + Level;
		TestTrue(*FString::Printf(TEXT("car tier %d unlock"), Level + 1), LotBalance::ItemUnlock[Item] == ELotUnlock::Car);
		if (Level > 0)
		{
			TestTrue(*FString::Printf(TEXT("car tier %d costs more"), Level + 1), LotBalance::ItemPrice[Item] > LotBalance::ItemPrice[Item - 1]);
			TestTrue(*FString::Printf(TEXT("car tier %d is faster"), Level + 1), LotBalance::CarRateCellsPerMin[Level] > LotBalance::CarRateCellsPerMin[Level - 1]);
			TestTrue(*FString::Printf(TEXT("car tier %d has a wider bar"), Level + 1), LotBalance::CarWidthCm[Level] > LotBalance::CarWidthCm[Level - 1]);
			TestTrue(*FString::Printf(TEXT("car tier %d has a bigger tank"), Level + 1), LotBalance::CarTankL[Level] > LotBalance::CarTankL[Level - 1]);
			TestTrue(*FString::Printf(TEXT("car tier %d has its bar farther back"), Level + 1), LotBalance::CarBarBehindCm[Level] > LotBalance::CarBarBehindCm[Level - 1]);
		}
	}
	TestEqual(TEXT("car tier 1 price"), LotBalance::ItemPrice[(int32)ELotItem::Car], 12000);
	TestEqual(TEXT("car tier 4 rate"), LotBalance::CarRateCellsPerMin[3], 480.0);
	TestEqual(TEXT("car tier 4 bar"), LotBalance::CarWidthCm[3], 500.0);
	TestEqual(TEXT("car tier 4 tank"), LotBalance::CarTankL[3], 18000.0);

	for (int32 I = 0; I < (int32)ELotItem::Count; ++I)
	{
		ELotItem Expected = ELotItem::Count;
		if (I == (int32)ELotItem::CarT2) { Expected = ELotItem::Car; }
		if (I == (int32)ELotItem::CarT3) { Expected = ELotItem::CarT2; }
		if (I == (int32)ELotItem::CarT4) { Expected = ELotItem::CarT3; }
		if (I == (int32)ELotItem::AutoPaver2) { Expected = ELotItem::AutoPaver; }
		if (I == (int32)ELotItem::AutoPaver3) { Expected = ELotItem::AutoPaver2; }
		TestTrue(*FString::Printf(TEXT("requires %d"), I), LotBalance::ItemRequires[I] == Expected);
	}
	return true;
}

bool FLotBalanceDerivedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("car speed, tier 1"), LotBalance::CarPaveSpeedCmS(0), 144.444, 0.01);
	TestEqual(TEXT("car speed, tier 2"), LotBalance::CarPaveSpeedCmS(1), 138.889, 0.01);
	TestEqual(TEXT("car speed, tier 3"), LotBalance::CarPaveSpeedCmS(2), 150.794, 0.01);
	TestEqual(TEXT("car speed, tier 4"), LotBalance::CarPaveSpeedCmS(3), 160.0, 0.01);
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
	for (const ELotItem Item : {ELotItem::Trolley, ELotItem::Cart, ELotItem::Station, ELotItem::AutoPaver, ELotItem::Car})
	{
		const FVector2D Spawn = LotLayout::ItemSpawn(Item);
		// The robot and the car leave their shops in the south strip; the rest appears on the lot.
		const bool bFromShop = Item == ELotItem::AutoPaver || Item == ELotItem::Car;
		TestEqual(TEXT("spawn is on the lot unless it comes from a shop"), CellAt(Spawn) != INDEX_NONE, !bFromShop);
		for (const FVector2D& Other : Spawns)
		{
			TestTrue(TEXT("spawns are apart"), FVector2D::Distance(Spawn, Other) >= 150.0);
		}
		Spawns.Add(Spawn);
	}
	TestEqual(TEXT("others spawn at the start"), LotLayout::ItemSpawn(ELotItem::Canister), LotLayout::PlayerStart);
	return true;
}

bool FLotBalanceItemShopTest::RunTest(const FString& Parameters)
{
	using LotBalance::ItemShop;
	const ELotShop Expected[(int32)ELotItem::Count] =
	{
		ELotShop::Tools, ELotShop::Tools, ELotShop::Tools, ELotShop::Tools, ELotShop::Tools,
		ELotShop::Robots,
		ELotShop::Robots,
		ELotShop::Garage, ELotShop::Garage, ELotShop::Garage, ELotShop::Garage,
		ELotShop::Robots, ELotShop::Robots
	};
	int32 Sold[(int32)ELotShop::Count] = {};
	for (int32 I = 0; I < (int32)ELotItem::Count; ++I)
	{
		TestTrue(*FString::Printf(TEXT("item %d is sold by the expected building"), I), ItemShop[I] == Expected[I]);
		++Sold[(int32)ItemShop[I]];
	}
	TestTrue(TEXT("plants are sold by the tool shop"), LotBalance::PlantShop == ELotShop::Tools);
	for (int32 Shop = 0; Shop < (int32)ELotShop::Count; ++Shop)
	{
		TestTrue(*FString::Printf(TEXT("building %d sells at least one item"), Shop), Sold[Shop] > 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
