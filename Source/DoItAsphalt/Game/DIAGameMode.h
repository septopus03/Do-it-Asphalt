// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DIAGameMode.generated.h"

/** Builds the scene when play starts: the ground, the plant, the six old road ends with their barriers and the five kiosks; the player is an ADIACharacter. Later tasks add the gate and the fence. */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADIAGameMode();

	virtual void StartPlay() override;

	/** The player starts at AsphaltLayout::PlayerStart facing the lot (+X), whatever PlayerStart actors the level has. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
