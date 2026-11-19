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
 * progress away: always in the pause menu, and in the main menu when a save exists.
 */
struct FDIAMenuModel
{
	EDIAMenuMode Mode = EDIAMenuMode::None;
	bool bHasSave = false;
	/** The "Start over?" question is up instead of the buttons. */
	bool bConfirming = false;

	void Open(EDIAMenuMode InMode, bool bInHasSave)
	{
		Mode = InMode;
		bHasSave = bInHasSave;
		bConfirming = false;
	}

	void Close()
	{
		Mode = EDIAMenuMode::None;
		bConfirming = false;
	}

	bool IsOpen() const { return Mode != EDIAMenuMode::None; }
	bool ShowsButtons() const { return IsOpen() && !bConfirming; }
	bool ShowsContinue() const { return ShowsButtons() && Mode == EDIAMenuMode::Main && bHasSave; }
	bool ShowsResume() const { return ShowsButtons() && Mode == EDIAMenuMode::Pause; }
	bool ShowsQuestion() const { return IsOpen() && bConfirming; }
	bool NeedsNewGameConfirmation() const { return Mode == EDIAMenuMode::Pause || (Mode == EDIAMenuMode::Main && bHasSave); }

	FText GetTitle() const
	{
		return Mode == EDIAMenuMode::Pause
			? NSLOCTEXT("DIAHud", "MenuPauseTitle", "Paused")
			: NSLOCTEXT("DIAHud", "MenuMainTitle", "Do it Asphalt");
	}

	static FText GetQuestion()
	{
		return NSLOCTEXT("DIAHud", "MenuQuestion", "Start over? Progress will be lost.");
	}
};
