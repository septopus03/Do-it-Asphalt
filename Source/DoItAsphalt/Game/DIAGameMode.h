// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DIAGameMode.generated.h"

/**
 * Builds the scene when play starts: the ground (the L-shaped car park) and the plant with its trough behind the bottom
 * edge; the player is an ADIACharacter that can walk around. Tools, shop, machines, mall and gate follow in later tasks.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADIAGameMode();

	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The player starts at LotLayout::PlayerStart, whatever PlayerStart actors the level has. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	/** ... facing the trough (LotLayout::PlayerStartYawDeg), so that the camera behind the player looks at the car park edge. */
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
};
