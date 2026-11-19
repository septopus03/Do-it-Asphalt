// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltBalance.h"
#include "AsphaltGame.h"
#include "AsphaltLayout.h"

DIA_TEST(FSpecialsRoadsTest, "DoItAsphalt.Core.Specials.Roads")
DIA_TEST(FSpecialsLockedUntilRoadCompleteTest, "DoItAsphalt.Core.Specials.LockedUntilRoadComplete")
DIA_TEST(FSpecialsNotEnoughMoneyTest, "DoItAsphalt.Core.Specials.NotEnoughMoney")
DIA_TEST(FSpecialsTwoHandsDoublesWidthTest, "DoItAsphalt.Core.Specials.TwoHandsDoublesWidth")
DIA_TEST(FSpecialsPositionsTest, "DoItAsphalt.Core.Specials.Positions")
DIA_TEST(FSpecialsCarTest, "DoItAsphalt.Core.Specials.Car")
DIA_TEST(FSpecialsCarRequiresOwnershipTest, "DoItAsphalt.Core.Specials.CarRequiresOwnership")
DIA_TEST(FSpecialsPourFromCarCompletesRoadTest, "DoItAsphalt.Core.Specials.PourFromCarCompletesRoad")
DIA_TEST(FAutoPaverPavesFiveInFiveMinutesTest, "DoItAsphalt.Core.AutoPaver.PavesFiveInFiveMinutes")
DIA_TEST(FAutoPaverRetargetsWhenCellTakenTest, "DoItAsphalt.Core.AutoPaver.RetargetsWhenCellTaken")
DIA_TEST(FAutoPaverIdleWhenAllCompleteTest, "DoItAsphalt.Core.AutoPaver.IdleWhenAllComplete")
DIA_TEST(FAutoPaverNotOwnedDoesNothingTest, "DoItAsphalt.Core.AutoPaver.NotOwnedDoesNothing")
DIA_TEST(FExitOpensAtAllCellsTest, "DoItAsphalt.Core.Exit.OpensAtAllCells")
DIA_TEST(FExitTotalRewardsTest, "DoItAsphalt.Core.Exit.TotalRewards")

namespace
{
	/** Completes the cells of a road one by one and sums up what happened. */
	FAsphaltEvents CompleteRoad(FAsphaltGame& G, int32 Road, int32 NumCells = FHexMap::CellsPerRoad)
	{
		FAsphaltEvents Total;
		const TConstArrayView<int32> Cells = G.GetMap().GetRoadCells(Road);
		for (int32 I = 0; I < NumCells; ++I)
		{
			Total.Append(G.DebugCompleteCell(Cells[I]));
		}
		return Total;
	}
}

bool FSpecialsRoadsTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	TestEqual(TEXT("Sprint"), G.GetSpecialRoad(EAsphaltSpecial::Sprint), 3);
	TestEqual(TEXT("TwoHands"), G.GetSpecialRoad(EAsphaltSpecial::TwoHands), 2);
	TestEqual(TEXT("PortableStation"), G.GetSpecialRoad(EAsphaltSpecial::PortableStation), 4);
	TestEqual(TEXT("AutoPaver"), G.GetSpecialRoad(EAsphaltSpecial::AutoPaver), 1);
	TestEqual(TEXT("PaverCar"), G.GetSpecialRoad(EAsphaltSpecial::PaverCar), 0);
	TestEqual(TEXT("Sprint price"), G.GetSpecialPrice(EAsphaltSpecial::Sprint), 100.0);
	TestEqual(TEXT("PaverCar price"), G.GetSpecialPrice(EAsphaltSpecial::PaverCar), 800.0);
	for (int32 I = 0; I < static_cast<int32>(EAsphaltSpecial::Count); ++I)
	{
		TestFalse(TEXT("nothing owned at the start"), G.HasSpecial(static_cast<EAsphaltSpecial>(I)));
	}
	TestFalse(TEXT("exit closed"), G.IsExitOpen());
	TestFalse(TEXT("not won"), G.HasWon());
	return true;
}

bool FSpecialsLockedUntilRoadCompleteTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGive(1000.0, 0);
	TestFalse(TEXT("locked: can buy"), G.CanBuySpecial(EAsphaltSpecial::Sprint));
	TestFalse(TEXT("locked: buy"), G.BuySpecial(EAsphaltSpecial::Sprint));
	TestEqual(TEXT("money untouched"), G.GetMoney(), 1000.0, 1e-9);

	const TConstArrayView<int32> Road = G.GetMap().GetRoadCells(3);
	for (int32 I = 0; I < 4; ++I)
	{
		const FAsphaltEvents E = G.DebugCompleteCell(Road[I]);
		TestEqual(TEXT("no road completed yet"), E.CompletedRoads.Num(), 0);
	}
	TestEqual(TEXT("progress"), G.GetRoadProgress(3), 4);
	TestFalse(TEXT("road incomplete"), G.IsRoadComplete(3));
	TestFalse(TEXT("still locked"), G.CanBuySpecial(EAsphaltSpecial::Sprint));
	TestFalse(TEXT("still cannot buy"), G.BuySpecial(EAsphaltSpecial::Sprint));

	const FAsphaltEvents Last = G.DebugCompleteCell(Road[4]);
	TestEqual(TEXT("one road completed"), Last.CompletedRoads.Num(), 1);
	TestTrue(TEXT("road 3 completed"), Last.CompletedRoads.Contains(3));
	TestFalse(TEXT("exit not open"), Last.bExitOpened);
	TestEqual(TEXT("progress 5"), G.GetRoadProgress(3), 5);
	TestTrue(TEXT("road complete"), G.IsRoadComplete(3));
	TestTrue(TEXT("can buy"), G.CanBuySpecial(EAsphaltSpecial::Sprint));
	TestTrue(TEXT("buy"), G.BuySpecial(EAsphaltSpecial::Sprint));
	TestTrue(TEXT("owned"), G.HasSpecial(EAsphaltSpecial::Sprint));
	TestEqual(TEXT("money"), G.GetMoney(), 1000.0 + 5 * 20.0 - 100.0, 1e-6);

	const double Money = G.GetMoney();
	TestFalse(TEXT("cannot buy twice"), G.CanBuySpecial(EAsphaltSpecial::Sprint));
	TestFalse(TEXT("buy twice"), G.BuySpecial(EAsphaltSpecial::Sprint));
	TestEqual(TEXT("money unchanged"), G.GetMoney(), Money);

	// Invalid specials are rejected without side effects.
	TestFalse(TEXT("invalid special"), G.BuySpecial(EAsphaltSpecial::Count));
	TestFalse(TEXT("invalid has"), G.HasSpecial(EAsphaltSpecial::Count));
	TestEqual(TEXT("invalid road"), G.GetSpecialRoad(EAsphaltSpecial::Count), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FSpecialsNotEnoughMoneyTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	// Completing the road pays 5 * 20 = 100 money, so top up to exactly 799.
	CompleteRoad(G, 0);
	G.DebugGive(-G.GetMoney() + 799.0, 0);
	TestEqual(TEXT("money"), G.GetMoney(), 799.0, 1e-9);
	TestTrue(TEXT("road complete"), G.IsRoadComplete(0));
	TestFalse(TEXT("cannot afford the car"), G.CanBuySpecial(EAsphaltSpecial::PaverCar));
	TestFalse(TEXT("buy rejected"), G.BuySpecial(EAsphaltSpecial::PaverCar));
	TestEqual(TEXT("money unchanged"), G.GetMoney(), 799.0, 1e-9);
	TestFalse(TEXT("not owned"), G.HasSpecial(EAsphaltSpecial::PaverCar));

	G.DebugGive(1.0, 0);
	TestTrue(TEXT("now affordable"), G.BuySpecial(EAsphaltSpecial::PaverCar));
	TestEqual(TEXT("money"), G.GetMoney(), 0.0, 1e-9);
	return true;
}

bool FSpecialsTwoHandsDoublesWidthTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	TestEqual(TEXT("base width"), G.GetToolWidthCm(), 20.0, 1e-9);
	G.DebugGrantSpecial(EAsphaltSpecial::TwoHands);
	TestEqual(TEXT("doubled"), G.GetToolWidthCm(), 40.0, 1e-9);
	G.DebugSetLevel(EAsphaltUpgrade::Tool, 6);
	TestEqual(TEXT("max tool doubled"), G.GetToolWidthCm(), 400.0, 1e-9);
	return true;
}

bool FSpecialsPositionsTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	TestEqual(TEXT("player start X"), G.World().PlayerPos.X, AsphaltLayout::PlayerStart.X);
	TestEqual(TEXT("player start Y"), G.World().PlayerPos.Y, AsphaltLayout::PlayerStart.Y);
	TestEqual(TEXT("the player starts facing the plant"), G.World().PlayerYawDeg, AsphaltLayout::PlayerStartYawDeg);
	const double Yaw = FMath::DegreesToRadians(AsphaltLayout::PlayerStartYawDeg);
	TestTrue(TEXT("the plant lies in the direction the player faces"),
		FVector2D::DotProduct(FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)), AsphaltLayout::PlantPos - AsphaltLayout::PlayerStart) > 0.0);
	TestFalse(TEXT("station not carried"), G.World().bStationCarried);
	TestFalse(TEXT("not in car"), G.World().bPlayerInCar);

	// The station spawns on the lot at the end of its road when bought.
	G.DebugGive(2000.0, 0);
	CompleteRoad(G, 4);
	TestTrue(TEXT("buy station"), G.BuySpecial(EAsphaltSpecial::PortableStation));
	const FVector2D Station = AsphaltLayout::SpecialSpawn(4);
	TestTrue(TEXT("station position"), G.World().StationPos.Equals(Station, 1e-9));

	// The auto paver starts at the spawn of its road too.
	CompleteRoad(G, 1);
	TestTrue(TEXT("buy paver"), G.BuySpecial(EAsphaltSpecial::AutoPaver));
	TestTrue(TEXT("paver position"), G.GetAutoPaverPos().Equals(AsphaltLayout::SpecialSpawn(1), 1e-9));
	TestEqual(TEXT("paver idle until the first tick"), G.GetAutoPaverTarget(), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FSpecialsCarTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGrantSpecial(EAsphaltSpecial::PaverCar);
	TestTrue(TEXT("owned"), G.HasSpecial(EAsphaltSpecial::PaverCar));
	TestTrue(TEXT("car position"), G.World().CarPos.Equals(AsphaltLayout::SpecialSpawn(0), 1e-9));
	TestEqual(TEXT("empty tank"), G.GetCarLitres(), 0.0);

	G.Tick(600.0); // plant stock 60
	TestEqual(TEXT("refill"), G.RefillCar(10.0), 60.0, 1e-9);
	TestEqual(TEXT("tank"), G.GetCarLitres(), 60.0, 1e-9);
	TestEqual(TEXT("stock empty"), G.GetPlantStock(), 0.0, 1e-9);
	TestEqual(TEXT("bad Dt"), G.RefillCar(-1.0), 0.0);

	const FAsphaltEvents E = G.PourFromCar({0, -10}, {0, 10});
	TestTrue(TEXT("poured something"), E.LitresPoured > 0.0);
	TestEqual(TEXT("money"), E.MoneyEarned, E.LitresPoured * AsphaltBalance::MoneyPerLitre, 1e-9);
	TestEqual(TEXT("tank after pouring"), G.GetCarLitres(), 60.0 - E.LitresPoured, 1e-9);
	TestEqual(TEXT("backpack untouched"), G.GetBackpackLitres(), 0.0);
	TestEqual(TEXT("total litres"), G.GetTotalLitresPoured(), E.LitresPoured, 1e-9);

	// The car pours with its own wide bar: 300 cm, not the tool width.
	TestTrue(TEXT("wider than the tool"), E.LitresPoured > 1.0);
	return true;
}

bool FSpecialsCarRequiresOwnershipTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.Tick(600.0);
	TestEqual(TEXT("no car, no refill"), G.RefillCar(10.0), 0.0);
	TestEqual(TEXT("tank empty"), G.GetCarLitres(), 0.0);
	TestEqual(TEXT("stock untouched"), G.GetPlantStock(), 60.0, 1e-9);
	const FAsphaltEvents E = G.PourFromCar({0, -10}, {0, 10});
	TestEqual(TEXT("no car, no pour"), E.LitresPoured, 0.0);
	TestEqual(TEXT("no money"), G.GetMoney(), 0.0);
	return true;
}

bool FSpecialsPourFromCarCompletesRoadTest::RunTest(const FString& Parameters)
{
	// The car's events carry road and exit news just like the backpack's.
	FAsphaltGame G;
	G.DebugGrantSpecial(EAsphaltSpecial::PaverCar);
	G.DebugSetLevel(EAsphaltUpgrade::Plant, 12);
	G.Tick(300.0);
	G.RefillCar(100.0);
	TestTrue(TEXT("car is full"), G.GetCarLitres() > 400.0);

	// The five road cells form a small cluster rather than a line. Over each cell centre the 3 m wide bar
	// covers the whole hexagon (radius 62 cm) once the stamp is longer than the hexagon.
	const FHexMap& Map = G.GetMap();
	FAsphaltEvents E;
	for (const int32 Cell : Map.GetRoadCells(2))
	{
		const FVector2D Centre = AsphaltHex::ToWorld(Map.GetCell(Cell).Coord);
		E.Append(G.PourFromCar(Centre - FVector2D(70.0, 0.0), Centre + FVector2D(70.0, 0.0)));
	}
	TestTrue(TEXT("road 2 reported"), E.CompletedRoads.Contains(2));
	TestTrue(TEXT("road 2 complete"), G.IsRoadComplete(2));
	return true;
}

bool FAutoPaverPavesFiveInFiveMinutesTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGrantSpecial(EAsphaltSpecial::AutoPaver);
	const FAsphaltEvents E = G.Tick(300.0);
	const int32 Done = G.GetGrid().NumCompleteCells();
	TestTrue(TEXT("4 or 5 cells in 5 minutes"), Done == 4 || Done == 5);
	TestEqual(TEXT("shards"), E.ShardsEarned, Done);
	TestEqual(TEXT("completed cell list"), E.CompletedCells.Num(), Done);
	TestTrue(TEXT("money"), E.MoneyEarned > 0.0);
	TestEqual(TEXT("money equals litres"), E.MoneyEarned, E.LitresPoured * AsphaltBalance::MoneyPerLitre, 1e-9);
	TestEqual(TEXT("wallet"), G.GetMoney(), E.MoneyEarned, 1e-9);
	TestEqual(TEXT("total litres"), G.GetTotalLitresPoured(), E.LitresPoured, 1e-9);
	// 20 l/min for 5 minutes is at most 100 l (the paver never pours more than it earns).
	TestTrue(TEXT("rate respected"), E.LitresPoured <= 100.0 + 1.0);

	// Ten more minutes (15 in all): 300 l pay for 15 cells minus the walking.
	G.Tick(600.0);
	const int32 Later = G.GetGrid().NumCompleteCells();
	TestTrue(TEXT("13..16 cells after 15 minutes"), Later >= 13 && Later <= 16);
	return true;
}

bool FAutoPaverRetargetsWhenCellTakenTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGrantSpecial(EAsphaltSpecial::AutoPaver);
	G.Tick(5.0);
	const int32 Target = G.GetAutoPaverTarget();
	TestTrue(TEXT("has a target"), Target != INDEX_NONE);
	G.DebugCompleteCell(Target);
	G.Tick(5.0);
	const int32 Next = G.GetAutoPaverTarget();
	TestTrue(TEXT("retargeted"), Next != Target);
	TestTrue(TEXT("new target valid"), Next != INDEX_NONE && !G.GetGrid().IsCellComplete(Next));
	return true;
}

bool FAutoPaverIdleWhenAllCompleteTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGrantSpecial(EAsphaltSpecial::AutoPaver);
	G.Tick(1.0); // gets a target first, so the idle state has to be reached by retargeting
	for (int32 Cell = 0; Cell < G.GetMap().Num(); ++Cell)
	{
		G.DebugCompleteCell(Cell);
	}
	const FAsphaltEvents E = G.Tick(60.0);
	TestEqual(TEXT("no litres"), E.LitresPoured, 0.0);
	TestEqual(TEXT("no cells"), E.CompletedCells.Num(), 0);
	TestEqual(TEXT("idle"), G.GetAutoPaverTarget(), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FAutoPaverNotOwnedDoesNothingTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	const FAsphaltEvents E = G.Tick(300.0);
	TestEqual(TEXT("no litres"), E.LitresPoured, 0.0);
	TestEqual(TEXT("no cells"), G.GetGrid().NumCompleteCells(), 0);
	TestEqual(TEXT("no target"), G.GetAutoPaverTarget(), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FExitOpensAtAllCellsTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	bool bOpenedEarly = false;
	for (int32 Cell = 0; Cell < 498; ++Cell)
	{
		bOpenedEarly |= G.DebugCompleteCell(Cell).bExitOpened;
	}
	TestFalse(TEXT("no early exit event"), bOpenedEarly);
	TestFalse(TEXT("exit closed"), G.IsExitOpen());
	G.SetWon();
	TestFalse(TEXT("SetWon ignored while closed"), G.HasWon());

	const FAsphaltEvents E = G.DebugCompleteCell(498);
	TestTrue(TEXT("exit opened event"), E.bExitOpened);
	TestTrue(TEXT("exit open"), G.IsExitOpen());
	TestFalse(TEXT("not won yet"), G.HasWon());
	G.SetWon();
	TestTrue(TEXT("won"), G.HasWon());

	const FAsphaltEvents Again = G.DebugCompleteCell(498);
	TestFalse(TEXT("the event comes only once"), Again.bExitOpened);
	return true;
}

bool FExitTotalRewardsTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	for (int32 Cell = 0; Cell < G.GetMap().Num(); ++Cell)
	{
		G.DebugCompleteCell(Cell);
	}
	TestEqual(TEXT("shards"), G.GetShards(), 499);
	TestEqual(TEXT("money"), G.GetMoney(), 9980.0, 1e-3);
	TestEqual(TEXT("all complete"), G.GetGrid().NumCompleteCells(), 499);
	return true;
}
