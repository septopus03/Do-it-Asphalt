// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Pawn.h"
#include "World/DIAInteractable.h"
#include "World/DIAToolPath.h"
#include "DIAPaverCar.generated.h"

class ADIACharacter;
class UBoxComponent;
class UCameraComponent;
class UStaticMeshComponent;

/** Handling and presentation of the paver car; its speed, turning and bar position are the core's (LotBalance::Car*). */
namespace DIAPaverCar
{
	/** The collision box rests this far above the ground, so that driving does not scrape along it. */
	inline constexpr double GroundLiftCm = 1.0;
	/** The actor's origin is the centre of the collision box: half the box height plus the lift. */
	inline double OriginHeightCm()
	{
		return DIAModelSizes::PaverCarBodyCm.Z * 0.5 + GroundLiftCm;
	}

	/** The eyes in the cabin: this high above the ground and this far ahead of the car's centre. */
	inline constexpr double CabinEyeHeightCm = 190.0;
	inline constexpr double CabinEyeAheadCm = 120.0;
	/** The camera looks this far down at rest, so that the bonnet shows at the bottom of the frame. */
	inline constexpr double CabinPitchDeg = -8.0;
	/**
	 * The cabin view is the inside of the model, whose windscreen and side windows are opaque slabs of the glass colour: seen from the
	 * cabin they cover the sky. So the body is hidden from the car's own view and this flat yellow bonnet stands in for it: its far edge
	 * is a little below the frame's bottom edge at rest. Centre of the slab ahead of the car's centre, and the height of its top.
	 */
	inline constexpr double BonnetAheadCm = 190.0;
	inline constexpr double BonnetTopCm = 105.0;
	inline const FVector BonnetSizeCm = FVector(120.0, 200.0, 8.0);
	/** The mouse turns the head by this much at most to each side, up and down (the car does not turn with it). */
	inline constexpr double LookYawLimitDeg = 100.0;
	inline constexpr double LookPitchMinDeg = -60.0;
	inline constexpr double LookPitchMaxDeg = 45.0;

	/** The player gets out this far to the side of the car (the door is on the left, -Y; the other side is the second choice). */
	inline constexpr double ExitSideCm = 200.0;
	/** The free-space test for the exit lifts the capsule this high, so that the ground does not count as blocking. */
	inline constexpr double ExitClearanceLiftCm = 2.0;

	/** How far the actor may be from the core's position / heading before it is moved there; the core's numbers are the truth. */
	inline constexpr double SyncToleranceCm = 0.5;
	inline constexpr double SyncToleranceDeg = 0.5;
}

/**
 * The paver car: a kinematic vehicle with a spreader bar at the rear that lays finished asphalt straight from the plant stock
 * (there is no tank and nothing to refill). E beside it with free hands gets the player in (the controller possesses the car and
 * the character rides along, hidden); E inside gets the player out at a free spot beside it. The camera sits in the cabin, in
 * first person, with the bonnet at the bottom of the frame. While driven the controller sets throttle and steering (WASD) and
 * pouring (left button).
 *
 * The core owns the state (FLotWorldState::CarPos / CarYawDeg / bPlayerInCar): the car writes its pose into the core every
 * frame, and while it is driven also the player's, so the core sees the car as the player; it follows the core when someone else
 * changed it (a loaded game). While pouring the top speed is LotBalance::CarPaveSpeedCmS (the speed that lays the car's rate over
 * its bar width), otherwise LotBalance::CarSpeedCmS; every frame of pouring it calls FLotGame::PourFromCar for the path the bar
 * drew. The game mode keeps exactly one actor while the car is owned.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPaverCar : public APawn, public IDIAInteractable
{
	GENERATED_BODY()

public:
	ADIAPaverCar();

	// IDIAInteractable
	/** "E – Get in" beside the car with free hands, "E – Get out" while driving; empty while something is held or without the car. */
	virtual FText GetPrompt(const ADIACharacter* By) const override;
	/** Driving: gets the player out. Otherwise gets the character in; nothing if there is no character with a controller, someone drives already, or the hands are not free. */
	virtual void Interact(ADIACharacter* By) override;

	/** Stops the car and puts the driver at a free spot beside it; false (nothing changes, a toast says so) if there is none. */
	bool ExitCar();
	bool HasDriver() const { return Driver != nullptr; }

	/** -1..1; positive = forward. The value stays until it is changed (the controller zeroes it when the keys are released). */
	void SetThrottle(float Value);
	/** -1..1; positive = to the right. */
	void SetSteer(float Value);
	void SetPouring(bool bPour);
	bool IsPouring() const { return bPouring; }
	float GetThrottle() const { return Throttle; }
	float GetSteer() const { return Steer; }
	/** Input routing, called by the player controller: turns the head in the cabin. */
	void DoLook(float Yaw, float Pitch);

	/** Where the spreader bar lays asphalt: actor XY - forward * LotBalance::CarBarBehindCm. */
	FVector2D GetBarPoint() const;
	/** Writes the pose the car has now into the core (and the player's with it while someone drives): a save asks for it so that it does not depend on the frame. */
	void WritePoseToCore();
	/** Current speed along the heading in cm/s; negative while reversing. */
	double GetSpeed() const { return Speed; }

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	/** The wide bar (SM_PaverCar_WideBar), shown once the car owns it. */
	UStaticMeshComponent* GetWideBarMesh() const { return WideBarMesh; }
	/** What the cabin shows at the bottom of the frame instead of the body (only the owner's own view sees it). */
	UStaticMeshComponent* GetCabinBonnet() const { return CabinBonnet; }
	UBoxComponent* GetCollision() const { return Collision; }
	UCameraComponent* GetCabinCamera() const { return CabinCamera; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Makes the actor match the core: where it stands, who is in it and the bar it has. Runs on every state change, so it must return early when nothing differs. */
	void SyncFromCore();
	void EnterBy(ADIACharacter* Character);
	/** The car stops, the driver (if there is one) is put at Spot facing YawDeg and the controller gets the character back. */
	void ReleaseDriver(const FVector& Spot, double YawDeg);
	/** A game was loaded or started: the car takes the core's pose at once, the stroke starts afresh, and the driver gets in or out as the core says. */
	void HandleGameReset();
	/** Moves by the speed and the steering for this frame, sweeping against the world. */
	void Drive(float DeltaSeconds);
	/** Lays asphalt along the path the bar drew since the previous frame. */
	void Pour(UDIAWorldSubsystem& Subsystem, float DeltaSeconds);
	/** Where the driver can stand beside the car; false if neither side is free. */
	bool FindExitSpot(const ADIACharacter* Character, FVector& OutSpot) const;
	/** Shows the wide bar if the core says the car has it. */
	void UpdateBar();
	void UpdateCamera();
	/** The top speed now: the pouring speed of the bar while pouring, else the driving speed. */
	double TopSpeed() const;
	/** The pose is written into the core (and remembered, so that a pose somebody else wrote is told apart). */
	void WritePose(FLotWorldState& State);

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> WideBarMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> CabinBonnet;
	UPROPERTY()
	TObjectPtr<UCameraComponent> CabinCamera;

	/** The character that drives; hidden and carried along while it does. */
	UPROPERTY()
	TObjectPtr<ADIACharacter> Driver;
	/** What the driver's movement was before it got in, to be restored when it gets out. */
	TEnumAsByte<EMovementMode> DriverMovementMode = MOVE_Walking;

	float Throttle = 0.f;
	float Steer = 0.f;
	bool bPouring = false;
	double Speed = 0.0;
	FDIAToolPath ToolPath;
	/** The head's turn against the car's heading. */
	double LookYawDeg = 0.0;
	double LookPitchDeg = DIAPaverCar::CabinPitchDeg;
	/** The control rotation this actor set last: what the controller added to it since is the player's turning of the head. */
	FRotator LastControlRotation = FRotator::ZeroRotator;
	/** The pose this actor wrote into the core last; a core pose that differs from it was written by someone else. */
	FVector2D WrittenPos = FVector2D::ZeroVector;
	double WrittenYawDeg = 0.0;
	bool bHasWritten = false;
	bool bShowsWideBar = false;

	/** The prompt text of the state it was built for; the prompt is queried every frame. */
	mutable int32 PromptKey = -1;
	mutable FText CachedPrompt;

	FDIAStateSubscription StateSub;
};
