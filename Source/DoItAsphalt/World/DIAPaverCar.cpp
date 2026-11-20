// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAPaverCar.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LotBalance.h"
#include "Player/DIACharacter.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"

namespace
{
	/** The wide bar's placeholder (no model of the name): a flat strip behind the body, this high above the ground. */
	constexpr double PlaceholderBarHeightCm = 60.0;
}

ADIAPaverCar::ADIAPaverCar()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	// The car turns by its own steering; the controller only turns the head.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// The box is the root, so that moving the actor sweeps it. The actor's origin is the box centre (see OriginHeightCm).
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetBoxExtent(DIAModelSizes::PaverCarBodyCm * 0.5);
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// The model's origin is on the ground; the box centre is half its height (and the lift) above it.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Collision);
	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, -DIAPaverCar::OriginHeightCm()));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The wide bar is a model of its own that shares the body's origin, so it sits on the body with no offset; shown once owned.
	WideBarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WideBarMesh"));
	WideBarMesh->SetupAttachment(BodyMesh);
	WideBarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WideBarMesh->SetVisibility(false);

	// First person: the eyes in the cabin. The head turns against the car, the camera does not follow the control rotation.
	CabinCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CabinCamera"));
	CabinCamera->SetupAttachment(Collision);
	CabinCamera->SetRelativeLocation(FVector(DIAPaverCar::CabinEyeAheadCm, 0.0, DIAPaverCar::CabinEyeHeightCm - DIAPaverCar::OriginHeightCm()));
	CabinCamera->SetRelativeRotation(FRotator(DIAPaverCar::CabinPitchDeg, 0.0, 0.0));
	CabinCamera->bUsePawnControlRotation = false;
	CabinCamera->FieldOfView = DIACharacter::NormalFovDeg;
}

void ADIAPaverCar::BeginPlay()
{
	Super::BeginPlay();

	// The real model (bar included) stands on the ground at the origin; the placeholder body is a box centred on the collision box.
	DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_PaverCar"), DIAAssets::EFallback::Cube, DIAModelSizes::PaverCarBodyCm,
		FVector(0.0, 0.0, DIAPaverCar::OriginHeightCm()), TEXT("DIA_Yellow"));
	DIAAssets::SetModelOrFallback(WideBarMesh, TEXT("SM_PaverCar_WideBar"), DIAAssets::EFallback::Cube, DIAModelSizes::WideBarCm,
		FVector(-LotBalance::CarBarBehindCm, 0.0, PlaceholderBarHeightCm), TEXT("DIA_Steel"));
	WideBarMesh->SetVisibility(false);
	bShowsWideBar = false;

	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->RegisterInteractable(this, this);
		// Fires every tick; SyncFromCore returns at once unless the core and the actor differ.
		StateSub.Bind(Subsystem, this, &ADIAPaverCar::SyncFromCore, &ADIAPaverCar::HandleGameReset);
	}
	// It stands where the core has it.
	HandleGameReset();
}

void ADIAPaverCar::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		// A car that is taken away (a new game) never takes its driver with it.
		if (Reason == EEndPlayReason::Destroyed && HasDriver())
		{
			const FLotWorldState& State = Subsystem->GetGame().World();
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
	if (!Subsystem || !Subsystem->GetGame().Owns(ELotItem::Car))
	{
		return FText::GetEmpty();
	}
	const bool bInCar = HasDriver();
	// Only free hands get in: what is held is let go of first (and that is the prompt the player sees).
	if (!bInCar && Subsystem->GetGame().GetHeld() != ELotMovable::Count)
	{
		return FText::GetEmpty();
	}
	// Rebuilt only when the state flips; the lookup is not free every frame.
	const int32 Key = bInCar ? 1 : 0;
	if (CachedPrompt.IsEmpty() || Key != PromptKey)
	{
		PromptKey = Key;
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
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	// A won game pours nothing any more.
	if (bPour && Subsystem && Subsystem->GetGame().HasWon())
	{
		bPour = false;
	}
	if (bPouring == bPour)
	{
		return;
	}
	bPouring = bPour;
	// A new stroke starts where the bar is now, and nothing is laid between where the last one ended and here.
	ToolPath.Reset();
}

void ADIAPaverCar::DoLook(float Yaw, float Pitch)
{
	// The controller turns its rotation, Tick turns the head by what was added (the same scale as walking, and it follows the car's turning).
	if (Controller && HasDriver())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

FVector2D ADIAPaverCar::GetBarPoint() const
{
	const FVector Location = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	return FVector2D(Location.X, Location.Y) - FVector2D(Forward.X, Forward.Y).GetSafeNormal() * LotBalance::CarBarBehindCm;
}

void ADIAPaverCar::UpdateBar()
{
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	const bool bWide = Subsystem && Subsystem->GetGame().GetCarLevel() == 1;
	if (bWide != bShowsWideBar)
	{
		bShowsWideBar = bWide;
		WideBarMesh->SetVisibility(bWide);
	}
}

void ADIAPaverCar::UpdateCamera()
{
	CabinCamera->SetRelativeRotation(FRotator(LookPitchDeg, LookYawDeg, 0.0));
}

double ADIAPaverCar::TopSpeed() const
{
	if (!bPouring)
	{
		return LotBalance::CarSpeedCmS;
	}
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	const int32 Level = Subsystem ? FMath::Max(Subsystem->GetGame().GetCarLevel(), 0) : 0;
	return LotBalance::CarPaveSpeedCmS(Level);
}

void ADIAPaverCar::WritePose(FLotWorldState& State)
{
	const FVector Location = GetActorLocation();
	WrittenPos = FVector2D(Location.X, Location.Y);
	WrittenYawDeg = GetActorRotation().Yaw;
	bHasWritten = true;
	State.CarPos = WrittenPos;
	State.CarYawDeg = WrittenYawDeg;
	// Whoever drives is the car, as far as the core can tell.
	if (HasDriver())
	{
		State.PlayerPos = WrittenPos;
		State.PlayerYawDeg = WrittenYawDeg;
	}
}

void ADIAPaverCar::SyncFromCore()
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	UpdateBar();
	const FLotWorldState& State = Subsystem->GetGame().World();

	// Normally the car wrote the core's pose itself, so nothing differs; a pose somebody else wrote (a loaded game) does.
	const bool bChanged = !bHasWritten || !WrittenPos.Equals(State.CarPos, DIAPaverCar::SyncToleranceCm)
		|| FMath::Abs(FMath::FindDeltaAngleDegrees(WrittenYawDeg, State.CarYawDeg)) > DIAPaverCar::SyncToleranceDeg;
	if (bChanged)
	{
		const FVector Location = GetActorLocation();
		SetActorLocationAndRotation(FVector(State.CarPos.X, State.CarPos.Y, Location.Z), FRotator(0.0, State.CarYawDeg, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
		WrittenPos = State.CarPos;
		WrittenYawDeg = State.CarYawDeg;
		bHasWritten = true;
		Speed = 0.0;
		ToolPath.Reset();
	}

	// A save can say "in the car" before the character exists; the next state change tries again.
	if (State.bPlayerInCar && !HasDriver())
	{
		if (ADIACharacter* Character = ADIACharacter::GetLocal(this))
		{
			EnterBy(Character);
		}
	}
}

void ADIAPaverCar::HandleGameReset()
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}
	const FLotWorldState& State = Subsystem->GetGame().World();

	// Whoever drives gets out where the core says the player is, if the game they are in now has them on foot.
	if (HasDriver() && !State.bPlayerInCar)
	{
		ReleaseDriver(FVector(State.PlayerPos.X, State.PlayerPos.Y, DIACharacter::CapsuleHalfHeightCm), State.PlayerYawDeg);
	}
	Throttle = 0.f;
	Steer = 0.f;
	bPouring = false;
	Speed = 0.0;
	ToolPath.Reset();
	// The car takes the core's pose at once (before its own tick writes the old one into the core), and lets the player in if the save says so.
	bHasWritten = false;
	SyncFromCore();
}

void ADIAPaverCar::Interact(ADIACharacter* By)
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	if (!Subsystem || !Subsystem->GetGame().Owns(ELotItem::Car))
	{
		return;
	}
	if (HasDriver())
	{
		ExitCar();
		return;
	}
	// Free hands only: with something held E lets it go, and the car is entered by the next E.
	if (!By || Subsystem->GetGame().GetHeld() != ELotMovable::Count || Subsystem->GetGame().World().bPlayerInCar)
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

	// The character stays in the world but out of sight and of everyone's way, carried along by the car (at its centre).
	Character->SetUsing(false);
	Character->SetSprinting(false);
	Character->SetFlap(false);
	Character->SetWorkView(false);
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	DriverMovementMode = Movement->MovementMode;
	Movement->DisableMovement();
	Character->SetActorEnableCollision(false);
	Character->SetActorHiddenInGame(true);
	Character->AttachToComponent(Collision, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Driver = Character;

	Throttle = 0.f;
	Steer = 0.f;
	bPouring = false;
	Speed = 0.0;
	ToolPath.Reset();
	LookYawDeg = 0.0;
	LookPitchDeg = DIAPaverCar::CabinPitchDeg;
	UpdateCamera();

	FLotWorldState& State = Subsystem->GetGame().World();
	State.bPlayerInCar = true;
	WritePose(State);
	PlayerController->Possess(this);
	PlayerController->SetViewTarget(this);
	LastControlRotation = FRotator(LookPitchDeg, GetActorRotation().Yaw + LookYawDeg, 0.0);
	PlayerController->SetControlRotation(LastControlRotation);
	Subsystem->Dispatch(FLotEvents());
}

bool ADIAPaverCar::FindExitSpot(const ADIACharacter* Character, FVector& OutSpot) const
{
	const FVector Location = GetActorLocation();
	const FVector Left = -GetActorRightVector();
	const double Height = DIACharacter::CapsuleHalfHeightCm + DIAPaverCar::ExitClearanceLiftCm;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaverCarExit), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Character);

	// The door side first, the other side second.
	const FVector Candidates[2] = {Left, -Left};
	for (const FVector& Side : Candidates)
	{
		const FVector Spot = FVector(Location.X, Location.Y, Height) + Side * DIAPaverCar::ExitSideCm;
		if (!World->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			OutSpot = Spot;
			return true;
		}
	}
	return false;
}

bool ADIAPaverCar::ExitCar()
{
	ADIACharacter* Character = Driver;
	if (!Character || !UDIAWorldSubsystem::Get(this))
	{
		return false;
	}
	FVector Spot;
	if (!FindExitSpot(Character, Spot))
	{
		// Nothing changes: the player stays at the wheel and is told why.
		if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (ADIAHUD* Hud = Cast<ADIAHUD>(PlayerController->GetHUD()))
			{
				Hud->ShowToast(DIAFormat::CarNoExitToast());
			}
		}
		return false;
	}
	ReleaseDriver(Spot, GetActorRotation().Yaw);
	return true;
}

void ADIAPaverCar::ReleaseDriver(const FVector& Spot, double YawDeg)
{
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	ADIACharacter* Character = Driver;
	if (!Character || !Subsystem)
	{
		return;
	}

	// The car stops where it is.
	Throttle = 0.f;
	Steer = 0.f;
	bPouring = false;
	Speed = 0.0;
	ToolPath.Reset();
	Driver = nullptr;

	FLotWorldState& State = Subsystem->GetGame().World();
	State.bPlayerInCar = false;
	State.PlayerPos = FVector2D(Spot.X, Spot.Y);
	State.PlayerYawDeg = YawDeg;

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
	Subsystem->Dispatch(FLotEvents());
}

void ADIAPaverCar::Drive(float DeltaSeconds)
{
	// The top speed is the pouring speed while pouring: a car that starts to pour at full speed slows down to it at once.
	const double Top = TopSpeed();
	const double Wanted = Throttle >= 0.f ? Throttle * Top : Throttle * Top * LotBalance::CarReverseSpeedFactor;
	Speed = FMath::FInterpConstantTo(Speed, Wanted, DeltaSeconds, LotBalance::CarAccelerationCmS2);
	Speed = FMath::Clamp(Speed, -Top, Top);

	// Turning scales with the speed (signed, so that reversing turns the other way like a real car), and a standing car does not turn.
	const double Yaw = GetActorRotation().Yaw + Steer * LotBalance::CarTurnRateDegPerSec * (Speed / LotBalance::CarSpeedCmS) * DeltaSeconds;
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
		// Slide along what was hit with the rest of the move; if that is blocked too, the car stops dead.
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

void ADIAPaverCar::Pour(UDIAWorldSubsystem& Subsystem, float DeltaSeconds)
{
	// The bar moves with the car and swings with its turning: the fastest it can go while pouring is that of the pouring speed at full lock.
	const double Top = TopSpeed();
	const double BarSpeed = Top * (1.0 + LotBalance::CarBarBehindCm * FMath::DegreesToRadians(LotBalance::CarTurnRateDegPerSec) / LotBalance::CarSpeedCmS);
	FVector2D From;
	FVector2D To;
	if (ToolPath.Advance(GetBarPoint(), DeltaSeconds, BarSpeed, From, To))
	{
		FLotGame& Game = Subsystem.GetGame();
		Subsystem.Dispatch(Game.PourFromCar(From, To));
	}
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

	// The head turns by what the controller turned since this tick last set its rotation; the view then turns with the car.
	if (APlayerController* PlayerController = HasDriver() ? Cast<APlayerController>(GetController()) : nullptr)
	{
		const FRotator Control = PlayerController->GetControlRotation();
		LookYawDeg = FMath::Clamp(LookYawDeg + FMath::FindDeltaAngleDegrees(LastControlRotation.Yaw, Control.Yaw), -DIAPaverCar::LookYawLimitDeg, DIAPaverCar::LookYawLimitDeg);
		LookPitchDeg = FMath::Clamp(LookPitchDeg + FMath::FindDeltaAngleDegrees(LastControlRotation.Pitch, Control.Pitch), DIAPaverCar::LookPitchMinDeg, DIAPaverCar::LookPitchMaxDeg);
		LastControlRotation = FRotator(LookPitchDeg, GetActorRotation().Yaw + LookYawDeg, 0.0);
		PlayerController->SetControlRotation(LastControlRotation);
		UpdateCamera();
	}

	FLotGame& Game = Subsystem->GetGame();
	WritePose(Game.World());

	if (Game.HasWon())
	{
		// The game is won: nothing keeps pouring.
		SetPouring(false);
	}
	else if (bPouring)
	{
		Pour(*Subsystem, DeltaSeconds);
	}
}
