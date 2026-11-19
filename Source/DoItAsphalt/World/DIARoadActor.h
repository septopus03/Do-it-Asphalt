// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIARoadActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * The old finished road behind the outer end of a road (SM_RoadStub, from AsphaltLayout::RoadEnd outwards to the fence)
 * and, on the five roads that are not the exit, the barrier (SM_Barrier) that closes it at AsphaltLayout::GatePos. The actor
 * stands at RoadEnd turned along the road. The barrier blocks the player but not the camera. Set Road before BeginPlay
 * (spawn deferred) or call Build afterwards.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIARoadActor : public AActor
{
	GENERATED_BODY()

public:
	ADIARoadActor();

	/** Road index 0..5; the road with AsphaltBalance::ExitRoad has no barrier (ADIAExitGate stands there). */
	int32 Road = 0;

	/** Position and yaw of the actor for a road: RoadEnd turned along RoadDirection. */
	static FTransform MakeTransform(int32 Road);

	/** Places the meshes and the barrier for the current Road. Called by BeginPlay. */
	void Build();

	bool HasBarrier() const { return bHasBarrier; }
	UStaticMeshComponent* GetStubMesh() const { return StubMesh; }
	UStaticMeshComponent* GetBarrierMesh() const { return BarrierMesh; }
	UBoxComponent* GetBarrierCollision() const { return BarrierCollision; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> StubMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BarrierMesh;
	UPROPERTY()
	TObjectPtr<UBoxComponent> BarrierCollision;

	bool bHasBarrier = false;
};
