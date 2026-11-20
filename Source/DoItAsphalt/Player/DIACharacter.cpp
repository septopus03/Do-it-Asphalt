// Copyright Do it Asphalt. All Rights Reserved.

#include "Player/DIACharacter.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotLayout.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIARefill.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** How one hand item hangs on the pivot (pivot axes: X forward, Y right, Z up), and the inside of a vessel where the asphalt disc sits. */
	struct FHandLayout
	{
		FName Model;
		DIAAssets::EFallback Shape;
		FVector SizeCm;
		FVector Location;
		FRotator Rotation;
		/** Height and radius of the vessel's floor and rim (model axes, cm); a zero rim means the item holds no asphalt on show. */
		double FloorZ = 0.0;
		double FloorRadius = 0.0;
		double RimZ = 0.0;
		double RimRadius = 0.0;
	};

	FHandLayout LayoutOf(ELotHand Hand)
	{
		FHandLayout Layout;
		switch (Hand)
		{
		case ELotHand::Scoop:
			Layout = {TEXT("SM_Scoop"), DIAAssets::EFallback::Cube, DIAModelSizes::ScoopCm, FVector(0.0, 0.0, 0.0), FRotator(-8.0, 0.0, 0.0)};
			break;
		case ELotHand::Bucket:
			// Inner measurements of SM_Bucket (docs/04, chapter 4.1): rim radius 13.2 cm, floor radius 9.6 cm.
			Layout = {TEXT("SM_Bucket"), DIAAssets::EFallback::Cylinder, DIAModelSizes::BucketCm, FVector(0.0, 0.0, -14.0), FRotator(0.0, 0.0, 0.0), 1.5, 9.6, 37.5, 13.2};
			break;
		case ELotHand::Trowel:
			Layout = {TEXT("SM_Trowel"), DIAAssets::EFallback::Cube, DIAModelSizes::TrowelCm, FVector(4.0, 0.0, 17.0), FRotator(10.0, 0.0, 0.0)};
			break;
		case ELotHand::Bowl:
		default:
			// Inner measurements of SM_Bowl: floor at 1.2 cm (radius 6.0 cm), rim at 9.0 cm (radius 11.7 cm).
			Layout = {TEXT("SM_Bowl"), DIAAssets::EFallback::Cylinder, DIAModelSizes::BowlCm, FVector(0.0, 0.0, 0.0), FRotator(0.0, 0.0, 0.0), 1.2, 6.0, 9.0, 11.7};
			break;
		}
		return Layout;
	}

	/** The spoon lies in the bowl with its working edge dipped towards the front. */
	const FVector SpoonLocationCm(-4.0, 0.0, 12.5);
	const FRotator SpoonRotation(-20.0, 0.0, 0.0);
	/** The disc stays this far inside the rim and the walls, so it never pokes out through the vessel. */
	constexpr double LiquidInsetCm = 0.4;
	/** Content below this shows nothing. */
	constexpr double LiquidMinLitres = 1e-4;
	/** Engine cylinder: 100 cm across and 100 cm high. */
	constexpr double BasicShapeCm = 100.0;

	void ConfigureToolMesh(UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetGenerateOverlapEvents(false);
	}
}

ADIACharacter::ADIACharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm);

	// First person: the body turns with the camera (the controller's yaw), it does not turn towards the walking direction.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = static_cast<float>(LotBalance::WalkSpeedCmS);
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	// The player cannot jump (no binding), and the lot is flat.
	Movement->JumpZVelocity = 0.f;

	// The eyes: at EyeHeightCm above the ground, turned by the controller.
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0, 0.0, DIACharacter::EyeHeightCm - DIACharacter::CapsuleHalfHeightCm));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = DIACharacter::NormalFovDeg;

	// The external view (dia.Camera): the template's boom behind the player, switched off until asked for.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DIACharacter::CameraBoomLengthCm;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetAutoActivate(false);

	// The tool hangs on a pivot in front of the camera, lower right.
	HandPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HandPivot"));
	HandPivot->SetupAttachment(FirstPersonCamera);
	HandPivot->SetRelativeLocation(DIACharacter::HandPivotOffsetCm);

	HandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandMesh"));
	HandMesh->SetupAttachment(HandPivot);
	ConfigureToolMesh(HandMesh);

	SpoonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpoonMesh"));
	SpoonMesh->SetupAttachment(HandPivot);
	ConfigureToolMesh(SpoonMesh);

	// The level of the asphalt in a vessel: a flat disc of the palette's asphalt, child of the vessel.
	LiquidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LiquidMesh"));
	LiquidMesh->SetupAttachment(HandMesh);
	ConfigureToolMesh(LiquidMesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		LiquidMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (UMaterialInterface* Asphalt = DIAAssets::Palette(TEXT("DIA_Asphalt")))
	{
		LiquidMesh->SetMaterial(0, Asphalt);
	}
	LiquidMesh->SetVisibility(false);

	// Mannequin: SKM_Manny_Simple with the unarmed animation blueprint (copied from the engine template in Task 8).
	// The owner does not see the body (the camera is inside it), but it casts its shadow on the ground.
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
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->SetCastHiddenShadow(true);
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
	RefreshHandMesh();
	StateSub.Bind(GetSubsystem(), this, &ADIACharacter::OnStateChanged, &ADIACharacter::HandleGameReset);
}

void ADIACharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StateSub.Unbind(GetSubsystem());
	Super::EndPlay(EndPlayReason);
}

void ADIACharacter::OnStateChanged()
{
	// Fires every tick; this only matters when the Sprint unlock was just reached or the core changed the hand.
	UpdateMovementSettings();
	RefreshHandMesh();
}

void ADIACharacter::HandleGameReset()
{
	SetSprinting(false);
	SetUsing(false);
	SetWorkView(false);

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
		// The view goes level again, along the character.
		Controller->SetControlRotation(FRotator(0.0, State.PlayerYawDeg, 0.0));
	}
	RefreshHandMesh();
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

void ADIACharacter::SetUsing(bool bUse)
{
	if (bUse && !bUsing)
	{
		// A new press: the bucket may be emptied once again.
		bBucketDumped = false;
	}
	if (!bUse)
	{
		bTrowelDown = false;
	}
	bUsing = bUse;
}

void ADIACharacter::SetWorkView(bool bWork)
{
	bWorkView = bWork;
}

bool ADIACharacter::SelectHand(ELotHand Hand)
{
	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->GetGame().SelectHand(Hand))
	{
		return false;
	}
	RefreshHandMesh();
	return true;
}

void ADIACharacter::NextHand()
{
	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const int32 Count = static_cast<int32>(ELotHand::Count);
	const int32 Current = static_cast<int32>(Subsystem->GetGame().GetHand());
	for (int32 Step = 1; Step < Count; ++Step)
	{
		const ELotHand Candidate = static_cast<ELotHand>((Current + Step) % Count);
		if (Subsystem->GetGame().CanSelectHand(Candidate))
		{
			SelectHand(Candidate);
			return;
		}
	}
}

float ADIACharacter::GetWorkViewBlend() const
{
	return FMath::SmoothStep(0.f, 1.f, WorkViewAlpha);
}

FQuat ADIACharacter::GetViewQuat() const
{
	// Without a controller (tests, a pawn nobody drives) the camera component keeps the view itself.
	const FQuat Base = Controller ? FQuat(Controller->GetControlRotation()) : FirstPersonCamera->GetComponentQuat();
	return Base * FQuat(FRotator(-DIACharacter::WorkViewTiltDeg * GetWorkViewBlend(), 0.0, 0.0));
}

FVector2D ADIACharacter::GetAimPoint() const
{
	const FVector Location = GetActorLocation();
	const FVector2D Player(Location.X, Location.Y);
	const FVector Eye(Location.X, Location.Y, Location.Z - DIACharacter::CapsuleHalfHeightCm + DIACharacter::EyeHeightCm);
	const double GroundZ = Location.Z - DIACharacter::CapsuleHalfHeightCm;
	const FVector Direction = GetViewQuat().GetForwardVector();

	FVector2D Hit = Player;
	if (Direction.Z < -UE_KINDA_SMALL_NUMBER)
	{
		const double Distance = (GroundZ - Eye.Z) / Direction.Z;
		Hit = FVector2D(Eye.X + Direction.X * Distance, Eye.Y + Direction.Y * Distance);
	}
	else
	{
		// The view never meets the ground: the point is as far ahead as the reach allows.
		Hit = Player + FVector2D(Direction.X, Direction.Y).GetSafeNormal() * LotBalance::ReachCm;
	}
	const FVector2D Offset = Hit - Player;
	return Offset.SizeSquared() > FMath::Square(LotBalance::ReachCm) ? Player + Offset.GetSafeNormal() * LotBalance::ReachCm : Hit;
}

void ADIACharacter::SetExternalView(bool bExternal, float DistanceCm)
{
	bExternalView = bExternal;
	if (bExternal)
	{
		CameraBoom->TargetArmLength = DistanceCm;
		FirstPersonCamera->Deactivate();
		FollowCamera->Activate();
	}
	else
	{
		FollowCamera->Deactivate();
		FirstPersonCamera->Activate();
	}
	// The body is for the owner to see only from outside; the tool hangs in front of the eyes and is shown only from them.
	GetMesh()->SetOwnerNoSee(!bExternal);
	ShownHand = ELotHand::Count;
	RefreshHandMesh();
}

void ADIACharacter::RefreshHandMesh()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const ELotHand Hand = Subsystem->GetGame().GetHand();
	if (Hand != ShownHand)
	{
		ShownHand = Hand;
		ShownLitres = -1.0;
		bTrowelDown = false;

		const FHandLayout Layout = LayoutOf(Hand);
		// The model's own origin is where the tool is held; a placeholder shape rests on it instead.
		HandMesh->SetRelativeScale3D(FVector::OneVector);
		HandMesh->SetRelativeLocationAndRotation(Layout.Location, Layout.Rotation);
		DIAAssets::SetModelOrFallback(HandMesh, Layout.Model, Layout.Shape, Layout.SizeCm, FVector(0.0, 0.0, Layout.SizeCm.Z * 0.5), TEXT("DIA_Steel"));
		HandMesh->SetVisibility(!bExternalView);

		const bool bSpoon = Hand == ELotHand::Bowl;
		if (bSpoon)
		{
			SpoonMesh->SetRelativeScale3D(FVector::OneVector);
			SpoonMesh->SetRelativeLocationAndRotation(SpoonLocationCm, SpoonRotation);
			DIAAssets::SetModelOrFallback(SpoonMesh, TEXT("SM_Spoon"), DIAAssets::EFallback::Cube, DIAModelSizes::SpoonCm, FVector::ZeroVector, TEXT("DIA_Steel"));
		}
		SpoonMesh->SetVisibility(bSpoon && !bExternalView);
	}
	UpdateLiquid();
}

void ADIACharacter::UpdateLiquid()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const FLotGame& Game = Subsystem->GetGame();
	const FHandLayout Layout = LayoutOf(Game.GetHand());
	const double Litres = Game.GetHandLitres(Game.GetHand());
	if (Litres == ShownLitres)
	{
		return;
	}
	ShownLitres = Litres;

	const double Capacity = Game.GetHandCapacity(Game.GetHand());
	if (Layout.RimZ <= 0.0 || Capacity <= 0.0 || Litres < LiquidMinLitres || bExternalView)
	{
		LiquidMesh->SetVisibility(false);
		return;
	}
	// The level rises linearly with the content; the disc is as wide as the vessel is at that height.
	const double Fill = FMath::Clamp(Litres / Capacity, 0.0, 1.0);
	const double Top = Layout.RimZ - LiquidInsetCm;
	const double Z = FMath::Lerp(Layout.FloorZ, Top, Fill);
	const double Across = (Z - Layout.FloorZ) / FMath::Max(Layout.RimZ - Layout.FloorZ, UE_KINDA_SMALL_NUMBER);
	const double Radius = FMath::Max(FMath::Lerp(Layout.FloorRadius, Layout.RimRadius, Across) - LiquidInsetCm, 1.0);
	LiquidMesh->SetRelativeScale3D(FVector(2.0 * Radius / BasicShapeCm, 2.0 * Radius / BasicShapeCm, DIACharacter::LiquidThicknessCm / BasicShapeCm));
	LiquidMesh->SetRelativeLocation(FVector(0.0, 0.0, Z));
	LiquidMesh->SetVisibility(true);
}

void ADIACharacter::UpdateWorkView(float DeltaSeconds)
{
	const float Target = bWorkView ? 1.f : 0.f;
	const float Step = DeltaSeconds / DIACharacter::WorkViewBlendSeconds;
	WorkViewAlpha = FMath::Clamp(WorkViewAlpha + FMath::Clamp(Target - WorkViewAlpha, -Step, Step), 0.f, 1.f);

	const float Blend = GetWorkViewBlend();
	const float Fov = FMath::Lerp(DIACharacter::NormalFovDeg, DIACharacter::WorkViewFovDeg, Blend);
	FirstPersonCamera->SetFieldOfView(Fov);
	const FRotator Tilt(-DIACharacter::WorkViewTiltDeg * Blend, 0.0, 0.0);
	FirstPersonCamera->AddAdditiveOffset(FTransform(Tilt), 0.f);

	// The tool stays in the same place and size of the picture: it follows the tilt, and comes closer as the field of view narrows.
	const double Zoom = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5)) / FMath::Tan(FMath::DegreesToRadians(DIACharacter::NormalFovDeg * 0.5));
	HandPivot->SetRelativeLocationAndRotation(FQuat(Tilt).RotateVector(DIACharacter::HandPivotOffsetCm * Zoom), Tilt);
	HandPivot->SetRelativeScale3D(FVector(Zoom));
}

void ADIACharacter::UseHand(UDIAWorldSubsystem& Subsystem, float DeltaSeconds)
{
	FLotGame& Game = Subsystem.GetGame();
	const FVector2D Aim = GetAimPoint();
	const FVector Location = GetActorLocation();
	const FVector2D Player(Location.X, Location.Y);
	const ELotHand Hand = Game.GetHand();

	if (Hand != ELotHand::Trowel)
	{
		bTrowelDown = false;
	}

	switch (Hand)
	{
	case ELotHand::Bowl:
	case ELotHand::Scoop:
		if (FVector2D::Distance(Aim, LotLayout::TroughPos) <= LotLayout::TroughReachCm)
		{
			Game.ScoopFromTrough(DeltaSeconds);
		}
		else
		{
			Subsystem.Dispatch(Game.PourHand(Aim, Player, DeltaSeconds));
		}
		break;
	case ELotHand::Bucket:
		if (DIARefill::IsAtOutlet(Game, Aim))
		{
			Game.FillBucket(ELotSource::Plant, DeltaSeconds);
		}
		else if (!bBucketDumped && !Game.IsHandBusy())
		{
			// One emptying per press of the button.
			bBucketDumped = true;
			Subsystem.Dispatch(Game.DumpBucket(Aim, Player));
		}
		break;
	case ELotHand::Trowel:
	{
		// The contact point starts at the aim on the press and follows it no faster than the trowel can be moved.
		if (!bTrowelDown)
		{
			bTrowelDown = true;
			TrowelContact = Aim;
		}
		const FVector2D Previous = TrowelContact;
		const FVector2D ToAim = Aim - TrowelContact;
		const double MaxStep = LotBalance::TrowelSpeedCmS * DeltaSeconds;
		TrowelContact = ToAim.SizeSquared() > FMath::Square(MaxStep) ? TrowelContact + ToAim.GetSafeNormal() * MaxStep : Aim;
		Subsystem.Dispatch(Game.Trowel(Previous, TrowelContact));
		break;
	}
	default:
		break;
	}
}

void ADIACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateWorkView(DeltaSeconds);

	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		const FVector Location = GetActorLocation();
		FLotWorldState& State = Subsystem->GetGame().World();
		State.PlayerPos = FVector2D(Location.X, Location.Y);
		State.PlayerYawDeg = GetActorRotation().Yaw;

		if (bUsing && !bExternalView)
		{
			UseHand(*Subsystem, DeltaSeconds);
		}
		else
		{
			bTrowelDown = false;
		}
		// The level of the bowl or bucket follows the scooping and pouring from tick to tick.
		UpdateLiquid();
	}
}
