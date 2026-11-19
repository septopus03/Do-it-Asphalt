// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAPlantActor.h"
#include "AsphaltLayout.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "AsphaltGame.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** The addons are solid for the player once built, but the camera boom must never be pushed in by them. */
	void SetupAddon(UStaticMeshComponent* Addon, USceneComponent* Root, const FVector& Location)
	{
		Addon->SetupAttachment(Root);
		Addon->SetRelativeLocation(Location);
		Addon->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Addon->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Addon->SetVisibility(false);
		Addon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	/** Sets the mesh of an addon: the real model stands on the ground at the origin; the placeholder shape is lifted by half its height. */
	void LoadAddon(UStaticMeshComponent* Addon, const FVector& Location, FName ModelName, DIAAssets::EFallback Shape, const FVector& SizeCm)
	{
		FVector Scale = FVector::OneVector;
		UStaticMesh* Mesh = DIAAssets::LoadMeshOrFallback(ModelName, Shape, SizeCm, Scale);
		Addon->SetStaticMesh(Mesh);
		Addon->SetRelativeScale3D(Scale);
		if (Mesh && Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
		{
			Addon->SetRelativeLocation(Location + FVector(0.0, 0.0, SizeCm.Z * 0.5));
			if (UMaterialInterface* Material = DIAAssets::Palette(TEXT("DIA_Concrete")))
			{
				Addon->SetMaterial(0, Material);
			}
		}
	}
}

ADIAPlantActor::ADIAPlantActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// The origin of the actor is the centre of the plant's footprint on the ground (like SM_Plant).
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Root);
	Collision->SetBoxExtent(DIAModelSizes::PlantCm * 0.5);
	Collision->SetRelativeLocation(FVector(0.0, 0.0, DIAModelSizes::PlantCm.Z * 0.5));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The camera boom must not be crushed against the plant (it stands right behind the player's start); the body still
	// blocks pawns and everything else.
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	PlantMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlantMesh"));
	PlantMesh->SetupAttachment(Root);
	PlantMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ZoneMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMarker"));
	ZoneMarker->SetupAttachment(Root);
	ZoneMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMarker->SetCastShadow(false);

	AddonA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AddonA"));
	SetupAddon(AddonA, Root, DIAPlant::AddonALocationCm);
	AddonB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AddonB"));
	SetupAddon(AddonB, Root, DIAPlant::AddonBLocationCm);
}

UDIAWorldSubsystem* ADIAPlantActor::GetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
}

void ADIAPlantActor::ShowAddon(UStaticMeshComponent* Addon, bool bShow)
{
	if (Addon->IsVisible() == bShow)
	{
		return;
	}
	Addon->SetVisibility(bShow);
	Addon->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ADIAPlantActor::RefreshAddons()
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const int32 Level = Subsystem->GetGame().GetLevel(EAsphaltUpgrade::Plant);
	ShowAddon(AddonA, Level >= DIAPlant::AddonALevel);
	ShowAddon(AddonB, Level >= DIAPlant::AddonBLevel);
}

void ADIAPlantActor::EndPlay(const EEndPlayReason::Type Reason)
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

void ADIAPlantActor::BeginPlay()
{
	Super::BeginPlay();

	// The plant: the real model stands on the ground at the origin; the placeholder box has to be lifted by half its height.
	FVector Scale = FVector::OneVector;
	UStaticMesh* Plant = DIAAssets::LoadMeshOrFallback(TEXT("SM_Plant"), DIAAssets::EFallback::Cube, DIAModelSizes::PlantCm, Scale);
	PlantMesh->SetStaticMesh(Plant);
	PlantMesh->SetRelativeScale3D(Scale);
	if (Plant && Plant->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
	{
		PlantMesh->SetRelativeLocation(FVector(0.0, 0.0, DIAModelSizes::PlantCm.Z * 0.5));
		if (UMaterialInterface* Material = DIAAssets::Palette(TEXT("DIA_Concrete")))
		{
			PlantMesh->SetMaterial(0, Material);
		}
	}

	// The refill zone: a flat disc of the zone's radius, centred on AsphaltLayout::RefillCenter, just above the ground.
	const double DiameterCm = AsphaltLayout::RefillRadiusCm * 2.0;
	const FVector2D Offset = AsphaltLayout::RefillCenter - AsphaltLayout::PlantPos;
	FVector MarkerScale = FVector::OneVector;
	UStaticMesh* Disc = DIAAssets::LoadMeshOrFallback(TEXT("SM_RefillZone"), DIAAssets::EFallback::Cylinder,
		FVector(DiameterCm, DiameterCm, DIAModelSizes::RefillMarkerThicknessCm), MarkerScale);
	ZoneMarker->SetStaticMesh(Disc);
	ZoneMarker->SetRelativeScale3D(MarkerScale);
	// The 1 cm lift keeps the disc clear of the ground plane, which lies at Z = 0.
	ZoneMarker->SetRelativeLocation(FVector(Offset.X, Offset.Y, DIAModelSizes::RefillMarkerThicknessCm));
	if (UMaterialInterface* Material = DIAAssets::Palette(TEXT("DIA_Yellow")))
	{
		ZoneMarker->SetMaterial(0, Material);
	}

	// The addons: the real models stand on the ground at their spot.
	LoadAddon(AddonA, DIAPlant::AddonALocationCm, TEXT("SM_Plant_AddonA"), DIAAssets::EFallback::Cylinder, DIAModelSizes::PlantAddonACm);
	LoadAddon(AddonB, DIAPlant::AddonBLocationCm, TEXT("SM_Plant_AddonB"), DIAAssets::EFallback::Cube, DIAModelSizes::PlantAddonBCm);
	RefreshAddons();
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		// Fires every tick; RefreshAddons returns at once unless a plant level was crossed.
		StateHandle = Subsystem->OnStateChanged.AddUObject(this, &ADIAPlantActor::RefreshAddons);
	}
}
