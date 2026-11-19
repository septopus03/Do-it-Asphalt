// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Pawn.h"
#include "World/DIAInteractable.h"
#include "DIAPaverCar.generated.h"

class ADIACharacter;
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** Handling of the paver truck (its speed, tank, bar width and bar position are the core's: AsphaltBalance::Car*). */
namespace DIAPaverCar
{
	/** Full steering turns the truck this fast at full speed; slower (or reversing) it turns in proportion to the speed. */
	inline constexpr double TurnRateDegPerSec = 90.0;
	/** Reversing is this much of the forward speed. */
	inline constexpr double ReverseSpeedFactor = 0.5;
	/** How fast the speed follows the throttle (cm/s per second); the truck is a heavy vehicle, so it does not jump to speed. */
	inline constexpr double AccelerationCmS2 = 1500.0;

	/** The collision box rests this far above the ground, so that driving does not scrape along it. */
	inline constexpr double GroundLiftCm = 1.0;
	/** The actor's origin is the centre of the collision box: half the box height plus the lift. */
	inline double OriginHeightCm()
	{
		return DIAModelSizes::PaverCarBodyCm.Z * 0.5 + GroundLiftCm;
	}

	/** The camera boom ("over the shoulder"), its target above the box centre, and the pitch the camera starts at. */
	inline constexpr float CameraBoomLengthCm = 900.f;
	inline constexpr float CameraTargetLiftCm = 100.f;
	inline constexpr double CameraPitchDeg = -20.0;

	/** The character gets out this far to the side of the truck (the door is on the left, -Y; the other side is the second choice). */
	inline constexpr double ExitSideCm = 200.0;
	/** The free-space test for the exit lifts the character's capsule this high, so that the ground does not count as blocking. */
	inline constexpr double ExitClearanceLiftCm = 2.0;

	/** The truck fills its tank when its centre is within AsphaltLayout::RefillRadiusCm plus this of a refill point. */
	inline constexpr double RefillExtraRadiusCm = 200.0;

	/** A bar that moved less than this (cm per frame) lays nothing: the pour stamp needs a direction. */
	inline constexpr double MinBarMoveCm = 0.01;
	/** How far the actor may be from the core's position / heading before it is moved there; the core's numbers are the truth. */
	inline constexpr double SyncToleranceCm = 0.5;
	inline constexpr double SyncToleranceDeg = 0.5;
}

/**
 * The paver truck: a large vehicle with a 500 l tank and a 3 m spreader bar at the rear. E beside it gets the player in
 * (the controller possesses it and the character rides along, hidden); E inside gets the player out at its left. While
 * driven the controller sets throttle and steering (WASD) and pouring (left button), exactly like the character's input.
 * The core owns the state (FAsphaltWorldState::CarPos / CarYawDeg / bPlayerInCar, the tank): the truck writes its position
 * into the core every frame, like the character writes the player's, and follows the core when that changes some other
 * way (a loaded save, at once when the whole game is replaced). The game mode spawns it once the special is owned. Movement is kinematic with a sweep: walls stop it,
 * it slides along them.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPaverCar : public APawn, public IDIAInteractable
{
	GENERATED_BODY()

public:
	ADIAPaverCar();

	// IDIAInteractable
	/** "E – Get in" beside the truck, "E – Get out" while driving; empty without the special. */
	virtual FText GetPrompt(const ADIACharacter* By) const override;
	/** Gets the character in (the controller possesses the truck); nothing if there is no character with a controller or someone drives already. */
	virtual void Interact(ADIACharacter* By) override;

	/** Stops the truck and puts the driver beside it at the left (or the right, if the left is blocked); the controller gets the character back. */
	void ExitCar();
	bool HasDriver() const { return Driver != nullptr; }

	/** -1..1; positive = forward. The value stays until it is changed (the controller zeroes it when the keys are released). */
	void SetThrottle(float Value);
	/** -1..1; positive = to the right. */
	void SetSteer(float Value);
	void SetPouring(bool bPour);
	bool IsPouring() const { return bPouring; }
	float GetThrottle() const { return Throttle; }
	float GetSteer() const { return Steer; }
	/** Input routing, called by the player controller. */
	void DoLook(float Yaw, float Pitch);

	/** Where the spreader bar lays asphalt: actor XY - forward * AsphaltBalance::CarBarBehindCm. */
	FVector2D GetBarPoint() const;
	/** Current speed along the heading in cm/s; negative while reversing. */
	double GetSpeed() const { return Speed; }

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UStaticMeshComponent* GetBarMesh() const { return BarMesh; }
	UBoxComponent* GetCollision() const { return Collision; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Makes the actor match the core: where it stands and (after a loaded save) who is in it. Runs on every state change, so it must return early when nothing differs. */
	void SyncFromCore();
	void EnterBy(ADIACharacter* Character);
	/** The truck stops, the driver (if there is one) is put at Spot facing YawDeg and the controller gets the character back. */
	void ReleaseDriver(const FVector& Spot, double YawDeg);
	/** A game was loaded or started: the truck takes the core's position at once (before its own tick writes the old one into the core), and the driver gets in or out as the core says. */
	void HandleGameReset();
	/** Moves by the speed and the steering for this frame, sweeping against the world. */
	void Drive(float DeltaSeconds);
	/** Lays asphalt behind the bar while pouring. */
	void Pour(UDIAWorldSubsystem& Subsystem);
	/** Where the driver can stand beside the truck; the left side if nothing is free. */
	FVector FindExitSpot(const ADIACharacter* Character) const;

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;
	/** Only the placeholder has a separate bar; the real model has it built in. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BarMesh;
	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY()
	TObjectPtr<UCameraComponent> FollowCamera;

	/** The character that drives; hidden and carried along while it does. */
	UPROPERTY()
	TObjectPtr<ADIACharacter> Driver;
	/** What the driver's movement was before it got in, to be restored when it gets out. */
	TEnumAsByte<EMovementMode> DriverMovementMode = MOVE_Walking;

	float Throttle = 0.f;
	float Steer = 0.f;
	bool bPouring = false;
	double Speed = 0.0;
	FVector2D PreviousBar = FVector2D::ZeroVector;

	/** The prompt text of the state it was built for; the prompt is queried every frame. */
	mutable bool bPromptInCar = false;
	mutable FText CachedPrompt;

	FDIAStateSubscription StateSub;
};
