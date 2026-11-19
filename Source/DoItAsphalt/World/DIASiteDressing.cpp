// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIASiteDressing.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "HexMap.h"

namespace
{
	/** How one kind of prop is shown and whether it blocks the player. */
	struct FDressingLook
	{
		FName Model;
		DIAAssets::EFallback Shape;
		FVector SizeCm;
		/** Where the placeholder shape stands relative to the model's origin, so that it occupies the place of the real model. */
		FVector FallbackOffsetCm;
		FName Palette;
		/** The model's own simple collision blocks pawns. */
		bool bSolid;
	};

	const FDressingLook& DressingLook(EDressingKind Kind)
	{
		static const FDressingLook Looks[] = {
			{TEXT("SM_Cone"), DIAAssets::EFallback::Cone, DIAModelSizes::ConeCm, FVector(0.0, 0.0, DIAModelSizes::ConeCm.Z * 0.5), TEXT("DIA_Orange"), false},
			{TEXT("SM_Barrel"), DIAAssets::EFallback::Cylinder, DIAModelSizes::BarrelCm, FVector(0.0, 0.0, DIAModelSizes::BarrelCm.Z * 0.5), TEXT("DIA_Red"), true},
			// The floodlight's pole is a separate box; the model itself (a pole with a long arm) would block too wide a strip.
			{TEXT("SM_Lamp"), DIAAssets::EFallback::Cube, DIAModelSizes::LampCm, DIAModelSizes::LampCentreCm, TEXT("DIA_Yellow"), false},
			{TEXT("SM_DirtPile"), DIAAssets::EFallback::Sphere, DIAModelSizes::DirtPileCm, FVector(0.0, 0.0, DIAModelSizes::DirtPileCm.Z * 0.5), TEXT("DIA_Dirt"), true},
		};
		return Looks[static_cast<int32>(Kind)];
	}
}

ADIASiteDressing::ADIASiteDressing()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADIASiteDressing::BeginPlay()
{
	Super::BeginPlay();

	const FHexMap Map;
	Items = AsphaltDressing::Build(Map);
	for (const FDressingItem& Item : Items)
	{
		const FDressingLook& Look = DressingLook(Item.Kind);
		// Every model has its origin on the ground, so the item stands at Z = 0.
		const FVector Where(Item.Pos.X, Item.Pos.Y, 0.0);
		const FRotator Turn(0.0, Item.YawDeg, 0.0);

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetRelativeLocationAndRotation(Where, Turn);
		if (Look.bSolid)
		{
			Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			// The camera boom is never pushed in by a prop.
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		else
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Mesh->RegisterComponent();
		// The offset is added to the relative location, which is in the actor's frame, so the turn is applied to it here.
		DIAAssets::SetModelOrFallback(Mesh, Look.Model, Look.Shape, Look.SizeCm, Turn.RotateVector(Look.FallbackOffsetCm), Look.Palette);
		Meshes.Add(Mesh);

		UBoxComponent* Pole = nullptr;
		if (Item.Kind == EDressingKind::Lamp)
		{
			Pole = NewObject<UBoxComponent>(this);
			Pole->SetupAttachment(GetRootComponent());
			Pole->SetBoxExtent(DIAModelSizes::LampPoleCm * 0.5);
			Pole->SetRelativeLocationAndRotation(Where + FVector(0.0, 0.0, DIAModelSizes::LampPoleCm.Z * 0.5), Turn);
			Pole->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Pole->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Pole->SetHiddenInGame(true);
			Pole->RegisterComponent();
		}
		Poles.Add(Pole);
	}
}
