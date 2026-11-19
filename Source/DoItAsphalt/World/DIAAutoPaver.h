// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "DIAAutoPaver.generated.h"

class UDIAWorldSubsystem;
class UStaticMeshComponent;

/** Handling of the automated paver's body (its speed and reach are the core's: AsphaltBalance::AutoPaverSpeedCmS and AutoPaverReachCm). */
namespace DIAAutoPaver
{
	/** How fast the robot turns towards its direction of travel, in degrees per second. */
	inline constexpr double TurnRateDegPerSec = 360.0;
	/** A move shorter than this (cm per frame) is standing still: the robot keeps its heading instead of twitching. */
	inline constexpr double MinMoveCm = 0.01;
}

/**
 * The automated paver: a low tracked robot that drives to the next unfinished cell and pours into it. The core owns
 * everything (position, target, litres); this actor only mirrors GetAutoPaverPos() and turns the body towards where
 * it is heading. It has no collision, so it neither blocks nor pushes the player. The game mode spawns it once the
 * special is owned.
 *
 * It follows the core on every state change, which the world subsystem broadcasts once per frame right after it has
 * stepped the core. Moving in its own Tick would show the position of the previous frame, because world subsystems
 * tick after the actors.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAAutoPaver : public AActor
{
	GENERATED_BODY()

public:
	ADIAAutoPaver();

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Moves to the core's position and turns towards the direction of travel. */
	void SyncFromCore();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	FDIAStateSubscription StateSub;
};
