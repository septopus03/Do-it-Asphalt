// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "DIAExitGate.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Numbers of the exit gate; the sizes of its models are in DIAModelSizes, its place in AsphaltLayout. */
namespace DIAExitGate
{
	/** The barrier arm rises by this angle in this time once the exit is open. */
	inline constexpr double OpenAngleDeg = 80.0;
	inline constexpr double OpenSeconds = 1.5;
	/** Height of the arm's axis above the ground; it must lie within the capsule of the character and the box of the truck. */
	inline constexpr double ArmHeightCm = 100.0;
	/** The columns stand on both sides of the 340 cm passage (20 cm square columns on 40 x 20 cm footings; the box covers the footing). */
	inline const FVector PostCm(40.0, 20.0, 300.0);
	/** The win trigger (AsphaltLayout::ExitTrigger is its centre on the ground): a sphere this big, centred at mid-height of the body. */
	inline constexpr double TriggerRadiusCm = 150.0;
	inline constexpr double TriggerHeightCm = 100.0;
}

/**
 * The exit gate on road 5 (AsphaltBalance::ExitRoad), in the fence line at AsphaltLayout::GatePos, across the road: the frame
 * (SM_ExitGate_Frame, its two columns are solid) and the barrier arm (SM_ExitGate_Barrier) that turns about its hinge at the
 * left column. While the exit is closed the arm blocks pawns; when the core says the exit is open (IsExitOpen, also when the
 * game was loaded that way, then at once) the arm rises by 80 degrees in 1.5 s and stops blocking. The trigger sphere
 * beyond the gate wins the game (UDIAWorldSubsystem::Win) when the character or the truck touches it. The gate mirrors the core
 * and never keeps a state of its own beyond the animation.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAExitGate : public AActor
{
	GENERATED_BODY()

public:
	ADIAExitGate();

	/** Where the gate stands: AsphaltLayout::GatePos of the exit road, turned so that its X axis runs along the road. */
	static FTransform MakeTransform();

	/** The arm is up and does not block any more. */
	bool IsOpen() const { return OpenFraction >= 1.0; }
	/** 0 with the arm down, 1 with the arm up (it follows the time while rising). */
	double GetOpenFraction() const { return OpenFraction; }

	UStaticMeshComponent* GetFrameMesh() const { return FrameMesh; }
	UStaticMeshComponent* GetArmMesh() const { return ArmMesh; }
	UBoxComponent* GetArmCollision() const { return ArmCollision; }
	USphereComponent* GetTrigger() const { return Trigger; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Follows the core: starts the rise when the exit opened, closes the arm again if it is no longer open. Runs on every state change, so it returns early when nothing differs. */
	void SyncFromCore();
	/** Turns the arm to the open fraction and lets it block only while it is not up. */
	void ApplyArm();
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FrameMesh;
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> PostCollisions;
	/** The hinge: the arm and its collision turn with it. */
	UPROPERTY()
	TObjectPtr<USceneComponent> ArmPivot;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ArmMesh;
	UPROPERTY()
	TObjectPtr<UBoxComponent> ArmCollision;
	UPROPERTY()
	TObjectPtr<USphereComponent> Trigger;

	/** Seconds the arm has been rising. */
	double OpenElapsed = 0.0;
	double OpenFraction = 0.0;

	FDIAStateSubscription StateSub;
};
