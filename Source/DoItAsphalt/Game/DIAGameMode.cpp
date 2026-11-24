// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAGameMode.h"
#include "Engine/World.h"
#include "Game/DIASettings.h"
#include "Game/DIAWorldSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "UI/DIAHUD.h"
#include "World/DIAAutoPaver.h"
#include "World/DIAEntryBoard.h"
#include "World/DIAExitGate.h"
#include "World/DIAGroundActor.h"
#include "World/DIALotEdge.h"
#include "World/DIAMallActor.h"
#include "World/DIAMovable.h"
#include "World/DIAPaverCar.h"
#include "World/DIAPlantActor.h"
#include "World/DIAShopBuilding.h"
#include "World/DIAShopPoint.h"
#include "World/DIAVehicleStation.h"

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
	// The player's settings (window, vsync, quality, volume) apply from the first frame.
	DIASettings::Apply();
	// Spawned before Super::StartPlay so that they get their BeginPlay together with everything else.
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ADIAGroundActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		World->SpawnActor<ADIAPlantActor>(FVector(LotLayout::PlantPos.X, LotLayout::PlantPos.Y, 0.0), FRotator(0.0, LotLayout::PlantYawDeg, 0.0), Params);
		// The fixed vehicle station: the car and the auto paver load asphalt here.
		World->SpawnActor<ADIAVehicleStation>(FVector(LotLayout::VehicleStationPos.X, LotLayout::VehicleStationPos.Y, 0.0), FRotator::ZeroRotator, Params);
		// The three shop buildings along the south strip; the kind is set before BeginPlay, which builds the sign according to it.
		for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
		{
			const ELotShop Shop = static_cast<ELotShop>(I);
			const FTransform Where = DIAShopBuilding::MakeTransform(Shop);
			if (ADIAShopBuilding* Building = World->SpawnActorDeferred<ADIAShopBuilding>(ADIAShopBuilding::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Building->Kind = Shop;
				Building->FinishSpawning(Where);
			}
			// The buy point of the building, where E opens its panel.
			const FTransform PointAt = ADIAShopPoint::MakeTransform(Shop);
			if (ADIAShopPoint* Point = World->SpawnActorDeferred<ADIAShopPoint>(ADIAShopPoint::StaticClass(), PointAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Point->Kind = Shop;
				Point->FinishSpawning(PointAt);
			}
		}
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
	// The gate stands in the gap of the fence and the wall, where the old road leaves the lot.
	World->SpawnActor<ADIAExitGate>(ADIAExitGate::StaticClass(), ADIAExitGate::MakeTransform(), Params);
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
	EnsureAutoPavers();
	EnsureCar();
}

void ADIAGameMode::EnsureCar()
{
	UWorld* World = GetWorld();
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!World || !Subsystem)
	{
		return;
	}
	const bool bOwned = Subsystem->GetGame().Owns(ELotItem::Car);
	ADIAPaverCar* Kept = nullptr;
	TArray<ADIAPaverCar*> Surplus;
	for (TActorIterator<ADIAPaverCar> It(World); It; ++It)
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
	for (ADIAPaverCar* Actor : Surplus)
	{
		Actor->Destroy();
	}
	if (!bOwned || Kept)
	{
		return;
	}
	// It appears where the core has it.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FLotWorldState& State = Subsystem->GetGame().World();
	World->SpawnActor<ADIAPaverCar>(FVector(State.CarPos.X, State.CarPos.Y, DIAPaverCar::OriginHeightCm(Subsystem->GetGame().GetCarLevel())), FRotator(0.0, State.CarYawDeg, 0.0), Params);
}

void ADIAGameMode::EnsureAutoPavers()
{
	UWorld* World = GetWorld();
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!World || !Subsystem)
	{
		return;
	}
	const int32 Owned = Subsystem->GetGame().GetNumAutoPavers();

	// What stands now: one actor per owned robot is kept, every other one goes.
	ADIAAutoPaver* Kept[MaxAutoPavers] = {};
	TArray<ADIAAutoPaver*> Surplus;
	for (TActorIterator<ADIAAutoPaver> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed())
		{
			continue;
		}
		const int32 Index = It->Index;
		if (Index >= 0 && Index < Owned && !Kept[Index])
		{
			Kept[Index] = *It;
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
	// A new robot appears where the core has it (its dock), facing out of the depot.
	for (int32 Index = 0; Index < Owned; ++Index)
	{
		if (Kept[Index])
		{
			continue;
		}
		const FVector2D Pos = Subsystem->GetGame().GetAutoPaverPos(Index);
		const FTransform Where(FRotator(0.0, LotLayout::PaverDockYawDeg, 0.0), FVector(Pos.X, Pos.Y, 0.0));
		if (ADIAAutoPaver* Robot = World->SpawnActorDeferred<ADIAAutoPaver>(ADIAAutoPaver::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Robot->Index = Index;
			Robot->FinishSpawning(Where);
		}
	}
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
