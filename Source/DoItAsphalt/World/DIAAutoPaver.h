// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "DIAAutoPaver.generated.h"

class UStaticMeshComponent;

/** Presentation constants of the auto paver's body (its speed and rates are the core's: LotBalance). */
namespace DIAAutoPaver
{
	/** The model is shown this much larger than its envelope in DIAModelSizes (the placeholder box too). */
	inline constexpr double ModelScale = 1.5;
	/** How fast the body turns towards its direction of travel, in degrees per second. */
	inline constexpr double TurnRateDegPerSec = 360.0;
	/** A move shorter than this (cm per frame) is standing still: the body keeps its heading instead of twitching. */
	inline constexpr double MinMoveCm = 0.01;
	/** The shown position closes in on the core's one at this rate (1/s): the core moves the machine from cell to cell and the body glides instead of jumping. */
	inline constexpr double FollowRatePerSec = 12.0;
	/** Closer than this to the core's position the body is simply put there. */
	inline constexpr double SnapCm = 0.05;
}

/**
 * The sector auto paver: a low tracked robot that drives to its sector and paves it cell by cell. The core owns everything
 * (position, sector, litres); this actor shows GetAutoPaverPos(Index) and turns the body towards where it is heading. It has no
 * collision, so it neither blocks the player nor aiming or pouring. The game mode keeps exactly one per robot the game owns.
 *
 * Because the core moves the machine from cell to cell, the shown position follows the core's with a short smoothing
 * (DIAAutoPaver::FollowRatePerSec); a new or a loaded or restarted game puts it on the core's position at once. The heading
 * follows the shown movement and stays while the body stands still.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAAutoPaver : public AActor
{
	GENERATED_BODY()

public:
	ADIAAutoPaver();

	/** Which of the core's robots this actor shows; set before BeginPlay (the game mode spawns it deferred). */
	int32 Index = 0;

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Puts the body on the core's position (the heading stays). */
	void SnapToCore();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	FDelegateHandle ResetHandle;
};
