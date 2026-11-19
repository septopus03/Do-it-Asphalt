// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "GameFramework/GameModeBase.h"
#include "DIAGameMode.generated.h"

class ADIAAutoPaver;
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

	/** The player starts at AsphaltLayout::PlayerStart facing the lot (+X), whatever PlayerStart actors the level has. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	void OnSpecialBought(EAsphaltSpecial Special);

	/** Null (or stale) until the special is owned. */
	TWeakObjectPtr<ADIAPortableStation> Station;
	/** Null (or stale) until the special is owned. */
	TWeakObjectPtr<ADIAAutoPaver> AutoPaver;
	FDelegateHandle SpecialHandle;
};
