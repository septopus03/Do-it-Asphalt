// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "World/DIAPortableStation.h"

namespace
{
	constexpr float StationTickSeconds = 0.1f;
	constexpr int32 StationTwoSecondsOfTicks = 20;
	constexpr double FloorGapToleranceCm = 5.0;

	TArray<ADIAPortableStation*> FindStationActors(UWorld* World)
	{
		TArray<ADIAPortableStation*> Result;
		for (TActorIterator<ADIAPortableStation> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		return Result;
	}

	/** What the cheat dia.Special does: grants the special for free and announces it like a purchase. */
	void GrantPortableStation(UDIAWorldSubsystem* Subsystem)
	{
		Subsystem->GetGame().DebugGrantSpecial(EAsphaltSpecial::PortableStation);
		Subsystem->OnSpecialBought.Broadcast(EAsphaltSpecial::PortableStation);
	}

	FVector2D StationWhere(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}
}

DIA_TEST(FStationSpawnTest, "DoItAsphalt.Game.StationSpawn")

bool FStationSpawnTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}

	TestEqual(TEXT("no station before the special is owned"), FindStationActors(World).Num(), 0);
	Mode->EnsureStation();
	TestEqual(TEXT("EnsureStation spawns nothing without the special"), FindStationActors(World).Num(), 0);

	// Bought (or the cheat): the station appears at the core's position, standing, with the model's size.
	GrantPortableStation(Subsystem);
	const TArray<ADIAPortableStation*> Stations = FindStationActors(World);
	if (!TestEqual(TEXT("one station after the purchase"), Stations.Num(), 1))
	{
		return false;
	}
	ADIAPortableStation* Station = Stations[0];
	const FVector2D Spawn = AsphaltLayout::SpecialSpawn(AsphaltBalance::SpecialRoad[static_cast<int32>(EAsphaltSpecial::PortableStation)]);
	TestTrue(TEXT("it stands at the spawn point of road 4"), StationWhere(Station).Equals(Spawn, 0.5));
	TestTrue(TEXT("the same as the core's StationPos"), StationWhere(Station).Equals(Subsystem->GetGame().World().StationPos, 0.5));
	TestFalse(TEXT("it is not carried"), Station->IsCarried());
	TestEqual(TEXT("it stands on the ground"), Station->GetActorLocation().Z, 0.0);
	TestTrue(TEXT("it has a mesh"), Station->GetBodyMesh()->GetStaticMesh() != nullptr);
	TestTrue(TEXT("its collision box has the model's size"), Station->GetCollision()->GetUnscaledBoxExtent().Equals(DIAModelSizes::StationCm * 0.5, 0.1));
	TestEqual(TEXT("the prompt of a standing station"), Station->GetPrompt(nullptr).ToString(), FString(TEXT("E – Pick up")));

	// A second announcement (or a second cheat) never makes a second station.
	GrantPortableStation(Subsystem);
	Mode->EnsureStation();
	TestEqual(TEXT("still one station"), FindStationActors(World).Num(), 1);

	// The actor follows the core: a changed StationPos moves it on the next state change (a loaded save does this).
	Subsystem->GetGame().World().StationPos = FVector2D(-800.0, 90.0);
	Wrapper.TickTestWorld(StationTickSeconds);
	TestTrue(TEXT("it follows StationPos"), StationWhere(Station).Equals(FVector2D(-800.0, 90.0), 0.5));

	// A station that is gone (stale pointer) is spawned again where the core says, once, at the start of a game that owns it.
	Station->Destroy();
	Mode->EnsureStation();
	const TArray<ADIAPortableStation*> Again = FindStationActors(World);
	if (TestEqual(TEXT("respawned once"), Again.Num(), 1))
	{
		TestTrue(TEXT("at the core's position"), StationWhere(Again[0]).Equals(FVector2D(-800.0, 90.0), 0.5));
	}

	// It is an interactable: a character next to it finds it.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-800.0, 190.0));
	if (TestNotNull(TEXT("character"), Character) && Again.Num() == 1)
	{
		AActor* Found = nullptr;
		IDIAInteractable* Interactable = Subsystem->FindInteractable(Character, &Found);
		TestTrue(TEXT("the station is found"), Found == Again[0] && Interactable == Cast<IDIAInteractable>(Again[0]));
	}
	return true;
}

DIA_TEST(FStationRefillsTest, "DoItAsphalt.Game.StationRefills")

bool FStationRefillsTest::RunTest(const FString& Parameters)
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
	FAsphaltGame& Game = Subsystem->GetGame();

	// The plant is full (60 l), the backpack empty; the player owns the station and has put it down at (500, 0).
	Game.Tick(600.0);
	TestEqual(TEXT("plant stock is 60 l"), Game.GetPlantStock(), 60.0);
	GrantPortableStation(Subsystem);
	Game.World().StationPos = FVector2D(500.0, 0.0);
	const TArray<ADIAPortableStation*> Stations = FindStationActors(World);
	if (!TestEqual(TEXT("one station"), Stations.Num(), 1))
	{
		return false;
	}
	ADIAPortableStation* Station = Stations[0];

	// 250 cm from the station is outside the 200 cm radius: nothing is refilled in 2 s.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(750.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	for (int32 I = 0; I < StationTwoSecondsOfTicks; ++I)
	{
		Wrapper.TickTestWorld(StationTickSeconds);
	}
	TestTrue(TEXT("the station moved to StationPos"), StationWhere(Station).Equals(FVector2D(500.0, 0.0), 0.5));
	TestEqual(TEXT("outside the radius the backpack stays empty"), Game.GetBackpackLitres(), 0.0);

	// While the player carries it, it refills nothing even right next to the player.
	Character->SetActorLocation(FVector(550.0, 0.0, DIACharacter::CapsuleHalfHeightCm));
	Station->Interact(Character);
	TestTrue(TEXT("picked up"), Game.World().bStationCarried);
	TestTrue(TEXT("the actor is carried"), Station->IsCarried());
	for (int32 I = 0; I < StationTwoSecondsOfTicks; ++I)
	{
		Wrapper.TickTestWorld(StationTickSeconds);
	}
	TestEqual(TEXT("a carried station does not refill"), Game.GetBackpackLitres(), 0.0);

	// Put down 150 cm in front of the player (700, 0): 150 cm from the character, within 200 cm, so the backpack fills (5 l at level 0).
	Station->Interact(Character);
	TestFalse(TEXT("put down"), Game.World().bStationCarried);
	TestFalse(TEXT("the actor is on the ground"), Station->IsCarried());
	TestTrue(TEXT("StationPos matches the actor"), Game.World().StationPos.Equals(StationWhere(Station), 0.5));
	for (int32 I = 0; I < StationTwoSecondsOfTicks; ++I)
	{
		Wrapper.TickTestWorld(StationTickSeconds);
	}
	TestEqual(TEXT("the placed station fills the backpack (5 l)"), Game.GetBackpackLitres(), 5.0);
	TestTrue(TEXT("the litres came out of the plant stock"), Game.GetPlantStock() < 60.0);

	return true;
}

DIA_TEST(FStationInteractTest, "DoItAsphalt.Game.StationInteract")

bool FStationInteractTest::RunTest(const FString& Parameters)
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
	FAsphaltGame& Game = Subsystem->GetGame();
	GrantPortableStation(Subsystem);
	Game.World().StationPos = FVector2D(0.0, 0.0);
	const TArray<ADIAPortableStation*> Stations = FindStationActors(World);
	if (!TestEqual(TEXT("one station"), Stations.Num(), 1))
	{
		return false;
	}
	ADIAPortableStation* Station = Stations[0];

	// A character at (-200, 0) looking along +X (yaw 0), with a controller and a HUD so that the toast can be seen.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-200.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}
	Controller->Possess(Character);
	Controller->MyHUD = Hud;
	Wrapper.TickTestWorld(StationTickSeconds);

	// Standing: the prompt says "Pick up", and without a character E does nothing.
	TestEqual(TEXT("standing prompt"), Station->GetPrompt(Character).ToString(), FString(TEXT("E – Pick up")));
	Station->Interact(nullptr);
	TestFalse(TEXT("no character, no pick-up"), Game.World().bStationCarried);

	// Picked up: carried 100 cm behind the character, collision off, prompt "Put down".
	Station->Interact(Character);
	TestTrue(TEXT("the core knows it is carried"), Game.World().bStationCarried);
	TestTrue(TEXT("the actor is attached"), Station->IsCarried());
	TestTrue(TEXT("attached to the character"), Station->GetAttachParentActor() == Character);
	TestEqual(TEXT("carried prompt"), Station->GetPrompt(Character).ToString(), FString(TEXT("E – Put down")));
	TestEqual(TEXT("no collision while carried"), Station->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("100 cm behind the character"), StationWhere(Station).Equals(StationWhere(Character) - FVector2D(DIAStation::CarryBehindCm, 0.0), 0.5));
	// A walking character hovers a couple of cm above the floor (the movement component keeps a small gap), and the station hangs at its feet.
	TestEqual(TEXT("feet on the ground"), Station->GetActorLocation().Z, 0.0, FloorGapToleranceCm);
	Character->SetActorLocation(FVector(-100.0, 0.0, DIACharacter::CapsuleHalfHeightCm));
	TestTrue(TEXT("it moves with the character"), StationWhere(Station).Equals(FVector2D(-200.0, 0.0), 0.5));
	TestTrue(TEXT("the carried station can still be used"), Subsystem->FindInteractable(Character) == Cast<IDIAInteractable>(Station));

	// Something blocks the spot 150 cm in front of the character: it stays carried and a toast says why.
	const FVector2D Target = StationWhere(Character) + FVector2D(DIAStation::PutDownAheadCm, 0.0);
	AActor* Blocker = World->SpawnActor<AActor>(Params);
	UBoxComponent* BlockerBox = NewObject<UBoxComponent>(Blocker);
	BlockerBox->SetBoxExtent(FVector(20.0, 20.0, 50.0));
	BlockerBox->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Blocker->SetRootComponent(BlockerBox);
	BlockerBox->RegisterComponent();
	Blocker->SetActorLocation(FVector(Target.X + 50.0, Target.Y, 50.0));
	const int32 ToastsBefore = Hud->GetToasts().Num();
	Station->Interact(Character);
	TestTrue(TEXT("still carried when there is no room"), Game.World().bStationCarried && Station->IsCarried());
	if (TestEqual(TEXT("one toast"), Hud->GetToasts().Num(), ToastsBefore + 1))
	{
		TestEqual(TEXT("the toast says why"), Hud->GetToasts().GetText(ToastsBefore).ToString(), FString(TEXT("No room here")));
	}

	// With the spot free again it is put down there, facing like the character.
	Blocker->Destroy();
	Station->Interact(Character);
	TestFalse(TEXT("put down"), Game.World().bStationCarried);
	TestFalse(TEXT("on the ground"), Station->IsCarried());
	TestTrue(TEXT("150 cm in front of the character"), StationWhere(Station).Equals(Target, 0.5));
	TestTrue(TEXT("StationPos is written"), Game.World().StationPos.Equals(Target, 0.5));
	TestEqual(TEXT("collision is back"), Station->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	TestEqual(TEXT("standing prompt again"), Station->GetPrompt(Character).ToString(), FString(TEXT("E – Pick up")));

	// The core drives the actor: a state that says "carried" (as a loaded save would) attaches the actor to the player.
	Game.World().bStationCarried = true;
	Wrapper.TickTestWorld(StationTickSeconds);
	TestTrue(TEXT("a carried state in the core attaches the station"), Station->GetAttachParentActor() == Character);
	return true;
}

DIA_TEST(FStationTextTest, "DoItAsphalt.Game.StationText")

bool FStationTextTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("pick up"), DIAFormat::StationPrompt(false).ToString(), FString(TEXT("E – Pick up")));
	TestEqual(TEXT("put down"), DIAFormat::StationPrompt(true).ToString(), FString(TEXT("E – Put down")));
	TestEqual(TEXT("no room"), DIAFormat::StationNoRoomToast().ToString(), FString(TEXT("No room here")));
	return true;
}
