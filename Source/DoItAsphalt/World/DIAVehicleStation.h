// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIAVehicleStation.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Where the parts of SM_VehicleStation stand, relative to its origin (the centre of the pad); the lane between the posts is 4 m wide. */
namespace DIAVehicleStation
{
	/** Half the clear width between the two posts. */
	inline constexpr double PostYCm = 220.0;
	inline const FVector PostHalfCm(20.0, 20.0, 230.0);
	/** The control cabinet beside the pad. */
	inline const FVector CabinetCm(-300.0, -270.0, 70.0);
	inline const FVector CabinetHalfCm(40.0, 25.0, 70.0);
}

/**
 * The fixed vehicle station (LotLayout::VehicleStationPos): a drive-through gantry whose hopper fills the paver car's tank and the
 * auto paver's. The car drives in along X under the nozzle; what happens there is the core's (FLotGame::LoadCar and the paver's
 * ToStation / Loading phases). The posts and the cabinet block, the lane between them does not.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAVehicleStation : public AActor
{
	GENERATED_BODY()

public:
	ADIAVehicleStation();

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Blockers;
};
