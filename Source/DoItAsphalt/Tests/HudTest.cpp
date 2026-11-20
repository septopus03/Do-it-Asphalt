// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "LotBalance.h"
#include "LotGame.h"
#include "AsphaltTest.h"
#include "Engine/World.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAWorldSubsystem.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"

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
	TestEqual(TEXT("unlock toast"), Hud->GetToasts().GetText(2).ToString(), FString(TEXT("Sprint unlocked")));

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

#endif // WITH_DEV_AUTOMATION_TESTS
