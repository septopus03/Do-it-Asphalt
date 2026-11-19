// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "UI/DIAHUD.h"
#include "World/DIAGroundActor.h"
#include "World/DIAKioskActor.h"
#include "World/DIARoadActor.h"
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
		World->SpawnActor<ADIAPlantActor>(FVector(AsphaltLayout::PlantPos.X, AsphaltLayout::PlantPos.Y, 0.0), FRotator::ZeroRotator, Params);

		// The old road behind the end of each of the six roads, closed by a barrier on the five that are not the exit.
		for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
		{
			ADIARoadActor* RoadActor = World->SpawnActorDeferred<ADIARoadActor>(ADIARoadActor::StaticClass(), ADIARoadActor::MakeTransform(Road), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (RoadActor)
			{
				RoadActor->Road = Road;
				RoadActor->FinishSpawning(ADIARoadActor::MakeTransform(Road));
			}
		}

		// One kiosk per special, beside the end of the road that unlocks it, with the screen towards the road.
		for (int32 Index = 0; Index < static_cast<int32>(EAsphaltSpecial::Count); ++Index)
		{
			const EAsphaltSpecial Special = static_cast<EAsphaltSpecial>(Index);
			const int32 Road = AsphaltBalance::SpecialRoad[Index];
			const FVector2D Where = AsphaltLayout::KioskPos(Road);
			const FVector2D ToRoad = AsphaltLayout::RoadEnd(Road) - Where;
			const FTransform Transform(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(ToRoad.Y, ToRoad.X)), 0.0), FVector(Where.X, Where.Y, 0.0));
			ADIAKioskActor* Kiosk = World->SpawnActorDeferred<ADIAKioskActor>(ADIAKioskActor::StaticClass(), Transform, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Kiosk)
			{
				Kiosk->Special = Special;
				Kiosk->FinishSpawning(Transform);
			}
		}
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
