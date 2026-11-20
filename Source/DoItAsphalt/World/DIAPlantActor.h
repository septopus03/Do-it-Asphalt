// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIAPlantActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UDIAWorldSubsystem;

/** Where the addons are built is LotBalance::PlantAddonALevel / PlantAddonBLevel (plant levels 0..5). */
namespace DIAPlant
{
	/** Where the addons stand, relative to the plant's origin (the centre of its footprint). */
	inline const FVector AddonALocationCm(-150.0, 280.0, 0.0);
	inline const FVector AddonBLocationCm(-150.0, -290.0, 0.0);
}

/**
 * The asphalt plant at LotLayout::PlantPos, turned by LotLayout::PlantYawDeg so that its outlet faces the car park, and the
 * trough (SM_Trough) at LotLayout::TroughPos under the outlet. Both block the player, but not the camera boom. The plant grows
 * with its level: the second silo (SM_Plant_AddonA) from LotBalance::PlantAddonALevel and the lying tank (SM_Plant_AddonB) from
 * LotBalance::PlantAddonBLevel; they are there from the start but hidden (and without collision) until then.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPlantActor : public AActor
{
	GENERATED_BODY()

public:
	ADIAPlantActor();

	UStaticMeshComponent* GetPlantMesh() const { return PlantMesh; }
	UStaticMeshComponent* GetTroughMesh() const { return TroughMesh; }
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

	/** The trough under the outlet; placed in the world at TroughPos whatever the plant's own yaw is. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TroughMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AddonA;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AddonB;

	FDelegateHandle StateHandle;
};
