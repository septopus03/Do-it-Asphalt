// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAPortableStation.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Player/DIACharacter.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"

namespace
{
	/** How far the actor may be from StationPos before it is moved there; the core's number is the truth. */
	constexpr double SyncToleranceCm = 0.5;
}

ADIAPortableStation::ADIAPortableStation()
{
	PrimaryActorTick.bCanEverTick = false;

	// The origin is the centre of the footprint on the ground (like SM_PortableStation).
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Root);
	Collision->SetBoxExtent(DIAModelSizes::StationCm * 0.5);
	Collision->SetRelativeLocation(FVector(0.0, 0.0, DIAModelSizes::StationCm.Z * 0.5));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The camera boom is never pushed in by the station.
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UDIAWorldSubsystem* ADIAPortableStation::GetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
}

ADIACharacter* ADIAPortableStation::GetLocalCharacter() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return Controller ? Cast<ADIACharacter>(Controller->GetPawn()) : nullptr;
}

bool ADIAPortableStation::IsCarried() const
{
	return GetAttachParentActor() != nullptr;
}

void ADIAPortableStation::BeginPlay()
{
	Super::BeginPlay();

	// The real model stands on the ground at the origin; the placeholder box is lifted by half its height.
	DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_PortableStation"), DIAAssets::EFallback::Cube, DIAModelSizes::StationCm,
		FVector(0.0, 0.0, DIAModelSizes::StationCm.Z * 0.5), TEXT("DIA_Steel"));

	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->RegisterInteractable(this, this);
		// Fires every tick; SyncFromCore returns at once unless the core and the actor differ.
		StateHandle = Subsystem->OnStateChanged.AddUObject(this, &ADIAPortableStation::SyncFromCore);
	}
	SyncFromCore();
}

void ADIAPortableStation::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		if (StateHandle.IsValid())
		{
			Subsystem->OnStateChanged.Remove(StateHandle);
		}
		Subsystem->UnregisterInteractable(this);
	}
	StateHandle.Reset();
	Super::EndPlay(Reason);
}

void ADIAPortableStation::SyncFromCore()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const FAsphaltWorldState& State = Subsystem->GetGame().World();
	if (State.bStationCarried)
	{
		if (!IsCarried())
		{
			// A save can say "carried" before the character exists; the next state change tries again.
			if (ADIACharacter* Carrier = GetLocalCharacter())
			{
				CarryBy(Carrier);
			}
		}
		return;
	}
	const FVector Location = GetActorLocation();
	if (IsCarried() || !FVector2D(Location.X, Location.Y).Equals(State.StationPos, SyncToleranceCm))
	{
		PlaceOnGround(State.StationPos, GetActorRotation().Yaw);
	}
}

void ADIAPortableStation::CarryBy(ADIACharacter* Carrier)
{
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The station hangs behind the character with its feet on the ground (the capsule's centre is half a height above it).
	AttachToComponent(Carrier->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeLocation(FVector(-DIAStation::CarryBehindCm, 0.0, -DIACharacter::CapsuleHalfHeightCm));
	SetActorRelativeRotation(FRotator::ZeroRotator);
}

void ADIAPortableStation::PlaceOnGround(const FVector2D& Where, double YawDeg)
{
	if (IsCarried())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	SetActorLocationAndRotation(FVector(Where.X, Where.Y, 0.0), FRotator(0.0, YawDeg, 0.0));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

FText ADIAPortableStation::GetPrompt(const ADIACharacter* By) const
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->GetGame().HasSpecial(EAsphaltSpecial::PortableStation))
	{
		return FText::GetEmpty();
	}
	// Rebuilt only when the state flips; the texts themselves are cheap but the lookup is not free every frame.
	const bool bCarried = Subsystem->GetGame().World().bStationCarried;
	if (CachedPrompt.IsEmpty() || bCarried != bPromptCarried)
	{
		bPromptCarried = bCarried;
		CachedPrompt = DIAFormat::StationPrompt(bCarried);
	}
	return CachedPrompt;
}

bool ADIAPortableStation::HasRoomAt(const FVector2D& Where, double YawDeg, const ADIACharacter* By) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// The same box the station blocks with, lifted a little off the ground.
	const FVector Extent = Collision->GetUnscaledBoxExtent();
	const FVector Centre(Where.X, Where.Y, Extent.Z + DIAStation::ClearanceLiftCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PortableStationRoom), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(By);
	return !World->OverlapBlockingTestByChannel(Centre, FQuat(FRotator(0.0, YawDeg, 0.0)), ECC_WorldStatic, FCollisionShape::MakeBox(Extent), Params);
}

void ADIAPortableStation::ShowNoRoomToast(const ADIACharacter* By) const
{
	const APlayerController* Controller = By ? Cast<APlayerController>(By->GetController()) : nullptr;
	if (ADIAHUD* Hud = Controller ? Cast<ADIAHUD>(Controller->GetHUD()) : nullptr)
	{
		Hud->ShowToast(DIAFormat::StationNoRoomToast());
	}
}

void ADIAPortableStation::Interact(ADIACharacter* By)
{
	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!By || !Subsystem || !Subsystem->GetGame().HasSpecial(EAsphaltSpecial::PortableStation))
	{
		return;
	}
	FAsphaltWorldState& State = Subsystem->GetGame().World();

	if (!State.bStationCarried)
	{
		State.bStationCarried = true;
		CarryBy(By);
		return;
	}

	// Put down in front of the character, facing the same way, if nothing is in the way.
	const FVector Forward = By->GetActorForwardVector();
	const FVector2D Ahead = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
	const FVector Location = By->GetActorLocation();
	const FVector2D Where = FVector2D(Location.X, Location.Y) + Ahead * DIAStation::PutDownAheadCm;
	const double YawDeg = By->GetActorRotation().Yaw;
	if (!HasRoomAt(Where, YawDeg, By))
	{
		ShowNoRoomToast(By);
		return;
	}
	State.bStationCarried = false;
	State.StationPos = Where;
	PlaceOnGround(Where, YawDeg);
}
