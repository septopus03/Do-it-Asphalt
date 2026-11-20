// Copyright Do it Asphalt. All Rights Reserved.

#include "Player/DIACharacter.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

ADIACharacter::ADIACharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm);

	// Like the third-person template: the controller only turns the camera, the body follows the movement direction.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, DIACharacter::TurnRateDegS, 0.f);
	Movement->MaxWalkSpeed = static_cast<float>(LotBalance::WalkSpeedCmS);
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	// The player cannot jump (no binding), and the lot is flat.
	Movement->JumpZVelocity = 0.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DIACharacter::CameraBoomLengthCm;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Mannequin: SKM_Manny_Simple with the unarmed animation blueprint (copied from the engine template in Task 8).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (MannyFinder.Succeeded())
	{
		BodyMesh->SetSkeletalMesh(MannyFinder.Object);
	}
	if (AnimFinder.Succeeded())
	{
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		BodyMesh->SetAnimInstanceClass(AnimFinder.Class);
	}
	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, -DIACharacter::CapsuleHalfHeightCm));
	BodyMesh->SetRelativeRotation(FRotator(0.0, DIACharacter::MeshYawDeg, 0.0));
}

ADIACharacter* ADIACharacter::GetLocal(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return Controller ? Cast<ADIACharacter>(Controller->GetPawn()) : nullptr;
}

UDIAWorldSubsystem* ADIACharacter::GetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
}

void ADIACharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateMovementSettings();
	StateSub.Bind(GetSubsystem(), this, &ADIACharacter::OnStateChanged, &ADIACharacter::HandleGameReset);
}

void ADIACharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StateSub.Unbind(GetSubsystem());
	Super::EndPlay(EndPlayReason);
}

void ADIACharacter::OnStateChanged()
{
	// Fires every tick; this only matters when the Sprint unlock was just reached.
	UpdateMovementSettings();
}

void ADIACharacter::HandleGameReset()
{
	SetSprinting(false);

	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const FLotWorldState& State = Subsystem->GetGame().World();
	SetActorLocationAndRotation(FVector(State.PlayerPos.X, State.PlayerPos.Y, GetActorLocation().Z), FRotator(0.0, State.PlayerYawDeg, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	if (Controller)
	{
		// The camera goes behind the character again.
		Controller->SetControlRotation(FRotator(0.0, State.PlayerYawDeg, 0.0));
	}
}

void ADIACharacter::SetSprinting(bool bSprint)
{
	bSprinting = bSprint;
	UpdateMovementSettings();
}

void ADIACharacter::UpdateMovementSettings()
{
	double Speed = LotBalance::WalkSpeedCmS;
	if (bSprinting)
	{
		const UDIAWorldSubsystem* Subsystem = GetSubsystem();
		if (Subsystem && Subsystem->GetGame().IsUnlocked(ELotUnlock::Sprint))
		{
			Speed *= LotBalance::SprintMultiplier;
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(Speed);
}

void ADIACharacter::DoMove(float Right, float Forward)
{
	if (Controller)
	{
		// The camera decides where forward is, exactly like the template.
		const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Forward);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Right);
	}
}

void ADIACharacter::DoLook(float Yaw, float Pitch)
{
	if (Controller)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ADIACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		const FVector Location = GetActorLocation();
		FLotWorldState& State = Subsystem->GetGame().World();
		State.PlayerPos = FVector2D(Location.X, Location.Y);
		State.PlayerYawDeg = GetActorRotation().Yaw;
	}
}
