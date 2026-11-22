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

/** The button that gets the yellow label: the one the screen leads with. */
enum class EDIAMenuButton : uint8
{
	None,
	Continue,
	Resume,
	NewGame,
	Yes,
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
	/** The "Start over?" question belongs to Dev Start, not to New Game: Yes starts the dev game. */
	bool bConfirmingDevStart = false;

	/** Dev Start (a new game with credits and every unlock step open, for trying the whole shop) exists only outside a Shipping build. */
	static constexpr bool bDevStartAvailable = !UE_BUILD_SHIPPING;

	void Open(EDIAMenuMode InMode, bool bInHasSave)
	{
		Mode = InMode;
		bHasSave = bInHasSave;
		bConfirming = false;
		bSettings = false;
		bConfirmingDevStart = false;
	}

	void Close()
	{
		Mode = EDIAMenuMode::None;
		bConfirming = false;
		bSettings = false;
		bConfirmingDevStart = false;
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
	bool ShowsDevStart() const { return bDevStartAvailable && ShowsButtons() && Mode == EDIAMenuMode::Main; }
	bool ShowsQuestion() const { return IsOpen() && bConfirming; }
	bool NeedsNewGameConfirmation() const { return Mode == EDIAMenuMode::Pause || (Mode == EDIAMenuMode::Main && bHasSave); }

	/** The button shown in yellow: Yes at the question, none in the settings or when closed, otherwise the first button of the screen. */
	EDIAMenuButton PrimaryButton() const
	{
		if (!IsOpen() || bSettings)
		{
			return EDIAMenuButton::None;
		}
		if (bConfirming)
		{
			return EDIAMenuButton::Yes;
		}
		if (Mode == EDIAMenuMode::Pause)
		{
			return EDIAMenuButton::Resume;
		}
		return bHasSave ? EDIAMenuButton::Continue : EDIAMenuButton::NewGame;
	}

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
