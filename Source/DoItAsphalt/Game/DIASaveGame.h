// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DIASaveGame.generated.h"

/** Numbers of the save slot and the autosave. */
namespace DIASave
{
	/** The slot of the real game; tests and screenshots use their own (UDIAWorldSubsystem::SetSlotNameOverride, -DIASlot=<name>). */
	inline constexpr const TCHAR* DefaultSlotName = TEXT("DoItAsphalt_Slot0");
	/** The one local user. */
	inline constexpr int32 UserIndex = 0;
	/** The game saves itself this often (game time, so never while it is paused). */
	inline constexpr double AutosaveIntervalSeconds = 30.0;
}

/** What is stored in the slot: the core's own serialisation (FAsphaltGame::SaveToBytes), which validates itself when it is read back. */
UCLASS()
class DOITASPHALT_API UDIASaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<uint8> Blob;
};
