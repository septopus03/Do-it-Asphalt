// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAExitGate.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Materials/MaterialInterface.h"
#include "Player/DIACharacter.h"
#include "UI/DIAFormat.h"
#include "World/DIAPaverCar.h"

namespace
{
	/** Y of the hinge: the inner face of the left column, half the passage's width from the centre. */
	double HingeY()
	{
		return -DIAModelSizes::ExitGateBarrierCm.Y * 0.5;
	}

	FVector2D Outward()
	{
		return AsphaltLot::EntryOutward(LotLayout::ExitEntry);
	}
}

ADIAExitGate::ADIAExitGate()
{
	PrimaryActorTick.bCanEverTick = true;
	// Ticks only while the arm rises.
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The origin is the centre of the frame's footprint on the ground; see MakeTransform for where it stands.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(Root);
	FrameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Only the columns are solid; the passage and the beam above it are not.
	const double PostY = DIAModelSizes::ExitGateBarrierCm.Y * 0.5 + DIAExitGate::PostCm.Y * 0.5;
	for (int32 Side = 0; Side < DIAExitGate::ColumnCount; ++Side)
	{
		UBoxComponent* Post = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Post%d"), Side));
		Post->SetupAttachment(Root);
		Post->SetBoxExtent(DIAExitGate::PostCm * 0.5);
		Post->SetRelativeLocation(FVector(0.0, Side == 0 ? -PostY : PostY, DIAExitGate::PostCm.Z * 0.5));
		Post->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Post->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		PostCollisions.Add(Post);
	}

	// The arm's origin is its hinge end and it runs towards +Y; the pivot is the hinge.
	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(Root);
	ArmPivot->SetRelativeLocation(FVector(0.0, HingeY(), DIAExitGate::ArmHeightCm));

	ArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmMesh"));
	ArmMesh->SetupAttachment(ArmPivot);
	ArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ArmCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("ArmCollision"));
	ArmCollision->SetupAttachment(ArmPivot);
	ArmCollision->SetBoxExtent(DIAModelSizes::ExitGateBarrierCm * 0.5);
	ArmCollision->SetRelativeLocation(FVector(0.0, DIAModelSizes::ExitGateBarrierCm.Y * 0.5, 0.0));
	ArmCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ArmCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// The word EXIT on both faces of the sign board: first the face towards the lot (-X), then the one towards the road (+X).
	for (int32 Side = 0; Side < DIAExitGate::SignFaceCount; ++Side)
	{
		const double Facing = Side == 0 ? -1.0 : 1.0;
		UTextRenderComponent* Sign = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("SignText%d"), Side));
		Sign->SetupAttachment(Root);
		Sign->SetRelativeLocationAndRotation(FVector(Facing * (DIAExitGate::SignFaceXCm + DIAExitGate::SignTextOffsetCm), 0.0, DIAExitGate::SignCentreZCm),
			FRotator(0.0, Side == 0 ? DIAExitGate::LotFaceYawDeg : 0.0, 0.0));
		Sign->SetHorizontalAlignment(EHTA_Center);
		Sign->SetVerticalAlignment(EVRTA_TextCenter);
		Sign->SetWorldSize(static_cast<float>(DIAExitGate::SignTextSizeCm));
		Sign->SetTextRenderColor(DIAExitGate::SignColor);
		if (UMaterialInterface* TextMaterial = DIAAssets::TextMaterial())
		{
			Sign->SetTextMaterial(TextMaterial);
		}
		Sign->SetCastShadow(false);
		SignTexts.Add(Sign);
	}

	// The trigger lies beyond the gate: a sphere that only notices pawns (the player on foot, the car).
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(static_cast<float>(LotLayout::ExitTriggerRadiusCm));
	const FVector2D Offset = LotLayout::ExitTrigger() - LotLayout::GatePos();
	Trigger->SetRelativeLocation(FVector(FVector2D::DotProduct(Offset, Outward()), 0.0, DIAExitGate::TriggerHeightCm));
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADIAExitGate::OnTriggerOverlap);
}

FTransform ADIAExitGate::MakeTransform()
{
	const FVector2D Where = LotLayout::GatePos();
	const FVector2D Dir = Outward();
	return FTransform(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0), FVector(Where.X, Where.Y, 0.0));
}

void ADIAExitGate::BeginPlay()
{
	Super::BeginPlay();

	// Both models stand on the ground / hang at their origin: the real frame's origin is the centre of its footprint, the arm's its hinge end.
	DIAAssets::SetModelOrFallback(FrameMesh, TEXT("SM_ExitGate_Frame"), DIAAssets::EFallback::Cube, DIAModelSizes::ExitGateFrameCm,
		FVector(0.0, 0.0, DIAModelSizes::ExitGateFrameCm.Z * 0.5), TEXT("DIA_Yellow"));
	DIAAssets::SetModelOrFallback(ArmMesh, TEXT("SM_ExitGate_Barrier"), DIAAssets::EFallback::Cube, DIAModelSizes::ExitGateBarrierCm,
		FVector(0.0, DIAModelSizes::ExitGateBarrierCm.Y * 0.5, 0.0), TEXT("DIA_Red"));

	for (UTextRenderComponent* Sign : SignTexts)
	{
		Sign->SetText(DIAFormat::ExitSign());
	}

	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	HandleGameReset();
	// Fires every tick; SyncFromCore returns at once unless the exit opened or closed.
	StateSub.Bind(Subsystem, this, &ADIAExitGate::SyncFromCore, &ADIAExitGate::HandleGameReset);
}

void ADIAExitGate::EndPlay(const EEndPlayReason::Type Reason)
{
	StateSub.Unbind(UDIAWorldSubsystem::Get(this));
	Super::EndPlay(Reason);
}

void ADIAExitGate::HandleGameReset()
{
	// A game that is loaded with the exit open starts with the gate open; a new one with it closed.
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	const bool bExitOpen = Subsystem && Subsystem->GetGame().IsExitOpen();
	OpenElapsed = bExitOpen ? DIAExitGate::OpenSeconds : 0.0;
	SetActorTickEnabled(false);
	ApplyArm();
}

void ADIAExitGate::SyncFromCore()
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	const bool bExitOpen = Subsystem->GetGame().IsExitOpen();
	if (bExitOpen && OpenElapsed < DIAExitGate::OpenSeconds)
	{
		SetActorTickEnabled(true);
	}
	else if (!bExitOpen && OpenElapsed > 0.0)
	{
		OpenElapsed = 0.0;
		SetActorTickEnabled(false);
		ApplyArm();
	}
	// Somebody who stands in the sphere when the exit opens never "enters" it: ask who is there.
	if (bExitOpen && !Subsystem->GetGame().HasWon())
	{
		TArray<AActor*> Inside;
		Trigger->GetOverlappingActors(Inside, APawn::StaticClass());
		for (const AActor* Actor : Inside)
		{
			if (Cast<ADIACharacter>(Actor) || Cast<ADIAPaverCar>(Actor))
			{
				Subsystem->Win();
				break;
			}
		}
	}
}

void ADIAExitGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	OpenElapsed = FMath::Min(OpenElapsed + static_cast<double>(DeltaSeconds), DIAExitGate::OpenSeconds);
	ApplyArm();
	if (OpenElapsed >= DIAExitGate::OpenSeconds)
	{
		SetActorTickEnabled(false);
	}
}

void ADIAExitGate::ApplyArm()
{
	OpenFraction = FMath::Clamp(OpenElapsed / DIAExitGate::OpenSeconds, 0.0, 1.0);
	// Positive roll turns +Y downwards; the arm runs towards +Y and rises, hence the minus.
	const double Angle = FMath::SmoothStep(0.0, 1.0, OpenFraction) * DIAExitGate::OpenAngleDeg;
	ArmPivot->SetRelativeRotation(FRotator(0.0, 0.0, -Angle));
	ArmCollision->SetCollisionEnabled(IsOpen() ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void ADIAExitGate::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Whoever the player is at the moment: the character on foot or the car. The core ignores it while the exit is closed.
	if (!Cast<ADIACharacter>(OtherActor) && !Cast<ADIAPaverCar>(OtherActor))
	{
		return;
	}
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->Win();
	}
}
