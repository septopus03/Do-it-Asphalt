// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "GameFramework/GameModeBase.h"
#include "DIAGameMode.generated.h"

class ADIAAutoPaver;
class ADIAPaverCar;
class ADIAPortableStation;

/**
 * Builds the scene when play starts: the ground, the plant, the six old road ends with their barriers and the five kiosks;
 * the player is an ADIACharacter. The portable station and the automated paver appear as soon as the player owns them
 * (at the start if already owned, or when they are bought). Later tasks add the gate and the fence.
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

	/** The player starts at AsphaltLayout::PlayerStart facing the lot (+X), whatever PlayerStart actors the level has. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	void OnSpecialBought(EAsphaltSpecial Special);

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
	FDelegateHandle SpecialHandle;
};
