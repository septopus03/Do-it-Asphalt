// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltBalance.h"
#include "AsphaltGame.h"

DIA_TEST(FGameNewGameTest, "DoItAsphalt.Core.Game.NewGame")
DIA_TEST(FGamePlantProducesTest, "DoItAsphalt.Core.Game.PlantProduces")
DIA_TEST(FGameTickIsRobustTest, "DoItAsphalt.Core.Game.TickIsRobust")
DIA_TEST(FGameRefillTest, "DoItAsphalt.Core.Game.Refill")
DIA_TEST(FGamePourPaysTest, "DoItAsphalt.Core.Game.PourPays")
DIA_TEST(FGamePourNeverGoesNegativeTest, "DoItAsphalt.Core.Game.PourNeverGoesNegative")
DIA_TEST(FGameNoMoneyFarmingTest, "DoItAsphalt.Core.Game.NoMoneyFarming")
DIA_TEST(FGamePourCompletesCellsTest, "DoItAsphalt.Core.Game.PourCompletesCells")
DIA_TEST(FGameCellGivesShardTest, "DoItAsphalt.Core.Game.CellGivesShard")
DIA_TEST(FGameBuyUpgradesTest, "DoItAsphalt.Core.Game.BuyUpgrades")
DIA_TEST(FGameBuyRejectedTest, "DoItAsphalt.Core.Game.BuyRejected")
DIA_TEST(FGameUpgradeLadderTest, "DoItAsphalt.Core.Game.UpgradeLadder")
DIA_TEST(FGameBackpackKeepsContentOnUpgradeTest, "DoItAsphalt.Core.Game.BackpackKeepsContentOnUpgrade")
DIA_TEST(FGameEventsAppendTest, "DoItAsphalt.Core.Game.EventsAppend")

namespace
{
	/** Brings a new game's backpack to 5 l through the plant, the way a player would. */
	void FillBackpackFromPlant(FAsphaltGame& G)
	{
		G.Tick(60.0);
		G.RefillBackpack(10.0);
	}
}

bool FGameNewGameTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	TestEqual(TEXT("money"), G.GetMoney(), 0.0);
	TestEqual(TEXT("shards"), G.GetShards(), 0);
	TestEqual(TEXT("playtime"), G.GetPlaytimeSeconds(), 0.0);
	TestEqual(TEXT("total litres"), G.GetTotalLitresPoured(), 0.0);
	TestEqual(TEXT("capacity"), G.GetBackpackCapacity(), 5.0, 1e-9);
	TestEqual(TEXT("backpack litres"), G.GetBackpackLitres(), 0.0);
	TestEqual(TEXT("tool width"), G.GetToolWidthCm(), 20.0, 1e-9);
	TestEqual(TEXT("plant rate"), G.GetPlantRatePerMin(), 15.0, 1e-9);
	TestEqual(TEXT("plant storage"), G.GetPlantStorage(), 60.0, 1e-9);
	TestEqual(TEXT("plant stock"), G.GetPlantStock(), 0.0);
	for (EAsphaltUpgrade U : {EAsphaltUpgrade::Backpack, EAsphaltUpgrade::Tool, EAsphaltUpgrade::Plant})
	{
		TestEqual(TEXT("level"), G.GetLevel(U), 0);
	}
	TestEqual(TEXT("backpack max level"), G.GetMaxLevel(EAsphaltUpgrade::Backpack), 12);
	TestEqual(TEXT("tool max level"), G.GetMaxLevel(EAsphaltUpgrade::Tool), 6);
	TestEqual(TEXT("plant max level"), G.GetMaxLevel(EAsphaltUpgrade::Plant), 12);
	TestEqual(TEXT("map"), G.GetMap().Num(), 499);
	TestEqual(TEXT("fresh grid"), G.GetGrid().NumCompleteCells(), 0);
	return true;
}

bool FGamePlantProducesTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	const FAsphaltEvents E = G.Tick(60.0);
	TestEqual(TEXT("stock after a minute"), G.GetPlantStock(), 15.0, 1e-6);
	TestEqual(TEXT("ticking earns nothing"), E.MoneyEarned, 0.0);
	TestEqual(TEXT("ticking pours nothing"), E.LitresPoured, 0.0);
	G.Tick(600.0);
	TestEqual(TEXT("stock is clamped to the storage"), G.GetPlantStock(), 60.0, 1e-9);
	TestEqual(TEXT("playtime"), G.GetPlaytimeSeconds(), 660.0, 1e-6);
	return true;
}

bool FGameTickIsRobustTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.Tick(10.0);
	const double Stock = G.GetPlantStock();
	const double Playtime = G.GetPlaytimeSeconds();

	G.Tick(-5.0);
	G.Tick(0.0);
	G.Tick(std::numeric_limits<double>::quiet_NaN());
	G.Tick(std::numeric_limits<double>::infinity());
	TestEqual(TEXT("stock unchanged by bad Dt"), G.GetPlantStock(), Stock);
	TestEqual(TEXT("playtime unchanged by bad Dt"), G.GetPlaytimeSeconds(), Playtime);
	TestEqual(TEXT("money unchanged by bad Dt"), G.GetMoney(), 0.0);

	// Many small steps and one big step agree.
	FAsphaltGame Small;
	for (int32 I = 0; I < 600; ++I)
	{
		Small.Tick(0.1);
	}
	FAsphaltGame Big;
	Big.Tick(60.0);
	TestEqual(TEXT("stock"), Small.GetPlantStock(), Big.GetPlantStock(), 1e-6);
	TestEqual(TEXT("playtime"), Small.GetPlaytimeSeconds(), Big.GetPlaytimeSeconds(), 1e-6);

	// A huge Dt must neither hang nor corrupt anything.
	FAsphaltGame Huge;
	Huge.Tick(1e15);
	TestEqual(TEXT("huge Dt fills the plant"), Huge.GetPlantStock(), Huge.GetPlantStorage(), 1e-9);
	TestTrue(TEXT("huge Dt keeps playtime finite"), FMath::IsFinite(Huge.GetPlaytimeSeconds()) && Huge.GetPlaytimeSeconds() > 0.0);
	return true;
}

bool FGameRefillTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.Tick(600.0); // stock 60
	TestEqual(TEXT("rate limited"), G.RefillBackpack(0.75), 2.5, 1e-9);
	TestEqual(TEXT("space limited"), G.RefillBackpack(10.0), 2.5, 1e-9);
	TestEqual(TEXT("backpack full"), G.GetBackpackLitres(), 5.0, 1e-9);
	TestEqual(TEXT("stock"), G.GetPlantStock(), 55.0, 1e-9);
	TestEqual(TEXT("nothing more fits"), G.RefillBackpack(10.0), 0.0);
	TestEqual(TEXT("negative Dt"), G.RefillBackpack(-1.0), 0.0);
	TestEqual(TEXT("NaN Dt"), G.RefillBackpack(std::numeric_limits<double>::quiet_NaN()), 0.0);
	TestEqual(TEXT("backpack unchanged"), G.GetBackpackLitres(), 5.0, 1e-9);

	FAsphaltGame Low;
	Low.Tick(4.0); // stock 1
	TestEqual(TEXT("stock limited"), Low.RefillBackpack(10.0), 1.0, 1e-9);
	TestEqual(TEXT("stock empty"), Low.GetPlantStock(), 0.0, 1e-9);
	TestEqual(TEXT("backpack"), Low.GetBackpackLitres(), 1.0, 1e-9);
	return true;
}

bool FGamePourPaysTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	FillBackpackFromPlant(G);
	TestEqual(TEXT("backpack"), G.GetBackpackLitres(), 5.0, 1e-9);

	const FAsphaltEvents E = G.PourFromBackpack({-20, 0}, {20, 0});
	TestTrue(TEXT("poured something"), E.LitresPoured > 0.0);
	TestEqual(TEXT("money earned"), E.MoneyEarned, E.LitresPoured * AsphaltBalance::MoneyPerLitre, 1e-9);
	TestEqual(TEXT("wallet"), G.GetMoney(), E.MoneyEarned, 1e-9);
	TestEqual(TEXT("backpack"), G.GetBackpackLitres(), 5.0 - E.LitresPoured, 1e-9);
	TestEqual(TEXT("total litres"), G.GetTotalLitresPoured(), E.LitresPoured, 1e-9);
	TestEqual(TEXT("no cell completed"), E.CompletedCells.Num(), 0);
	TestEqual(TEXT("no shards"), E.ShardsEarned, 0);
	TestEqual(TEXT("wallet shards"), G.GetShards(), 0);
	return true;
}

bool FGamePourNeverGoesNegativeTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.Tick(0.12); // 0.03 l in the plant
	G.RefillBackpack(10.0);
	const double Before = G.GetBackpackLitres();
	TestEqual(TEXT("backpack holds 0.03 l"), Before, 0.03, 1e-9);

	G.DebugSetLevel(EAsphaltUpgrade::Tool, 6); // 2 m wide
	const FAsphaltEvents E = G.PourFromBackpack({-100, 0}, {100, 0});
	TestTrue(TEXT("poured no more than it had"), E.LitresPoured <= Before + 1e-9);
	TestTrue(TEXT("backpack never negative"), G.GetBackpackLitres() >= 0.0);
	TestTrue(TEXT("no more money than litres"), G.GetMoney() <= Before + 1e-9);
	TestEqual(TEXT("money matches litres"), E.MoneyEarned, E.LitresPoured, 1e-9);
	TestEqual(TEXT("backpack and pour add up"), G.GetBackpackLitres() + E.LitresPoured, Before, 1e-9);

	// A texel costs about 0.049 l, so 0.12 l pays for two texels and the rest stays in the backpack.
	FAsphaltGame Little;
	Little.Tick(0.48);
	Little.RefillBackpack(10.0);
	Little.DebugSetLevel(EAsphaltUpgrade::Tool, 6);
	const double Held = Little.GetBackpackLitres();
	TestEqual(TEXT("backpack holds 0.12 l"), Held, 0.12, 1e-9);
	const FAsphaltEvents Some = Little.PourFromBackpack({-100, 0}, {100, 0});
	TestTrue(TEXT("poured something"), Some.LitresPoured > 0.0);
	TestTrue(TEXT("poured no more than it had"), Some.LitresPoured <= Held + 1e-9);
	TestTrue(TEXT("backpack never negative"), Little.GetBackpackLitres() >= 0.0);
	TestEqual(TEXT("backpack and pour add up"), Little.GetBackpackLitres() + Some.LitresPoured, Held, 1e-9);
	TestEqual(TEXT("money matches litres"), Little.GetMoney(), Some.LitresPoured, 1e-9);

	// An empty backpack pours nothing.
	FAsphaltGame Empty;
	const FAsphaltEvents None = Empty.PourFromBackpack({-100, 0}, {100, 0});
	TestEqual(TEXT("empty backpack pours nothing"), None.LitresPoured, 0.0);
	TestEqual(TEXT("empty backpack earns nothing"), Empty.GetMoney(), 0.0);
	TestEqual(TEXT("empty backpack stays empty"), Empty.GetBackpackLitres(), 0.0);
	TestEqual(TEXT("empty backpack paints nothing"), Empty.GetGrid().GetCellFill(0), 0.0);
	return true;
}

bool FGameNoMoneyFarmingTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	FillBackpackFromPlant(G);
	const FAsphaltEvents First = G.PourFromBackpack({-20, 0}, {20, 0});
	TestTrue(TEXT("first pour pays"), First.MoneyEarned > 0.0);
	const double Money = G.GetMoney();
	const double Backpack = G.GetBackpackLitres();

	const FAsphaltEvents Again = G.PourFromBackpack({-20, 0}, {20, 0});
	TestEqual(TEXT("re-pour pours nothing"), Again.LitresPoured, 0.0);
	TestEqual(TEXT("re-pour earns nothing"), Again.MoneyEarned, 0.0);
	TestEqual(TEXT("re-pour wallet"), G.GetMoney(), Money);
	TestEqual(TEXT("re-pour backpack"), G.GetBackpackLitres(), Backpack);

	// Off the map and off the grid: free, and nothing is earned.
	const FAsphaltEvents Outside = G.PourFromBackpack({1500, 1500}, {1550, 1550});
	const FAsphaltEvents OffGrid = G.PourFromBackpack({5000, 5000}, {6000, 6000});
	TestEqual(TEXT("outside the map"), Outside.LitresPoured, 0.0);
	TestEqual(TEXT("outside the grid"), OffGrid.LitresPoured, 0.0);
	TestEqual(TEXT("wallet after outside pours"), G.GetMoney(), Money);
	TestEqual(TEXT("backpack after outside pours"), G.GetBackpackLitres(), Backpack);

	// Garbage coordinates must not corrupt anything.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const FAsphaltEvents Bad = G.PourFromBackpack({NaN, 0}, {20, 0});
	TestEqual(TEXT("NaN coordinates"), Bad.LitresPoured, 0.0);
	TestEqual(TEXT("wallet after NaN pour"), G.GetMoney(), Money);
	TestEqual(TEXT("total litres"), G.GetTotalLitresPoured(), First.LitresPoured, 1e-9);
	return true;
}

bool FGamePourCompletesCellsTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugSetLevel(EAsphaltUpgrade::Backpack, 12); // 200 l
	G.DebugSetLevel(EAsphaltUpgrade::Plant, 12);
	G.DebugSetLevel(EAsphaltUpgrade::Tool, 6);
	G.Tick(60.0);
	G.RefillBackpack(10.0);
	TestEqual(TEXT("full backpack"), G.GetBackpackLitres(), 200.0, 1e-9);

	const FAsphaltEvents E = G.PourFromBackpack({-100, 0}, {100, 0});
	TestTrue(TEXT("completed cells"), E.CompletedCells.Num() > 0);
	TestEqual(TEXT("shards"), E.ShardsEarned, E.CompletedCells.Num() * AsphaltBalance::ShardsPerCell);
	TestEqual(TEXT("wallet shards"), G.GetShards(), E.ShardsEarned);
	TestEqual(TEXT("grid agrees"), G.GetGrid().NumCompleteCells(), E.CompletedCells.Num());
	TestEqual(TEXT("money equals litres"), E.MoneyEarned, E.LitresPoured * AsphaltBalance::MoneyPerLitre, 1e-9);
	TestEqual(TEXT("total litres"), G.GetTotalLitresPoured(), E.LitresPoured, 1e-9);
	return true;
}

bool FGameCellGivesShardTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	const FAsphaltEvents E = G.DebugCompleteCell(0);
	TestEqual(TEXT("shards"), E.ShardsEarned, 1);
	TestEqual(TEXT("money"), E.MoneyEarned, 20.0, 1e-6);
	TestEqual(TEXT("litres"), E.LitresPoured, 20.0, 1e-6);
	TestEqual(TEXT("completed cells"), E.CompletedCells.Num(), 1);
	TestTrue(TEXT("completed cell 0"), E.CompletedCells.Contains(0));
	TestEqual(TEXT("wallet shards"), G.GetShards(), 1);
	TestEqual(TEXT("wallet money"), G.GetMoney(), 20.0, 1e-6);
	TestTrue(TEXT("grid"), G.GetGrid().IsCellComplete(0));

	const FAsphaltEvents Again = G.DebugCompleteCell(0);
	TestEqual(TEXT("again: shards"), Again.ShardsEarned, 0);
	TestEqual(TEXT("again: money"), Again.MoneyEarned, 0.0);
	TestEqual(TEXT("again: litres"), Again.LitresPoured, 0.0);
	TestEqual(TEXT("again: cells"), Again.CompletedCells.Num(), 0);
	TestEqual(TEXT("again: wallet shards"), G.GetShards(), 1);
	TestEqual(TEXT("again: wallet money"), G.GetMoney(), 20.0, 1e-6);

	const FAsphaltEvents Invalid = G.DebugCompleteCell(-1);
	TestEqual(TEXT("invalid cell"), Invalid.CompletedCells.Num(), 0);
	TestEqual(TEXT("invalid cell money"), Invalid.MoneyEarned, 0.0);

	// A partly covered cell only pays for the missing part.
	FAsphaltGame Partial;
	FillBackpackFromPlant(Partial);
	const FAsphaltEvents Poured = Partial.PourFromBackpack({-20, 0}, {20, 0});
	const FAsphaltEvents Rest = Partial.DebugCompleteCell(0);
	TestEqual(TEXT("missing litres"), Rest.LitresPoured, 20.0 - Poured.LitresPoured, 1e-6);
	TestEqual(TEXT("whole cell paid once"), Partial.GetMoney(), 20.0, 1e-6);
	return true;
}

bool FGameBuyUpgradesTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGive(30.0, 1);
	TestTrue(TEXT("plant price"), G.GetNextPrice(EAsphaltUpgrade::Plant) == 30.0);
	TestTrue(TEXT("can buy plant"), G.CanBuy(EAsphaltUpgrade::Plant));
	TestTrue(TEXT("buy plant"), G.Buy(EAsphaltUpgrade::Plant));
	TestEqual(TEXT("money"), G.GetMoney(), 0.0, 1e-9);
	TestEqual(TEXT("plant level"), G.GetLevel(EAsphaltUpgrade::Plant), 1);
	TestEqual(TEXT("rate"), G.GetPlantRatePerMin(), 20.0, 1e-9);
	TestEqual(TEXT("storage"), G.GetPlantStorage(), 80.0, 1e-9);

	TestTrue(TEXT("backpack price"), G.GetNextPrice(EAsphaltUpgrade::Backpack) == 1.0);
	TestTrue(TEXT("buy backpack"), G.Buy(EAsphaltUpgrade::Backpack));
	TestEqual(TEXT("shards"), G.GetShards(), 0);
	TestEqual(TEXT("capacity"), G.GetBackpackCapacity(), 7.0, 1e-9);

	G.DebugGive(25.0, 0);
	TestTrue(TEXT("tool price"), G.GetNextPrice(EAsphaltUpgrade::Tool) == 25.0);
	TestTrue(TEXT("buy tool"), G.Buy(EAsphaltUpgrade::Tool));
	TestEqual(TEXT("width"), G.GetToolWidthCm(), 30.0, 1e-9);
	TestEqual(TEXT("money after tool"), G.GetMoney(), 0.0, 1e-9);
	return true;
}

bool FGameBuyRejectedTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGive(14.0, 0);
	TestFalse(TEXT("cannot afford plant"), G.CanBuy(EAsphaltUpgrade::Plant));
	TestFalse(TEXT("buy plant rejected"), G.Buy(EAsphaltUpgrade::Plant));
	TestEqual(TEXT("money unchanged"), G.GetMoney(), 14.0);
	TestEqual(TEXT("level unchanged"), G.GetLevel(EAsphaltUpgrade::Plant), 0);
	TestEqual(TEXT("rate unchanged"), G.GetPlantRatePerMin(), 15.0, 1e-9);

	// The wrong currency does not buy: shards do not pay for the plant, money does not pay for the backpack.
	TestFalse(TEXT("no shards for the backpack"), G.Buy(EAsphaltUpgrade::Backpack));
	TestEqual(TEXT("backpack level"), G.GetLevel(EAsphaltUpgrade::Backpack), 0);
	TestEqual(TEXT("money after backpack attempt"), G.GetMoney(), 14.0);
	G.DebugGive(0.0, 3);
	TestFalse(TEXT("shards do not pay the plant"), G.Buy(EAsphaltUpgrade::Plant));
	TestEqual(TEXT("shards unchanged"), G.GetShards(), 3);

	G.DebugSetLevel(EAsphaltUpgrade::Tool, 6);
	G.DebugGive(1e6, 0);
	const double Money = G.GetMoney();
	TestTrue(TEXT("no next price at max level"), G.GetNextPrice(EAsphaltUpgrade::Tool) == -1.0);
	TestFalse(TEXT("cannot buy at max level"), G.CanBuy(EAsphaltUpgrade::Tool));
	TestFalse(TEXT("buy at max level rejected"), G.Buy(EAsphaltUpgrade::Tool));
	TestEqual(TEXT("money unchanged at max"), G.GetMoney(), Money);
	TestEqual(TEXT("level stays at max"), G.GetLevel(EAsphaltUpgrade::Tool), 6);
	return true;
}

bool FGameUpgradeLadderTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	G.DebugGive(1e6, 1000);
	for (int32 Level = 0; Level < 12; ++Level)
	{
		TestTrue(TEXT("backpack price"), G.GetNextPrice(EAsphaltUpgrade::Backpack) == AsphaltBalance::BackpackPriceShards[Level]);
		TestTrue(TEXT("plant price"), G.GetNextPrice(EAsphaltUpgrade::Plant) == AsphaltBalance::PlantPriceMoney[Level]);
		TestTrue(TEXT("buy backpack"), G.Buy(EAsphaltUpgrade::Backpack));
		TestTrue(TEXT("buy plant"), G.Buy(EAsphaltUpgrade::Plant));
		TestEqual(TEXT("capacity"), G.GetBackpackCapacity(), AsphaltBalance::BackpackCapacityL[Level + 1], 1e-9);
		TestEqual(TEXT("rate"), G.GetPlantRatePerMin(), AsphaltBalance::PlantRateLPerMin[Level + 1], 1e-9);
	}
	for (int32 Level = 0; Level < 6; ++Level)
	{
		TestTrue(TEXT("tool price"), G.GetNextPrice(EAsphaltUpgrade::Tool) == AsphaltBalance::ToolPriceMoney[Level]);
		TestTrue(TEXT("buy tool"), G.Buy(EAsphaltUpgrade::Tool));
		TestEqual(TEXT("width"), G.GetToolWidthCm(), 100.0 * AsphaltBalance::ToolWidthM[Level + 1], 1e-9);
	}
	TestFalse(TEXT("backpack maxed"), G.Buy(EAsphaltUpgrade::Backpack));
	TestFalse(TEXT("plant maxed"), G.Buy(EAsphaltUpgrade::Plant));
	TestFalse(TEXT("tool maxed"), G.Buy(EAsphaltUpgrade::Tool));
	TestEqual(TEXT("backpack level"), G.GetLevel(EAsphaltUpgrade::Backpack), 12);
	TestEqual(TEXT("capacity"), G.GetBackpackCapacity(), 200.0, 1e-9);

	// Level changes clamp to the valid range and keep contents within capacity.
	G.DebugSetLevel(EAsphaltUpgrade::Plant, 99);
	TestEqual(TEXT("plant clamped high"), G.GetLevel(EAsphaltUpgrade::Plant), 12);
	G.DebugSetLevel(EAsphaltUpgrade::Plant, -4);
	TestEqual(TEXT("plant clamped low"), G.GetLevel(EAsphaltUpgrade::Plant), 0);
	return true;
}

bool FGameBackpackKeepsContentOnUpgradeTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	FillBackpackFromPlant(G);
	TestEqual(TEXT("5 l inside"), G.GetBackpackLitres(), 5.0, 1e-9);
	G.DebugGive(0.0, 1);
	TestTrue(TEXT("buy backpack"), G.Buy(EAsphaltUpgrade::Backpack));
	TestEqual(TEXT("still 5 l"), G.GetBackpackLitres(), 5.0, 1e-9);
	TestEqual(TEXT("capacity"), G.GetBackpackCapacity(), 7.0, 1e-9);

	// Lowering a level by hand cannot leave more inside than fits: fill the 7 l backpack, then drop to level 0.
	G.Tick(60.0);
	G.RefillBackpack(10.0);
	TestEqual(TEXT("7 l inside"), G.GetBackpackLitres(), 7.0, 1e-9);
	G.DebugSetLevel(EAsphaltUpgrade::Backpack, 0);
	TestEqual(TEXT("capacity back to 5"), G.GetBackpackCapacity(), 5.0, 1e-9);
	TestEqual(TEXT("content clamped to 5 l"), G.GetBackpackLitres(), 5.0, 1e-9);
	return true;
}

bool FGameEventsAppendTest::RunTest(const FString& Parameters)
{
	FAsphaltEvents A;
	A.LitresPoured = 1.5;
	A.MoneyEarned = 1.5;
	A.ShardsEarned = 1;
	A.CompletedCells = {3};
	FAsphaltEvents B;
	B.LitresPoured = 2.0;
	B.MoneyEarned = 2.0;
	B.ShardsEarned = 2;
	B.CompletedCells = {4, 5};
	B.CompletedRoads = {1};
	B.bExitOpened = true;

	A.Append(B);
	TestEqual(TEXT("litres"), A.LitresPoured, 3.5);
	TestEqual(TEXT("money"), A.MoneyEarned, 3.5);
	TestEqual(TEXT("shards"), A.ShardsEarned, 3);
	TestEqual(TEXT("cells"), A.CompletedCells.Num(), 3);
	TestEqual(TEXT("roads"), A.CompletedRoads.Num(), 1);
	TestTrue(TEXT("exit"), A.bExitOpened);
	return true;
}
