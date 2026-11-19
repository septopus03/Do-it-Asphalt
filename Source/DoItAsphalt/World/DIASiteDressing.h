// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltDressing.h"
#include "GameFramework/Actor.h"
#include "DIASiteDressing.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * The scenery between the lot and the fence: the cones, barrels, dirt piles and floodlights that AsphaltDressing::Build lays out
 * (SM_Cone, SM_Barrel, SM_DirtPile, SM_Lamp), standing on the ground. Barrels and dirt piles are solid (like props), a floodlight is
 * solid at its pole, cones are not. Built once in BeginPlay; the dressing has no state.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIASiteDressing : public AActor
{
	GENERATED_BODY()

public:
	ADIASiteDressing();

	int32 GetNumItems() const { return Items.Num(); }
	const FDressingItem& GetItem(int32 Index) const { return Items[Index]; }
	/** The visible model of an item (same index as the layout). */
	UStaticMeshComponent* GetMesh(int32 Index) const { return Meshes.IsValidIndex(Index) ? Meshes[Index].Get() : nullptr; }
	/** The solid pole of a floodlight; null for the other kinds. */
	UBoxComponent* GetPole(int32 Index) const { return Poles.IsValidIndex(Index) ? Poles[Index].Get() : nullptr; }

protected:
	virtual void BeginPlay() override;

private:
	TArray<FDressingItem> Items;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Poles;
};
