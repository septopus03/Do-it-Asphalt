// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAVehicleStation.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"

ADIAVehicleStation::ADIAVehicleStation()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Two posts and the cabinet block; boxes are the same for the real model and the placeholder.
	const FVector Centres[3] = {
		FVector(0.0, DIAVehicleStation::PostYCm, DIAVehicleStation::PostHalfCm.Z),
		FVector(0.0, -DIAVehicleStation::PostYCm, DIAVehicleStation::PostHalfCm.Z),
		DIAVehicleStation::CabinetCm};
	const FVector Halves[3] = {DIAVehicleStation::PostHalfCm, DIAVehicleStation::PostHalfCm, DIAVehicleStation::CabinetHalfCm};
	for (int32 I = 0; I < 3; ++I)
	{
		UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Blocker%d"), I));
		Box->SetupAttachment(Root);
		Box->SetRelativeLocation(Centres[I]);
		Box->SetBoxExtent(Halves[I]);
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Blockers.Add(Box);
	}
}

void ADIAVehicleStation::BeginPlay()
{
	Super::BeginPlay();
	// The real model stands on the ground at the pad's centre; the placeholder is a box of its envelope.
	const FVector Size = DIAModelSizes::VehicleStationCm;
	DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_VehicleStation"), DIAAssets::EFallback::Cube, Size, FVector(0.0, 0.0, Size.Z * 0.5), TEXT("DIA_Steel"));
}
