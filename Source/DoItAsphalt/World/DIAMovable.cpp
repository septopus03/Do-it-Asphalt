// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAMovable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIARefill.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Player/DIACharacter.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * What differs between the four kinds: the model, its envelope (model axes, cm; Tools/ModelGen/manifest.json) and where the mouth
	 * is. How it is held is DIAMovableHold's.
	 */
	struct FKindSpec
	{
		FName Model;
		FVector BoundsMin;
		FVector BoundsMax;
		/** The flap or the outlet in model axes; ignored unless bHasMouth. */
		FVector Mouth;
		bool bHasMouth;

		FVector SizeCm() const { return BoundsMax - BoundsMin; }
		/** The middle of the envelope in model axes. */
		FVector CentreCm() const { return (BoundsMin + BoundsMax) * 0.5; }
	};

	FKindSpec SpecOf(ELotMovable Kind)
	{
		switch (Kind)
		{
		case ELotMovable::Suitcase:
			return {TEXT("SM_Suitcase"), FVector(-5.0, -25.4, 0.0), FVector(37.0, 25.4, 105.5), DIAModelSizes::SuitcaseMouthCm, true};
		case ELotMovable::Station:
			return {TEXT("SM_PortableStation"), FVector(-60.0, -40.0, 0.0), FVector(60.0, 40.0, 130.0), FVector::ZeroVector, false};
		case ELotMovable::Finisher:
			return {TEXT("SM_Finisher"), FVector(-122.0, -53.6, 0.0), FVector(10.0, 53.6, 100.0), FVector::ZeroVector, false};
		case ELotMovable::Cart:
		default:
			return {TEXT("SM_Cart"), FVector(-65.0, -33.5, 0.0), FVector(61.0, 33.5, 99.0), DIAModelSizes::CartMouthCm, true};
		}
	}

	/** Engine cube: 100 cm on a side. */
	constexpr double CubeShapeCm = 100.0;
	/** The level stays this far inside the walls and below the rim. */
	constexpr double LevelInsetCm = 0.4;
	/** The inside of the cart's tub (Tools/ModelGen/cart.py: 12 mm walls, outer 95 x 66 cm at the rim Z = 76, 66 x 44 cm at the floor Z = 30): centre X, floor and rim height, and the inner width (X) and depth (Y) at both. */
	constexpr double TubCentreXCm = -3.0;
	constexpr double TubFloorZCm = 31.2;
	constexpr double TubRimZCm = 76.0;
	const FVector2D TubFloorInnerCm(63.6, 41.6);
	const FVector2D TubRimInnerCm(92.6, 63.6);
	/** The suitcase's sight strip (Tools/ModelGen/suitcase.py: 3.6 cm wide, 30 cm high from Z = 19, in the front glass at X = 31, Y = -15): the column fills it from the bottom. */
	constexpr double SightBottomZCm = 19.0;
	constexpr double SightHeightCm = 30.0;
	constexpr double SightWidthCm = 3.0;
	constexpr double SightDepthCm = 0.3;
	const FVector2D SightXYCm(31.2, -15.0);
}

ADIAMovable::ADIAMovable()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Blocks the pawn like a small obstacle. Aiming and pouring are arithmetic on the ground plane, so nothing here can get in their way;
	// the camera boom is never pushed in by it.
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Root);
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The content: a flat box of the palette's asphalt (a disc in the cart's tub, a column in the suitcase's sight strip).
	LevelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LevelMesh"));
	LevelMesh->SetupAttachment(Root);
	LevelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LevelMesh->SetCastShadow(false);
	LevelMesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		LevelMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (UMaterialInterface* Asphalt = DIAAssets::Palette(TEXT("DIA_Asphalt")))
	{
		LevelMesh->SetMaterial(0, Asphalt);
	}
	LevelMesh->SetVisibility(false);
}

void ADIAMovable::Init(ELotMovable InKind)
{
	Kind = InKind;
	const FKindSpec Spec = SpecOf(Kind);

	// The footprint box is the envelope of the model; the real model has its origin where the envelope says, a placeholder is moved onto it.
	Collision->SetBoxExtent(Spec.SizeCm() * 0.5);
	Collision->SetRelativeLocation(Spec.CentreCm());
	BodyMesh->SetRelativeScale3D(FVector::OneVector);
	BodyMesh->SetRelativeLocation(FVector::ZeroVector);
	DIAAssets::SetModelOrFallback(BodyMesh, Spec.Model, DIAAssets::EFallback::Cube, Spec.SizeCm(), Spec.CentreCm(), TEXT("DIA_Steel"));
	ShownLitres = -1.0;
}

UDIAWorldSubsystem* ADIAMovable::GetSubsystem() const
{
	return UDIAWorldSubsystem::Get(this);
}

ADIACharacter* ADIAMovable::GetPlayer() const
{
	if (!Holder.IsValid())
	{
		ADIACharacter* Found = ADIACharacter::GetLocal(this);
		if (!Found)
		{
			// Without a controller (tests, a pawn nobody drives) any character will do.
			const UWorld* World = GetWorld();
			TActorIterator<ADIACharacter> It(World);
			Found = It ? *It : nullptr;
		}
		Holder = Found;
	}
	return Holder.Get();
}

bool ADIAMovable::IsHeld() const
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	return Subsystem && Subsystem->GetGame().GetHeld() == Kind;
}

void ADIAMovable::BeginPlay()
{
	Super::BeginPlay();

	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->RegisterInteractable(this, this);
		// OnStateChanged fires every tick; SyncFromCore returns at once unless the core and the actor differ.
		StateSub.Bind(Subsystem, this, &ADIAMovable::SyncFromCore, &ADIAMovable::HandleGameReset);
	}
	SyncFromCore();
	UpdateLevel();
}

void ADIAMovable::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		StateSub.Unbind(Subsystem);
		Subsystem->UnregisterInteractable(this);
	}
	Super::EndPlay(Reason);
}

void ADIAMovable::HandleGameReset()
{
	ToolPath.Reset();
	SyncFromCore();
}

void ADIAMovable::SyncFromCore()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->GetGame().OwnsMovable(Kind))
	{
		return;
	}
	const FLotGame& Game = Subsystem->GetGame();
	if (Game.GetHeld() == Kind)
	{
		if (!bShownHeld)
		{
			bShownHeld = true;
			ToolPath.Reset();
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		// The pose is the player's; the core's position is only written when it is let go.
		if (ADIACharacter* Player = GetPlayer())
		{
			TickAfter(Player);
			FollowPlayer(Player);
		}
		return;
	}
	const FLotMovable& State = Game.World().Movables[static_cast<int32>(Kind)];
	const FVector Location = GetActorLocation();
	const bool bMoved = !FVector2D(Location.X, Location.Y).Equals(State.Pos, DIAMovable::SyncToleranceCm)
		|| FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, State.YawDeg)) > DIAMovable::SyncToleranceDeg;
	if (bShownHeld || bMoved)
	{
		PlaceStanding(State.Pos, State.YawDeg);
	}
}

void ADIAMovable::PlaceStanding(FVector2D Where, double YawDeg)
{
	bShownHeld = false;
	ToolPath.Reset();
	TickAfter(nullptr);
	SetActorLocationAndRotation(FVector(Where.X, Where.Y, 0.0), FRotator(0.0, YawDeg, 0.0));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void ADIAMovable::FollowPlayer(const ADIACharacter* Player)
{
	const DIAMovableHold::FHold Hold = DIAMovableHold::Of(Kind);
	const FVector Location = Player->GetActorLocation();
	const double Yaw = Player->GetActorRotation().Yaw;
	const FVector2D Forward = FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const FVector2D Origin = FVector2D(Location.X, Location.Y) + Forward * Hold.AheadCm;
	SetActorLocationAndRotation(FVector(Origin.X, Origin.Y, 0.0), FRotator(Hold.TiltDeg, FRotator::NormalizeAxis(Yaw + Hold.YawDeg), 0.0));
}

void ADIAMovable::TickAfter(ADIACharacter* Character)
{
	if (TickedAfter.Get() == Character)
	{
		return;
	}
	if (ADIACharacter* Old = TickedAfter.Get())
	{
		RemoveTickPrerequisiteActor(Old);
		RemoveTickPrerequisiteComponent(Old->GetCharacterMovement());
	}
	TickedAfter = Character;
	if (Character)
	{
		// The character moves in its own tick and in its movement component's: the pose followed here must be after both.
		AddTickPrerequisiteActor(Character);
		AddTickPrerequisiteComponent(Character->GetCharacterMovement());
	}
}

FVector2D ADIAMovable::GetMouthPoint() const
{
	const FKindSpec Spec = SpecOf(Kind);
	const FVector Point = Spec.bHasMouth ? GetActorTransform().TransformPosition(Spec.Mouth) : GetActorLocation();
	return FVector2D(Point.X, Point.Y);
}

double ADIAMovable::GetDistanceToFootprint(FVector2D Point) const
{
	const FVector Location = GetActorLocation();
	// Into the actor's frame (it stands upright, turned about Z only).
	const FVector2D Local = (Point - FVector2D(Location.X, Location.Y)).GetRotated(-GetActorRotation().Yaw);
	const FVector Centre = Collision->GetRelativeLocation();
	const FVector Extent = Collision->GetUnscaledBoxExtent();
	const double OutsideX = FMath::Max(FMath::Abs(Local.X - Centre.X) - Extent.X, 0.0);
	const double OutsideY = FMath::Max(FMath::Abs(Local.Y - Centre.Y) - Extent.Y, 0.0);
	return FMath::Sqrt(FMath::Square(OutsideX) + FMath::Square(OutsideY));
}

bool ADIAMovable::FindTapAt(const UWorld* World, FVector2D Point, ELotSource& OutSource)
{
	const UDIAWorldSubsystem* Subsystem = World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return false;
	}
	for (TActorIterator<ADIAMovable> It(World); It; ++It)
	{
		const ELotMovable Movable = It->GetKind();
		const bool bTap = Movable == ELotMovable::Suitcase || Movable == ELotMovable::Cart;
		if (bTap && Subsystem->GetGame().OwnsMovable(Movable) && Subsystem->GetGame().GetHeld() != Movable && It->ContainsPoint(Point))
		{
			OutSource = Movable == ELotMovable::Suitcase ? ELotSource::Suitcase : ELotSource::Cart;
			return true;
		}
	}
	return false;
}

FText ADIAMovable::GetPrompt(const ADIACharacter* By) const
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->GetGame().OwnsMovable(Kind))
	{
		return FText::GetEmpty();
	}
	const FLotGame& Game = Subsystem->GetGame();
	const bool bHeld = Game.GetHeld() == Kind;
	if (!bHeld && (Game.GetHeld() != ELotMovable::Count || Game.World().bPlayerInCar))
	{
		// The hands are full, or the player sits in the car: it cannot be taken.
		return FText::GetEmpty();
	}
	// Rebuilt only when the state flips; the lookup is not free every frame.
	const int32 Key = bHeld ? 1 : 0;
	if (Key != PromptKey)
	{
		PromptKey = Key;
		CachedPrompt = DIAFormat::MovablePrompt(Kind, bHeld);
	}
	return CachedPrompt;
}

bool ADIAMovable::HasRoomAt(FVector2D Where, double YawDeg, const ADIACharacter* By) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// The footprint box, lifted a little off the ground: whatever else blocks it (wall, mall, plant, another movable) is in the way.
	const FVector Extent = Collision->GetUnscaledBoxExtent();
	const FQuat Turn(FRotator(0.0, YawDeg, 0.0));
	FVector Centre = FVector(Where.X, Where.Y, 0.0) + Turn.RotateVector(Collision->GetRelativeLocation());
	Centre.Z = Extent.Z + DIAMovable::ClearanceLiftCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MovableRoom), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(By);
	return !World->OverlapBlockingTestByChannel(Centre, Turn, ECC_WorldStatic, FCollisionShape::MakeBox(Extent), Params);
}

void ADIAMovable::ShowNoRoomToast(const ADIACharacter* By) const
{
	const APlayerController* Controller = By ? Cast<APlayerController>(By->GetController()) : nullptr;
	if (ADIAHUD* Hud = Controller ? Cast<ADIAHUD>(Controller->GetHUD()) : nullptr)
	{
		Hud->ShowToast(DIAFormat::MovableNoRoomToast());
	}
}

bool ADIAMovable::CanBePutDownHere(const ADIACharacter* By) const
{
	// It is set down exactly where it is now, upright.
	return HasRoomAt(FVector2D(GetActorLocation().X, GetActorLocation().Y), FRotator::NormalizeAxis(GetActorRotation().Yaw), By ? By : GetPlayer());
}

bool ADIAMovable::TryRelease(ADIACharacter* By)
{
	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !IsHeld())
	{
		return false;
	}
	if (!CanBePutDownHere(By))
	{
		ShowNoRoomToast(By);
		return false;
	}
	const FVector2D Where(GetActorLocation().X, GetActorLocation().Y);
	const double YawDeg = FRotator::NormalizeAxis(GetActorRotation().Yaw);
	Subsystem->GetGame().Release(Where, YawDeg);
	PlaceStanding(Where, YawDeg);
	Holder.Reset();
	Subsystem->Dispatch(FLotEvents());
	return true;
}

void ADIAMovable::Interact(ADIACharacter* By)
{
	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!By || !Subsystem)
	{
		return;
	}
	if (IsHeld())
	{
		TryRelease(By);
		return;
	}
	if (Subsystem->GetGame().Grab(Kind))
	{
		Holder = By;
		SyncFromCore();
		Subsystem->Dispatch(FLotEvents());
	}
}

bool ADIAMovable::NextToolPath(const ADIACharacter* Player, float DeltaSeconds, double SpeedCmS, FVector2D& OutFrom, FVector2D& OutTo)
{
	if (!Player || !Player->IsUsing())
	{
		ToolPath.Reset();
		return false;
	}
	return ToolPath.Advance(GetMouthPoint(), DeltaSeconds, SpeedCmS, OutFrom, OutTo);
}

void ADIAMovable::WorkTheStrip(UDIAWorldSubsystem& Subsystem, const ADIACharacter* Player, float DeltaSeconds)
{
	const bool bCart = Kind == ELotMovable::Cart;
	FVector2D From;
	FVector2D To;
	if (NextToolPath(Player, DeltaSeconds, bCart ? LotBalance::CartLaySpeedCmS : LotBalance::FinisherSpeedCmS(), From, To))
	{
		FLotGame& Game = Subsystem.GetGame();
		Subsystem.Dispatch(bCart ? Game.LayCartStrip(From, To) : Game.FinisherPass(From, To));
	}
}

void ADIAMovable::UpdateLevel()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || (Kind != ELotMovable::Cart && Kind != ELotMovable::Suitcase))
	{
		return;
	}
	const FLotGame& Game = Subsystem->GetGame();
	const double Litres = Game.GetMovableLitres(Kind);
	if (Litres == ShownLitres)
	{
		return;
	}
	ShownLitres = Litres;

	const double Capacity = Game.GetMovableCapacity(Kind);
	if (Capacity <= 0.0 || Litres < DIAMovable::LevelMinLitres)
	{
		LevelMesh->SetVisibility(false);
		return;
	}
	const double Fill = FMath::Clamp(Litres / Capacity, 0.0, 1.0);
	if (Kind == ELotMovable::Cart)
	{
		// The level rises linearly with the content; the box is as wide as the tub is at that height.
		const double Z = FMath::Lerp(TubFloorZCm, TubRimZCm - LevelInsetCm, Fill);
		const double Across = (Z - TubFloorZCm) / (TubRimZCm - TubFloorZCm);
		const FVector2D Inner = FMath::Lerp(TubFloorInnerCm, TubRimInnerCm, Across) - FVector2D(2.0 * LevelInsetCm);
		LevelMesh->SetRelativeScale3D(FVector(Inner.X / CubeShapeCm, Inner.Y / CubeShapeCm, DIACharacter::LiquidThicknessCm / CubeShapeCm));
		LevelMesh->SetRelativeLocation(FVector(TubCentreXCm, 0.0, Z));
	}
	else
	{
		// The column in the sight strip grows from its bottom.
		const double Height = SightHeightCm * Fill;
		LevelMesh->SetRelativeScale3D(FVector(SightDepthCm / CubeShapeCm, SightWidthCm / CubeShapeCm, Height / CubeShapeCm));
		LevelMesh->SetRelativeLocation(FVector(SightXYCm.X, SightXYCm.Y, SightBottomZCm + Height * 0.5));
	}
	LevelMesh->SetVisibility(true);
}

void ADIAMovable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->GetGame().OwnsMovable(Kind))
	{
		return;
	}
	FLotGame& Game = Subsystem->GetGame();
	SyncFromCore();
	const ADIACharacter* Player = GetPlayer();

	if (IsHeld())
	{
		if (Kind == ELotMovable::Cart || Kind == ELotMovable::Finisher)
		{
			WorkTheStrip(*Subsystem, Player, DeltaSeconds);
		}
	}
	else
	{
		ToolPath.Reset();
		const bool bTank = Kind == ELotMovable::Suitcase || Kind == ELotMovable::Cart;
		if (bTank && DIARefill::IsInZone(Game, FVector2D(GetActorLocation().X, GetActorLocation().Y)))
		{
			// The one place that fills a standing suitcase or cart.
			Game.FillMovable(Kind, DeltaSeconds);
		}
		if (Kind == ELotMovable::Suitcase && Player && Player->IsFlapHeld())
		{
			const FVector PlayerLocation = Player->GetActorLocation();
			if (GetDistanceToFootprint(FVector2D(PlayerLocation.X, PlayerLocation.Y)) <= LotBalance::InteractionRangeCm)
			{
				const FVector Location = GetActorLocation();
				Subsystem->Dispatch(Game.DoseSuitcase(GetMouthPoint(), FVector2D(Location.X, Location.Y), DeltaSeconds));
			}
		}
	}
	UpdateLevel();
}
