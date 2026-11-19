// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Game/DIAGameMode.h"
#include "GameFramework/WorldSettings.h"
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
}
