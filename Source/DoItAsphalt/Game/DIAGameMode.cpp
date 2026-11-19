// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "Engine/World.h"
#include "Game/DIAWorldSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "UI/DIAHUD.h"
#include "World/DIAAutoPaver.h"
#include "World/DIAExitGate.h"
#include "World/DIAGroundActor.h"
#include "World/DIAKioskActor.h"
#include "World/DIAPaverCar.h"
#include "World/DIAPortableStation.h"
#include "World/DIARoadActor.h"
#include "World/DIASiteFence.h"
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

	// The exit gate closes the gap of road 5 and the fence the rest of the circle around the site.
	EnsureExitGate();
	EnsureSiteFence();

	// The station, the paver and the truck follow the core: a purchase (or the cheat) announces itself here, and ones owned from the start are spawned now.
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		SpecialHandle = Subsystem->OnSpecialBought.AddUObject(this, &ADIAGameMode::OnSpecialBought);
		ResetHandle = Subsystem->OnGameReset.AddUObject(this, &ADIAGameMode::OnGameReset);
	}
	EnsureStation();
	EnsureAutoPaver();
	EnsureCar();
	Super::StartPlay();
}

void ADIAGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		// The last save of a session: the actors are all still there to say where the player stands.
		Subsystem->Autosave();
		Subsystem->OnSpecialBought.Remove(SpecialHandle);
		Subsystem->OnGameReset.Remove(ResetHandle);
	}
	SpecialHandle.Reset();
	ResetHandle.Reset();
	Super::EndPlay(Reason);
}

void ADIAGameMode::OnGameReset()
{
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	const FAsphaltGame& Game = Subsystem->GetGame();
	if (Game.HasSpecial(EAsphaltSpecial::PortableStation))
	{
		EnsureStation();
	}
	else
	{
		RemoveSpecialActor(Station);
	}
	if (Game.HasSpecial(EAsphaltSpecial::AutoPaver))
	{
		EnsureAutoPaver();
	}
	else
	{
		RemoveSpecialActor(AutoPaver);
	}
	if (Game.HasSpecial(EAsphaltSpecial::PaverCar))
	{
		EnsureCar();
	}
	else
	{
		RemoveSpecialActor(Car);
	}
}

template <typename ActorType>
void ADIAGameMode::RemoveSpecialActor(TWeakObjectPtr<ActorType>& Slot)
{
	if (ActorType* Actor = Slot.Get())
	{
		Actor->Destroy();
	}
	Slot.Reset();
}

void ADIAGameMode::OnSpecialBought(EAsphaltSpecial Special)
{
	if (Special == EAsphaltSpecial::PortableStation)
	{
		EnsureStation();
	}
	else if (Special == EAsphaltSpecial::AutoPaver)
	{
		EnsureAutoPaver();
	}
	else if (Special == EAsphaltSpecial::PaverCar)
	{
		EnsureCar();
	}
}

template <typename ActorType>
void ADIAGameMode::EnsureSpecialActor(TWeakObjectPtr<ActorType>& Slot, EAsphaltSpecial Special, TFunctionRef<FVector2D(const FAsphaltGame&)> WhereInCore, double HeightCm)
{
	UWorld* World = GetWorld();
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!World || !Subsystem || Slot.IsValid() || !Subsystem->GetGame().HasSpecial(Special))
	{
		return;
	}
	// It starts where the core says (SpecialSpawn of its road after a purchase); the actor then keeps itself in line with the core.
	const FVector2D Where = WhereInCore(Subsystem->GetGame());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Slot = World->SpawnActor<ActorType>(FVector(Where.X, Where.Y, HeightCm), FRotator::ZeroRotator, Params);
}

void ADIAGameMode::EnsureStation()
{
	EnsureSpecialActor(Station, EAsphaltSpecial::PortableStation, [](const FAsphaltGame& Game) { return Game.World().StationPos; });
}

void ADIAGameMode::EnsureAutoPaver()
{
	EnsureSpecialActor(AutoPaver, EAsphaltSpecial::AutoPaver, [](const FAsphaltGame& Game) { return Game.GetAutoPaverPos(); });
}

void ADIAGameMode::EnsureCar()
{
	// The truck's origin is the centre of its collision box, which stands on the ground.
	EnsureSpecialActor(Car, EAsphaltSpecial::PaverCar, [](const FAsphaltGame& Game) { return Game.World().CarPos; }, DIAPaverCar::OriginHeightCm());
}

void ADIAGameMode::EnsureExitGate()
{
	UWorld* World = GetWorld();
	if (!World || ExitGate.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ExitGate = World->SpawnActor<ADIAExitGate>(ADIAExitGate::StaticClass(), ADIAExitGate::MakeTransform(), Params);
}

void ADIAGameMode::EnsureSiteFence()
{
	UWorld* World = GetWorld();
	if (!World || SiteFence.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SiteFence = World->SpawnActor<ADIASiteFence>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
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
