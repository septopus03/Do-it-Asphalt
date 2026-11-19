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
 *
 * The same actions drive whatever the controller possesses: the character, or the paver truck (move = throttle and steering,
 * left button = pour, E = get out; the look input turns the camera).
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

	/**
	 * Called by the HUD when the win screen appears: the mouse cursor and the UI take all the input (UI only, so no key reaches the
	 * game any more), the character stops, pouring stops and the truck lets go of throttle and steering. There is no way back: the game is won;
	 * New Game loads a fresh level with a fresh controller.
	 */
	void SetWinInput();
	bool IsWinInput() const { return bWinInput; }

	/** What E would use right now (null if nothing is within reach, or the shop is open). */
	class IDIAInteractable* FindInteractable() const;

	virtual void PlayerTick(float DeltaTime) override;

	/** The input handlers, bound to the actions; public so that tests can drive them without a keyboard. */
	void OnMove(const FInputActionValue& Value);
	void OnMoveStopped();
	void OnPourStarted();
	void OnPourCompleted();
	/** E: gets out of the truck while driving it, otherwise uses what is within reach. */
	void OnInteract();

protected:
	virtual void InitInputSystem() override;
	virtual void SetupInputComponent() override;

private:
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted();
	void OnSprintCompleted();
	void OnShop();
	void OnPause();

	class ADIACharacter* GetDIACharacter() const;
	/** The truck while the controller drives it; null otherwise. */
	class ADIAPaverCar* GetDIACar() const;
	/** Stops pouring, with the character's spreader or the truck's bar. */
	void StopPouring();
	/** The shop or the win screen covers the screen and owns the mouse: nothing behind it can be used. */
	bool IsUiInput() const { return bShopInput || bWinInput; }
	/** Sends the prompt of the nearest interactable to the HUD (a no-op for unchanged text). */
	void UpdatePrompt();

	UPROPERTY()
	FDIAInput Input;

	bool bShopInput = false;
	bool bWinInput = false;
};
