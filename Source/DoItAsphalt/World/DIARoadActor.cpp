// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIARoadActor.h"
#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "HexMap.h"

ADIARoadActor::ADIARoadActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StubMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StubMesh"));
	StubMesh->SetupAttachment(Root);
	StubMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StubMesh->SetCastShadow(false);

	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetupAttachment(Root);
	BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BarrierCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrierCollision"));
	BarrierCollision->SetupAttachment(Root);
	BarrierCollision->SetBoxExtent(DIAModelSizes::BarrierCm * 0.5);
	BarrierCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The camera boom is never pushed in by a barrier.
	BarrierCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

FTransform ADIARoadActor::MakeTransform(int32 Road)
{
	const FVector2D End = AsphaltLayout::RoadEnd(Road);
	const FVector2D Dir = FHexMap::RoadDirection(Road);
	return FTransform(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0), FVector(End.X, End.Y, 0.0));
}

void ADIARoadActor::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ADIARoadActor::Build()
{
	// The stub: the real model runs along +X from the origin (the centre of the near edge); the placeholder box is moved the same way.
	DIAAssets::SetModelOrFallback(StubMesh, TEXT("SM_RoadStub"), DIAAssets::EFallback::Cube, DIAModelSizes::RoadStubCm,
		FVector(DIAModelSizes::RoadStubCm.X * 0.5, 0.0, DIAModelSizes::RoadStubCm.Z * 0.5), TEXT("DIA_Asphalt"));

	bHasBarrier = Road != AsphaltBalance::ExitRoad;
	BarrierMesh->SetVisibility(bHasBarrier);
	BarrierCollision->SetCollisionEnabled(bHasBarrier ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (!bHasBarrier)
	{
		return;
	}
	// The barrier stands in the fence line, at GatePos; its origin is the centre of its footprint on the ground. The actor's
	// local X runs along the road, so the offset from RoadEnd to the gate is a distance along it.
	const FVector2D Offset = AsphaltLayout::GatePos(Road) - AsphaltLayout::RoadEnd(Road);
	const FVector Local(FVector2D::DotProduct(Offset, FHexMap::RoadDirection(Road)), 0.0, 0.0);
	BarrierMesh->SetRelativeLocation(Local);
	BarrierCollision->SetRelativeLocation(Local + FVector(0.0, 0.0, DIAModelSizes::BarrierCm.Z * 0.5));
	DIAAssets::SetModelOrFallback(BarrierMesh, TEXT("SM_Barrier"), DIAAssets::EFallback::Cube, DIAModelSizes::BarrierCm,
		FVector(0.0, 0.0, DIAModelSizes::BarrierCm.Z * 0.5), TEXT("DIA_Red"));
}
