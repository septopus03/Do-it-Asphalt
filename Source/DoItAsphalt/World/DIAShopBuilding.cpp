// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAShopBuilding.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "LotLayout.h"
#include "Materials/MaterialInterface.h"
#include "UI/DIAFormat.h"
#include "World/DIAEntryBoard.h"

const TCHAR* DIAShopBuilding::ModelName(ELotShop Shop)
{
	switch (Shop)
	{
	case ELotShop::Tools: return TEXT("SM_ToolShop");
	case ELotShop::Robots: return TEXT("SM_PaverDepot");
	case ELotShop::Garage: return TEXT("SM_Garage");
	default: return TEXT("");
	}
}

FVector DIAShopBuilding::SignCentreCm(ELotShop Shop)
{
	switch (Shop)
	{
	case ELotShop::Tools: return FVector(116.0, -130.0, 312.0);
	case ELotShop::Robots: return FVector(174.0, -60.0, 298.0);
	case ELotShop::Garage: return FVector(557.0, 0.0, 456.0);
	default: return FVector::ZeroVector;
	}
}

FVector2D DIAShopBuilding::SignAreaCm(ELotShop Shop)
{
	switch (Shop)
	{
	case ELotShop::Tools: return FVector2D(240.0, 50.0);
	case ELotShop::Robots: return FVector2D(240.0, 45.0);
	case ELotShop::Garage: return FVector2D(300.0, 46.0);
	default: return FVector2D::ZeroVector;
	}
}

FTransform DIAShopBuilding::MakeTransform(ELotShop Shop)
{
	const FVector2D Where = LotLayout::ShopPos(Shop);
	return FTransform(FRotator(0.0, LotLayout::ShopYawDeg(Shop), 0.0), FVector(Where.X, Where.Y, 0.0));
}

ADIAShopBuilding::ADIAShopBuilding()
{
	PrimaryActorTick.bCanEverTick = false;

	// The model's own origin (docs/03-3d-modely.md, 4.5) is the actor's; +X of the model is the front with the sign.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	SetRootComponent(BodyMesh);
	// The simple collision of the model (the boxes made at import) blocks the player and the car; the camera boom is never pushed in.
	BodyMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BodyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(BodyMesh);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	if (UMaterialInterface* TextMaterial = DIAAssets::TextMaterial())
	{
		Text->SetTextMaterial(TextMaterial);
	}
	Text->SetCastShadow(false);
}

void ADIAShopBuilding::BeginPlay()
{
	Super::BeginPlay();

	// The real model stands on the ground at its origin; the placeholder box is lifted by half its height (its origin is not the centre,
	// so it only roughly marks the place).
	const FVector Size = Kind == ELotShop::Tools ? DIAModelSizes::ToolShopCm : Kind == ELotShop::Robots ? DIAModelSizes::PaverDepotCm : DIAModelSizes::GarageCm;
	DIAAssets::SetModelOrFallback(BodyMesh, DIAShopBuilding::ModelName(Kind), DIAAssets::EFallback::Cube, Size, FVector(0.0, 0.0, Size.Z * 0.5), TEXT("DIA_Concrete"));

	// An empirical workaround, root cause not established: a freshly loaded model had no collision shapes at its first use in a session
	// (the body is built before the physics meshes exist), so they are created here. Guarded by the tests
	// DoItAsphalt.Game.Shops.GarageDoorOpen and DoItAsphalt.Game.Shops.PointsFree (each is meant to be run alone, as a cold editor).
	if (UStaticMesh* Loaded = BodyMesh->GetStaticMesh())
	{
		if (UBodySetup* Setup = Loaded->GetBodySetup())
		{
			Setup->CreatePhysicsMeshes();
		}
	}

	const FText Name = DIAFormat::ShopName(Kind).ToUpper();
	Text->SetRelativeLocation(DIAShopBuilding::SignCentreCm(Kind) + FVector(DIAShopBuilding::SignTextStandOffCm, 0.0, 0.0));
	Text->SetWorldSize(static_cast<float>(DIABoard::FitTextSize(Name.ToString(), DIAShopBuilding::SignAreaCm(Kind))));
	Text->SetTextRenderColor(DIABoard::SignTextColor);
	Text->SetText(Name);
}
