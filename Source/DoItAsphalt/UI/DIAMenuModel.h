// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Which menu is open: the main menu of the start (the game waits behind it) or the pause menu (Esc). */
enum class EDIAMenuMode : uint8
{
	None,
	Main,
	Pause,
};

/**
 * What the menu shows, as plain data (the widget reads it through attribute lambdas; tests read it without Slate). The main menu
 * offers Continue only when a save exists; the pause menu offers Resume. New Game asks "Start over?" first whenever it would throw
 * progress away: always in the pause menu, and in the main menu when a save exists. The Settings button swaps the buttons for the
 * settings screen (mouse sensitivity and Back).
 */
struct FDIAMenuModel
{
	EDIAMenuMode Mode = EDIAMenuMode::None;
	bool bHasSave = false;
	/** The "Start over?" question is up instead of the buttons. */
	bool bConfirming = false;
	/** The settings (mouse sensitivity) are up instead of the buttons. */
	bool bSettings = false;

	void Open(EDIAMenuMode InMode, bool bInHasSave)
	{
		Mode = InMode;
		bHasSave = bInHasSave;
		bConfirming = false;
		bSettings = false;
	}

	void Close()
	{
		Mode = EDIAMenuMode::None;
		bConfirming = false;
		bSettings = false;
	}

	/** The Settings button: only from the buttons. */
	void OpenSettings()
	{
		if (ShowsButtons())
		{
			bSettings = true;
		}
	}

	/** Back (or Esc) in the settings: the buttons again. */
	void CloseSettings() { bSettings = false; }

	bool IsOpen() const { return Mode != EDIAMenuMode::None; }
	bool ShowsButtons() const { return IsOpen() && !bConfirming && !bSettings; }
	bool ShowsSettings() const { return IsOpen() && bSettings; }
	bool ShowsContinue() const { return ShowsButtons() && Mode == EDIAMenuMode::Main && bHasSave; }
	bool ShowsResume() const { return ShowsButtons() && Mode == EDIAMenuMode::Pause; }
	bool ShowsQuestion() const { return IsOpen() && bConfirming; }
	bool NeedsNewGameConfirmation() const { return Mode == EDIAMenuMode::Pause || (Mode == EDIAMenuMode::Main && bHasSave); }

	FText GetTitle() const
	{
		if (bSettings)
		{
			return NSLOCTEXT("DIAHud", "MenuSettingsTitle", "Settings");
		}
		return Mode == EDIAMenuMode::Pause
			? NSLOCTEXT("DIAHud", "MenuPauseTitle", "Paused")
			: NSLOCTEXT("DIAHud", "MenuMainTitle", "Do it Asphalt");
	}

	static FText GetQuestion()
	{
		return NSLOCTEXT("DIAHud", "MenuQuestion", "Start over? Progress will be lost.");
	}
};
