// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "AsphaltLayout.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
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
}

void ADIAGameMode::StartPlay()
{
	// Spawned before Super::StartPlay so that they get their BeginPlay together with everything else.
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ADIAGroundActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		World->SpawnActor<ADIAPlantActor>(FVector(AsphaltLayout::PlantPos.X, AsphaltLayout::PlantPos.Y, 0.0), FRotator::ZeroRotator, Params);
	}
	Super::StartPlay();
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
		if (FVector2D::Distance(FVector2D(It->GetActorLocation()), AsphaltLayout::PlayerStart) <= 1.0)
		{
			return *It;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APlayerStart>(FVector(AsphaltLayout::PlayerStart.X, AsphaltLayout::PlayerStart.Y, SpawnHeightCm), FRotator::ZeroRotator, Params);
}
