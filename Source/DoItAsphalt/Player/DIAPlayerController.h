// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/DIAInput.h"
#include "DIAPlayerController.generated.h"

struct FInputActionValue;

/**
 * Creates the input assets, adds the mapping context to the local player and routes the actions to the character.
 * Shop (Tab) toggles the shop panel of the HUD and Pause (Esc) closes it. Every frame it looks for the nearest interactable
 * (kiosk, ...) within reach and shows its prompt; Interact (E) uses it. While the shop is open there is no prompt and E
 * does nothing. The rest of Pause is bound but does nothing yet (a later task fills it in).
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

	/** What E would use right now (null if nothing is within reach, or the shop is open). */
	class IDIAInteractable* FindInteractable() const;

	virtual void PlayerTick(float DeltaTime) override;

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
	/** Sends the prompt of the nearest interactable to the HUD (a no-op for unchanged text). */
	void UpdatePrompt();

	UPROPERTY()
	FDIAInput Input;

	bool bShopInput = false;
};
