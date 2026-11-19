// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DIAGameMode.generated.h"

/** Builds the scene when play starts. Later tasks add the plant, kiosks, gate, fence and the character. */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void StartPlay() override;
};
