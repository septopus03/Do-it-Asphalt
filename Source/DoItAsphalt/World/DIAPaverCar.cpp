// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAPaverCar.h"
#include "AsphaltBalance.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIARefill.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/DIACharacter.h"
#include "UI/DIAFormat.h"

ADIAPaverCar::ADIAPaverCar()
{
	PrimaryActorTick.bCanEverTick = true;
	// The truck turns by its own steering; the controller only turns the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// The box is the root, so that moving the actor sweeps it. The actor's origin is the box centre (see OriginHeightCm).
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetBoxExtent(DIAModelSizes::PaverCarBodyCm * 0.5);
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Collision->SetCollisionObjectType(ECC_Pawn);
	// The camera boom is never pushed in by the truck itself.
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// The model's origin is on the ground; the box centre is half its height (and the lift) above it.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Collision);
	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, -DIAPaverCar::OriginHeightCm()));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarMesh"));
	BarMesh->SetupAttachment(Collision);
	BarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Collision);
	CameraBoom->TargetArmLength = DIAPaverCar::CameraBoomLengthCm;
	CameraBoom->SetRelativeLocation(FVector(0.0, 0.0, DIAPaverCar::CameraTargetLiftCm));
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ADIAPaverCar::BeginPlay()
{
	Super::BeginPlay();

	// The real model (bar included) stands on the ground at the origin; the placeholder body is a box centred on the collision box.
	const bool bRealModel = DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_PaverCar"), DIAAssets::EFallback::Cube, DIAModelSizes::PaverCarBodyCm,
		FVector(0.0, 0.0, DIAPaverCar::OriginHeightCm()), TEXT("DIA_Yellow"));
	if (bRealModel)
	{
		BarMesh->SetVisibility(false);
	}
	else
	{
		// The placeholder bar is a flat strip just behind the body, resting on the ground (no model of this name exists: it is always the placeholder).
		const FVector BarCentre(-AsphaltBalance::CarBarBehindCm, 0.0, DIAModelSizes::PaverCarBarCm.Z * 0.5 - DIAPaverCar::OriginHeightCm());
		BarMesh->SetRelativeLocation(BarCentre);
		DIAAssets::SetModelOrFallback(BarMesh, TEXT("SM_PaverCar_Bar"), DIAAssets::EFallback::Cube, DIAModelSizes::PaverCarBarCm, FVector::ZeroVector, TEXT("DIA_Steel"));
	}

	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->RegisterInteractable(this, this);
		// Fires every tick; SyncFromCore returns at once unless the core and the actor differ.
		StateSub.Bind(Subsystem, this, &ADIAPaverCar::SyncFromCore, &ADIAPaverCar::HandleGameReset);
	}
	SyncFromCore();
}

void ADIAPaverCar::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		// A truck that is taken away (a new game) never takes its driver with it.
		if (Reason == EEndPlayReason::Destroyed && HasDriver())
		{
			const FAsphaltWorldState& State = Subsystem->GetGame().World();
			ReleaseDriver(FVector(State.PlayerPos.X, State.PlayerPos.Y, DIACharacter::CapsuleHalfHeightCm), State.PlayerYawDeg);
		}
		StateSub.Unbind(Subsystem);
		Subsystem->UnregisterInteractable(this);
	}
	Super::EndPlay(Reason);
}

FText ADIAPaverCar::GetPrompt(const ADIACharacter* By) const
{
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem || !Subsystem->GetGame().HasSpecial(EAsphaltSpecial::PaverCar))
	{
		return FText::GetEmpty();
	}
	// Rebuilt only when the state flips; the lookup is not free every frame.
	const bool bInCar = HasDriver();
	if (CachedPrompt.IsEmpty() || bInCar != bPromptInCar)
	{
		bPromptInCar = bInCar;
		CachedPrompt = DIAFormat::CarPrompt(bInCar);
	}
	return CachedPrompt;
}

void ADIAPaverCar::SetThrottle(float Value)
{
	Throttle = FMath::Clamp(Value, -1.f, 1.f);
}

void ADIAPaverCar::SetSteer(float Value)
{
	Steer = FMath::Clamp(Value, -1.f, 1.f);
}

void ADIAPaverCar::SetPouring(bool bPour)
{
	if (bPouring == bPour)
	{
		return;
	}
	bPouring = bPour;
	if (bPouring)
	{
		// The stroke starts where the bar really is.
		PreviousBar = GetBarPoint();
	}
}

void ADIAPaverCar::DoLook(float Yaw, float Pitch)
{
	if (Controller)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

FVector2D ADIAPaverCar::GetBarPoint() const
{
	const FVector Location = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	return FVector2D(Location.X, Location.Y) - FVector2D(Forward.X, Forward.Y).GetSafeNormal() * AsphaltBalance::CarBarBehindCm;
}

void ADIAPaverCar::SyncFromCore()
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	const FAsphaltWorldState& State = Subsystem->GetGame().World();

	// Normally the truck wrote these itself this frame, so nothing differs; a loaded save does differ.
	const FVector Location = GetActorLocation();
	const bool bMoved = !FVector2D(Location.X, Location.Y).Equals(State.CarPos, DIAPaverCar::SyncToleranceCm);
	const bool bTurned = FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, State.CarYawDeg)) > DIAPaverCar::SyncToleranceDeg;
	if (bMoved || bTurned)
	{
		SetActorLocationAndRotation(FVector(State.CarPos.X, State.CarPos.Y, Location.Z), FRotator(0.0, State.CarYawDeg, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
		Speed = 0.0;
	}

	// A save can say "in the truck" before the character exists; the next state change tries again.
	if (State.bPlayerInCar && !HasDriver())
	{
		if (ADIACharacter* Character = ADIACharacter::GetLocal(this))
		{
			EnterBy(Character);
		}
	}
}

void ADIAPaverCar::Interact(ADIACharacter* By)
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!By || !Subsystem || HasDriver() || !Subsystem->GetGame().HasSpecial(EAsphaltSpecial::PaverCar))
	{
		return;
	}
	EnterBy(By);
}

void ADIAPaverCar::EnterBy(ADIACharacter* Character)
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (!Subsystem || !PlayerController)
	{
		return;
	}

	// The character stays in the world but out of sight and of everyone's way, carried along by the truck.
	Character->SetPouring(false);
	Character->SetSprinting(false);
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	DriverMovementMode = Movement->MovementMode;
	Movement->DisableMovement();
	Character->SetActorEnableCollision(false);
	Character->SetActorHiddenInGame(true);
	Character->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Driver = Character;

	Subsystem->GetGame().World().bPlayerInCar = true;
	PlayerController->Possess(this);
	PlayerController->SetViewTarget(this);
	// The camera starts behind the truck, looking slightly down.
	PlayerController->SetControlRotation(FRotator(DIAPaverCar::CameraPitchDeg, GetActorRotation().Yaw, 0.0));
}

FVector ADIAPaverCar::FindExitSpot(const ADIACharacter* Character) const
{
	const FVector Location = GetActorLocation();
	const FVector Left = -GetActorRightVector();
	const double Height = DIACharacter::CapsuleHalfHeightCm + DIAPaverCar::ExitClearanceLiftCm;
	const UWorld* World = GetWorld();
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaverCarExit), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Character);

	const FVector Candidates[2] = {Left, -Left};
	for (const FVector& Side : Candidates)
	{
		const FVector Spot = FVector(Location.X, Location.Y, Height) + Side * DIAPaverCar::ExitSideCm;
		if (!World || !World->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			return Spot;
		}
	}
	// Nothing is free: the door side anyway (the character is moved without a sweep, so it cannot get stuck in the truck).
	return FVector(Location.X, Location.Y, Height) + Left * DIAPaverCar::ExitSideCm;
}

void ADIAPaverCar::ExitCar()
{
	ADIACharacter* Character = Driver;
	if (!Character || !UDIAWorldSubsystem::Get(this))
	{
		return;
	}
	ReleaseDriver(FindExitSpot(Character), GetActorRotation().Yaw);
}

void ADIAPaverCar::ReleaseDriver(const FVector& Spot, double YawDeg)
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	ADIACharacter* Character = Driver;
	if (!Character || !Subsystem)
	{
		return;
	}

	// The truck stops where it is.
	SetThrottle(0.f);
	SetSteer(0.f);
	SetPouring(false);
	Speed = 0.0;
	Driver = nullptr;
	Subsystem->GetGame().World().bPlayerInCar = false;

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocationAndRotation(Spot, FRotator(0.0, YawDeg, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorEnableCollision(true);
	Character->SetActorHiddenInGame(false);
	Character->GetCharacterMovement()->SetMovementMode(DriverMovementMode);

	if (PlayerController)
	{
		PlayerController->Possess(Character);
		PlayerController->SetViewTarget(Character);
		PlayerController->SetControlRotation(FRotator(0.0, YawDeg, 0.0));
	}
}

void ADIAPaverCar::HandleGameReset()
{
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	const FAsphaltWorldState& State = Subsystem->GetGame().World();

	// Whoever drives gets out where the core says the player is, if the game they are in now has them on foot.
	if (HasDriver() && !State.bPlayerInCar)
	{
		ReleaseDriver(FVector(State.PlayerPos.X, State.PlayerPos.Y, DIACharacter::CapsuleHalfHeightCm), State.PlayerYawDeg);
	}
	SetThrottle(0.f);
	SetSteer(0.f);
	SetPouring(false);
	Speed = 0.0;
	// Moves the truck to the core's position and lets the player in if the save says so.
	SyncFromCore();
}

void ADIAPaverCar::Drive(float DeltaSeconds)
{
	const double Wanted = Throttle >= 0.f ? Throttle * AsphaltBalance::CarSpeedCmS : Throttle * AsphaltBalance::CarSpeedCmS * DIAPaverCar::ReverseSpeedFactor;
	Speed = FMath::FInterpConstantTo(Speed, Wanted, DeltaSeconds, DIAPaverCar::AccelerationCmS2);

	// Turning scales with the speed (signed, so that reversing turns the other way like a real truck), and a standing truck does not turn.
	const double Yaw = GetActorRotation().Yaw + Steer * DIAPaverCar::TurnRateDegPerSec * (Speed / AsphaltBalance::CarSpeedCmS) * DeltaSeconds;
	const FQuat NewRotation = FRotator(0.0, Yaw, 0.0).Quaternion();
	if (FMath::IsNearlyZero(Speed) && NewRotation.Equals(GetActorQuat()))
	{
		return;
	}
	const FVector Delta = NewRotation.GetForwardVector() * (Speed * DeltaSeconds);

	FHitResult Hit;
	Collision->MoveComponent(Delta, NewRotation, /*bSweep*/ true, &Hit);
	if (Hit.bBlockingHit)
	{
		// Slide along what was hit with the rest of the move; if that is blocked too, the truck stops dead.
		FVector Slide = FVector::VectorPlaneProject(Delta, Hit.Normal) * (1.0 - Hit.Time);
		Slide.Z = 0.0;
		FHitResult SlideHit;
		Collision->MoveComponent(Slide, Collision->GetComponentQuat(), /*bSweep*/ true, &SlideHit);
		if (SlideHit.bBlockingHit)
		{
			Speed = 0.0;
		}
	}
}

void ADIAPaverCar::Pour(UDIAWorldSubsystem& Subsystem)
{
	const FVector2D Current = GetBarPoint();
	// A bar that does not move has no direction to lay in; the next stroke starts from here.
	if (!(Current - PreviousBar).IsNearlyZero(DIAPaverCar::MinBarMoveCm))
	{
		Subsystem.Pour(PreviousBar, Current, /*bFromCar*/ true);
	}
	PreviousBar = Current;
}

void ADIAPaverCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	Drive(DeltaSeconds);

	FAsphaltGame& Game = Subsystem->GetGame();
	const FVector Location = GetActorLocation();
	const FVector2D Position(Location.X, Location.Y);
	Game.World().CarPos = Position;
	Game.World().CarYawDeg = GetActorRotation().Yaw;

	if (bPouring)
	{
		Pour(*Subsystem);
	}

	// The subsystem broadcasts OnStateChanged every tick, so the HUD sees the refilled tank without a second dispatch.
	if (DIARefill::IsInZone(Game, Position, DIAPaverCar::RefillExtraRadiusCm))
	{
		Game.RefillCar(DeltaSeconds);
	}
}
