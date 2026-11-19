// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAAutoPaver.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"

ADIAAutoPaver::ADIAAutoPaver()
{
	// It follows the core through OnStateChanged (see the header), so it needs no tick of its own.
	PrimaryActorTick.bCanEverTick = false;

	// The origin is the centre of the footprint on the ground (like SM_AutoPaver); front is +X.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Visible, but never in the way of the player (or of the camera).
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UDIAWorldSubsystem* ADIAAutoPaver::GetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
}

void ADIAAutoPaver::BeginPlay()
{
	Super::BeginPlay();

	// The real model stands on the ground at the origin; the placeholder box is lifted by half its height.
	DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_AutoPaver"), DIAAssets::EFallback::Cube, DIAModelSizes::AutoPaverCm,
		FVector(0.0, 0.0, DIAModelSizes::AutoPaverCm.Z * 0.5), TEXT("DIA_Steel"));

	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		StateHandle = Subsystem->OnStateChanged.AddUObject(this, &ADIAAutoPaver::SyncFromCore);
	}
	SyncFromCore();
}

void ADIAAutoPaver::EndPlay(const EEndPlayReason::Type Reason)
{
	if (StateHandle.IsValid())
	{
		if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->OnStateChanged.Remove(StateHandle);
		}
		StateHandle.Reset();
	}
	Super::EndPlay(Reason);
}

void ADIAAutoPaver::SyncFromCore()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const FVector2D Wanted = Subsystem->GetGame().GetAutoPaverPos();
	const FVector Location = GetActorLocation();
	const FVector2D Move = Wanted - FVector2D(Location.X, Location.Y);
	if (Move.IsNearlyZero(DIAAutoPaver::MinMoveCm))
	{
		// At rest the heading stays; the position still follows the core exactly.
		if (!Move.IsZero())
		{
			SetActorLocation(FVector(Wanted.X, Wanted.Y, 0.0));
		}
		return;
	}

	// Turn towards the direction of travel at a fixed rate (the frame's delta, so the result depends on the ticks only).
	const double TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Move.Y, Move.X));
	const double DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0;
	const double Yaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, DIAAutoPaver::TurnRateDegPerSec * DeltaSeconds);
	SetActorLocationAndRotation(FVector(Wanted.X, Wanted.Y, 0.0), FRotator(0.0, Yaw, 0.0));
}
