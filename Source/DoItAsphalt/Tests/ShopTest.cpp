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
#include "UI/DIAMenuModel.h"
#include "UI/SDIAShopPanel.h"

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
	if (!TestEqual(TEXT("the plant, thirteen items and six robot upgrades"), FDIAShopModel::NumRows, 20))
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
	Is(TEXT("bucket price"), Model.GetRow(1).Price, TEXT("35"));
	Is(TEXT("canister price"), Model.GetRow(2).Price, TEXT("70"));
	Is(TEXT("tub price"), Model.GetRow(3).Price, TEXT("160"));
	Is(TEXT("trolley value"), Model.GetRow(4).Value, TEXT("80 l"));
	Is(TEXT("cart value"), Model.GetRow(5).Value, TEXT("200 l"));
	Is(TEXT("robot 1 name"), Model.GetRow(7).Name, TEXT("Auto Paver 1"));
	Is(TEXT("robot 1 value"), Model.GetRow(7).Value, TEXT("120 cells/min · 2,000\u00A0l\u00A0tank"));
	Is(TEXT("robot 1 price"), Model.GetRow(7).Price, TEXT("3,000"));
	Is(TEXT("car value"), Model.GetRow(8).Value, TEXT("260 cells/min"));
	Is(TEXT("car II value"), Model.GetRow(9).Value, TEXT("260 → 300 cells/min"));
	Is(TEXT("car III value"), Model.GetRow(10).Value, TEXT("300 → 380 cells/min"));
	Is(TEXT("car IV value"), Model.GetRow(11).Value, TEXT("380 → 480 cells/min"));
	Is(TEXT("car II price"), Model.GetRow(9).Price, TEXT("24,000"));
	Is(TEXT("car III price"), Model.GetRow(10).Price, TEXT("48,000"));
	Is(TEXT("car IV price"), Model.GetRow(11).Price, TEXT("96,000"));
	Is(TEXT("robot 2 name"), Model.GetRow(12).Name, TEXT("Auto Paver 2"));
	Is(TEXT("robot 3 name"), Model.GetRow(13).Name, TEXT("Auto Paver 3"));
	Is(TEXT("robot 2 value: the same level-0 stats as robot 1"), Model.GetRow(12).Value, TEXT("120 cells/min · 2,000\u00A0l\u00A0tank"));
	Is(TEXT("robot 3 value"), Model.GetRow(13).Value, TEXT("120 cells/min · 2,000\u00A0l\u00A0tank"));
	Is(TEXT("robot 3 price"), Model.GetRow(13).Price, TEXT("80,000"));
	Is(TEXT("the robots come right after the car"), Model.GetRow(11).Name, TEXT("Paver Car IV"));
	TestTrue(TEXT("the bucket is too expensive for nothing"), Model.GetRow(1).State == EDIAShopState::NoCredits);

	// The machines and the big vessels are visible and locked, with the reason.
	TestTrue(TEXT("the station is locked"), Model.GetRow(6).State == EDIAShopState::Locked);
	Is(TEXT("the station's reason"), Model.GetRow(6).Reason, TEXT("350 cells + entry 3"));
	Is(TEXT("the auto paver's reason"), Model.GetRow(7).Reason, TEXT("800 cells + entry 4"));
	Is(TEXT("the car's reason"), Model.GetRow(8).Reason, TEXT("6,000 cells + entry 5"));
	TestTrue(TEXT("the unlocked rows have no reason"), Model.GetRow(1).Reason.IsEmpty());
	TestTrue(TEXT("the tub needs the sprint step"), Model.GetRow(3).State == EDIAShopState::Locked);

	// With money the first items and the plant can be bought.
	Game.DebugGive(1000.0);
	TestTrue(TEXT("money changes the buttons"), Model.Update(Game));
	TestTrue(TEXT("the plant can be bought"), Model.GetRow(0).State == EDIAShopState::Buy);
	TestTrue(TEXT("the cart can be bought"), Model.GetRow(5).State == EDIAShopState::Buy);
	TestTrue(TEXT("the station stays locked however rich"), Model.GetRow(6).State == EDIAShopState::Locked);
	return true;
}

DIA_TEST(FShopRequirementsTest, "DoItAsphalt.Game.Shop.Requirements")

bool FShopRequirementsTest::RunTest(const FString& Parameters)
{
	// Everything up to the car is open, but the extra robots wait for the one before them.
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

	const FDIAShopRow& Second = Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver2));
	TestTrue(TEXT("robot 2 is locked"), Second.State == EDIAShopState::Locked);
	TestEqual(TEXT("it names the first robot"), Second.Reason.ToString(), FString(TEXT("needs Auto Paver 1")));
	TestEqual(TEXT("robot 3 needs robot 2"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver3)).Reason.ToString(), FString(TEXT("needs Auto Paver 2")));
	TestTrue(TEXT("the first robot itself can be bought"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver)).State == EDIAShopState::Buy);
	TestEqual(TEXT("car II needs the car"), Model.GetRow(RowOfItem(Model, ELotItem::CarT2)).Reason.ToString(), FString(TEXT("needs Paver Car")));
	TestEqual(TEXT("car III needs car II"), Model.GetRow(RowOfItem(Model, ELotItem::CarT3)).Reason.ToString(), FString(TEXT("needs Paver Car II")));
	TestEqual(TEXT("car IV needs car III"), Model.GetRow(RowOfItem(Model, ELotItem::CarT4)).Reason.ToString(), FString(TEXT("needs Paver Car III")));

	Game.DebugGrantItem(ELotItem::AutoPaver);
	TestTrue(TEXT("buying it changes the rows"), Model.Update(Game));
	TestTrue(TEXT("robot 2 can be bought now"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver2)).State == EDIAShopState::Buy);
	TestTrue(TEXT("robot 3 still needs robot 2"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver3)).State == EDIAShopState::Locked);
	TestEqual(TEXT("it names robot 2"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver3)).Reason.ToString(), FString(TEXT("needs Auto Paver 2")));
	return true;
}

DIA_TEST(FShopReasonTest, "DoItAsphalt.Game.Shop.ReasonFollowsTheGame")

bool FShopReasonTest::RunTest(const FString& Parameters)
{
	// The locked reason is part of what the model watches: when its step opens, the text moves from the step to the item.
	FLotGame Game;
	FDIAShopModel Model;
	Model.Update(Game);
	const int32 Faster = RowOfItem(Model, ELotItem::AutoPaver2);
	TestEqual(TEXT("closed step"), Model.GetRow(Faster).Reason.ToString(), FString(TEXT("800 cells + entry 4")));
	const int32 Rebuilds = Model.GetRebuildCount();
	Game.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::AutoPaver)]);
	for (int32 Entry = 0; Entry <= static_cast<int32>(ELotUnlock::AutoPaver); ++Entry)
	{
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			Game.DebugCompleteCell(AsphaltLot::EntryCell(Entry, I));
		}
	}
	TestTrue(TEXT("opening the step rebuilds"), Model.Update(Game));
	TestEqual(TEXT("one pass"), Model.GetRebuildCount(), Rebuilds + 1);
	TestEqual(TEXT("the reason names the item now"), Model.GetRow(RowOfItem(Model, ELotItem::AutoPaver2)).Reason.ToString(), FString(TEXT("needs Auto Paver 1")));
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

	// The highest level is owned: no price, and the rate that is reached; the row stays first.
	const FDIAShopRow& Plant = Model.GetRow(0);
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
	Game.DebugGive(35.0 - Game.GetCredits());
	TestEqual(TEXT("exactly the price of a bucket"), Game.GetCredits(), 35.0);

	int32 Broadcasts = 0;
	Subsystem->OnItemBought.AddLambda([&Broadcasts](ELotItem Item) { Broadcasts += Item == ELotItem::Bucket ? 1 : 0; });

	FDIAShopModel Model;
	Model.Update(Game);
	const int32 Scoop = RowOfItem(Model, ELotItem::Bucket);
	TestEqual(TEXT("the bucket is the first item row"), Scoop, 1);
	TestTrue(TEXT("it can be bought"), Model.GetRow(Scoop).State == EDIAShopState::Buy);
	TestFalse(TEXT("the plant row cannot be bought with the price of a bucket"), Model.Buy(Model.TargetOf(0, Game), *Subsystem));

	TestTrue(TEXT("bought through the row"), Model.Buy(Model.TargetOf(Scoop, Game), *Subsystem));
	TestEqual(TEXT("the credits are spent"), Game.GetCredits(), 0.0);
	TestTrue(TEXT("the bucket is owned"), Game.Owns(ELotItem::Bucket));
	TestEqual(TEXT("the purchase was broadcast"), Broadcasts, 1);
	TestTrue(TEXT("the rows are refreshed"), Model.Update(Game));
	TestEqual(TEXT("the owned row stays where it was"), RowOfItem(Model, ELotItem::Bucket), Scoop);
	TestTrue(TEXT("it is owned"), Model.GetRow(Scoop).State == EDIAShopState::Owned);
	TestTrue(TEXT("owned rows have no price"), Model.GetRow(Scoop).Price.IsEmpty());
	TestFalse(TEXT("a row cannot be bought twice"), Model.Buy(Model.TargetOf(Scoop, Game), *Subsystem));
	TestEqual(TEXT("the canister did not move up"), Model.GetRow(Scoop + 1).Item, ELotItem::Canister);

	// Owned rows stay where they are, however many there are.
	Game.DebugGrantItem(ELotItem::Trolley);
	Model.Update(Game);
	TestEqual(TEXT("the bucket stays"), RowOfItem(Model, ELotItem::Bucket), Scoop);
	TestEqual(TEXT("the trolley also stays"), RowOfItem(Model, ELotItem::Trolley), Scoop + 3);
	TestTrue(TEXT("both are owned"), Model.GetRow(Scoop).State == EDIAShopState::Owned && Model.GetRow(Scoop + 3).State == EDIAShopState::Owned);

	// The plant goes through the subsystem as well.
	Game.DebugGive(1000.0);
	Model.Update(Game);
	TestTrue(TEXT("the plant is bought through the row"), Model.Buy(Model.TargetOf(0, Game), *Subsystem));
	TestEqual(TEXT("one level more"), Game.GetPlantLevel(), 1);
	return true;
}

DIA_TEST(FShopRowsStayPutTest, "DoItAsphalt.Game.Shop.RowsStayPutWhileOpen")

bool FShopRowsStayPutTest::RunTest(const FString& Parameters)
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
	Game.DebugGive(500.0 - Game.GetCredits());
	FDIAShopModel Model;
	Model.Update(Game);

	ELotItem Before[FDIAShopModel::NumRows];
	for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
	{
		Before[I] = Model.GetRow(I).Item;
	}

	// Buy the bucket through its row.
	const int32 Row = RowOfItem(Model, ELotItem::Bucket);
	const FDIAShopTarget Pressed = Model.TargetOf(Row, Game);
	TestTrue(TEXT("the bucket can be bought"), Pressed.bValid && Pressed.Item == ELotItem::Bucket);
	TestTrue(TEXT("bought"), Model.Buy(Pressed, *Subsystem));
	Model.Update(Game);
	TestTrue(TEXT("the same row is Owned"), Model.GetRow(Row).State == EDIAShopState::Owned && Model.GetRow(Row).Item == ELotItem::Bucket);
	for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
	{
		TestTrue(*FString::Printf(TEXT("row %d did not move"), I), Model.GetRow(I).Item == Before[I]);
	}

	// Pressing the same spot again meets the owned bucket: nothing to buy, and the rows next to it are not touched.
	TestFalse(TEXT("the same index offers nothing"), Model.TargetOf(Row, Game).bValid);
	TestFalse(TEXT("so nothing is bought"), Model.Buy(Model.TargetOf(Row, Game), *Subsystem));
	TestFalse(TEXT("the trolley is not owned"), Game.Owns(ELotItem::Trolley));
	TestEqual(TEXT("only the bucket was paid"), Game.GetCredits(), 465.0);

	// A different row right after the first purchase is a legitimate click.
	const FDIAShopTarget Trolley = Model.TargetOf(RowOfItem(Model, ELotItem::Trolley), Game);
	TestTrue(TEXT("the trolley row offers the trolley"), Trolley.bValid && Trolley.Item == ELotItem::Trolley);
	TestTrue(TEXT("a quick second purchase of another row works"), Model.Buy(Trolley, *Subsystem));
	TestTrue(TEXT("the trolley is owned"), Game.Owns(ELotItem::Trolley));

	// A request that is not valid any more buys nothing (not affordable, not offered, no target).
	Game.DebugGive(-Game.GetCredits());
	Model.Update(Game);
	FDIAShopTarget Cart;
	Cart.bValid = true;
	Cart.Item = ELotItem::Cart;
	TestFalse(TEXT("not affordable"), Model.Buy(Cart, *Subsystem));
	Game.DebugGive(100000.0);
	FDIAShopTarget Station;
	Station.bValid = true;
	Station.Item = ELotItem::Station;
	TestFalse(TEXT("not offered"), Model.Buy(Station, *Subsystem));
	TestFalse(TEXT("no target"), Model.Buy(FDIAShopTarget(), *Subsystem));

	// The plant stays in its row; a request for a level that was already bought does nothing.
	Model.Update(Game);
	const FDIAShopTarget Plant = Model.TargetOf(0, Game);
	TestTrue(TEXT("the plant request names the level"), Plant.bValid && Plant.bPlant && Plant.PlantLevel == 0);
	TestTrue(TEXT("level 1"), Model.Buy(Plant, *Subsystem));
	TestFalse(TEXT("the same request again buys nothing"), Model.Buy(Plant, *Subsystem));
	TestEqual(TEXT("still level 1"), Game.GetPlantLevel(), 1);

	// Whatever was bought, nothing has moved.
	Model.Update(Game);
	TestEqual(TEXT("the trolley is still where it was"), RowOfItem(Model, ELotItem::Trolley), 4);
	return true;
}

DIA_TEST(FShopDevStartTest, "DoItAsphalt.Game.Shop.DevStart")

bool FShopDevStartTest::RunTest(const FString& Parameters)
{
	// Dev Start: a new game in which the whole shop can be bought (and the exit is still closed).
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
	Subsystem->DevStart();
	FLotGame& Game = Subsystem->GetGame();
	TestFalse(TEXT("the exit is closed"), Game.IsExitOpen());
	TestTrue(TEXT("the unlock cells are done"), Game.GetCompletedCells() >= LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::Car)]);

	for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
	{
		const ELotItem Item = static_cast<ELotItem>(I);
		TestTrue(*FString::Printf(TEXT("item %d is offered"), I), Game.IsItemOffered(Item));
		TestTrue(*FString::Printf(TEXT("item %d can be bought"), I), Game.BuyItem(Item));
	}
	for (int32 Level = 1; Level <= static_cast<int32>(std::size(LotBalance::PlantPrice)); ++Level)
	{
		TestTrue(*FString::Printf(TEXT("plant level %d can be bought"), Level), Game.BuyPlant());
	}
	TestEqual(TEXT("everything is owned"), Game.GetNumItemsOwned(), static_cast<int32>(ELotItem::Count));
	TestTrue(TEXT("the credits were enough"), Game.GetCredits() >= 0.0);
	return true;
}

DIA_TEST(FMenuDevStartTest, "DoItAsphalt.Game.Menu.DevStartButton")

bool FMenuDevStartTest::RunTest(const FString& Parameters)
{
	// Only the main menu offers Dev Start, and not while the question or the settings are up.
	FDIAMenuModel Model;
	Model.Open(EDIAMenuMode::Main, false);
	TestEqual(TEXT("the main menu offers it outside Shipping"), Model.ShowsDevStart(), FDIAMenuModel::bDevStartAvailable);
	Model.bConfirming = true;
	TestFalse(TEXT("not at the question"), Model.ShowsDevStart());
	Model.Open(EDIAMenuMode::Pause, false);
	TestFalse(TEXT("not in the pause menu"), Model.ShowsDevStart());
	Model.Open(EDIAMenuMode::Main, true);
	Model.OpenSettings();
	TestFalse(TEXT("not in the settings"), Model.ShowsDevStart());
	return true;
}

DIA_TEST(FShopDoubleClickTest, "DoItAsphalt.Game.Shop.DoubleClick")

bool FShopDoubleClickTest::RunTest(const FString& Parameters)
{
	// The smallest unit that embodies the rule: the buy button swallows a double click, so no press starts and no click follows.
	// A real click through Slate needs mouse capture and hover from the application, which a headless test does not have; what is
	// proven here is that the second press of a double click does not reach OnPressed and does not leave the button pressed.
	int32 Presses = 0;
	int32 Clicks = 0;
	auto Press = [&Presses]() { ++Presses; };
	auto Click = [&Clicks]() { ++Clicks; return FReply::Handled(); };

	TSharedRef<SDIAShopButton> Button = SNew(SDIAShopButton).OnPressed_Lambda(Press).OnClicked_Lambda(Click);
	TSharedRef<SButton> Plain = SNew(SButton).OnPressed_Lambda(Press).OnClicked_Lambda(Click);
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(100.0, 40.0), FSlateLayoutTransform());
	const FPointerEvent Event(0, 0, FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), TSet<FKey>({EKeys::LeftMouseButton}), EKeys::LeftMouseButton, 0.f, FModifierKeysState());

	// The engine's own button treats the second press of a double click as a press: that is the double purchase.
	TestTrue(TEXT("the engine's button handles a double click"), Plain->OnMouseButtonDoubleClick(Geometry, Event).IsEventHandled());
	TestTrue(TEXT("and presses"), Plain->IsPressed());
	TestEqual(TEXT("which fires OnPressed"), Presses, 1);

	Presses = 0;
	TestTrue(TEXT("the shop's button handles the double click"), Button->OnMouseButtonDoubleClick(Geometry, Event).IsEventHandled());
	TestFalse(TEXT("but does not press"), Button->IsPressed());
	TestEqual(TEXT("OnPressed does not fire"), Presses, 0);
	// With no press, the release that follows cannot click.
	Button->OnMouseButtonUp(Geometry, Event);
	TestEqual(TEXT("the release after a double click does not click"), Clicks, 0);
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
	Game.DebugGive(30.0);
	TestTrue(TEXT("35 credits buy the bucket"), Model.Update(Game));
	TestEqual(TEXT("one rebuild more"), Model.GetRebuildCount(), Rebuilds + 1);

	// An unchanged key formats nothing: the rebuild counter counts the passes that format the rows.
	const int32 Passes = Model.GetRebuildCount();
	for (int32 I = 0; I < 50; ++I)
	{
		Model.Update(Game);
	}
	TestEqual(TEXT("an unchanged key does no row formatting"), Model.GetRebuildCount(), Passes);
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
	Hud->SetShopOpen(!Hud->IsShopOpen());
	TestTrue(TEXT("the HUD opens the shop"), Hud->IsShopOpen());
	TestTrue(TEXT("with no building named it is the tools"), Hud->GetOpenShop() == ELotShop::Tools);

	// The game runs on while the shop is open: the plant keeps producing.
	const double Stock = Subsystem->GetGame().GetPlantStock();
	for (int32 I = 0; I < 10; ++I)
	{
		Wrapper.TickTestWorld(0.1f);
	}
	TestTrue(TEXT("the plant produces while the shop is open"), Subsystem->GetGame().GetPlantStock() > Stock);
	TestTrue(TEXT("still open"), Hud->IsShopOpen());

	Hud->SetShopOpen(!Hud->IsShopOpen());
	TestFalse(TEXT("the HUD closes the shop"), Hud->IsShopOpen());
	TestTrue(TEXT("a closed shop names no building"), Hud->GetOpenShop() == ELotShop::Count);
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

namespace
{
	/** A panel on a started game world, for the tests that look at what one building shows. */
	struct FShopPanelFixture
	{
		FTestWorldWrapper Wrapper;
		UDIAWorldSubsystem* Subsystem = nullptr;
		ADIAHUD* Hud = nullptr;
		TSharedPtr<SDIAShopPanel> Panel;

		bool Start(FAutomationTestBase& Test)
		{
			if (!DIATest::StartGameWorld(Wrapper, Test))
			{
				return false;
			}
			UWorld* World = Wrapper.GetTestWorld();
			Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Hud = World->SpawnActor<ADIAHUD>(Params);
			if (!Test.TestNotNull(TEXT("HUD"), Hud) || !Test.TestNotNull(TEXT("world subsystem"), Subsystem))
			{
				return false;
			}
			Panel = SNew(SDIAShopPanel).Hud(Hud).Subsystem(Subsystem);
			return true;
		}
	};
}

DIA_TEST(FShopRowsByBuildingTest, "DoItAsphalt.Game.Shop.RowsByBuilding")

bool FShopRowsByBuildingTest::RunTest(const FString& Parameters)
{
	FShopPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const FDIAShopModel& Model = F.Panel->GetModel();

	F.Panel->SetShop(ELotShop::Tools);
	const TArray<int32> Tools = F.Panel->GetShownRows();
	F.Panel->SetShop(ELotShop::Robots);
	const TArray<int32> Robots = F.Panel->GetShownRows();
	F.Panel->SetShop(ELotShop::Garage);
	const TArray<int32> Garage = F.Panel->GetShownRows();

	TestEqual(TEXT("Tools: the plant and five items"), Tools.Num(), 6);
	TestEqual(TEXT("Robots: the station, three robots and their six upgrades"), Robots.Num(), 10);
	TestEqual(TEXT("Garage: four"), Garage.Num(), 4);
	if (Tools.Num() == 6)
	{
		TestTrue(TEXT("the plant row comes first in Tools"), Model.GetRow(Tools[0]).bPlant);
	}
	for (const int32 Row : Robots)
	{
		TestTrue(TEXT("a robot row is Robots"), Model.GetRow(Row).Shop == ELotShop::Robots);
	}
	for (const int32 Row : Garage)
	{
		TestTrue(TEXT("a garage row is Garage"), Model.GetRow(Row).Shop == ELotShop::Garage);
	}
	// Model order inside a building.
	for (int32 I = 1; I < Robots.Num(); ++I)
	{
		TestTrue(TEXT("rows keep the model order"), Robots[I - 1] < Robots[I]);
	}

	// Together they are every row, and none is in two buildings.
	TSet<int32> All;
	for (const TArray<int32>* Shop : {&Tools, &Robots, &Garage})
	{
		for (const int32 Row : *Shop)
		{
			TestFalse(TEXT("a row is in one building only"), All.Contains(Row));
			All.Add(Row);
		}
	}
	TestEqual(TEXT("the union is every row of the model"), All.Num(), FDIAShopModel::NumRows);
	// The upgrades are the robots' rows (nothing else sells them).
	for (int32 I = FDIAShopModel::FirstUpgradeRow; I < FDIAShopModel::NumRows; ++I)
	{
		TestTrue(TEXT("an upgrade row is in Robots"), Robots.Contains(I));
	}
	return true;
}

DIA_TEST(FShopTitlePerBuildingTest, "DoItAsphalt.Game.Shop.TitlePerBuilding")

bool FShopTitlePerBuildingTest::RunTest(const FString& Parameters)
{
	struct FExpected { ELotShop Shop; const TCHAR* Title; const TCHAR* Subtitle; };
	const FExpected Expected[] = {
		{ELotShop::Tools, TEXT("Tools"), TEXT("Hand tools and the plant")},
		{ELotShop::Robots, TEXT("Robots"), TEXT("Robots and the portable station")},
		{ELotShop::Garage, TEXT("Garage"), TEXT("Trucks")}};
	for (const FExpected& E : Expected)
	{
		TestEqual(TEXT("title"), DIAFormat::ShopName(E.Shop).ToString(), FString(E.Title));
		TestEqual(TEXT("subtitle"), DIAFormat::ShopSubtitle(E.Shop).ToString(), FString(E.Subtitle));
	}

	// The panel shows the title of the building it was given.
	FShopPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	for (const FExpected& E : Expected)
	{
		F.Panel->SetShop(E.Shop);
		TestTrue(TEXT("the panel keeps its building"), F.Panel->GetShop() == E.Shop);
	}
	F.Hud->OpenShop(ELotShop::Garage);
	TestTrue(TEXT("the HUD names the open building"), F.Hud->GetOpenShop() == ELotShop::Garage);
	F.Hud->OpenShop(ELotShop::Robots);
	TestTrue(TEXT("opening another building switches"), F.Hud->GetOpenShop() == ELotShop::Robots);
	F.Hud->SetShopOpen(false);
	TestTrue(TEXT("closed names none"), F.Hud->GetOpenShop() == ELotShop::Count);
	F.Hud->SetShopOpen(true);
	TestTrue(TEXT("SetShopOpen(true) opens the last one"), F.Hud->GetOpenShop() == ELotShop::Robots);
	return true;
}

DIA_TEST(FShopHandStripOnlyInToolsTest, "DoItAsphalt.Game.Shop.HandStripOnlyInTools")

bool FShopHandStripOnlyInToolsTest::RunTest(const FString& Parameters)
{
	FShopPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Panel->SetShop(ELotShop::Tools);
	TestTrue(TEXT("the strip is in Tools"), F.Panel->IsHandStripShown());
	F.Panel->SetShop(ELotShop::Robots);
	TestFalse(TEXT("not in Robots"), F.Panel->IsHandStripShown());
	F.Panel->SetShop(ELotShop::Garage);
	TestFalse(TEXT("not in Garage"), F.Panel->IsHandStripShown());
	F.Panel->SetShop(ELotShop::Tools);
	TestTrue(TEXT("and back in Tools"), F.Panel->IsHandStripShown());
	return true;
}

DIA_TEST(FShopBuyInItsBuildingTest, "DoItAsphalt.Game.Shop.BuyInItsBuilding")

bool FShopBuyInItsBuildingTest::RunTest(const FString& Parameters)
{
	FShopPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Subsystem->DevStart();
	FLotGame& Game = F.Subsystem->GetGame();

	// Each thing is bought from the card of its own building, as it was from the one panel.
	auto BuyShown = [&](ELotShop Shop, ELotItem Item)
	{
		F.Panel->SetShop(Shop);
		F.Panel->Refresh();
		const FDIAShopModel& Model = F.Panel->GetModel();
		const int32 Row = RowOfItem(Model, Item);
		TestTrue(TEXT("the item's card is in this building"), F.Panel->GetShownRows().Contains(Row));
		TestFalse(TEXT("not owned before"), Game.Owns(Item));
		TestTrue(TEXT("bought from its card"), FDIAShopModel::Buy(Model.TargetOf(Row, Game), *F.Subsystem));
		TestTrue(TEXT("owned after"), Game.Owns(Item));
	};
	BuyShown(ELotShop::Tools, ELotItem::Bucket);
	BuyShown(ELotShop::Robots, ELotItem::AutoPaver);
	BuyShown(ELotShop::Garage, ELotItem::Car);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
