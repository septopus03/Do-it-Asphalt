// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "Engine/World.h"
#include "World/DIAGroundActor.h"

void ADIAGameMode::StartPlay()
{
	// Spawned before Super::StartPlay so that the ground gets its BeginPlay together with everything else.
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ADIAGroundActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	Super::StartPlay();
}
