// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "Engine/World.h"
#include "Game/DIAWorldSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "UI/DIAHUD.h"
#include "World/DIAAutoPaver.h"
#include "World/DIAEntryBoard.h"
#include "World/DIAGroundActor.h"
#include "World/DIALotEdge.h"
#include "World/DIAMallActor.h"
#include "World/DIAMovable.h"
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
		EnsureSite();
	}
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		// The actors of the movables follow what the game owns: a purchase and a loaded or restarted game both change that.
		ItemBoughtHandle = Subsystem->OnItemBought.AddUObject(this, &ADIAGameMode::HandleItemBought);
		GameResetHandle = Subsystem->OnGameReset.AddUObject(this, &ADIAGameMode::EnsureAllMovables);
	}
	EnsureAllMovables();
	Super::StartPlay();
}

void ADIAGameMode::EnsureSite()
{
	UWorld* World = GetWorld();
	if (!World || TActorIterator<ADIALotEdge>(World))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ADIAMallActor>(FVector(LotLayout::MallCentre.X, LotLayout::MallCentre.Y, 0.0), FRotator::ZeroRotator, Params);
	World->SpawnActor<ADIALotEdge>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	for (int32 Entry = 0; Entry < AsphaltLot::NumEntries; ++Entry)
	{
		// The entry is set before BeginPlay, which builds the board according to it.
		const FTransform Where = DIABoard::BoardTransform(Entry);
		if (ADIAEntryBoard* Board = World->SpawnActorDeferred<ADIAEntryBoard>(ADIAEntryBoard::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Board->Entry = Entry;
			Board->FinishSpawning(Where);
		}
	}
}

void ADIAGameMode::EnsureAllMovables()
{
	for (int32 I = 0; I < static_cast<int32>(ELotMovable::Count); ++I)
	{
		EnsureMovable(static_cast<ELotMovable>(I));
	}
	EnsureAutoPaver();
}

void ADIAGameMode::EnsureAutoPaver()
{
	UWorld* World = GetWorld();
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!World || !Subsystem)
	{
		return;
	}
	const bool bOwned = Subsystem->GetGame().Owns(ELotItem::AutoPaver);
	ADIAAutoPaver* Kept = nullptr;
	TArray<ADIAAutoPaver*> Surplus;
	for (TActorIterator<ADIAAutoPaver> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed())
		{
			continue;
		}
		if (bOwned && !Kept)
		{
			Kept = *It;
		}
		else
		{
			Surplus.Add(*It);
		}
	}
	for (ADIAAutoPaver* Actor : Surplus)
	{
		Actor->Destroy();
	}
	if (!bOwned || Kept)
	{
		return;
	}
	// It appears where the core has it (the actor puts itself there in BeginPlay).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector2D Pos = Subsystem->GetGame().GetAutoPaverPos();
	World->SpawnActor<ADIAAutoPaver>(FVector(Pos.X, Pos.Y, 0.0), FRotator::ZeroRotator, Params);
}

void ADIAGameMode::EnsureMovable(ELotMovable Kind)
{
	UWorld* World = GetWorld();
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!World || !Subsystem)
	{
		return;
	}
	const FLotGame& Game = Subsystem->GetGame();
	const bool bOwned = Game.OwnsMovable(Kind);

	// What stands now: one actor of the kind at most is kept, and none if the game does not own it.
	ADIAMovable* Kept = nullptr;
	TArray<ADIAMovable*> Surplus;
	for (TActorIterator<ADIAMovable> It(World); It; ++It)
	{
		if (It->GetKind() != Kind || It->IsActorBeingDestroyed())
		{
			continue;
		}
		if (bOwned && !Kept)
		{
			Kept = *It;
		}
		else
		{
			Surplus.Add(*It);
		}
	}
	for (ADIAMovable* Actor : Surplus)
	{
		Actor->Destroy();
	}
	if (!bOwned || Kept)
	{
		return;
	}
	const FLotMovable& State = Game.World().Movables[static_cast<int32>(Kind)];
	const FTransform Where(FRotator(0.0, State.YawDeg, 0.0), FVector(State.Pos.X, State.Pos.Y, 0.0));
	// The kind is set before BeginPlay, which registers the actor and shows its model.
	if (ADIAMovable* Spawned = World->SpawnActorDeferred<ADIAMovable>(ADIAMovable::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawned->Init(Kind);
		Spawned->FinishSpawning(Where);
	}
}

void ADIAGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->OnItemBought.Remove(ItemBoughtHandle);
		Subsystem->OnGameReset.Remove(GameResetHandle);
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
