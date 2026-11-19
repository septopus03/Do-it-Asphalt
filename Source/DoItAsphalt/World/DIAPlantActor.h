// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIAPlantActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * The asphalt plant at AsphaltLayout::PlantPos (it faces +X) with a flat disc on the ground that marks the refill
 * zone. The plant blocks the player; the zone is only a marker, the refilling itself is done by the character.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPlantActor : public AActor
{
	GENERATED_BODY()

public:
	ADIAPlantActor();

	UStaticMeshComponent* GetPlantMesh() const { return PlantMesh; }
	UStaticMeshComponent* GetZoneMarker() const { return ZoneMarker; }
	UBoxComponent* GetCollision() const { return Collision; }

protected:
	virtual void BeginPlay() override;

private:
	/** Blocking collision of the plant body (the real model has none of its own). */
	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlantMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ZoneMarker;
};
