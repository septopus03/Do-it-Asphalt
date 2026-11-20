// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "DIAExitGate.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

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
	/**
	 * The white sign board of the frame (2.0 x 0.5 m, centred at Z = 275 cm) stands proud of both faces of the beam, which are at X = +-11 cm;
	 * the word is written just in front of each face (the +X face looks outwards, the -X face towards the lot).
	 */
	inline constexpr double SignFaceXCm = 11.0;
	inline constexpr double SignTextOffsetCm = 0.6;
	inline constexpr double SignCentreZCm = 275.0;
	/** Height of the letters (the board is 50 cm high); the word fills about half of the board's width. */
	inline constexpr double SignTextSizeCm = 44.0;
	/** The gate has two columns (one each side of the passage) and its sign is written on two faces (towards the lot, towards the road). */
	inline constexpr int32 ColumnCount = 2;
	inline constexpr int32 SignFaceCount = 2;
	/** Yaw of the text on the face towards the lot (-X); the one towards the road (+X) needs none. */
	inline constexpr double LotFaceYawDeg = 180.0;
	/**
	 * Colour of the letters (shown through the unlit M_DIA_Text material). A deep red: the letters keep a little of the surrounding glare,
	 * so a lighter red turns salmon against the white board.
	 */
	inline const FColor SignColor(150, 0, 0);
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
	/** The word EXIT on the board: [0] on the face towards the lot (-X), [1] on the face towards the road (+X). */
	UTextRenderComponent* GetSignText(int32 Side) const { return SignTexts.IsValidIndex(Side) ? SignTexts[Side].Get() : nullptr; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Follows the core: starts the rise when the exit opened, closes the arm again if it is no longer open. Runs on every state change, so it returns early when nothing differs. */
	void SyncFromCore();
	/** A game was loaded or started: the arm is up at once if the exit is open in it (no rise), down otherwise. */
	void HandleGameReset();
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
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> SignTexts;

	/** Seconds the arm has been rising. */
	double OpenElapsed = 0.0;
	double OpenFraction = 0.0;

	FDIAStateSubscription StateSub;
};
