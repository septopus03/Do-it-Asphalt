// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "DIACharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/** Visual and handling constants of the character (game numbers live in LotBalance / LotLayout). */
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
}

/**
 * The player: third-person movement and camera as in the engine template, and sprinting once the Sprint unlock is
 * reached. The state of the game lives in the world subsystem; the character only reads it and reports its position.
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

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** Walk / sprint speed follows the current state. */
	void UpdateMovementSettings();
	void OnStateChanged();
	/** A game was loaded or started: stands where the core says, without sprinting. */
	void HandleGameReset();

	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> FollowCamera;

	bool bSprinting = false;

	FDIAStateSubscription StateSub;
};
