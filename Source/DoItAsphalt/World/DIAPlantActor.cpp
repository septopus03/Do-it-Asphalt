// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAPlantActor.h"
#include "AsphaltLayout.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Materials/MaterialInterface.h"

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

	PlantMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlantMesh"));
	PlantMesh->SetupAttachment(Root);
	PlantMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ZoneMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMarker"));
	ZoneMarker->SetupAttachment(Root);
	ZoneMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMarker->SetCastShadow(false);
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
}
