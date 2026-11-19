// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Game/DIAGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Player/DIACharacter.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace DIATest
{
	/** Creates a game world that uses ADIAGameMode regardless of the project config and starts play in it. */
	inline bool StartGameWorld(FTestWorldWrapper& Wrapper, FAutomationTestBase& Test)
	{
		if (!Wrapper.CreateTestWorld(EWorldType::Game))
		{
			Wrapper.ForwardErrorMessages(&Test);
			return false;
		}
		AWorldSettings* Settings = Wrapper.GetTestWorld()->GetWorldSettings();
		if (!Test.TestNotNull(TEXT("world settings"), Settings))
		{
			return false;
		}
		Settings->DefaultGameMode = ADIAGameMode::StaticClass();
		if (!Wrapper.BeginPlayInTestWorld())
		{
			Wrapper.ForwardErrorMessages(&Test);
			return false;
		}
		return true;
	}

	/**
	 * Spawns a character standing on the ground at the given spot. Walking is switched off: without a controller nothing
	 * drives the movement component, and MOVE_None also keeps gravity from touching the tests, which move it by hand.
	 */
	inline ADIACharacter* SpawnCharacter(UWorld* World, FVector2D Where)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADIACharacter* Character = World->SpawnActor<ADIACharacter>(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm), FRotator::ZeroRotator, Params);
		if (Character)
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
		}
		return Character;
	}
}
