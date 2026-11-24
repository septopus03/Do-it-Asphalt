// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Pawn.h"
#include "LotBalance.h"
#include "World/DIAInteractable.h"
#include "World/DIAToolPath.h"
#include "DIAPaverCar.generated.h"

class ADIACharacter;
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** Handling and presentation of the paver car; its speed, turning and bar position are the core's (LotBalance::Car*). */
namespace DIAPaverCar
{
	/** The collision box rests this far above the ground, so that driving does not scrape along it. */
	inline constexpr double GroundLiftCm = 1.0;
	/** Number of tiers (the car level is 0..NumLevels-1); a level outside it counts as the nearest one. */
	inline constexpr int32 NumLevels = LotBalance::NumCarLevels;
	inline int32 ClampLevel(int32 Level)
	{
		return FMath::Clamp(Level, 0, NumLevels - 1);
	}
	/** The model of a tier: /Game/Models/<name>. */
	inline const TCHAR* MeshName(int32 Level)
	{
		static const TCHAR* const Names[NumLevels] = {TEXT("SM_PaverCar"), TEXT("SM_PaverCar_T2"), TEXT("SM_PaverCar_T3"), TEXT("SM_PaverCar_T4")};
		return Names[ClampLevel(Level)];
	}
	/** The actor's origin is the centre of the collision box: half the box height of the tier plus the lift. */
	inline double OriginHeightCm(int32 Level = 0)
	{
		return DIAModelSizes::PaverCarBodyCm[ClampLevel(Level)].Z * 0.5 + GroundLiftCm;
	}

	/**
	 * Third person: the camera hangs on a boom behind the car (ArmLengthCm long, from a pivot PivotHeightCm above the ground over the car's
	 * centre). The mouse swings the boom round the car (yaw against the car's heading, pitch up and down); the boom is pushed in by whatever
	 * stands between it and the car.
	 */
	inline constexpr double ArmLengthCm = 900.0;
	inline constexpr double PivotHeightCm = 170.0;
	/** The two numbers above are those of tier 1; a bigger truck gets a boom longer in proportion to its length and a pivot higher in proportion to its height, so that it is framed alike. */
	inline double ArmLengthFor(int32 Level)
	{
		return ArmLengthCm * DIAModelSizes::PaverCarCm[ClampLevel(Level)].X / DIAModelSizes::PaverCarCm[0].X;
	}
	inline double PivotHeightFor(int32 Level)
	{
		return PivotHeightCm * DIAModelSizes::PaverCarCm[ClampLevel(Level)].Z / DIAModelSizes::PaverCarCm[0].Z;
	}
	inline constexpr double DefaultPitchDeg = -16.0;
	inline constexpr double LookPitchMinDeg = -70.0;
	inline constexpr double LookPitchMaxDeg = 10.0;
	/** How fast the boom swings back behind the car while the mouse is left alone, in degrees per second (0 = never). */
	inline constexpr double RecentreDegPerSec = 0.0;

	/** The player gets out this far beyond the side of the body (the door is on the left, -Y; the other side is the second choice). */
	inline constexpr double ExitMarginCm = 90.0;
	/** Distance of the exit spot from the car's centre: the half width of the tier's body plus the margin (200 cm for tier 1). */
	inline double ExitSideCm(int32 Level = 0)
	{
		return DIAModelSizes::PaverCarBodyCm[ClampLevel(Level)].Y * 0.5 + ExitMarginCm;
	}
	/** The free-space test for the exit lifts the capsule this high, so that the ground does not count as blocking. */
	inline constexpr double ExitClearanceLiftCm = 2.0;

	/** How far the actor may be from the core's position / heading before it is moved there; the core's numbers are the truth. */
	inline constexpr double SyncToleranceCm = 0.5;
	inline constexpr double SyncToleranceDeg = 0.5;
}

/**
 * The paver car: a kinematic vehicle with a spreader bar at the rear that lays asphalt from its own tank, which it fills at a
 * vehicle station (FLotGame::LoadCar: the fixed one or the portable one) by standing in the station's radius. E beside it with free hands gets the player in (the controller possesses the car and
 * the character rides along, hidden); E inside gets the player out at a free spot beside it. The camera is in third person, on a boom behind the car
 * that the mouse swings round it. While driven the controller sets throttle and steering (WASD) and
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
	/** Input routing, called by the player controller: swings the camera boom round the car. */
	void DoLook(float Yaw, float Pitch);

	/** Where the spreader bar lays asphalt: actor XY - forward * LotBalance::CarBarBehindCm[level]. */
	FVector2D GetBarPoint() const;
	/** Writes the pose the car has now into the core (and the player's with it while someone drives): a save asks for it so that it does not depend on the frame. */
	void WritePoseToCore();
	/** Current speed along the heading in cm/s; negative while reversing. */
	double GetSpeed() const { return Speed; }

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	/** The car level the actor shows now (0..3; the core's level, the actor follows it in SyncFromCore). */
	int32 GetShownLevel() const { return ShownLevel; }
	UBoxComponent* GetCollision() const { return Collision; }
	UCameraComponent* GetCamera() const { return Camera; }
	USpringArmComponent* GetBoom() const { return Boom; }

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
	/** The core's car level, at least 0 (the actor exists as the plain car before it is bought). */
	int32 CoreLevel() const;
	/** Shows the tier the core says the car has: mesh, collision box, height above the ground, boom. bCheckRoom: a bigger box that overlaps something is moved to the car's spawn. */
	void UpdateTier(bool bCheckRoom);
	/** True if the collision box of the shown tier at the given place blocks nothing. */
	bool HasRoomAt(const FVector& Location, double YawDeg) const;
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
	TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

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
	/** The heading the car had when the bar's last point was taken; with it a turn is laid as one swept strip. */
	FVector2D PreviousBarHeading = FVector2D(1.0, 0.0);
	bool bHasPreviousHeading = false;
	/** The boom's turn against the car's heading. */
	double LookYawDeg = 0.0;
	double LookPitchDeg = DIAPaverCar::DefaultPitchDeg;
	/** The control rotation this actor set last: what the controller added to it since is the player's turning of the head. */
	FRotator LastControlRotation = FRotator::ZeroRotator;
	/** The pose this actor wrote into the core last; a core pose that differs from it was written by someone else. */
	FVector2D WrittenPos = FVector2D::ZeroVector;
	double WrittenYawDeg = 0.0;
	bool bHasWritten = false;
	/** The tier whose mesh and box the actor has now; -1 until BeginPlay has set the first one. */
	int32 ShownLevel = -1;

	/** The prompt text of the state it was built for; the prompt is queried every frame. */
	mutable int32 PromptKey = -1;
	mutable FText CachedPrompt;

	FDIAStateSubscription StateSub;
};
