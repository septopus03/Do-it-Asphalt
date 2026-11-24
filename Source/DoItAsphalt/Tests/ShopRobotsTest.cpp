// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"
#include "UI/SDIAShopPanel.h"
#include "World/DIAAutoPaver.h"

namespace
{
	constexpr ELotPaverUpgrade Kinds[2] = {ELotPaverUpgrade::Speed, ELotPaverUpgrade::Capacity};

	/** A panel on a started game world, with a HUD that listens to the game (so that a purchase is toasted and announced as in play). */
	struct FRobotsPanelFixture
	{
		/** A new game and a Dev Start delete the save slot: the tests own a slot of their own, so the real one is never touched. */
		struct FSlot
		{
			FSlot() { UDIAWorldSubsystem::SetSlotNameOverride(TEXT("DIA_Test_ShopRobots")); }
			~FSlot() { UDIAWorldSubsystem::SetSlotNameOverride(FString()); }
		} Slot;
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

		FLotGame& Game() const { return Subsystem->GetGame(); }

		/** The robot actors that stand in the world now. */
		int32 CountPaverActors() const
		{
			int32 Count = 0;
			for (TActorIterator<ADIAAutoPaver> It(Wrapper.GetTestWorld()); It; ++It)
			{
				Count += It->IsActorBeingDestroyed() ? 0 : 1;
			}
			return Count;
		}

		bool HasPaverActor(int32 Index) const
		{
			for (TActorIterator<ADIAAutoPaver> It(Wrapper.GetTestWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed() && It->Index == Index)
				{
					return true;
				}
			}
			return false;
		}

		/** The model row of an item. */
		int32 RowOf(ELotItem Item) const
		{
			for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
			{
				if (!Panel->GetModel().GetRow(I).bPlant && Panel->GetModel().GetRow(I).Item == Item)
				{
					return I;
				}
			}
			return INDEX_NONE;
		}

		/**
		 * What Slate does between two frames: the attributes that the widgets read from lambdas (enabled, visible) are polled again. The panel
		 * does not tick in a test, and a widget that is asked before this returns the value it had when it was built.
		 */
		void Frame() const
		{
			Panel->Invalidate(EInvalidateWidgetReason::Prepass);
			Panel->SlatePrepass(1.f);
		}

		/** Clicks the buy button of the card of a model row, as the mouse does; false if there is no card or its button is off. */
		bool Click(int32 Row) const
		{
			Frame();
			const TSharedPtr<SDIAShopButton> Button = Panel->FindBuyButton(Row);
			if (!Button.IsValid() || !Button->GetVisibility().IsHitTestVisible())
			{
				return false;
			}
			// The mouse reaches the button only through widgets that are shown (a card that is hidden, or the button of an owned card, is not).
			for (const SWidget* Parent = Button->GetParentWidget().Get(); Parent; Parent = Parent->GetParentWidget().Get())
			{
				if (!Parent->GetVisibility().IsVisible() || !Parent->GetVisibility().AreChildrenHitTestVisible())
				{
					return false;
				}
			}
			return Button->SimulateClick();
		}

		int32 Level(int32 Paver, ELotPaverUpgrade Kind) const { return Game().GetPaverUpgradeLevel(Paver, Kind); }
	};

	FString Text(const FText& Value) { return Value.ToString(); }
}

DIA_TEST(FShopUpgradeRowsTest, "DoItAsphalt.Game.Shop.UpgradeRows")

bool FShopUpgradeRowsTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAShopModel Model;
	Model.Update(Game);

	// Six rows after the items: speed then capacity of robot 1, then robot 2, then robot 3.
	TestEqual(TEXT("the rows start after the items"), FDIAShopModel::FirstUpgradeRow, 14);
	TestEqual(TEXT("robot 3 capacity is the last row"), FDIAShopModel::UpgradeRow(2, ELotPaverUpgrade::Capacity), FDIAShopModel::NumRows - 1);
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (const ELotPaverUpgrade Kind : Kinds)
		{
			const FDIAShopRow& Row = Model.GetRow(FDIAShopModel::UpgradeRow(Paver, Kind));
			TestTrue(TEXT("an upgrade row"), Row.bPaverUpgrade && !Row.bPlant && Row.Item == ELotItem::Count);
			TestTrue(TEXT("its robot and kind"), Row.Paver == Paver && Row.Upgrade == Kind);
			TestTrue(TEXT("sold in Robots"), Row.Shop == ELotShop::Robots);
			TestEqual(TEXT("its name"), Text(Row.Name), Kind == ELotPaverUpgrade::Speed ? FString(TEXT("Speed")) : FString(TEXT("Capacity")));
			TestTrue(TEXT("locked while the robot is not owned"), Row.State == EDIAShopState::Locked);
			TestEqual(TEXT("it names the robot"), Text(Row.Reason), FString::Printf(TEXT("needs Auto Paver %d"), Paver + 1));
			// A robot that is not owned shows the first step, so that the card is complete before it is bought.
			TestEqual(TEXT("the first step"), Text(Row.Value), Kind == ELotPaverUpgrade::Speed ? FString(TEXT("120 → 150 cells/min")) : FString(TEXT("2,000 → 3,000 l tank")));
			TestEqual(TEXT("its price"), Text(Row.Price), Kind == ELotPaverUpgrade::Speed ? FString(TEXT("20,000")) : FString(TEXT("5,000")));
		}
	}
	// The robot cards show the stats of level 0 while the robots are not owned.
	for (const ELotItem Item : AutoPaverItem)
	{
		for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
		{
			if (!Model.GetRow(I).bPlant && Model.GetRow(I).Item == Item)
			{
				TestEqual(TEXT("a robot that is not owned"), Text(Model.GetRow(I).Value), FString(TEXT("120 cells/min · 2,000\u00A0l\u00A0tank")));
			}
		}
	}

	// Robot 1 is owned: its upgrades are on offer, by the credits; the other robots' are still shut.
	Game.DebugGrantItem(ELotItem::AutoPaver);
	TestTrue(TEXT("owning the robot rebuilds the rows"), Model.Update(Game));
	const int32 SpeedRow = FDIAShopModel::UpgradeRow(0, ELotPaverUpgrade::Speed);
	const int32 CapacityRow = FDIAShopModel::UpgradeRow(0, ELotPaverUpgrade::Capacity);
	TestTrue(TEXT("no credits for speed"), Model.GetRow(SpeedRow).State == EDIAShopState::NoCredits);
	TestTrue(TEXT("no credits for capacity"), Model.GetRow(CapacityRow).State == EDIAShopState::NoCredits);
	TestTrue(TEXT("the reason is gone"), Model.GetRow(SpeedRow).Reason.IsEmpty());
	TestTrue(TEXT("robot 2's speed is still locked"), Model.GetRow(FDIAShopModel::UpgradeRow(1, ELotPaverUpgrade::Speed)).State == EDIAShopState::Locked);
	TestFalse(TEXT("nothing to buy yet"), Model.TargetOf(CapacityRow, Game).bValid);

	// 5,000 credits buy capacity, not speed.
	Game.DebugGive(5000.0);
	TestTrue(TEXT("credits rebuild the rows"), Model.Update(Game));
	TestTrue(TEXT("capacity can be bought"), Model.GetRow(CapacityRow).State == EDIAShopState::Buy);
	TestTrue(TEXT("speed cannot"), Model.GetRow(SpeedRow).State == EDIAShopState::NoCredits);

	// A level changes the texts, the price and the robot's card (the model must see it).
	Game.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 1);
	TestTrue(TEXT("a level changes the key"), Model.Update(Game));
	TestEqual(TEXT("capacity, next step"), Text(Model.GetRow(CapacityRow).Value), FString(TEXT("3,000 → 4,500 l tank")));
	TestEqual(TEXT("capacity, next price"), Text(Model.GetRow(CapacityRow).Price), FString(TEXT("15,000")));
	TestEqual(TEXT("the robot card shows the bigger tank"), Text(Model.GetRow(7).Value), FString(TEXT("120 cells/min · 3,000\u00A0l\u00A0tank")));
	Game.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 1);
	TestTrue(TEXT("a speed level changes the key"), Model.Update(Game));
	TestEqual(TEXT("speed, next step"), Text(Model.GetRow(SpeedRow).Value), FString(TEXT("150 → 180 cells/min")));
	TestEqual(TEXT("speed, next price"), Text(Model.GetRow(SpeedRow).Price), FString(TEXT("60,000")));
	TestEqual(TEXT("the robot card shows both"), Text(Model.GetRow(7).Value), FString(TEXT("150 cells/min · 3,000\u00A0l\u00A0tank")));

	// At the top level: owned, no price, only what is reached.
	Game.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, MaxPaverUpgradeLevel);
	Game.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, MaxPaverUpgradeLevel);
	TestTrue(TEXT("the last level changes the key"), Model.Update(Game));
	TestTrue(TEXT("speed is owned"), Model.GetRow(SpeedRow).State == EDIAShopState::Owned);
	TestTrue(TEXT("capacity is owned"), Model.GetRow(CapacityRow).State == EDIAShopState::Owned);
	TestEqual(TEXT("speed at the maximum"), Text(Model.GetRow(SpeedRow).Value), FString(TEXT("180 cells/min")));
	TestEqual(TEXT("capacity at the maximum"), Text(Model.GetRow(CapacityRow).Value), FString(TEXT("4,500 l tank")));
	TestTrue(TEXT("no price at the maximum"), Model.GetRow(SpeedRow).Price.IsEmpty() && Model.GetRow(CapacityRow).Price.IsEmpty());
	TestFalse(TEXT("no arrow at the maximum"), Text(Model.GetRow(SpeedRow).Value).Contains(TEXT("→")) || Text(Model.GetRow(CapacityRow).Value).Contains(TEXT("→")));
	TestEqual(TEXT("the robot card at the top"), Text(Model.GetRow(7).Value), FString(TEXT("180 cells/min · 4,500\u00A0l\u00A0tank")));
	TestFalse(TEXT("nothing to buy at the maximum"), Model.TargetOf(SpeedRow, Game).bValid);
	TestFalse(TEXT("an unchanged shop is not rebuilt"), Model.Update(Game));
	return true;
}

DIA_TEST(FShopUpgradeBuyTest, "DoItAsphalt.Game.Shop.UpgradeBuyThroughModel")

bool FShopUpgradeBuyTest::RunTest(const FString& Parameters)
{
	FRobotsPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Subsystem->DevStart();
	FLotGame& Game = F.Game();
	Game.DebugGive(1000000.0);
	for (const ELotItem Item : AutoPaverItem)
	{
		TestTrue(TEXT("a robot is bought"), F.Subsystem->BuyItem(Item));
	}
	FDIAShopModel Model;
	Model.Update(Game);

	int32 Announced = 0;
	F.Subsystem->OnPaverUpgradeBought.AddLambda([&Announced](int32, ELotPaverUpgrade) { ++Announced; });

	// Each of the six rows buys its own robot's own kind, one level, and nothing else.
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (const ELotPaverUpgrade Kind : Kinds)
		{
			const int32 Row = FDIAShopModel::UpgradeRow(Paver, Kind);
			const FDIAShopTarget Target = Model.TargetOf(Row, Game);
			TestTrue(TEXT("the target names the upgrade"), Target.bValid && Target.bPaverUpgrade && !Target.bPlant && Target.Paver == Paver && Target.Upgrade == Kind && Target.UpgradeLevel == 0);

			int32 Before[MaxAutoPavers][2];
			for (int32 P = 0; P < MaxAutoPavers; ++P)
			{
				for (int32 K = 0; K < 2; ++K)
				{
					Before[P][K] = Game.GetPaverUpgradeLevel(P, Kinds[K]);
				}
			}
			const double Price = Game.GetPaverUpgradePrice(Paver, Kind);
			const double Credits = Game.GetCredits();
			TestTrue(TEXT("bought through the row"), FDIAShopModel::Buy(Target, *F.Subsystem));
			TestEqual(TEXT("the price is paid"), Game.GetCredits(), Credits - Price);
			for (int32 P = 0; P < MaxAutoPavers; ++P)
			{
				for (int32 K = 0; K < 2; ++K)
				{
					const bool bThis = P == Paver && Kinds[K] == Kind;
					TestEqual(TEXT("only that level rose"), Game.GetPaverUpgradeLevel(P, Kinds[K]), Before[P][K] + (bThis ? 1 : 0));
				}
			}

			// The same request again is a click on a spot that has moved on: it buys nothing.
			TestFalse(TEXT("a stale target buys nothing"), FDIAShopModel::Buy(Target, *F.Subsystem));
			TestEqual(TEXT("so nothing is paid"), Game.GetCredits(), Credits - Price);
		}
	}
	TestEqual(TEXT("six purchases were announced"), Announced, 6);

	// A target that names a level the robot does not have, or a robot that is not owned, buys nothing.
	Model.Update(Game);
	FDIAShopTarget Wrong;
	Wrong.bValid = true;
	Wrong.bPaverUpgrade = true;
	Wrong.Paver = 1;
	Wrong.Upgrade = ELotPaverUpgrade::Speed;
	Wrong.UpgradeLevel = 0;
	TestFalse(TEXT("a level the robot has moved past"), FDIAShopModel::Buy(Wrong, *F.Subsystem));
	Wrong.UpgradeLevel = 1;
	TestTrue(TEXT("the level it really has"), FDIAShopModel::Buy(Wrong, *F.Subsystem));
	TestEqual(TEXT("robot 2 is at the top speed"), Game.GetPaverUpgradeLevel(1, ELotPaverUpgrade::Speed), MaxPaverUpgradeLevel);
	Wrong.UpgradeLevel = MaxPaverUpgradeLevel;
	TestFalse(TEXT("nothing above the maximum"), FDIAShopModel::Buy(Wrong, *F.Subsystem));
	Wrong.Paver = MaxAutoPavers;
	Wrong.UpgradeLevel = -1;
	TestFalse(TEXT("a robot that cannot exist"), FDIAShopModel::Buy(Wrong, *F.Subsystem));

	// Without the credits nothing is bought, and the row says so.
	Game.DebugGive(-Game.GetCredits());
	Model.Update(Game);
	TestTrue(TEXT("robot 1 capacity is on offer"), Model.GetRow(FDIAShopModel::UpgradeRow(0, ELotPaverUpgrade::Capacity)).State == EDIAShopState::NoCredits);
	FDIAShopTarget Broke;
	Broke.bValid = true;
	Broke.bPaverUpgrade = true;
	Broke.Paver = 0;
	Broke.Upgrade = ELotPaverUpgrade::Capacity;
	Broke.UpgradeLevel = 1;
	TestFalse(TEXT("not affordable"), FDIAShopModel::Buy(Broke, *F.Subsystem));
	return true;
}

DIA_TEST(FShopRobotsLayoutTest, "DoItAsphalt.Game.Shop.RobotsLayout")

bool FShopRobotsLayoutTest::RunTest(const FString& Parameters)
{
	FRobotsPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Panel->Refresh();

	// Tools and Garage are the grid: no robot layout, no upgrade cards, one buy button for each card.
	F.Panel->SetShop(ELotShop::Tools);
	TestFalse(TEXT("Tools is the grid"), F.Panel->IsRobotsLayout());
	TestFalse(TEXT("no upgrade card in Tools"), F.Panel->FindUpgradeCard(0, ELotPaverUpgrade::Speed).IsValid());
	TestFalse(TEXT("no hint in Tools"), F.Panel->FindUpgradeHint(0).IsValid());
	for (const int32 Row : F.Panel->GetShownRows())
	{
		TestTrue(TEXT("every card of Tools has its button"), F.Panel->FindBuyButton(Row).IsValid());
	}
	F.Panel->SetShop(ELotShop::Garage);
	TestFalse(TEXT("Garage is the grid"), F.Panel->IsRobotsLayout());
	TestEqual(TEXT("four cards in Garage"), F.Panel->GetShownRows().Num(), 4);

	// Robots: ten cards (the station, three robots, six upgrades), each with its button.
	F.Panel->SetShop(ELotShop::Robots);
	TestTrue(TEXT("Robots is its own layout"), F.Panel->IsRobotsLayout());
	TestEqual(TEXT("ten cards"), F.Panel->GetShownRows().Num(), 10);
	for (const int32 Row : F.Panel->GetShownRows())
	{
		TestTrue(TEXT("every card of Robots has its button"), F.Panel->FindBuyButton(Row).IsValid());
	}
	TestTrue(TEXT("the station card is there"), F.Panel->GetShownRows().Contains(F.RowOf(ELotItem::Station)));
	for (const ELotItem Item : AutoPaverItem)
	{
		TestTrue(TEXT("a robot card is there"), F.Panel->GetShownRows().Contains(F.RowOf(Item)));
	}
	TestFalse(TEXT("a model row of another building has no button here"), F.Panel->FindBuyButton(F.RowOf(ELotItem::Bucket)).IsValid());

	// Nothing owned: the upgrade cards are Hidden (not Collapsed: they keep their room) and the hints stand in it.
	F.Frame();
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (const ELotPaverUpgrade Kind : Kinds)
		{
			const TSharedPtr<SWidget> Card = F.Panel->FindUpgradeCard(Paver, Kind);
			if (!TestTrue(TEXT("the upgrade card is built"), Card.IsValid()))
			{
				return false;
			}
			TestTrue(TEXT("hidden while the robot is not owned"), Card->GetVisibility() == EVisibility::Hidden);
		}
		const TSharedPtr<SWidget> Hint = F.Panel->FindUpgradeHint(Paver);
		if (!TestTrue(TEXT("the hint is built"), Hint.IsValid()))
		{
			return false;
		}
		TestTrue(TEXT("the hint is shown"), Hint->GetVisibility().IsVisible());
	}

	// The panel keeps its size when robots are bought (it is measured, with the cards hidden and shown).
	F.Panel->SlatePrepass(1.f);
	const FVector2D Empty = FVector2D(F.Panel->GetDesiredSize());

	F.Subsystem->DevStart();
	F.Game().DebugGive(1000000.0);
	F.Panel->Refresh();
	TestTrue(TEXT("robot 1 is bought"), F.Subsystem->BuyItem(ELotItem::AutoPaver));
	F.Frame();
	for (const ELotPaverUpgrade Kind : Kinds)
	{
		TestTrue(TEXT("robot 1's card is visible"), F.Panel->FindUpgradeCard(0, Kind)->GetVisibility() == EVisibility::Visible);
		TestTrue(TEXT("robot 2's card is still hidden"), F.Panel->FindUpgradeCard(1, Kind)->GetVisibility() == EVisibility::Hidden);
	}
	TestFalse(TEXT("robot 1's hint is gone"), F.Panel->FindUpgradeHint(0)->GetVisibility().IsVisible());
	TestTrue(TEXT("robot 2's hint stays"), F.Panel->FindUpgradeHint(1)->GetVisibility().IsVisible());
	TestTrue(TEXT("robot 2 is bought"), F.Subsystem->BuyItem(ELotItem::AutoPaver2));
	TestTrue(TEXT("robot 3 is bought"), F.Subsystem->BuyItem(ELotItem::AutoPaver3));
	F.Frame();
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (const ELotPaverUpgrade Kind : Kinds)
		{
			TestTrue(TEXT("every upgrade card is visible"), F.Panel->FindUpgradeCard(Paver, Kind)->GetVisibility() == EVisibility::Visible);
		}
		TestFalse(TEXT("no hint is left"), F.Panel->FindUpgradeHint(Paver)->GetVisibility().IsVisible());
	}
	F.Panel->SlatePrepass(1.f);
	const FVector2D Full = FVector2D(F.Panel->GetDesiredSize());
	TestEqual(TEXT("the width does not change"), Full.X, Empty.X, 0.01);
	TestEqual(TEXT("the height does not change"), Full.Y, Empty.Y, 0.01);
	// At 1280 x 720 the viewport scale is 720 / 1080: the panel must leave 10 screen pixels (15 units) above and below in the 1080 units of height.
	TestTrue(TEXT("the Robots panel fits a 720p screen with room above and below"), Full.Y + 30.0 <= 1080.0);

	// A new game closes them again; a held press does not survive the switch.
	F.Subsystem->NewGame();
	F.Frame();
	for (const ELotPaverUpgrade Kind : Kinds)
	{
		TestTrue(TEXT("hidden again after a new game"), F.Panel->FindUpgradeCard(2, Kind)->GetVisibility() == EVisibility::Hidden);
	}
	TestTrue(TEXT("and the hint is back"), F.Panel->FindUpgradeHint(2)->GetVisibility().IsVisible());
	return true;
}

DIA_TEST(FShopRobotsClickPathTest, "DoItAsphalt.Game.Shop.RobotsClickPath")

bool FShopRobotsClickPathTest::RunTest(const FString& Parameters)
{
	FRobotsPanelFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Subsystem->DevStart();
	FLotGame& Game = F.Game();
	Game.DebugGive(1000000.0);
	F.Panel->SetShop(ELotShop::Robots);
	F.Panel->Refresh();
	TestEqual(TEXT("no robot yet"), Game.GetNumAutoPavers(), 0);
	TestEqual(TEXT("no robot actor yet"), F.CountPaverActors(), 0);

	// Robots 2 and 3 are shut until the one before them is owned: their buttons are off and a click does nothing.
	TestFalse(TEXT("robot 2 cannot be clicked first"), F.Click(F.RowOf(ELotItem::AutoPaver2)));
	TestFalse(TEXT("robot 3 cannot be clicked first"), F.Click(F.RowOf(ELotItem::AutoPaver3)));
	TestFalse(TEXT("an upgrade of a robot that is not owned cannot be clicked"), F.Click(FDIAShopModel::UpgradeRow(0, ELotPaverUpgrade::Speed)));
	TestEqual(TEXT("still no robot"), Game.GetNumAutoPavers(), 0);

	// Each robot is bought by pressing and releasing its own button, and its actor stands in the world.
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		const double Credits = Game.GetCredits();
		TestTrue(*FString::Printf(TEXT("the button of robot %d is clicked"), Paver + 1), F.Click(F.RowOf(AutoPaverItem[Paver])));
		TestEqual(*FString::Printf(TEXT("robots owned after %d clicks"), Paver + 1), Game.GetNumAutoPavers(), Paver + 1);
		TestEqual(TEXT("the robot is paid"), Game.GetCredits(), Credits - Game.GetItemPrice(AutoPaverItem[Paver]));
		TestEqual(TEXT("the actors stand in the world"), F.CountPaverActors(), Paver + 1);
		TestTrue(TEXT("the new one among them"), F.HasPaverActor(Paver));
		// The card is owned now and its button is off: a second click buys nothing more.
		TestTrue(TEXT("the card is owned"), F.Panel->GetModel().GetRow(F.RowOf(AutoPaverItem[Paver])).State == EDIAShopState::Owned);
		TestFalse(TEXT("a robot cannot be bought twice"), F.Click(F.RowOf(AutoPaverItem[Paver])));
		TestEqual(TEXT("still the same number"), Game.GetNumAutoPavers(), Paver + 1);
	}
	TestEqual(TEXT("three robots stand"), F.CountPaverActors(), 3);

	// Upgrades: every click raises the level of its robot only.
	auto ExpectLevels = [&](const TCHAR* When, const int32 (&Speed)[MaxAutoPavers], const int32 (&Capacity)[MaxAutoPavers])
	{
		for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
		{
			TestEqual(*FString::Printf(TEXT("%s: speed of robot %d"), When, Paver + 1), F.Level(Paver, ELotPaverUpgrade::Speed), Speed[Paver]);
			TestEqual(*FString::Printf(TEXT("%s: capacity of robot %d"), When, Paver + 1), F.Level(Paver, ELotPaverUpgrade::Capacity), Capacity[Paver]);
		}
	};
	// The toasts of the three purchases so far are let to expire (the HUD holds only MaxToasts, so a count could not tell a new toast from a dropped one).
	F.Hud->Tick(AsphaltHud::ToastHoldSeconds + AsphaltHud::ToastFadeSeconds + 1.f);
	TestEqual(TEXT("no toast is left"), F.Hud->GetToasts().Num(), 0);
	TestTrue(TEXT("speed of robot 2"), F.Click(FDIAShopModel::UpgradeRow(1, ELotPaverUpgrade::Speed)));
	{
		const int32 Speed[MaxAutoPavers] = {0, 1, 0};
		const int32 Capacity[MaxAutoPavers] = {0, 0, 0};
		ExpectLevels(TEXT("after robot 2's speed"), Speed, Capacity);
	}
	// The HUD toasts the purchase (the panel adds nothing of its own): the click adds exactly one toast in all, and it is the HUD's.
	TestEqual(TEXT("exactly one toast for the click"), F.Hud->GetToasts().Num(), 1);
	TestEqual(TEXT("and it is the upgrade toast"), Text(F.Hud->GetToasts().GetText(0)), FString(TEXT("Paver 2: speed 150 cells/min")));
	TestTrue(TEXT("capacity of robot 3"), F.Click(FDIAShopModel::UpgradeRow(2, ELotPaverUpgrade::Capacity)));
	{
		const int32 Speed[MaxAutoPavers] = {0, 1, 0};
		const int32 Capacity[MaxAutoPavers] = {0, 0, 1};
		ExpectLevels(TEXT("after robot 3's capacity"), Speed, Capacity);
	}
	TestEqual(TEXT("robot 3's tank grew"), Game.GetAutoPaverTankCapacity(2), LotBalance::AutoPaverTankLByLevel[1]);
	TestEqual(TEXT("robot 1's tank did not"), Game.GetAutoPaverTankCapacity(0), LotBalance::AutoPaverTankLByLevel[0]);
	TestEqual(TEXT("robot 2 is faster"), Game.GetAutoPaverRateCellsPerMin(1), LotBalance::AutoPaverRateCellsPerMin[1]);
	TestEqual(TEXT("robot 1 is not"), Game.GetAutoPaverRateCellsPerMin(0), LotBalance::AutoPaverRateCellsPerMin[0]);
	TestTrue(TEXT("capacity of robot 1"), F.Click(FDIAShopModel::UpgradeRow(0, ELotPaverUpgrade::Capacity)));
	TestTrue(TEXT("speed of robot 1"), F.Click(FDIAShopModel::UpgradeRow(0, ELotPaverUpgrade::Speed)));
	{
		const int32 Speed[MaxAutoPavers] = {1, 1, 0};
		const int32 Capacity[MaxAutoPavers] = {1, 0, 1};
		ExpectLevels(TEXT("after robot 1's two"), Speed, Capacity);
	}

	// Every upgrade up to the top, through the buttons; at the top the button is off.
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		for (const ELotPaverUpgrade Kind : Kinds)
		{
			// At most one click per level (a loop on the level could never end if a click did nothing).
			for (int32 Step = F.Level(Paver, Kind); Step < MaxPaverUpgradeLevel; ++Step)
			{
				TestTrue(TEXT("a click raises a level"), F.Click(FDIAShopModel::UpgradeRow(Paver, Kind)));
			}
			TestEqual(TEXT("the top level is reached"), F.Level(Paver, Kind), MaxPaverUpgradeLevel);
			TestFalse(TEXT("nothing above the maximum"), F.Click(FDIAShopModel::UpgradeRow(Paver, Kind)));
			TestEqual(TEXT("the maximum holds"), F.Level(Paver, Kind), MaxPaverUpgradeLevel);
		}
	}
	TestEqual(TEXT("the robots stand as before"), F.CountPaverActors(), 3);

	// Without credits the buttons are off.
	Game.DebugSetPaverUpgrade(1, ELotPaverUpgrade::Speed, 0);
	Game.DebugGive(-Game.GetCredits());
	F.Panel->Refresh();
	TestFalse(TEXT("no credits, no purchase"), F.Click(FDIAShopModel::UpgradeRow(1, ELotPaverUpgrade::Speed)));
	TestEqual(TEXT("the level stays"), F.Level(1, ELotPaverUpgrade::Speed), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
