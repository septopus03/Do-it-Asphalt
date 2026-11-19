// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIAPlantActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UDIAWorldSubsystem;

/** Plant levels from which the addons are built (levels are 0-based like AsphaltBalance::PlantRateLPerMin). */
namespace DIAPlant
{
	inline constexpr int32 AddonALevel = 5;
	inline constexpr int32 AddonBLevel = 10;
	/** Where the addons stand, relative to the plant's origin (the centre of its footprint). */
	inline const FVector AddonALocationCm(-150.0, 280.0, 0.0);
	inline const FVector AddonBLocationCm(-150.0, -290.0, 0.0);
}

/**
 * The asphalt plant at AsphaltLayout::PlantPos (it faces +X) with a flat disc on the ground that marks the refill
 * zone. The plant blocks the player; the zone is only a marker, the refilling itself is done by the character.
 * The plant grows with its upgrade: the second silo (SM_Plant_AddonA) from plant level 5 and the lying tank
 * (SM_Plant_AddonB) from level 10; they are there from the start but hidden (and without collision) until then.
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
	UStaticMeshComponent* GetAddonA() const { return AddonA; }
	UStaticMeshComponent* GetAddonB() const { return AddonB; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** Shows the addons the plant level has earned; returns at once when nothing changed. */
	void RefreshAddons();
	void ShowAddon(UStaticMeshComponent* Addon, bool bShow);

	/** Blocking collision of the plant body (the real model has none of its own). */
	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlantMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ZoneMarker;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AddonA;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AddonB;

	FDelegateHandle StateHandle;
};
