// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "Engine/World.h"
#include "Game/DIAWorldSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LotLayout.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "UI/DIAHUD.h"
#include "World/DIAGroundActor.h"
#include "World/DIAPlantActor.h"

namespace
{
	/** The capsule's centre height when the character stands on the ground (the ground's top is Z = 0). */
	constexpr double SpawnHeightCm = DIACharacter::CapsuleHalfHeightCm;
}

ADIAGameMode::ADIAGameMode()
{
	DefaultPawnClass = ADIACharacter::StaticClass();
	PlayerControllerClass = ADIAPlayerController::StaticClass();
	HUDClass = ADIAHUD::StaticClass();
}

void ADIAGameMode::StartPlay()
{
	// Spawned before Super::StartPlay so that they get their BeginPlay together with everything else.
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ADIAGroundActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		World->SpawnActor<ADIAPlantActor>(FVector(LotLayout::PlantPos.X, LotLayout::PlantPos.Y, 0.0), FRotator(0.0, LotLayout::PlantYawDeg, 0.0), Params);
	}
	Super::StartPlay();
}

void ADIAGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		// The last save of a session: the actors are all still there to say where the player stands.
		Subsystem->Autosave();
	}
	Super::EndPlay(Reason);
}

AActor* ADIAGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// The level's PlayerStart at the layout's position is used as it is; without one a start is created (and kept),
	// so a level without PlayerStart works too.
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (FVector2D::Distance(FVector2D(It->GetActorLocation()), LotLayout::PlayerStart) <= 1.0)
		{
			return *It;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APlayerStart>(FVector(LotLayout::PlayerStart.X, LotLayout::PlayerStart.Y, SpawnHeightCm), FRotator(0.0, LotLayout::PlayerStartYawDeg, 0.0), Params);
}

void ADIAGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
	// The level's own PlayerStart (made by Tools/UE/create_level.py) has no say in where the player looks: body and camera face the trough.
	if (NewPlayer)
	{
		const FRotator Facing(0.0, LotLayout::PlayerStartYawDeg, 0.0);
		if (APawn* Pawn = NewPlayer->GetPawn())
		{
			Pawn->SetActorRotation(Facing);
		}
		NewPlayer->SetControlRotation(Facing);
	}
}
