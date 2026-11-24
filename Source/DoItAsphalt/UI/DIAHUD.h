// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/DIAFormat.h"
#include "UI/DIAMenuModel.h"
#include "DIAHUD.generated.h"

class SDIAHudOverlay;
class SDIAMenu;
class SDIAPaverMap;
class SDIAShopPanel;
class ADIAGroundActor;
class SDIAWinScreen;
class UDIAWorldSubsystem;

/**
 * The HUD actor: creates the Slate overlay in the game viewport (not when nothing can render, e.g. in tests), keeps the
 * contextual prompt, the toasts, the shop panel, the check view (V), the win screen (shown when the game is won) and the menu (the main menu
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
	/** Dev Start (main menu, not in Shipping): a new game with credits and every unlock step open (the subsystem's DevStart); leaves the menu. */
	void DevStart();
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
#if !UE_BUILD_SHIPPING
	/** The dia.Screen cheat: opens Pause, Settings, Question (the pause menu in that state) or Win, so that the screen can be photographed. False for another name. */
	bool DebugShowScreen(const FString& Name);
#endif
	EDIAMenuMode GetMenuMode() const { return MenuMode; }
	bool IsMenuOpen() const { return MenuMode != EDIAMenuMode::None; }

	/**
	 * Opens the shop panel on one building's goods (or switches an open panel to it). The game keeps running; the mouse goes to the
	 * shop, and the paver map closes. Nothing opens over a menu or the win screen.
	 */
	void OpenShop(ELotShop Shop);
	/** The building whose goods are shown; ELotShop::Count while the shop is closed. */
	ELotShop GetOpenShop() const { return bShopOpen ? LastShop : ELotShop::Count; }
	/** True opens the building that was open last (Tools at the start); false closes. */
	void SetShopOpen(bool bOpen);
	bool IsShopOpen() const { return bShopOpen; }

	/** M: opens or closes the paver map (the auto paver's sectors). Like the shop it takes the mouse; nothing opens over a menu or the win screen, and the shop and the map never stand together. */
	void TogglePaverMap();
	void SetPaverMapOpen(bool bOpen);
	bool IsPaverMapOpen() const { return bPaverMapOpen; }

	/** The controls legend is up: not while a menu, the win screen, a shop panel or the paver map has the screen. */
	bool IsControlsLegendShown() const { return !IsMenuOpen() && !bWinShown && !bShopOpen && !bPaverMapOpen; }

	/** The robot that the map's clicks and H send: always one that is owned (0 when none is, and after a new or loaded game). */
	int32 GetSelectedPaver() const;
	/** Selects a robot (0-based); ignored for one that is not owned. The selection stays while the map is closed and opened. */
	void SelectPaver(int32 Paver);

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
	void HandlePaverUpgradeBought(int32 Paver, ELotPaverUpgrade Upgrade);
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
	/** The building that is shown, or was shown last. */
	ELotShop LastShop = ELotShop::Tools;
	bool bPaverMapOpen = false;
	/** The robot of the map and of H; GetSelectedPaver checks it against the robots owned. */
	int32 SelectedPaver = 0;
	bool bCheckView = false;
	FDIACheckModel CheckModel;
	TSharedPtr<SDIAHudOverlay> Overlay;
	/** Built once with the overlay, hidden until the shop opens. */
	TSharedPtr<SDIAShopPanel> ShopPanel;
	/** Built once with the overlay, hidden until the paver map opens. */
	TSharedPtr<SDIAPaverMap> PaverMap;
	/** The controller takes the mouse while the shop or the map is open. */
	void UpdatePanelInput();
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
	FDelegateHandle PaverUpgradeHandle;
	FDelegateHandle ExitHandle;
	FDelegateHandle WonHandle;
	FDelegateHandle ResetHandle;
};
