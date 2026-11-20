// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Engine/World.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotMap.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"

namespace
{
	/** The index of the row of an item; INDEX_NONE if the model has none. */
	int32 RowOfItem(const FDIAShopModel& Model, ELotItem Item)
	{
		for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
		{
			if (!Model.GetRow(I).bPlant && Model.GetRow(I).Item == Item)
			{
				return I;
			}
		}
		return INDEX_NONE;
	}
}

DIA_TEST(FShopRowsTest, "DoItAsphalt.Game.Shop.Rows")

bool FShopRowsTest::RunTest(const FString& Parameters)
{
	auto Is = [this](const TCHAR* What, const FText& Actual, const TCHAR* Expected)
	{
		TestEqual(What, Actual.ToString(), FString(Expected));
	};

	FLotGame Game;
	FDIAShopModel Model;
	TestTrue(TEXT("the first update builds the rows"), Model.Update(Game));
	if (!TestEqual(TEXT("the plant and eleven items"), FDIAShopModel::NumRows, 12))
	{
		return false;
	}

	// The plant first.
	const FDIAShopRow& Plant = Model.GetRow(0);
	TestTrue(TEXT("row 0 is the plant"), Plant.bPlant);
	Is(TEXT("plant value"), Plant.Value, TEXT("150 → 300 l/min"));
	Is(TEXT("plant price"), Plant.Price, TEXT("100"));
	TestTrue(TEXT("the plant is too expensive for a new game"), Plant.State == EDIAShopState::NoCredits);
	TestTrue(TEXT("a plant that cannot be bought says nothing is locked"), Plant.Reason.IsEmpty());

	// The items follow in ELotItem order.
	for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
	{
		const FDIAShopRow& Row = Model.GetRow(1 + I);
		TestFalse(TEXT("an item row"), Row.bPlant);
		TestTrue(*FString::Printf(TEXT("row %d is item %d"), 1 + I, I), Row.Item == static_cast<ELotItem>(I));
		TestEqual(TEXT("its name"), Row.Name.ToString(), DIAFormat::ItemName(static_cast<ELotItem>(I)).ToString());
		TestFalse(TEXT("it has a value"), Row.Value.IsEmpty());
	}
	Is(TEXT("scoop price"), Model.GetRow(1).Price, TEXT("10"));
	Is(TEXT("suitcase value"), Model.GetRow(3).Value, TEXT("60 l"));
	Is(TEXT("cart value"), Model.GetRow(4).Value, TEXT("200 l"));
	Is(TEXT("finisher value"), Model.GetRow(6).Value, TEXT("85 cells/min"));
	Is(TEXT("auto paver upgrade"), Model.GetRow(8).Value, TEXT("120 → 170 cells/min"));
	Is(TEXT("auto paver upgrade price"), Model.GetRow(8).Price, TEXT("9,000"));
	Is(TEXT("wide bar value"), Model.GetRow(11).Value, TEXT("260 → 300 cells/min"));
	TestTrue(TEXT("the scoop is too expensive for nothing"), Model.GetRow(1).State == EDIAShopState::NoCredits);

	// The machines are visible and locked, with the reason.
	TestTrue(TEXT("the station is locked"), Model.GetRow(5).State == EDIAShopState::Locked);
	Is(TEXT("the station's reason"), Model.GetRow(5).Reason, TEXT("350 cells + entry 3"));
	Is(TEXT("the finisher's reason"), Model.GetRow(6).Reason, TEXT("350 cells + entry 3"));
	Is(TEXT("the auto paver's reason"), Model.GetRow(7).Reason, TEXT("800 cells + entry 4"));
	Is(TEXT("the car's reason"), Model.GetRow(10).Reason, TEXT("6,000 cells + entry 5"));
	TestTrue(TEXT("the unlocked rows have no reason"), Model.GetRow(1).Reason.IsEmpty());

	// With money the first four items and the plant can be bought.
	Game.DebugGive(1000.0);
	TestTrue(TEXT("money changes the buttons"), Model.Update(Game));
	TestTrue(TEXT("the plant can be bought"), Model.GetRow(0).State == EDIAShopState::Buy);
	TestTrue(TEXT("the cart can be bought"), Model.GetRow(4).State == EDIAShopState::Buy);
	TestTrue(TEXT("the station stays locked however rich"), Model.GetRow(5).State == EDIAShopState::Locked);
	return true;
}

DIA_TEST(FShopRequirementsTest, "DoItAsphalt.Game.Shop.Requirements")

bool FShopRequirementsTest::RunTest(const FString& Parameters)
{
	// Everything up to the car is open, but the upgrades wait for what they upgrade.
	FLotGame Game;
	Game.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::Car)]);
	for (int32 Entry = 0; Entry < static_cast<int32>(ELotUnlock::Exit); ++Entry)
	{
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			Game.DebugCompleteCell(AsphaltLot::EntryCell(Entry, I));
		}
	}
	Game.DebugGive(100000.0);
	FDIAShopModel Model;
	Model.Update(Game);

	const FDIAShopRow& Fast = Model.GetRow(RowOfItem(Model, ELotItem::AutoPaverFast));
	TestTrue(TEXT("the faster auto paver is locked"), Fast.State == EDIAShopState::Locked);
	TestEqual(TEXT("it names the auto paver"), Fast.Reason.ToString(), FString(TEXT("needs Auto Paver")));
	TestTrue(TEXT("the auto paver itself can be bought"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver)).State == EDIAShopState::Buy);
	TestEqual(TEXT("the wide bar needs the car"), Model.GetRow(RowOfItem(Model, ELotItem::CarWideBar)).Reason.ToString(), FString(TEXT("needs Paver Car")));

	Game.DebugGrantItem(ELotItem::AutoPaver);
	TestTrue(TEXT("buying it changes the rows"), Model.Update(Game));
	TestTrue(TEXT("the upgrade can be bought now"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaverFast)).State == EDIAShopState::Buy);
	TestTrue(TEXT("the second upgrade still needs the first"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaverFaster)).State == EDIAShopState::Locked);
	TestEqual(TEXT("it names the first upgrade"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaverFaster)).Reason.ToString(), FString(TEXT("needs Fast Auto Paver")));
	return true;
}

DIA_TEST(FShopPlantMaxTest, "DoItAsphalt.Game.Shop.PlantMax")

bool FShopPlantMaxTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	Game.DebugGive(100000.0);
	Game.DebugSetPlantLevel(static_cast<int32>(std::size(LotBalance::PlantPrice)));
	FDIAShopModel Model;
	Model.Update(Game);

	// The highest level is owned: no price, the rate that is reached, and the row goes to the end with the owned ones.
	const FDIAShopRow& Plant = Model.GetRow(FDIAShopModel::NumRows - 1);
	TestTrue(TEXT("the plant row"), Plant.bPlant);
	TestTrue(TEXT("owned"), Plant.State == EDIAShopState::Owned);
	TestTrue(TEXT("no price"), Plant.Price.IsEmpty());
	TestEqual(TEXT("the rate that is reached"), Plant.Value.ToString(), FString(TEXT("5,200 l/min")));
	TestFalse(TEXT("no arrow at the maximum"), Plant.Value.ToString().Contains(TEXT("→")));
	return true;
}

DIA_TEST(FShopBuyThroughModelTest, "DoItAsphalt.Game.Shop.BuyThroughModel")

bool FShopBuyThroughModelTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = Wrapper.GetTestWorld()->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FLotGame& Game = Subsystem->GetGame();
	Game.DebugGive(10.0 - Game.GetCredits());
	TestEqual(TEXT("ten credits"), Game.GetCredits(), 10.0);

	int32 Broadcasts = 0;
	Subsystem->OnItemBought.AddLambda([&Broadcasts](ELotItem Item) { Broadcasts += Item == ELotItem::Scoop ? 1 : 0; });

	FDIAShopModel Model;
	Model.Update(Game);
	const int32 Scoop = RowOfItem(Model, ELotItem::Scoop);
	TestEqual(TEXT("the scoop is the first item row"), Scoop, 1);
	TestTrue(TEXT("it can be bought"), Model.GetRow(Scoop).State == EDIAShopState::Buy);
	TestFalse(TEXT("the plant row cannot be bought with ten credits"), Model.Buy(0, *Subsystem));

	TestTrue(TEXT("bought through the row"), Model.Buy(Scoop, *Subsystem));
	TestEqual(TEXT("the credits are spent"), Game.GetCredits(), 0.0);
	TestTrue(TEXT("the scoop is owned"), Game.Owns(ELotItem::Scoop));
	TestEqual(TEXT("the purchase was broadcast"), Broadcasts, 1);
	TestTrue(TEXT("the rows are rebuilt"), Model.Update(Game));
	const int32 Moved = RowOfItem(Model, ELotItem::Scoop);
	TestEqual(TEXT("the owned row is the last one"), Moved, FDIAShopModel::NumRows - 1);
	TestTrue(TEXT("it is owned"), Model.GetRow(Moved).State == EDIAShopState::Owned);
	TestTrue(TEXT("owned rows have no price"), Model.GetRow(Moved).Price.IsEmpty());
	TestFalse(TEXT("a row cannot be bought twice"), Model.Buy(Moved, *Subsystem));
	TestEqual(TEXT("the bucket moved up"), Model.GetRow(1).Item, ELotItem::Bucket);

	// Owned rows keep their order at the end.
	Game.DebugGrantItem(ELotItem::Bucket);
	Model.Update(Game);
	TestEqual(TEXT("the scoop before the bucket"), RowOfItem(Model, ELotItem::Scoop), FDIAShopModel::NumRows - 2);
	TestEqual(TEXT("the bucket last"), RowOfItem(Model, ELotItem::Bucket), FDIAShopModel::NumRows - 1);

	// The plant goes through the subsystem as well.
	Game.DebugGive(1000.0);
	Model.Update(Game);
	TestTrue(TEXT("the plant is bought through the row"), Model.Buy(0, *Subsystem));
	TestEqual(TEXT("one level more"), Game.GetPlantLevel(), 1);
	return true;
}

DIA_TEST(FShopOnlyChangesTest, "DoItAsphalt.Game.Shop.OnlyChangesWhenShownValueChanges")

bool FShopOnlyChangesTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = Wrapper.GetTestWorld()->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FLotGame& Game = Subsystem->GetGame();
	FDIAShopModel Model;
	Model.Update(Game);
	const int32 Rebuilds = Model.GetRebuildCount();

	// The subsystem reports a state change on every tick; nothing shown changes, so nothing is rebuilt.
	for (int32 I = 0; I < 20; ++I)
	{
		Subsystem->Dispatch(FLotEvents());
		TestFalse(TEXT("an unchanged shop is not rebuilt"), Model.Update(Game));
	}
	TestEqual(TEXT("no rebuild"), Model.GetRebuildCount(), Rebuilds);

	// Credits that change nothing about what can be bought: no rebuild; crossing a price: rebuild.
	Game.DebugGive(5.0);
	TestFalse(TEXT("five credits buy nothing new"), Model.Update(Game));
	Game.DebugGive(5.0);
	TestTrue(TEXT("ten credits buy the scoop"), Model.Update(Game));
	TestEqual(TEXT("one rebuild more"), Model.GetRebuildCount(), Rebuilds + 1);
	return true;
}

DIA_TEST(FShopToggleTest, "DoItAsphalt.Game.Shop.Toggle")

bool FShopToggleTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("HUD"), Hud) || !TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}

	TestFalse(TEXT("the shop starts closed"), Hud->IsShopOpen());
	Hud->ToggleShop();
	TestTrue(TEXT("Tab opens the shop"), Hud->IsShopOpen());

	// The game runs on while the shop is open: the plant keeps producing.
	const double Stock = Subsystem->GetGame().GetPlantStock();
	for (int32 I = 0; I < 10; ++I)
	{
		Wrapper.TickTestWorld(0.1f);
	}
	TestTrue(TEXT("the plant produces while the shop is open"), Subsystem->GetGame().GetPlantStock() > Stock);
	TestTrue(TEXT("still open"), Hud->IsShopOpen());

	Hud->ToggleShop();
	TestFalse(TEXT("Tab closes the shop"), Hud->IsShopOpen());
	Hud->SetShopOpen(true);
	Hud->SetShopOpen(true);
	TestTrue(TEXT("SetShopOpen(true) is idempotent"), Hud->IsShopOpen());

	// A menu or the win screen owns the screen: the shop closes under the menu and does not open over it.
	Hud->OpenMenu(EDIAMenuMode::Pause);
	TestFalse(TEXT("the shop closes when a menu opens"), Hud->IsShopOpen());
	Hud->SetShopOpen(true);
	TestFalse(TEXT("the shop does not open over a menu"), Hud->IsShopOpen());
	Hud->CloseMenu();
	Hud->SetShopOpen(true);
	TestTrue(TEXT("it opens again once the menu is closed"), Hud->IsShopOpen());
	Hud->SetShopOpen(false);
	TestFalse(TEXT("SetShopOpen(false) closes"), Hud->IsShopOpen());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
