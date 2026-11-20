// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/DIAFormat.h"
#include "UI/DIAMenuModel.h"
#include "DIAHUD.generated.h"

class SDIAHudOverlay;
class SDIAMenu;
class SDIAShopPanel;
class ADIAGroundActor;
class SDIAWinScreen;
class UDIAWorldSubsystem;

/**
 * The HUD actor: creates the Slate overlay in the game viewport (not when nothing can render, e.g. in tests), keeps the
 * contextual prompt, the toasts, the shop (Tab), the check view (V), the win screen (shown when the game is won) and the menu (the main menu
 * at the start, the pause menu on Esc), and turns the game's events into toasts. The game state stays in the subsystem. Where there
 * is no overlay (tests) the game starts without the menu; the menu's state and the input it takes are the same either way.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADIAHUD();

	/** Contextual prompt at the bottom of the screen; empty text hides it. */
	void SetPrompt(const FText& Text);
	/** A short message at the top of the screen; it fades after AsphaltHud::ToastHoldSeconds. */
	void ShowToast(const FText& Text);

	/**
	 * Shows the win screen: the controller hands the mouse and the input to the screen. The
	 * screen is shown once the game is won (the HUD listens to OnWon), and there is no way back to the game from it.
	 */
	void ShowWinScreen();
	bool IsWinScreenShown() const { return bWinShown; }
	/** New Game (menu, win screen): starts over in the same world (the subsystem's NewGame) and leaves the menu. */
	void NewGame();
	/** Continue of the main menu: loads the saved game and leaves the menu; if the save is unusable the menu stays and offers a new game only. */
	void ContinueGame();
	/** Quit (menu, win screen); the game saves itself when the world ends. */
	void QuitGame();

	/**
	 * Opens the main menu or the pause menu: the game pauses and the controller hands the mouse to the menu. Nothing
	 * happens while a menu is open or the win screen is shown.
	 */
	void OpenMenu(EDIAMenuMode Mode);
	/**
	 * Closes the menu and lets the game run. Leaving the main menu is what starts the game for good: from then on it saves itself
	 * (a menu that is only quit from never overwrites the save).
	 */
	void CloseMenu();
	/** Esc: opens the pause menu, or closes it again. The main menu does not answer to it (there is nothing to go back to). */
	void TogglePause();
	/** Closes the menu that is open without choosing anything (the dia.SkipMenu cheat for screenshots). */
	void SkipMenu();
	EDIAMenuMode GetMenuMode() const { return MenuMode; }
	bool IsMenuOpen() const { return MenuMode != EDIAMenuMode::None; }

	/** Tab: opens or closes the shop. The game keeps running; the mouse goes to the shop. Nothing opens over a menu or the win screen. */
	void ToggleShop();
	void SetShopOpen(bool bOpen);
	bool IsShopOpen() const { return bShopOpen; }

	/** The check view (V, held): unfinished ground turns orange and the HUD points to the nearest unfinished cell. */
	void SetCheckView(bool bOn);
	bool IsCheckView() const { return bCheckView; }
	const FDIACheckView& GetCheckView() const { return CheckModel.GetView(); }

	const FText& GetPrompt() const { return Prompt; }
	const FDIAToasts& GetToasts() const { return Toasts; }
	bool HasOverlay() const { return Overlay.IsValid(); }
	bool HasWinScreen() const { return WinScreen.IsValid(); }
	bool HasMenu() const { return Menu.IsValid(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void CreateOverlay(UDIAWorldSubsystem* Subsystem);
	void DestroyOverlay();

	void HandleCellsCompleted(const TArray<int32>& Cells);
	void HandleEntryCompleted(int32 Entry);
	void HandleUnlocked(ELotUnlock Unlock);
	void HandleItemBought(ELotItem Item);
	void HandleExitOpened();
	void HandleWon();
	/** A game was loaded or started: the win screen goes if that game is not won, and appears if it is. */
	void HandleGameReset();
	void HideWinScreen();
	/** Hides the menu and gives the input back, whichever menu it was; no autosave decision. */
	void DismissMenu();

	FText Prompt;
	FDIAToasts Toasts;
	bool bShopOpen = false;
	bool bCheckView = false;
	FDIACheckModel CheckModel;
	TSharedPtr<SDIAHudOverlay> Overlay;
	/** Built once with the overlay, hidden until the shop opens. */
	TSharedPtr<SDIAShopPanel> ShopPanel;
	/** The ground, found when the check view is first switched on. */
	TWeakObjectPtr<ADIAGroundActor> Ground;
	/** Built once with the overlay, hidden until the game is won. */
	TSharedPtr<SDIAWinScreen> WinScreen;
	bool bWinShown = false;
	/** Built once with the overlay, hidden until a menu opens. */
	TSharedPtr<SDIAMenu> Menu;
	EDIAMenuMode MenuMode = EDIAMenuMode::None;

	TWeakObjectPtr<UDIAWorldSubsystem> BoundSubsystem;
	FDelegateHandle CellsHandle;
	FDelegateHandle EntryHandle;
	FDelegateHandle UnlockHandle;
	FDelegateHandle ItemHandle;
	FDelegateHandle ExitHandle;
	FDelegateHandle WonHandle;
	FDelegateHandle ResetHandle;
};
