// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAMallActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"

ADIAMallActor::ADIAMallActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// The origin is the centre of the footprint on the ground (like SM_Mall).
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The camera boom is never pushed in by the mall.
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ADIAMallActor::BeginPlay()
{
	Super::BeginPlay();
	// The real model stands on the ground at its origin; the placeholder box is lifted by half its height.
	DIAAssets::SetModelOrFallback(Mesh, TEXT("SM_Mall"), DIAAssets::EFallback::Cube, DIAModelSizes::MallCm,
		FVector(0.0, 0.0, DIAModelSizes::MallCm.Z * 0.5), TEXT("DIA_Concrete"));
}
