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
class SDIAWinScreen;
class UDIAWorldSubsystem;

/**
 * The HUD actor: creates the Slate overlay in the game viewport (not when nothing can render, e.g. in tests), keeps the
 * contextual prompt, the toasts, the shop panel (Tab), the win screen (shown when the game is won) and the menu (the main menu
 * at the start, the pause menu on Esc), and turns the game's events into toasts. The game state stays in the subsystem. Where there
 * is no overlay (tests) the game starts without the menu; the menu's state and the input it takes are the same either way.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADIAHUD();

	/** Contextual prompt above the backpack bar; empty text hides it. */
	void SetPrompt(const FText& Text);
	/** A short message at the top of the screen; it fades after AsphaltHud::ToastHoldSeconds. */
	void ShowToast(const FText& Text);

	/** Opens the shop if it is closed and closes it if it is open; the game runs on either way. */
	void ToggleShop();
	void SetShopOpen(bool bOpen);
	bool IsShopOpen() const { return bShopOpen; }

	/**
	 * Shows the win screen: the shop closes (and stays closed), the controller hands the mouse and the input to the screen. The
	 * screen is shown once the game is won (the HUD listens to OnWon), and there is no way back to the game from it.
	 */
	void ShowWinScreen();
	bool IsWinScreenShown() const { return bWinShown; }
	/** New Game (menu, win screen): starts over in the same world (the subsystem's NewGame) and leaves the menu. */
	void NewGame();
	/** Quit (menu, win screen); the game saves itself when the world ends. */
	void QuitGame();

	/**
	 * Opens the main menu or the pause menu: the shop closes, the game pauses and the controller hands the mouse to the menu. Nothing
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

	const FText& GetPrompt() const { return Prompt; }
	const FDIAToasts& GetToasts() const { return Toasts; }
	bool HasOverlay() const { return Overlay.IsValid(); }
	bool HasShopPanel() const { return ShopPanel.IsValid(); }
	bool HasWinScreen() const { return WinScreen.IsValid(); }
	bool HasMenu() const { return Menu.IsValid(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void CreateOverlay(UDIAWorldSubsystem* Subsystem);
	void DestroyOverlay();

	void HandleCellsCompleted(const TArray<int32>& Cells);
	void HandleRoadCompleted(int32 Road);
	void HandleSpecialBought(EAsphaltSpecial Special);
	void HandleExitOpened();
	void HandleWon();
	/** A game was loaded or started: the win screen goes if that game is not won, and appears if it is. */
	void HandleGameReset();
	void HideWinScreen();
	/** Continue of the main menu. */
	void ContinueGame();
	/** Hides the menu and gives the input back, whichever menu it was; no autosave decision. */
	void DismissMenu();

	FText Prompt;
	FDIAToasts Toasts;
	TSharedPtr<SDIAHudOverlay> Overlay;
	/** Built once with the overlay; shown and hidden, never rebuilt. */
	TSharedPtr<SDIAShopPanel> ShopPanel;
	bool bShopOpen = false;
	/** Built once with the overlay, hidden until the game is won. */
	TSharedPtr<SDIAWinScreen> WinScreen;
	bool bWinShown = false;
	/** Built once with the overlay, hidden until a menu opens. */
	TSharedPtr<SDIAMenu> Menu;
	EDIAMenuMode MenuMode = EDIAMenuMode::None;

	TWeakObjectPtr<UDIAWorldSubsystem> BoundSubsystem;
	FDelegateHandle CellsHandle;
	FDelegateHandle RoadHandle;
	FDelegateHandle SpecialHandle;
	FDelegateHandle ExitHandle;
	FDelegateHandle WonHandle;
	FDelegateHandle ResetHandle;
};
