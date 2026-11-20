// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DIAInteractable.generated.h"

class ADIACharacter;

UINTERFACE(MinimalAPI, NotBlueprintable)
class UDIAInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something the player can use with E: the kiosk, the portable station or the paver car. The player controller
 * searches the registered interactables every frame, so both functions have to be cheap (no allocation, no work that is
 * not needed when nothing changed).
 */
class DOITASPHALT_API IDIAInteractable
{
	GENERATED_BODY()

public:
	/** The prompt for the HUD; empty text = nothing to do here right now. */
	virtual FText GetPrompt(const ADIACharacter* By) const = 0;
	virtual void Interact(ADIACharacter* By) = 0;
	/** True while the player holds this thing (a suitcase, cart or machine in the hands): E then always addresses it, in range or not. */
	virtual bool IsHeldByPlayer() const { return false; }
};
