// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/DIAInput.h"
#include "DIAPlayerController.generated.h"

struct FInputActionValue;

/**
 * Creates the input assets, adds the mapping context to the local player and routes the actions to the character.
 * Pause (Esc) opens and closes the pause menu (the pause action also fires while the game is paused, so that Esc can
 * close the menu). Every frame it looks for the nearest interactable within reach and shows its prompt; Interact (E)
 * uses it. While a menu or the win screen is up there is no prompt and E does nothing.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	const FDIAInput& GetInput() const { return Input; }

	/**
	 * Called by the HUD when the main or pause menu opens or closes: the cursor and the mouse go to the menu (the
	 * camera stops following the mouse, the character stops sprinting), and the game is paused while it is open; closing it gives everything back.
	 */
	void SetMenuInput(bool bOpen);
	bool IsMenuInput() const { return bMenuInput; }

	/**
	 * Called by the HUD when the win screen appears (bOn) or goes (a new game started from it, or a game that is not won was loaded):
	 * the mouse cursor and the UI take all the input (UI only, so no key reaches the game any more) and the character stops.
	 */
	void SetWinInput(bool bOn = true);
	bool IsWinInput() const { return bWinInput; }

	/** What E would use right now (null if nothing is within reach, or a menu or the win screen is up). */
	class IDIAInteractable* FindInteractable() const;

	virtual void PlayerTick(float DeltaTime) override;

	/** The input handlers, bound to the actions; public so that tests can drive them without a keyboard. */
	void OnMove(const FInputActionValue& Value);
	/** E: uses what is within reach. */
	void OnInteract();
	/** Esc: opens or closes the pause menu (nothing on the win screen). */
	void OnPause();

protected:
	virtual void InitInputSystem() override;
	virtual void SetupInputComponent() override;

private:
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted();
	void OnSprintCompleted();

	class ADIACharacter* GetDIACharacter() const;
	/** Lets go of everything a UI that takes over the screen must not leave running: sprinting. */
	void ReleaseControls();
	/** The menu or the win screen covers the screen and owns the mouse: nothing behind it can be used. */
	bool IsUiInput() const { return bMenuInput || bWinInput; }
	/** The mouse cursor and the input mode that go with the UI that is up now (nothing to do without a window). */
	void ApplyInputMode();
	/** Sends the prompt of the nearest interactable to the HUD (a no-op for unchanged text). */
	void UpdatePrompt();

	UPROPERTY()
	FDIAInput Input;

	bool bMenuInput = false;
	bool bWinInput = false;
};
