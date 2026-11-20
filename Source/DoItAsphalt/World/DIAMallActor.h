// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIAMallActor.generated.h"

class UStaticMeshComponent;

/**
 * The shopping centre (SM_Mall) in the corner cut-out of the car park, at LotLayout::MallCentre without any turn: its footprint
 * is exactly the cut-out, so it only touches the car park. It blocks the player (the model's single box collision covers the
 * whole footprint) but not the camera boom.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAMallActor : public AActor
{
	GENERATED_BODY()

public:
	ADIAMallActor();

	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;
};
