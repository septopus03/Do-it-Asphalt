// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LotBalance.h"
#include "DIAGameMode.generated.h"

/**
 * Builds the scene when play starts: the ground (the L-shaped car park), the plant with its trough behind the bottom
 * edge, and the site around the car park (EnsureSite); the player is an ADIACharacter that can walk around, but only on the site.
 * Tools, shop, machines and the gate follow in later tasks.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADIAGameMode();

	virtual void StartPlay() override;

	/** Builds the surroundings of the car park once: the mall, the kerb and fence with the invisible walls, the exit gate and the six entry boards. Does nothing if they stand already. */
	void EnsureSite();
	/**
	 * Makes the actors match the core for one movable: exactly one ADIAMovable of the kind if the game owns it (spawned at its
	 * position in the core), none if it does not. Runs at the start, after a purchase and after a loaded or restarted game.
	 */
	void EnsureMovable(ELotMovable Kind);
	/** Makes the actors match the core for the auto paver robots: exactly one ADIAAutoPaver per robot the game owns (Index 0, 1, ...), none for the others. */
	void EnsureAutoPavers();
	/** Makes the actors match the core for the paver car: exactly one ADIAPaverCar while the game owns it (at the core's pose), none otherwise. */
	void EnsureCar();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The player starts at LotLayout::PlayerStart, whatever PlayerStart actors the level has. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	/** ... facing the trough (LotLayout::PlayerStartYawDeg), so that the camera behind the player looks at the car park edge. */
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;

private:
	/** EnsureMovable for every kind, EnsureAutoPavers and EnsureCar: the start of play, a purchase and a loaded or restarted game all end here. */
	void EnsureAllMovables();
	void HandleItemBought(ELotItem Item) { EnsureAllMovables(); }

	FDelegateHandle ItemBoughtHandle;
	FDelegateHandle GameResetHandle;
};
