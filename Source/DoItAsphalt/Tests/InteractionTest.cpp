// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAHUD.h"
#include "World/DIAInteractable.h"

namespace
{
	/** An interactable with a fixed prompt; the box is its collision, which the range is measured to. */
	struct FInteractionTestThing : public IDIAInteractable
	{
		FText Prompt;
		int32 Uses = 0;

		virtual FText GetPrompt(const ADIACharacter* By) const override { return Prompt; }
		virtual void Interact(ADIACharacter* By) override { ++Uses; }
	};

	/** Half the side of the collision box of the fake interactable (cm). */
	constexpr double InteractionTestHalfSideCm = 30.0;

	/** An actor with a blocking box of half side InteractionTestHalfSideCm at Where. */
	AActor* SpawnBoxActor(UWorld* World, FVector2D Where)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(Where.X, Where.Y, 0.0)), Params);
		if (!Actor)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Box"));
		Box->SetBoxExtent(FVector(InteractionTestHalfSideCm, InteractionTestHalfSideCm, 50.0));
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->SetRootComponent(Box);
		Box->RegisterComponent();
		// Level with the character's centre, so that the range is the horizontal distance to the box.
		Actor->SetActorLocation(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm));
		return Actor;
	}

	void PlaceAt(AActor* Actor, FVector2D Where)
	{
		Actor->SetActorLocation(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm));
	}
}

DIA_TEST(FInteractionSearchTest, "DoItAsphalt.Game.InteractionSearch")

bool FInteractionSearchTest::RunTest(const FString& Parameters)
{
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
	const FVector2D ThingPos(-3000.0, 500.0);
	AActor* ThingActor = SpawnBoxActor(World, ThingPos);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, ThingPos + FVector2D(100.0, 0.0));
	if (!TestNotNull(TEXT("interactable actor"), ThingActor) || !TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	FInteractionTestThing Thing;
	Thing.Prompt = FText::FromString(TEXT("E - Use"));
	Subsystem->RegisterInteractable(ThingActor, &Thing);

	AActor* Found = nullptr;
	TestTrue(TEXT("found when close"), Subsystem->FindInteractable(Character, &Found) == &Thing && Found == ThingActor);

	// The range is measured to the collision box, not to the origin: 175 cm from the origin is 145 cm from the box, 190 cm is 160 cm.
	PlaceAt(Character, ThingPos + FVector2D(175.0, 0.0));
	TestTrue(TEXT("still in range 145 cm from the collision"), Subsystem->FindInteractable(Character) != nullptr);
	PlaceAt(Character, ThingPos + FVector2D(190.0, 0.0));
	TestTrue(TEXT("out of range 160 cm from the collision"), Subsystem->FindInteractable(Character) == nullptr);
	PlaceAt(Character, LotLayout::PlayerStart);
	TestTrue(TEXT("nothing at the player's start"), Subsystem->FindInteractable(Character) == nullptr);

	// An empty prompt means nothing to do right now.
	PlaceAt(Character, ThingPos + FVector2D(100.0, 0.0));
	Thing.Prompt = FText::GetEmpty();
	TestTrue(TEXT("an empty prompt is not offered"), Subsystem->FindInteractable(Character) == nullptr);
	Thing.Prompt = FText::FromString(TEXT("E - Use"));

	// The player controller uses the same search, but not while an input mode other than the game takes the keys.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params))
	{
		Controller->Possess(Character);
		TestTrue(TEXT("the controller finds it"), Controller->FindInteractable() == &Thing);
		Controller->SetMenuInput(true);
		TestTrue(TEXT("with the menu open nothing can be used"), Controller->FindInteractable() == nullptr);
		Controller->SetMenuInput(false);
		TestTrue(TEXT("with the menu closed again it is found"), Controller->FindInteractable() == &Thing);
	}
	else
	{
		AddError(TEXT("no controller"));
	}

	// An actor that goes away is forgotten.
	Subsystem->UnregisterInteractable(ThingActor);
	TestTrue(TEXT("an unregistered thing is not found"), Subsystem->FindInteractable(Character) == nullptr);
	return true;
}

DIA_TEST(FWinScreenShownTest, "DoItAsphalt.Game.WinScreenShown")

bool FWinScreenShownTest::RunTest(const FString& Parameters)
{
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
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::PlayerStart);
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	Controller->Possess(Character);
	Params.Owner = Controller;
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}
	AActor* ThingActor = SpawnBoxActor(World, LotLayout::PlayerStart + FVector2D(100.0, 0.0));
	FInteractionTestThing Thing;
	Thing.Prompt = FText::FromString(TEXT("E - Use"));
	if (TestNotNull(TEXT("interactable actor"), ThingActor))
	{
		Subsystem->RegisterInteractable(ThingActor, &Thing);
	}

	TestFalse(TEXT("no win screen at first"), Hud->IsWinScreenShown());
	TestFalse(TEXT("and the controller is in game input"), Controller->IsWinInput());
	TestTrue(TEXT("a thing can be used"), Controller->FindInteractable() == &Thing);

	// A win before the exit is open shows nothing.
	Subsystem->Win();
	TestFalse(TEXT("an early win shows nothing"), Hud->IsWinScreenShown());
	Subsystem->Dispatch(Subsystem->GetGame().DebugCompleteCells(AsphaltLot::NumCells));
	TestTrue(TEXT("the exit is open"), Subsystem->GetGame().IsExitOpen());
	Subsystem->Win();
	TestTrue(TEXT("the win shows the win screen"), Hud->IsWinScreenShown());
	TestTrue(TEXT("the controller now takes UI input"), Controller->IsWinInput());
	TestTrue(TEXT("nothing can be used"), Controller->FindInteractable() == nullptr);

	// The HUD going away gives the input back.
	Hud->Destroy();
	TestFalse(TEXT("the controller gave the win input back"), Controller->IsWinInput());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
