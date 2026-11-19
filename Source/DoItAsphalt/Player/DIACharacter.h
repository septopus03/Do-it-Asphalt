// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DIACharacter.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDIAWorldSubsystem;

/** Visual and handling constants of the character (game numbers live in AsphaltBalance / AsphaltLayout). */
namespace DIACharacter
{
	inline constexpr float CapsuleRadiusCm = 34.f;
	inline constexpr float CapsuleHalfHeightCm = 90.f;
	/** Length of the camera boom, as in the third-person template. */
	inline constexpr float CameraBoomLengthCm = 400.f;
	/** Turn speed towards the movement direction (deg/s), as in the third-person template. */
	inline constexpr float TurnRateDegS = 500.f;
	/** The Mannequin mesh faces +Y and its origin is at the feet: rotate -90 yaw and lower by the capsule half height. */
	inline constexpr float MeshYawDeg = -90.f;

	/**
	 * The axes of the Mannequin's spine and head bones in the reference pose: X runs up the body, Y points to the front
	 * and Z to the right (measured from the skeleton). This rotation turns them into the character's axes (X forward,
	 * Y right, Z up), so everything attached to a pivot below can be described like a model in the character's frame.
	 */
	inline const FRotator BoneToCharacter = FRotationMatrix::MakeFromXY(FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0)).Rotator();

	/**
	 * Offsets of the pivots from their bones, in the bone's axes (X up, Y front, Z right). Backpack: on the back plane
	 * of the chest; helmet: where the lower edge of the helmet meets the head.
	 */
	inline const FVector BackpackBoneOffsetCm(10.0, -13.0, 0.0);
	inline const FVector HelmetBoneOffsetCm(2.0, 2.0, 0.0);
}

/**
 * The player: third-person movement and camera as in the engine template, pouring (the nozzle sweeps coverage while
 * the left button is held), refilling in the plant's zone and the visible equipment (spreader, backpack, helmet).
 * The state of the game lives in the world subsystem; the character only reads it and acts on it.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIACharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADIACharacter();

	/** Starts or stops pouring; pouring slows the character down and turns it to face the camera direction. */
	void SetPouring(bool bPour);
	bool IsPouring() const { return bPouring; }

	/** Sprinting only has an effect once the Sprint special is owned, and never while pouring. */
	void SetSprinting(bool bSprint);
	bool IsSprinting() const { return bSprinting; }

	/** Where the nozzle of the spreader is: actor XY plus forward * NozzleAheadCm. */
	FVector2D GetNozzlePoint() const;

	/** Re-reads the levels and specials and swaps the meshes whose tier changed (nothing happens if none did). */
	void RefreshEquipment();

	/** Input routing, called by the player controller. */
	void DoMove(float Right, float Forward);
	void DoLook(float Yaw, float Pitch);

	/** The spreader (the left one with TwoHands); the second one only exists with the TwoHands special. */
	UStaticMeshComponent* GetSpreaderMesh() const { return SpreaderMesh; }
	UStaticMeshComponent* GetSecondSpreaderMesh() const { return SecondSpreaderMesh; }
	UStaticMeshComponent* GetBackpackMesh() const { return BackpackMesh; }
	UStaticMeshComponent* GetHelmetMesh() const { return HelmetMesh; }
	USceneComponent* GetBackpackPivot() const { return BackpackPivot; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** Walk / pour / sprint speed and the facing mode follow the current state. */
	void UpdateMovementSettings();
	void OnStateChanged();
	void BuildHelmet();

	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Pivots keep the placement; the meshes below are swapped when the equipment changes. */
	UPROPERTY()
	TObjectPtr<USceneComponent> SpreaderPivot;
	UPROPERTY()
	TObjectPtr<USceneComponent> BackpackPivot;
	UPROPERTY()
	TObjectPtr<USceneComponent> HelmetPivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SpreaderMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SecondSpreaderMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BackpackMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HelmetMesh;

	bool bPouring = false;
	bool bSprinting = false;
	FVector2D PreviousNozzle = FVector2D::ZeroVector;

	/** What the meshes currently show; INDEX_NONE until built. */
	int32 ShownBackpackTier = INDEX_NONE;
	int32 ShownToolLevel = INDEX_NONE;
	bool bShownTwoHands = false;

	FDelegateHandle StateHandle;
};
