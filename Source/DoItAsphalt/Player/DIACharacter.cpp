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
#include "Game/DIASettings.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIARefill.h"
#include "Game/DIAWorldSubsystem.h"
#include "World/DIAMovable.h"
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

	/** Where the bowl sits on the pivot (the left hand), and where the spoon is held (the right hand). Pivot axes: X forward, Y right, Z up. */
	const FVector BowlLocationCm(-4.0, -36.0, 6.0);

	FHandLayout LayoutOf(ELotHand Hand)
	{
		FHandLayout Layout;
		switch (Hand)
		{
		case ELotHand::Canister:
			// Inner measurements of SM_Canister: floor radius 9.0 cm at 1.5 cm, rim radius 10.5 cm at 40 cm; the pouring beak is on +X.
			Layout = {TEXT("SM_Canister"), DIAAssets::EFallback::Cylinder, DIAModelSizes::CanisterCm, FVector(12.0, 3.0, -16.0), FRotator(0.0, 0.0, 0.0), 1.5, 9.0, 40.0, 10.5};
			break;
		case ELotHand::Tub:
			// Inner measurements of SM_Tub: floor radius 15.0 cm at 1.5 cm, rim radius 23.5 cm at 22 cm; the lip is on +X.
			Layout = {TEXT("SM_Tub"), DIAAssets::EFallback::Cylinder, DIAModelSizes::TubCm, FVector(20.0, 0.0, -12.0), FRotator(0.0, 0.0, 0.0), 1.5, 15.0, 22.0, 23.5};
			break;
		case ELotHand::Bucket:
			// Inner measurements of SM_Bucket (docs/04, chapter 4.1): rim radius 13.2 cm, floor radius 9.6 cm; the body ends at about 25 cm, the bail rises above it.
			Layout = {TEXT("SM_Bucket"), DIAAssets::EFallback::Cylinder, DIAModelSizes::BucketCm, FVector(10.0, 2.0, -8.0), FRotator(0.0, 0.0, 0.0), 1.5, 9.6, 25.0, 13.2};
			break;
		case ELotHand::Bowl:
		default:
			// Inner measurements of SM_Bowl: floor at 1.2 cm (radius 6.0 cm), rim at 9.0 cm (radius 11.7 cm).
			// The bowl is held in the left hand, to the left of the pivot, and the spoon in the right.
			Layout = {TEXT("SM_Bowl"), DIAAssets::EFallback::Cylinder, DIAModelSizes::BowlCm, BowlLocationCm, FRotator(0.0, 0.0, 0.0), 1.2, 6.0, 9.0, 11.7};
			break;
		}
		return Layout;
	}

	/** The spoon is held at its handle in the right hand, its working edge dipped towards the bowl in the left. */
	const FVector SpoonLocationCm(-6.0, -8.0, 14.0);
	const FRotator SpoonRotation(-30.0, -36.0, 0.0);
	/** The spoon's other end of the stroke: over the trough, ahead of the player and low (pivot axes); the rest pose above is over the bowl. */
	const FVector SpoonDipLocationCm(26.0, -4.0, -6.0);
	const FRotator SpoonDipRotation(-62.0, -8.0, 0.0);
	/** The vessel under a tap or the outlet is held forward and a little up. */
	const FVector FillHoldOffsetCm(9.0, 0.0, 4.0);
	/** The gloves grip the rim of the bowl (left) and the handle of the spoon (right); a sleeve runs from each out of the picture. */
	const FVector LeftGloveCm(-4.0, -49.0, 8.0);
	const FVector RightGloveCm(-9.0, -5.0, 14.0);
	const FVector GloveSizeCm(8.0, 8.0, 7.0);
	const FVector SleeveSizeCm(6.0, 6.0, 46.0);
	/** Directions the sleeves run in, away from the gloves (pivot axes). */
	const FVector LeftSleeveDirection(-0.75, -0.45, -0.45);
	const FVector RightSleeveDirection(-0.75, 0.45, -0.45);
	/** The disc stays this far inside the rim and the walls, so it never pokes out through the vessel. */
	constexpr double LiquidInsetCm = 0.4;
	/** Content below this shows nothing. */
	constexpr double LiquidMinLitres = 1e-4;
	/** Engine cylinder: 100 cm across and 100 cm high. */
	constexpr double LiquidShapeCm = 100.0;

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

	// The two hands of the bowl and the spoon.
	auto MakePart = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(HandPivot);
		ConfigureToolMesh(Part);
		Part->SetVisibility(false);
		return Part;
	};
	LeftGlove = MakePart(TEXT("LeftGlove"));
	LeftSleeve = MakePart(TEXT("LeftSleeve"));
	RightGlove = MakePart(TEXT("RightGlove"));
	RightSleeve = MakePart(TEXT("RightSleeve"));

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
	SetFlap(false);
	SetFilling(false);
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
	// The one place that decides the speed: what the player holds sets it, and only a free player may sprint.
	double Speed = LotBalance::WalkSpeedCmS;
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	const FLotGame* Game = Subsystem ? &Subsystem->GetGame() : nullptr;
	switch (Game ? Game->GetHeld() : ELotMovable::Count)
	{
	case ELotMovable::Suitcase:
		Speed = LotBalance::SuitcasePullSpeedCmS;
		break;
	case ELotMovable::Cart:
		Speed = bUsing ? LotBalance::CartLaySpeedCmS : LotBalance::CartPushSpeedCmS;
		break;
	case ELotMovable::Station:
		Speed = LotBalance::StationCarrySpeedCmS;
		break;
	default:
		if (bSprinting && Game && Game->IsUnlocked(ELotUnlock::Sprint))
		{
			Speed *= LotBalance::SprintMultiplier;
		}
		break;
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
	bUsing = bUse;
	// Laying a strip with the cart is slower than pushing it.
	UpdateMovementSettings();
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
	// A movable in the hands hides the tool, and the tool comes back when it is let go.
	const ELotMovable Held = Subsystem->GetGame().GetHeld();
	const bool bToolShown = !bExternalView && Held == ELotMovable::Count;
	if (Hand != ShownHand || Held != ShownHeld)
	{
		ShownHand = Hand;
		ShownHeld = Held;
		ShownLitres = -1.0;
		AppliedRightHandDelta = FVector::ZeroVector;
		PourBlend = 0.f;
		FillBlend = 0.f;
		StrokePhase = 0.f;

		const FHandLayout Layout = LayoutOf(Hand);
		// The model's own origin is where the tool is held; a placeholder shape rests on it instead.
		HandMesh->SetRelativeScale3D(FVector::OneVector);
		HandMesh->SetRelativeLocationAndRotation(Layout.Location, Layout.Rotation);
		DIAAssets::SetModelOrFallback(HandMesh, Layout.Model, Layout.Shape, Layout.SizeCm, FVector(0.0, 0.0, Layout.SizeCm.Z * 0.5), TEXT("DIA_Steel"));
		HandMesh->SetVisibility(bToolShown);

		const bool bSpoon = Hand == ELotHand::Bowl;
		if (bSpoon)
		{
			SpoonMesh->SetRelativeScale3D(FVector::OneVector);
			SpoonMesh->SetRelativeLocationAndRotation(SpoonLocationCm, SpoonRotation);
			DIAAssets::SetModelOrFallback(SpoonMesh, TEXT("SM_Spoon"), DIAAssets::EFallback::Cube, DIAModelSizes::SpoonCm, FVector::ZeroVector, TEXT("DIA_Steel"));
		}
		SpoonMesh->SetVisibility(bSpoon && bToolShown);

		// The bowl and the spoon take both hands: a glove and a sleeve on each.
		const auto PlaceHand = [bSpoon, bToolShown](UStaticMeshComponent* Glove, UStaticMeshComponent* Sleeve, const FVector& At, const FVector& Away)
		{
			if (bSpoon)
			{
				Glove->SetRelativeScale3D(FVector::OneVector);
				Glove->SetRelativeLocationAndRotation(At, FRotator::ZeroRotator);
				DIAAssets::SetModelOrFallback(Glove, TEXT("SM_Glove"), DIAAssets::EFallback::Sphere, GloveSizeCm, FVector::ZeroVector, TEXT("DIA_Orange"));
				const FVector Direction = Away.GetSafeNormal();
				Sleeve->SetRelativeScale3D(FVector::OneVector);
				Sleeve->SetRelativeLocationAndRotation(At + Direction * (SleeveSizeCm.Z * 0.5 - 2.0), FQuat::FindBetweenNormals(FVector::UpVector, Direction).Rotator());
				DIAAssets::SetModelOrFallback(Sleeve, TEXT("SM_Sleeve"), DIAAssets::EFallback::Cylinder, SleeveSizeCm, FVector::ZeroVector, TEXT("DIA_DarkSteel"));
			}
			Glove->SetVisibility(bSpoon && bToolShown);
			Sleeve->SetVisibility(bSpoon && bToolShown);
		};
		PlaceHand(LeftGlove, LeftSleeve, LeftGloveCm, LeftSleeveDirection);
		PlaceHand(RightGlove, RightSleeve, RightGloveCm, RightSleeveDirection);
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
	if (Layout.RimZ <= 0.0 || Capacity <= 0.0 || Litres < LiquidMinLitres || bExternalView || Game.GetHeld() != ELotMovable::Count)
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
	LiquidMesh->SetRelativeScale3D(FVector(2.0 * Radius / LiquidShapeCm, 2.0 * Radius / LiquidShapeCm, DIACharacter::LiquidThicknessCm / LiquidShapeCm));
	LiquidMesh->SetRelativeLocation(FVector(0.0, 0.0, Z));
	LiquidMesh->SetVisibility(true);
}

void ADIACharacter::UpdateWorkView(float DeltaSeconds)
{
	const float Target = bWorkView ? 1.f : 0.f;
	const float Step = DeltaSeconds / DIACharacter::WorkViewBlendSeconds;
	WorkViewAlpha = FMath::Clamp(WorkViewAlpha + FMath::Clamp(Target - WorkViewAlpha, -Step, Step), 0.f, 1.f);

	const float Blend = GetWorkViewBlend();
	// The player's field of view is the normal one; the work view is narrower by the same factor as before.
	const float BaseFov = DIASettings::GetFieldOfView();
	const float Fov = FMath::Lerp(BaseFov, BaseFov * DIACharacter::WorkViewFovDeg / DIACharacter::NormalFovDeg, Blend);
	FirstPersonCamera->SetFieldOfView(Fov);
	const FRotator Tilt(-DIACharacter::WorkViewTiltDeg * Blend, 0.0, 0.0);
	// AddAdditiveOffset accumulates (offset = offset * new), so the old tilt is dropped first: the offset is always exactly the tilt of this tick.
	FirstPersonCamera->ClearAdditiveOffset();
	if (Blend > 0.f)
	{
		FirstPersonCamera->AddAdditiveOffset(FTransform(Tilt), 0.f);
	}

	// The tool stays in the same place and size of the picture: it follows the tilt, and its sideways and vertical offsets and its size shrink
	// with the field of view (the picture magnifies by the same factor), while its distance stays.
	const double Zoom = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5)) / FMath::Tan(FMath::DegreesToRadians(DIACharacter::NormalFovDeg * 0.5));
	const FVector Offset(DIACharacter::HandPivotOffsetCm.X, DIACharacter::HandPivotOffsetCm.Y * Zoom, DIACharacter::HandPivotOffsetCm.Z * Zoom);
	HandPivot->SetRelativeLocationAndRotation(FQuat(Tilt).RotateVector(Offset), Tilt);
	HandPivot->SetRelativeScale3D(FVector(Zoom));
}

void ADIACharacter::UpdateHandAnimation(float DeltaSeconds)
{
	const float Step = DIACharacter::AnimBlendPerSec * DeltaSeconds;
	const auto Approach = [Step](float& Value, bool bOn) { Value = FMath::Clamp(Value + (bOn ? Step : -Step), 0.f, 1.f); };
	Approach(PourBlend, bPourActive);
	Approach(FillBlend, bFillActive);

	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || ShownHand == ELotHand::Count || !HandMesh->IsVisible())
	{
		return;
	}
	const ELotHand Hand = ShownHand;
	const FHandLayout Layout = LayoutOf(Hand);
	const float Smooth = FMath::SmoothStep(0.f, 1.f, PourBlend);
	const float FillSmooth = FMath::SmoothStep(0.f, 1.f, FillBlend);

	if (Hand == ELotHand::Bowl)
	{
		// The spoon: one stroke per cycle, from over the bowl to the trough and back; it only moves while it fills, and rests over the bowl otherwise.
		StrokePhase = FMath::Frac(StrokePhase + DeltaSeconds / static_cast<float>(LotBalance::ScoopCycleSeconds) * (bFillActive ? 1.f : 0.f));
		if (!bFillActive && FillBlend <= 0.f)
		{
			StrokePhase = 0.f;
		}
		const float Reach = (0.5f - 0.5f * FMath::Cos(StrokePhase * UE_TWO_PI)) * FillSmooth;
		const FVector SpoonAt = FMath::Lerp(SpoonLocationCm, SpoonDipLocationCm, Reach);
		SpoonMesh->SetRelativeLocationAndRotation(SpoonAt, FMath::Lerp(SpoonRotation, SpoonDipRotation, Reach));
		// The right hand follows the spoon.
		const FVector Delta = SpoonAt - SpoonLocationCm;
		RightGlove->AddRelativeLocation(Delta - AppliedRightHandDelta);
		RightSleeve->AddRelativeLocation(Delta - AppliedRightHandDelta);
		AppliedRightHandDelta = Delta;
	}

	// The vessel in hand: tipped over its front rim while it pours; held forward and rocked while it fills.
	const float Tilt = DIACharacter::PourTiltDeg[FMath::Clamp(static_cast<int32>(Hand), 0, 3)] * Smooth;
	RockTime += DeltaSeconds;
	const float Rock = Hand == ELotHand::Bowl ? 0.f : DIACharacter::FillRockDeg * FillSmooth * FMath::Sin(RockTime * UE_TWO_PI * DIACharacter::FillRockHz);
	const FVector Rim = Layout.Location + FVector(Layout.RimRadius, 0.0, Layout.RimZ);
	const FQuat Turn = FRotator(-Tilt + Rock, 0.0, 0.0).Quaternion();
	const FVector Held = Layout.Location + (Hand == ELotHand::Bowl ? FVector::ZeroVector : FillHoldOffsetCm * FillSmooth);
	const FVector Rim2 = Rim + (Held - Layout.Location);
	const FVector At = Rim2 + Turn.RotateVector(Held - Rim2);
	HandMesh->SetRelativeLocationAndRotation(At, (Turn * Layout.Rotation.Quaternion()).Rotator());
}

void ADIACharacter::UseHand(UDIAWorldSubsystem& Subsystem, float DeltaSeconds)
{
	FLotGame& Game = Subsystem.GetGame();
	const FVector2D Aim = GetAimPoint();
	const FVector Location = GetActorLocation();
	const FVector2D Player(Location.X, Location.Y);

	if (Game.GetHeld() != ELotMovable::Count)
	{
		// The hands are on a suitcase, cart or machine: what they do is that object's own (the cart lays its strip from its own tick).
		return;
	}

	// The bowl and the vessels pour as long as the button is held, as a stream where the player aims.
	Subsystem.Dispatch(Game.PourHand(Aim, Player, DeltaSeconds));
}

void ADIACharacter::FillHand(UDIAWorldSubsystem& Subsystem, float DeltaSeconds)
{
	FLotGame& Game = Subsystem.GetGame();
	if (Game.GetHeld() != ELotMovable::Count)
	{
		return;
	}
	const FVector2D Aim = GetAimPoint();
	const ELotHand Hand = Game.GetHand();
	const bool bRoom = Game.GetHandLitres(Hand) < Game.GetHandCapacity(Hand) - 1e-6;
	if (Hand == ELotHand::Bowl)
	{
		if (FVector2D::Distance(Aim, LotLayout::TroughPos) <= LotLayout::TroughReachCm)
		{
			Game.ScoopFromTrough(DeltaSeconds);
			bFillActive = bRoom && Game.GetPlantStock() > 0.0;
		}
	}
	else if (FLotGame::IsVessel(Hand))
	{
		if (ELotSource Source; ADIAMovable::FindTapAt(GetWorld(), Aim, Source))
		{
			// The tap of a standing suitcase or cart comes before the outlet: the vessel fills from what stands there.
			Game.FillVessel(Hand, Source, DeltaSeconds);
			bFillActive = bRoom;
		}
		else if (DIARefill::IsAtOutlet(Game, Aim))
		{
			Game.FillVessel(Hand, ELotSource::Plant, DeltaSeconds);
			bFillActive = bRoom && Game.GetPlantStock() > 0.0;
		}
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

		bFillActive = false;
		if (bUsing && !bExternalView)
		{
			UseHand(*Subsystem, DeltaSeconds);
		}
		if (bFilling && !bExternalView)
		{
			FillHand(*Subsystem, DeltaSeconds);
		}
		{
			const FLotGame& Game = Subsystem->GetGame();
			bPourActive = bUsing && !bExternalView && Game.GetHeld() == ELotMovable::Count && Game.GetHandLitres(Game.GetHand()) > 0.0;
		}
		UpdateHandAnimation(DeltaSeconds);
		// The level of the bowl or bucket follows the scooping and pouring from tick to tick.
		UpdateLiquid();
	}
}
