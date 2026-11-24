// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LotBalance.h"
#include "Player/DIAInput.h"
#include "DIAPlayerController.generated.h"

struct FInputActionValue;

/**
 * Creates the input assets, adds the mapping context to the local player and routes the actions to the character.
 * Pause (Esc) opens and closes the pause menu (the pause action also fires while the game is paused, so that Esc can
 * close the menu). Every frame it looks for the nearest interactable within reach and shows its prompt; Interact (E)
 * uses it. While the player drives the paver car the same handlers steer it instead: WASD throttle and steering, the mouse
 * turns the head, Use pours the bar and E gets out; the hand keys, Shift, R, H, the work view and V do nothing; M still opens the paver map. V holds the check view on foot.
 * E at the buy point of a shop building opens its panel. While the panel is up, E closes it (before any other interactable is looked for), and
 * it also closes when the player walks more than DIAShopPoint::CloseRangeCm away or gets into the car; the shop leaves the game running.
 * While a menu or the win screen is up there is no prompt and E does nothing.
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
	 * Called by the HUD when the shop opens or closes: the cursor goes to the shop and the camera stops following the mouse (the
	 * character stops using its hand, sprinting and looking at the work), but unlike a menu the game keeps running.
	 */
	void SetShopInput(bool bOpen);
	bool IsShopInput() const { return bShopInput; }

	/**
	 * Called by the HUD when the win screen appears (bOn) or goes (a new game started from it, or a game that is not won was loaded):
	 * the mouse cursor and the UI take all the input (UI only, so no key reaches the game any more) and the character stops.
	 */
	void SetWinInput(bool bOn = true);
	bool IsWinInput() const { return bWinInput; }

	/** What E would use right now (null if nothing is within reach, or a menu or the win screen is up). */
	class IDIAInteractable* FindInteractable() const;

	virtual void PlayerTick(float DeltaTime) override;
	/** Every tick: closes the shop panel when the player sits in the car or has walked further than DIAShopPoint::CloseRangeCm from the building it shows (public for tests, which have no player input to tick). */
	void CloseShopWhenAway();

	/** The input handlers, bound to the actions; public so that tests can drive them without a keyboard. */
	void OnMove(const FInputActionValue& Value);
	/** The movement keys went up: a driven car stops accelerating and steering (the character stops by itself). */
	void OnMoveStopped();
	/** E: closes the shop panel if one is open; else uses what is within reach (at the wheel of the car it gets out). */
	void OnInteract();
	/** Esc: closes the shop if it is open, else opens or closes the pause menu (nothing on the win screen). */
	void OnPause();
	/** M: opens or closes the paver map (click a sector to send the auto paver there); "No paver" without it. */
	void OnPaverMap();
	/** V, held: the check view; it does nothing while a menu, the shop or the win screen is up. */
	void OnCheckViewStarted();
	void OnCheckViewCompleted();
	/** Use and WorkView: held buttons; they do nothing while a menu or the win screen is up. */
	void OnUseStarted();
	void OnUseCompleted();
	/** Fill (R, held): scoops from the trough (bowl, scoop) or fills the bucket at a tap or the outlet. Pouring is the left button. */
	void OnFillStarted();
	void OnFillCompleted();
	void OnWorkViewStarted();
	void OnWorkViewCompleted();
	/**
	 * H: sends the auto paver to the sector the player stands in (toast: "Paver → sector n", "No paver", "Not on the lot" or "Paver is
	 * already here"). It does nothing while a menu, the shop or the win screen is up; the same code path serves dia.SendPaver.
	 */
	void OnSendPaver();
	/** 1 - 4 and Q: take an item into the hand. */
	void OnSelectHand(ELotHand Hand);
	void OnNextHand();

protected:
	virtual void InitInputSystem() override;
	virtual void SetupInputComponent() override;

private:
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted();
	void OnSprintCompleted();

	class ADIACharacter* GetDIACharacter() const;
	/** The car the player drives; null on foot. */
	class ADIAPaverCar* GetDIACar() const;
	/** Lets go of everything a UI that takes over the screen must not leave running: sprinting, using the item, the work view and the check view. */
	void ReleaseControls();
	/** The menu, the shop or the win screen covers the screen and owns the mouse: nothing behind it can be used. */
	bool IsUiInput() const { return bMenuInput || bShopInput || bWinInput; }
	/** The mouse cursor and the input mode that go with the UI that is up now (nothing to do without a window). */
	void ApplyInputMode();
	/** Sends the prompt of the nearest interactable to the HUD (a no-op for unchanged text). */
	void UpdatePrompt();

	UPROPERTY()
	FDIAInput Input;

	bool bMenuInput = false;
	bool bShopInput = false;
	bool bWinInput = false;
};
