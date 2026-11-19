// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "GameFramework/GameModeBase.h"
#include "DIAGameMode.generated.h"

class ADIAAutoPaver;
class ADIAExitGate;
class ADIAPaverCar;
class ADIAPortableStation;
class ADIASiteDressing;
class ADIASiteFence;

/**
 * Builds the scene when play starts: the ground, the plant, the six old road ends with their barriers and the five kiosks;
 * the player is an ADIACharacter. The portable station, the automated paver and the truck appear as soon as the player owns them
 * (at the start if already owned, when they are bought, or when a saved game that owns them is loaded) and go when a game without them starts. The exit gate, the fence around the site and the scenery inside it stand from the start.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADIAGameMode();

	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Spawns the portable station at the core's StationPos if the player owns it and there is no live station yet; otherwise does nothing. */
	void EnsureStation();

	/** Spawns the automated paver at the core's GetAutoPaverPos() if the player owns it and there is no live one yet; otherwise does nothing. */
	void EnsureAutoPaver();

	/** Spawns the paver truck at the core's CarPos if the player owns it and there is no live one yet; otherwise does nothing. */
	void EnsureCar();

	/** Spawns the exit gate at the end of road 5 if there is no live one yet; otherwise does nothing. */
	void EnsureExitGate();

	/** Spawns the fence and the invisible wall around the site if there is no live one yet; otherwise does nothing. */
	void EnsureSiteFence();

	/** Spawns the scenery (cones, barrels, dirt piles, floodlights) between the lot and the fence if there is none yet; otherwise does nothing. */
	void EnsureDressing();

	/** The player starts at AsphaltLayout::PlayerStart, whatever PlayerStart actors the level has. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	/** ... facing the plant (AsphaltLayout::PlayerStartYawDeg), so that the camera behind the player is not inside it. */
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;

private:
	void OnSpecialBought(EAsphaltSpecial Special);
	/** A game was loaded or started: the specials of that game stand, the others are taken away. */
	void OnGameReset();

	/** Takes the special's actor out of the world (when it is not owned any more) and forgets it. */
	template <typename ActorType>
	void RemoveSpecialActor(TWeakObjectPtr<ActorType>& Slot);

	/**
	 * The one way a special's actor comes into the world: nothing happens while the slot still holds a live actor or the
	 * special is not owned; otherwise the actor is spawned where the core says (WhereInCore reads the position from the
	 * core) at the given height. The actor then keeps itself in line with the core.
	 */
	template <typename ActorType>
	void EnsureSpecialActor(TWeakObjectPtr<ActorType>& Slot, EAsphaltSpecial Special, TFunctionRef<FVector2D(const FAsphaltGame&)> WhereInCore, double HeightCm = 0.0);

	/** Null (or stale) until the special is owned. */
	TWeakObjectPtr<ADIAPortableStation> Station;
	/** Null (or stale) until the special is owned. */
	TWeakObjectPtr<ADIAAutoPaver> AutoPaver;
	/** Null (or stale) until the special is owned. */
	TWeakObjectPtr<ADIAPaverCar> Car;
	TWeakObjectPtr<ADIAExitGate> ExitGate;
	TWeakObjectPtr<ADIASiteFence> SiteFence;
	TWeakObjectPtr<ADIASiteDressing> Dressing;
	FDelegateHandle SpecialHandle;
	FDelegateHandle ResetHandle;
};
