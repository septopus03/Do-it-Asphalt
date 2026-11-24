// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAAutoPaver.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"

ADIAAutoPaver::ADIAAutoPaver()
{
	// The shown position glides towards the core's one every frame.
	PrimaryActorTick.bCanEverTick = true;

	// The origin is the centre of the footprint on the ground (like SM_AutoPaver); front is +X.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Visible, but never in the way of the player, the aim or the pouring.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADIAAutoPaver::BeginPlay()
{
	Super::BeginPlay();

	// The real model stands on the ground at the origin; the placeholder box is lifted by half its (scaled) height.
	const FVector Size = DIAModelSizes::AutoPaverCm * DIAAutoPaver::ModelScale;
	BodyMesh->SetRelativeScale3D(FVector::OneVector);
	const bool bReal = DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_AutoPaver"), DIAAssets::EFallback::Cube, Size,
		FVector(0.0, 0.0, Size.Z * 0.5), TEXT("DIA_Steel"));
	if (bReal)
	{
		BodyMesh->SetRelativeScale3D(FVector(DIAAutoPaver::ModelScale));
	}

	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		ResetHandle = Subsystem->OnGameReset.AddUObject(this, &ADIAAutoPaver::SnapToCore);
	}
	SnapToCore();
}

void ADIAAutoPaver::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this); Subsystem && ResetHandle.IsValid())
	{
		Subsystem->OnGameReset.Remove(ResetHandle);
	}
	ResetHandle.Reset();
	Super::EndPlay(Reason);
}

void ADIAAutoPaver::SnapToCore()
{
	if (const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		const FVector2D Wanted = Subsystem->GetGame().GetAutoPaverPos(Index);
		SetActorLocation(FVector(Wanted.X, Wanted.Y, 0.0));
	}
}

void ADIAAutoPaver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	const FVector2D Wanted = Subsystem->GetGame().GetAutoPaverPos(Index);
	const FVector Location = GetActorLocation();
	const FVector2D Gap = Wanted - FVector2D(Location.X, Location.Y);
	if (Gap.IsNearlyZero(DIAAutoPaver::SnapCm))
	{
		// Arrived: the position is the core's exactly; the heading stays.
		if (!Gap.IsZero())
		{
			SetActorLocation(FVector(Wanted.X, Wanted.Y, 0.0));
		}
		return;
	}

	// Close in on the core's position (an exponential approach, independent of the frame rate).
	const double Alpha = 1.0 - FMath::Exp(-DIAAutoPaver::FollowRatePerSec * DeltaSeconds);
	const FVector2D Move = Gap * Alpha;
	const FVector NewLocation(Location.X + Move.X, Location.Y + Move.Y, 0.0);
	if (Move.SizeSquared() < FMath::Square(DIAAutoPaver::MinMoveCm))
	{
		// Too small a step to give a direction: it moves, the heading stays.
		SetActorLocation(NewLocation);
		return;
	}

	// Turn towards the direction of travel at a fixed rate (the frame's delta, so the result depends on the ticks only).
	const double TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Move.Y, Move.X));
	const double Yaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, DIAAutoPaver::TurnRateDegPerSec * DeltaSeconds);
	SetActorLocationAndRotation(NewLocation, FRotator(0.0, Yaw, 0.0));
}
