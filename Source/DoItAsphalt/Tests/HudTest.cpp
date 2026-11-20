// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "LotBalance.h"
#include "LotGame.h"
#include "AsphaltTest.h"
#include "Engine/World.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAWorldSubsystem.h"
#include "EngineUtils.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"
#include "World/DIAGroundActor.h"

namespace
{
	/** Completes the twelve cells of an entry segment. */
	void CompleteEntryCells(FLotGame& Game, int32 Entry)
	{
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			Game.DebugCompleteCell(AsphaltLot::EntryCell(Entry, I));
		}
	}
}

DIA_TEST(FHudActorTest, "DoItAsphalt.Game.HudActor")

bool FHudActorTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("the game mode uses the HUD"), GetDefault<ADIAGameMode>()->HUDClass.Get(), ADIAHUD::StaticClass());

	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	// The world clamps long frames, so time passes in small steps.
	auto TickFor = [&Wrapper](float Seconds)
	{
		for (float T = 0.f; T < Seconds - 1e-4f; T += 0.1f)
		{
			Wrapper.TickTestWorld(0.1f);
		}
	};
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}
	TestFalse(TEXT("no widgets when nothing can render"), Hud->HasOverlay());

	// Prompt: empty by default, set, hidden again by empty text.
	TestTrue(TEXT("no prompt at first"), Hud->GetPrompt().IsEmpty());
	Hud->SetPrompt(FText::FromString(TEXT("E - Buy Scoop (10)")));
	TestEqual(TEXT("prompt shown"), Hud->GetPrompt().ToString(), FString(TEXT("E - Buy Scoop (10)")));
	Hud->SetPrompt(FText::GetEmpty());
	TestTrue(TEXT("empty text hides the prompt"), Hud->GetPrompt().IsEmpty());

	// Toast: shown, fades after 2 s (the world ticks the HUD).
	Hud->ShowToast(FText::FromString(TEXT("Hello")));
	TestEqual(TEXT("one toast"), Hud->GetToasts().Num(), 1);
	TickFor(1.9f);
	TestEqual(TEXT("still there before 2 s"), Hud->GetToasts().Num(), 1);
	TestEqual(TEXT("fully visible before 2 s"), Hud->GetToasts().GetAlpha(0), 1.f);
	TickFor(1.0f);
	TestEqual(TEXT("gone after the fade"), Hud->GetToasts().Num(), 0);

	// Game events become toasts: cells -> credits, entry, unlock.
	FLotEvents Events;
	Events.CompletedCells = {1, 2, 3};
	Events.CreditsEarned = 3 * LotBalance::CreditsPerCell;
	Events.CompletedEntries = {1};
	Events.Unlocked = {ELotUnlock::Sprint};
	Subsystem->Dispatch(Events);
	TestEqual(TEXT("cells, entry and unlock give three toasts"), Hud->GetToasts().Num(), 3);
	TestEqual(TEXT("credits toast"), Hud->GetToasts().GetText(0).ToString(), FString(TEXT("+30 credits")));
	TestEqual(TEXT("entry toast"), Hud->GetToasts().GetText(1).ToString(), FString(TEXT("Entry 2 complete")));
	TestEqual(TEXT("unlock toast"), Hud->GetToasts().GetText(2).ToString(), DIAFormat::UnlockToast(ELotUnlock::Sprint).ToString());

	TickFor(3.0f);
	FLotEvents ExitEvents;
	ExitEvents.bExitOpened = true;
	Subsystem->Dispatch(ExitEvents);
	TestEqual(TEXT("exit toast"), Hud->GetToasts().GetText(0).ToString(), FString(TEXT("The exit is open!")));

	TickFor(3.0f);
	TestEqual(TEXT("toasts are gone"), Hud->GetToasts().Num(), 0);
	Subsystem->GetGame().DebugGive(1000.0);
	TestTrue(TEXT("the scoop can be bought"), Subsystem->BuyItem(ELotItem::Scoop));
	TestEqual(TEXT("item toast"), Hud->GetToasts().GetText(0).ToString(), FString(TEXT("Scoop bought")));

	// Once destroyed the HUD no longer listens.
	Hud->Destroy();
	Wrapper.TickTestWorld(0.1f);
	Events = FLotEvents();
	Events.bExitOpened = true;
	Subsystem->Dispatch(Events);
	return true;
}

DIA_TEST(FHudNextUnlockLineTest, "DoItAsphalt.Game.Hud.NextUnlockLine")

bool FHudNextUnlockLineTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAHudModel Model;
	Model.Update(Game);
	TestEqual(TEXT("a new game asks for the first step"), Model.GetView().Next.ToString(), FString(TEXT("Next: Quick Swap – entry 0 / 12, cells 0 / 10")));

	// The entry of the first step done: it is unlocked, the second is next, with its own entry cells and the cells of the car park.
	CompleteEntryCells(Game, 0);
	for (int32 I = 0; I < 3; ++I)
	{
		Game.DebugCompleteCell(AsphaltLot::EntryCell(1, I));
	}
	TestTrue(TEXT("the first step is open"), Game.IsUnlocked(ELotUnlock::QuickSwap));
	TestTrue(TEXT("the line follows"), Model.Update(Game));
	TestEqual(TEXT("the next step"), Model.GetView().Next.ToString(), FString(TEXT("Next: Sprint – entry 3 / 12, cells 15 / 100")));

	// Everything but the exit open: the exit is next.
	Game.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::Car)]);
	for (int32 Entry = 1; Entry < static_cast<int32>(ELotUnlock::Exit); ++Entry)
	{
		CompleteEntryCells(Game, Entry);
	}
	for (int32 I = 0; I < static_cast<int32>(ELotUnlock::Exit); ++I)
	{
		TestTrue(*FString::Printf(TEXT("step %d is open"), I), Game.IsUnlocked(static_cast<ELotUnlock>(I)));
	}
	TestTrue(TEXT("the line follows again"), Model.Update(Game));
	TestTrue(TEXT("the exit is next"), Model.GetView().Next.ToString().StartsWith(TEXT("Next: Exit – entry ")));
	TestTrue(TEXT("with the cells of the whole car park"), Model.GetView().Next.ToString().EndsWith(TEXT(" / 30,000")));

	// Won: nothing is next.
	Game.DebugCompleteCells(AsphaltLot::NumCells);
	TestTrue(TEXT("the exit is open"), Game.IsExitOpen());
	Game.SetWon();
	TestTrue(TEXT("won"), Game.HasWon());
	TestTrue(TEXT("the line changes"), Model.Update(Game));
	TestTrue(TEXT("no next step after the win"), Model.GetView().Next.IsEmpty());
	return true;
}

DIA_TEST(FHudHandGaugeTest, "DoItAsphalt.Game.Hud.HandGauge")

bool FHudHandGaugeTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAHudModel Model;
	Model.Update(Game);
	TestEqual(TEXT("the bowl is in the hand"), Model.GetView().HandLabel.ToString(), FString(TEXT("Bowl")));
	TestEqual(TEXT("an empty bowl"), Model.GetView().HandGauge.ToString(), FString(TEXT("0 / 2.5 l")));

	// Five spoons of a quarter litre; litres are rounded down, so that "full" means full.
	for (int32 I = 0; I < 5; ++I)
	{
		Game.ScoopFromTrough(LotBalance::ScoopCycleSeconds);
	}
	TestEqual(TEXT("litres in the bowl"), Game.GetHandLitres(ELotHand::Bowl), 5 * LotBalance::SpoonLitres, 1e-9);
	TestTrue(TEXT("the gauge follows"), Model.Update(Game));
	TestEqual(TEXT("1.25 litres show as 1.2"), Model.GetView().HandGauge.ToString(), FString(TEXT("1.2 / 2.5 l")));
	Game.DebugFillHand();
	Model.Update(Game);
	TestEqual(TEXT("a full bowl"), Model.GetView().HandGauge.ToString(), FString(TEXT("2.5 / 2.5 l")));

	// The trowel holds nothing: no gauge.
	TestTrue(TEXT("the trowel is taken"), Game.SelectHand(ELotHand::Trowel));
	Model.Update(Game);
	TestTrue(TEXT("no gauge for the trowel"), Model.GetView().HandGauge.IsEmpty());

	// A held suitcase shows its content instead.
	Game.DebugGrantItem(ELotItem::Suitcase);
	Game.FillMovable(ELotMovable::Suitcase, 4.0);
	TestTrue(TEXT("the suitcase is taken"), Game.Grab(ELotMovable::Suitcase));
	Model.Update(Game);
	TestEqual(TEXT("the suitcase is held"), Model.GetView().HandLabel.ToString(), FString(TEXT("Suitcase")));
	TestEqual(TEXT("the content of the suitcase"), Model.GetView().HandGauge.ToString(), FString(TEXT("10 / 60 l")));
	Game.Release(FVector2D(0.0, 0.0), 0.0);
	Model.Update(Game);
	TestTrue(TEXT("set down: the trowel again, no gauge"), Model.GetView().HandGauge.IsEmpty());

	// The held finisher has no content to show.
	Game.DebugGrantItem(ELotItem::Finisher);
	TestTrue(TEXT("the finisher is taken"), Game.Grab(ELotMovable::Finisher));
	Model.Update(Game);
	TestTrue(TEXT("no gauge for the finisher"), Model.GetView().HandGauge.IsEmpty());
	return true;
}

DIA_TEST(FHudRefillLineTest, "DoItAsphalt.Game.Hud.RefillLine")

bool FHudRefillLineTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAHudModel Model;
	Game.World().PlayerPos = LotLayout::PlayerStart;
	Model.Update(Game);
	TestFalse(TEXT("the start is outside the refill zone"), DIAFormat::IsInRefillZone(Game));
	TestTrue(TEXT("no refill line outside the zone"), Model.GetView().Refill.IsEmpty());

	// The plant keeps producing, but outside the zone nothing shown changes.
	const int32 Rebuilds = Model.GetRebuildCount();
	Game.Tick(0.5);
	TestFalse(TEXT("production outside the zone does not rebuild the view"), Model.Update(Game));
	TestEqual(TEXT("no rebuild"), Model.GetRebuildCount(), Rebuilds);

	Game.World().PlayerPos = LotLayout::TroughPos + FVector2D(0.0, 150.0);
	TestTrue(TEXT("beside the trough"), DIAFormat::IsInRefillZone(Game));
	TestTrue(TEXT("the line appears"), Model.Update(Game));
	TestEqual(TEXT("stock and storage"), Model.GetView().Refill.ToString(), DIAFormat::Litres(Game.GetPlantStock(), Game.GetPlantStorage()).ToString());
	TestTrue(TEXT("the storage is four minutes of output"), Model.GetView().Refill.ToString().EndsWith(TEXT(" / 600 l")));

	// In the zone the stock moves, and the line follows it.
	Game.Tick(1.0);
	TestTrue(TEXT("the stock grows in the zone"), Model.Update(Game));

	// The suitcase held by the player counts: the player may stand beside the suitcase that is held in the zone.
	Game.World().PlayerPos = LotLayout::PlayerStart;
	Model.Update(Game);
	TestTrue(TEXT("out of the zone again"), Model.GetView().Refill.IsEmpty());
	Game.DebugGrantItem(ELotItem::Suitcase);
	TestTrue(TEXT("the suitcase is taken"), Game.Grab(ELotMovable::Suitcase));
	Game.World().Movables[static_cast<int32>(ELotMovable::Suitcase)].Pos = LotLayout::TroughPos + FVector2D(0.0, 100.0);
	Model.Update(Game);
	TestFalse(TEXT("the held suitcase in the zone shows the line"), Model.GetView().Refill.IsEmpty());
	return true;
}

DIA_TEST(FCheckModelTest, "DoItAsphalt.Game.Hud.CheckModel")

bool FCheckModelTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIACheckModel Model;
	const FVector2D From(0.0, -9950.0);

	Model.Update(Game, false, 1.0f, From);
	TestFalse(TEXT("off: inactive"), Model.GetView().bActive);
	TestTrue(TEXT("off: no text"), Model.GetView().Nearest.IsEmpty());
	TestEqual(TEXT("off: nothing is searched"), Model.GetSearchCount(), 0);

	Game.World().PlayerPos = FVector2D(0.0, 9000.0); // a stale copy in the core must not matter
	Model.Update(Game, true, 0.016f, From);
	TestTrue(TEXT("the search starts from the position it is given"), Model.GetView().Target.Y < -9000.0);
	TestTrue(TEXT("on: active"), Model.GetView().bActive);
	TestEqual(TEXT("on: searched at once"), Model.GetSearchCount(), 1);
	TestFalse(TEXT("a text for the nearest cell"), Model.GetView().Nearest.IsEmpty());

	// Twice a second at most.
	for (int32 I = 0; I < 10; ++I)
	{
		Model.Update(Game, true, 0.04f, From);
	}
	TestEqual(TEXT("0.4 s later: still the first search"), Model.GetSearchCount(), 1);
	Model.Update(Game, true, 0.11f, From);
	TestEqual(TEXT("past half a second: searched again"), Model.GetSearchCount(), 2);

	Model.Update(Game, false, 0.1f, From);
	TestFalse(TEXT("released: inactive"), Model.GetView().bActive);
	TestTrue(TEXT("released: no text"), Model.GetView().Nearest.IsEmpty());
	Model.Update(Game, true, 0.016f, From);
	TestEqual(TEXT("held again: searched at once"), Model.GetSearchCount(), 3);

	// Nothing unfinished: no text, but the view is on.
	Game.DebugCompleteCells(AsphaltLot::NumCells);
	Model.Update(Game, true, 1.0f, From);
	TestTrue(TEXT("all done: active"), Model.GetView().bActive);
	TestTrue(TEXT("all done: no text"), Model.GetView().Nearest.IsEmpty());
	return true;
}

DIA_TEST(FHudCheckViewTest, "DoItAsphalt.Game.Hud.CheckView")

bool FHudCheckViewTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	TActorIterator<ADIAGroundActor> GroundIt(World);
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestTrue(TEXT("ground"), static_cast<bool>(GroundIt)))
	{
		return false;
	}
	UMaterialInstanceDynamic* Material = GroundIt->GetGroundMaterial();
	if (!TestNotNull(TEXT("ground material"), Material))
	{
		return false;
	}
	auto Highlight = [Material]()
	{
		float Value = -1.f;
		return Material->GetScalarParameterValue(TEXT("MissingHighlight"), Value) ? Value : -1.f;
	};
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}

	// The first five rows of the car park are done: standing at the very bottom, the nearest unfinished cell is about five metres away.
	FLotGame& Game = Subsystem->GetGame();
	Game.DebugCompleteCells(5 * AsphaltLot::Side);
	Game.World().PlayerPos = FVector2D(0.0, -9950.0);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(0.0, -9950.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}

	TestFalse(TEXT("the check view starts off"), Hud->IsCheckView());
	TestEqual(TEXT("no highlight at first"), Highlight(), 0.f);
	Wrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("no text while off"), Hud->GetCheckView().Nearest.IsEmpty());

	Hud->SetCheckView(true);
	TestTrue(TEXT("held"), Hud->IsCheckView());
	TestEqual(TEXT("the ground highlights what is missing"), Highlight(), 1.f);
	Wrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("the HUD is active"), Hud->GetCheckView().bActive);
	// Distance and arrow come from where the pawn is, not from the core's copy.
	const FVector2D Here(Character->GetActorLocation());
	const int32 Nearest = Game.FindNearestIncomplete(Here);
	if (TestTrue(TEXT("something is unfinished"), Nearest != INDEX_NONE))
	{
		const double Expected = FVector2D::Distance(Here, AsphaltLot::CellCentre(Nearest)) / 100.0;
		TestEqual(TEXT("about five metres"), Expected, 5.0, 1.0);
		TestTrue(TEXT("the text names the distance"), Hud->GetCheckView().Nearest.ToString().Contains(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Expected))));
		TestTrue(TEXT("the arrow points at the cell"), Hud->GetCheckView().Target.Equals(AsphaltLot::CellCentre(Nearest), 1.0));
	}

	Hud->SetCheckView(false);
	TestEqual(TEXT("the highlight goes"), Highlight(), 0.f);
	Wrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("no text after the release"), Hud->GetCheckView().Nearest.IsEmpty());
	TestFalse(TEXT("inactive after the release"), Hud->GetCheckView().bActive);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
