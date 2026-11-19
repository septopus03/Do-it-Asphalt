// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/DIAInput.h"
#include "DIAPlayerController.generated.h"

struct FInputActionValue;

/**
 * Creates the input assets, adds the mapping context to the local player and routes the actions to the character.
 * Shop (Tab) toggles the shop panel of the HUD and Pause (Esc) closes it; Interact and the rest of Pause are bound but do
 * nothing yet (later tasks fill them in).
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	const FDIAInput& GetInput() const { return Input; }

	/**
	 * Called by the HUD when the shop opens or closes: the mouse cursor and the mouse go to the panel (game and UI input,
	 * the camera stops following the mouse, pouring stops and cannot start), and back to the game when it closes.
	 * The game itself keeps running.
	 */
	void SetShopInput(bool bOpen);
	bool IsShopInput() const { return bShopInput; }

protected:
	virtual void InitInputSystem() override;
	virtual void SetupInputComponent() override;

private:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnPourStarted();
	void OnPourCompleted();
	void OnSprintStarted();
	void OnSprintCompleted();
	void OnInteract();
	void OnShop();
	void OnPause();

	class ADIACharacter* GetDIACharacter() const;

	UPROPERTY()
	FDIAInput Input;

	bool bShopInput = false;
};
