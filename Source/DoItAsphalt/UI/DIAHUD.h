// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/DIAFormat.h"
#include "DIAHUD.generated.h"

class SDIAHudOverlay;
class SDIAShopPanel;
class SDIAWinScreen;
class UDIAWorldSubsystem;

/**
 * The HUD actor: creates the Slate overlay in the game viewport (not when nothing can render, e.g. in tests), keeps the
 * contextual prompt, the toasts, the shop panel (Tab) and the win screen (shown when the game is won), and turns the game's events
 * into toasts. The game state stays in the subsystem.
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
	/** New Game of the win screen: loads the level again. */
	void NewGame();
	/** Quit of the win screen. */
	void QuitGame();

	const FText& GetPrompt() const { return Prompt; }
	const FDIAToasts& GetToasts() const { return Toasts; }
	bool HasOverlay() const { return Overlay.IsValid(); }
	bool HasShopPanel() const { return ShopPanel.IsValid(); }
	bool HasWinScreen() const { return WinScreen.IsValid(); }

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

	FText Prompt;
	FDIAToasts Toasts;
	TSharedPtr<SDIAHudOverlay> Overlay;
	/** Built once with the overlay; shown and hidden, never rebuilt. */
	TSharedPtr<SDIAShopPanel> ShopPanel;
	bool bShopOpen = false;
	/** Built once with the overlay, hidden until the game is won. */
	TSharedPtr<SDIAWinScreen> WinScreen;
	bool bWinShown = false;

	TWeakObjectPtr<UDIAWorldSubsystem> BoundSubsystem;
	FDelegateHandle CellsHandle;
	FDelegateHandle RoadHandle;
	FDelegateHandle SpecialHandle;
	FDelegateHandle ExitHandle;
	FDelegateHandle WonHandle;
};
