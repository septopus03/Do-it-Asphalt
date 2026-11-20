// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "DIACharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** Visual and handling constants of the character (game numbers live in LotBalance / LotLayout). */
namespace DIACharacter
{
	inline constexpr float CapsuleRadiusCm = 34.f;
	inline constexpr float CapsuleHalfHeightCm = 90.f;
	/** Length of the camera boom of the external view (dia.Camera), as in the third-person template. */
	inline constexpr float CameraBoomLengthCm = 400.f;
	/** Height of the eyes above the ground. */
	inline constexpr float EyeHeightCm = 160.f;
	/** Field of view of the normal view and of the work view, the tilt of the work view and the time the change takes. */
	inline constexpr float NormalFovDeg = 90.f;
	inline constexpr float WorkViewFovDeg = 60.f;
	inline constexpr float WorkViewTiltDeg = 15.f;
	inline constexpr float WorkViewBlendSeconds = 0.2f;
	/** Where the hand pivot hangs in front of the camera (camera axes: X forward, Y right, Z up): lower right of the picture. */
	inline const FVector HandPivotOffsetCm(58.0, 25.0, -27.0);
	/** Thickness of the disc that shows the level of the asphalt in the bowl and the bucket. */
	inline constexpr float LiquidThicknessCm = 0.4f;
	/** The Mannequin mesh faces +Y and its origin is at the feet: rotate -90 yaw and lower by the capsule half height. */
	inline constexpr float MeshYawDeg = -90.f;
}

/**
 * The player: sees the world from the eyes and holds the item the game says is in the hand (bowl with spoon, scoop,
 * bucket, trowel) on a pivot in front of the camera. Holding Use scoops at the trough, fills the bucket at the outlet
 * and pours or trowels on the lot; every result goes to the world subsystem's Dispatch. Sprinting only has an effect
 * once the Sprint unlock is reached. The state of the game lives in the world subsystem; the character reports its position.
 * The Mannequin body stays for the shadow and the external view (dia.Camera).
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIACharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADIACharacter();

	/** The character the first local player controls; null while there is none. The one lookup the world actors share. */
	static ADIACharacter* GetLocal(const UObject* WorldContext);

	/** Sprinting only has an effect once the Sprint unlock is reached. */
	void SetSprinting(bool bSprint);
	bool IsSprinting() const { return bSprinting; }

	/** Input routing, called by the player controller. */
	void DoMove(float Right, float Forward);
	void DoLook(float Yaw, float Pitch);

	/** Use (left button, held): scoops, fills, pours, dumps or trowels with the item in hand. */
	void SetUsing(bool bUse);
	bool IsUsing() const { return bUsing; }
	/** Work view (right button, held): the field of view narrows and the view tilts down; it eases back on release. */
	void SetWorkView(bool bWork);
	bool IsWorkView() const { return bWorkView; }
	/** Takes the item into the hand (the core decides: owned, swap delay, busy hands). */
	bool SelectHand(ELotHand Hand);
	/** Takes the next item the player may take. */
	void NextHand();

	/** The point on the ground the player aims at: the view ray meets the ground, pulled in to LotBalance::ReachCm from the player. */
	FVector2D GetAimPoint() const;

	/** Switches between the view from the eyes and the external view behind the player (dia.Camera). */
	void SetExternalView(bool bExternal, float DistanceCm = DIACharacter::CameraBoomLengthCm);
	bool IsExternalView() const { return bExternalView; }

	UCameraComponent* GetCamera() const { return FirstPersonCamera; }
	USceneComponent* GetHandPivot() const { return HandPivot; }
	/** Main mesh of the hand item (bowl, scoop, bucket, trowel), the spoon that goes with the bowl, and the asphalt disc in a vessel. */
	UStaticMeshComponent* GetHandMesh() const { return HandMesh; }
	UStaticMeshComponent* GetSpoonMesh() const { return SpoonMesh; }
	UStaticMeshComponent* GetLiquidMesh() const { return LiquidMesh; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** Walk / sprint speed follows the current state. */
	void UpdateMovementSettings();
	void OnStateChanged();
	/** Gives the hand pivot the meshes of the item the core says is in the hand. */
	void RefreshHandMesh();
	/** Sets height and radius of the asphalt disc from the content of the vessel in hand. */
	void UpdateLiquid();
	/** Moves the field of view and the tilt towards the work view (or back) and keeps the tool in the same place of the picture. */
	void UpdateWorkView(float DeltaSeconds);
	/** The direction the player looks in: the controller's, or the camera's own without a controller, with the work view tilt. */
	FQuat GetViewQuat() const;
	/** Smoothed work view amount, 0..1. */
	float GetWorkViewBlend() const;
	/** One tick of the held Use button with the item in hand. */
	void UseHand(UDIAWorldSubsystem& Subsystem, float DeltaSeconds);
	/** A game was loaded or started: stands where the core says, without sprinting. */
	void HandleGameReset();

	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** The camera of the external view (inactive in the normal view). */
	UPROPERTY()
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY()
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY()
	TObjectPtr<USceneComponent> HandPivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SpoonMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LiquidMesh;

	bool bSprinting = false;
	bool bUsing = false;
	bool bWorkView = false;
	bool bExternalView = false;
	/** 0 = normal view, 1 = work view. */
	float WorkViewAlpha = 0.f;
	/** The bucket was already emptied during this press of Use. */
	bool bBucketDumped = false;
	/** The trowel has a contact point from the press on; it follows the aim at most TrowelSpeedCmS. */
	bool bTrowelDown = false;
	FVector2D TrowelContact = FVector2D::ZeroVector;
	/** The item the pivot shows now; Count before the first refresh. */
	ELotHand ShownHand = ELotHand::Count;
	/** Content of the vessel the disc shows now, to skip the work when nothing changed. */
	double ShownLitres = -1.0;

	FDIAStateSubscription StateSub;
};
