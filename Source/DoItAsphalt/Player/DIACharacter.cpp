// Copyright Do it Asphalt. All Rights Reserved.

#include "Player/DIACharacter.h"
#include "AsphaltBalance.h"
#include "AsphaltGame.h"
#include "AsphaltLayout.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
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
	/** Bones of the Mannequin skeleton that carry the backpack and the helmet. */
	const FName BackpackBone(TEXT("spine_03"));
	const FName HelmetBone(TEXT("head"));

	bool IsEngineShape(const UStaticMesh* Mesh)
	{
		return Mesh && Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"));
	}

	/**
	 * Adds a static mesh below Pivot. The real model (/Game/Models/<MeshName>) sits at the pivot as it is, scaled in Y by
	 * RealYScale; if it does not exist an engine shape of SizeCm is used instead, moved by FallbackOffsetCm (in the
	 * pivot's axes) so that it occupies the place the real model would, and painted with the palette material. SideCm moves
	 * either of them sideways (along the pivot's Y) as a whole.
	 */
	UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Pivot, FName MeshName, DIAAssets::EFallback Shape, const FVector& SizeCm,
		const FVector& FallbackOffsetCm, FName PaletteName, double RealYScale, double SideCm = 0.0)
	{
		FVector Scale = FVector::OneVector;
		UStaticMesh* Mesh = DIAAssets::LoadMeshOrFallback(MeshName, Shape, SizeCm, Scale);
		if (!Mesh)
		{
			return nullptr;
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetupAttachment(Pivot);
		if (IsEngineShape(Mesh))
		{
			Component->SetRelativeLocation(FallbackOffsetCm + FVector(0.0, SideCm, 0.0));
			Component->SetRelativeScale3D(Scale);
			if (UMaterialInterface* Material = DIAAssets::Palette(PaletteName))
			{
				Component->SetMaterial(0, Material);
			}
		}
		else
		{
			Component->SetRelativeLocation(FVector(0.0, SideCm, 0.0));
			Component->SetRelativeScale3D(FVector(1.0, RealYScale, 1.0));
		}
		Component->RegisterComponent();
		return Component;
	}

	void DestroyPart(TObjectPtr<UStaticMeshComponent>& Part)
	{
		if (Part)
		{
			Part->DestroyComponent();
			Part = nullptr;
		}
	}
}

ADIACharacter::ADIACharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm);

	// Like the third-person template: the controller only turns the camera, the body follows the movement direction.
	// (While pouring it follows the camera instead, see UpdateMovementSettings.)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, DIACharacter::TurnRateDegS, 0.f);
	Movement->MaxWalkSpeed = static_cast<float>(AsphaltBalance::WalkSpeedCmS);
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

	// Equipment pivots. The spreader's origin lies on the ground under the middle of the bar, NozzleAheadCm in front.
	SpreaderPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SpreaderPivot"));
	SpreaderPivot->SetupAttachment(GetCapsuleComponent());
	SpreaderPivot->SetRelativeLocation(FVector(AsphaltBalance::NozzleAheadCm, 0.0, -DIACharacter::CapsuleHalfHeightCm));

	BackpackPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BackpackPivot"));
	BackpackPivot->SetupAttachment(BodyMesh, BackpackBone);
	BackpackPivot->SetRelativeLocationAndRotation(DIACharacter::BackpackBoneOffsetCm, DIACharacter::BoneToCharacter);

	HelmetPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HelmetPivot"));
	HelmetPivot->SetupAttachment(BodyMesh, HelmetBone);
	HelmetPivot->SetRelativeLocationAndRotation(DIACharacter::HelmetBoneOffsetCm, DIACharacter::BoneToCharacter);
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

	BuildHelmet();
	RefreshEquipment();
	UpdateMovementSettings();
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		StateHandle = Subsystem->OnStateChanged.AddUObject(this, &ADIACharacter::OnStateChanged);
	}
}

void ADIACharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StateHandle.IsValid())
	{
		if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->OnStateChanged.Remove(StateHandle);
		}
		StateHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ADIACharacter::OnStateChanged()
{
	// Fires every tick; both calls return at once unless a level or a special changed.
	RefreshEquipment();
	UpdateMovementSettings();
}

void ADIACharacter::SetPouring(bool bPour)
{
	if (bPouring == bPour)
	{
		return;
	}
	bPouring = bPour;
	if (bPouring)
	{
		// Face the camera first, so that the stroke starts where the nozzle really is.
		if (Controller)
		{
			SetActorRotation(FRotator(0.0, Controller->GetControlRotation().Yaw, 0.0));
		}
		PreviousNozzle = GetNozzlePoint();
	}
	UpdateMovementSettings();
}

void ADIACharacter::SetSprinting(bool bSprint)
{
	bSprinting = bSprint;
	UpdateMovementSettings();
}

void ADIACharacter::UpdateMovementSettings()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	double Speed = bPouring ? AsphaltBalance::PourSpeedCmS : AsphaltBalance::WalkSpeedCmS;
	if (!bPouring && bSprinting)
	{
		const UDIAWorldSubsystem* Subsystem = GetSubsystem();
		if (Subsystem && Subsystem->GetGame().HasSpecial(EAsphaltSpecial::Sprint))
		{
			Speed *= AsphaltBalance::SprintMultiplier;
		}
	}
	Movement->MaxWalkSpeed = static_cast<float>(Speed);
	// While pouring the body looks where the camera looks; otherwise it turns towards where it walks.
	Movement->bOrientRotationToMovement = !bPouring;
	bUseControllerRotationYaw = bPouring;
}

FVector2D ADIACharacter::GetNozzlePoint() const
{
	const FVector Location = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	return FVector2D(Location.X, Location.Y) + FVector2D(Forward.X, Forward.Y).GetSafeNormal() * AsphaltBalance::NozzleAheadCm;
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

	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	FAsphaltGame& Game = Subsystem->GetGame();

	const FVector Location = GetActorLocation();
	const FVector2D Position(Location.X, Location.Y);
	Game.World().PlayerPos = Position;
	Game.World().PlayerYawDeg = GetActorRotation().Yaw;

	if (bPouring)
	{
		const FVector2D Current = GetNozzlePoint();
		Subsystem->Pour(PreviousNozzle, Current, /*bFromCar*/ false);
		PreviousNozzle = Current;
	}

	// The subsystem broadcasts OnStateChanged every tick, so the HUD sees the refilled backpack without a second dispatch.
	// Driving the truck the character is carried along: the truck's own tank is the one that fills then.
	if (!Game.World().bPlayerInCar && DIARefill::IsInZone(Game, Position))
	{
		Game.RefillBackpack(DeltaSeconds);
	}
}

void ADIACharacter::BuildHelmet()
{
	DestroyPart(HelmetMesh);
	// Origin of SM_Helmet is the centre of its lower edge; the placeholder is a flattened sphere with the same envelope.
	HelmetMesh = AddPart(this, HelmetPivot, TEXT("SM_Helmet"), DIAAssets::EFallback::Sphere, DIAModelSizes::HelmetCm,
		FVector(0.0, 0.0, DIAModelSizes::HelmetCm.Z * 0.5), TEXT("DIA_Yellow"), 1.0);
}

void ADIACharacter::RefreshEquipment()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !GetWorld())
	{
		return;
	}
	const FAsphaltGame& Game = Subsystem->GetGame();
	const int32 BackpackLevel = Game.GetLevel(EAsphaltUpgrade::Backpack);
	const int32 BackpackTier = AsphaltBalance::BackpackTier(BackpackLevel);
	const int32 ToolLevel = Game.GetLevel(EAsphaltUpgrade::Tool);
	const bool bTwoHands = Game.HasSpecial(EAsphaltSpecial::TwoHands);

	if (BackpackTier != ShownBackpackTier || !BackpackMesh)
	{
		ShownBackpackTier = BackpackTier;
		DestroyPart(BackpackMesh);
		static const TCHAR* TierNames[] = {TEXT("SM_Backpack_S"), TEXT("SM_Backpack_M"), TEXT("SM_Backpack_L"), TEXT("SM_Backpack_XL")};
		const FVector& Size = DIAModelSizes::BackpackCm[BackpackTier];
		// The origin is the middle of the plane that touches the back, the tank extends towards -X.
		BackpackMesh = AddPart(this, BackpackPivot, TierNames[BackpackTier], DIAAssets::EFallback::Cube, Size,
			FVector(-Size.X * 0.5, 0.0, 0.0), TEXT("DIA_Orange"), 1.0);
	}

	if (ToolLevel != ShownToolLevel || bTwoHands != bShownTwoHands || !SpreaderMesh)
	{
		ShownToolLevel = ToolLevel;
		bShownTwoHands = bTwoHands;
		DestroyPart(SpreaderMesh);
		DestroyPart(SecondSpreaderMesh);
		// W is the width of one spreader. With TwoHands there are two of them side by side at Y = -W/2 and +W/2, together
		// 2 W wide, which is what GetToolWidthCm() reports (and what pouring uses).
		const double LevelWidthCm = AsphaltBalance::ToolWidthM[ToolLevel] * 100.0;
		const FString Name = FString::Printf(TEXT("SM_Spreader_%03d"), FMath::RoundToInt(LevelWidthCm));
		const FVector PlaceholderSizeCm(DIAModelSizes::SpreaderDepthCm, LevelWidthCm, DIAModelSizes::SpreaderHeightCm);
		const FVector PlaceholderOffsetCm(0.0, 0.0, DIAModelSizes::SpreaderHeightCm * 0.5);
		const double SideCm = bTwoHands ? LevelWidthCm * 0.5 : 0.0;
		// The placeholder is as wide as the bar and rests on the ground.
		SpreaderMesh = AddPart(this, SpreaderPivot, FName(*Name), DIAAssets::EFallback::Cube, PlaceholderSizeCm, PlaceholderOffsetCm,
			TEXT("DIA_Yellow"), 1.0, -SideCm);
		if (bTwoHands)
		{
			SecondSpreaderMesh = AddPart(this, SpreaderPivot, FName(*Name), DIAAssets::EFallback::Cube, PlaceholderSizeCm, PlaceholderOffsetCm,
				TEXT("DIA_Yellow"), 1.0, SideCm);
		}
	}
}
