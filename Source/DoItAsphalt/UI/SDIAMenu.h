// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAMenuModel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The main menu (Continue when there is a save, New Game, Quit) and the pause menu (Resume, New Game, Quit) on a dimmed backdrop that
 * takes the mouse. The widget tree is built once, hidden; Open / Close set the model (which the buttons' visibility reads through
 * attribute lambdas) and show or hide the screen. New Game that would throw progress away turns the buttons into the question
 * "Start over? Progress will be lost." with Yes and No; only Yes calls OnNewGame. The Settings button turns the buttons into the
 * settings screen: a slider for the mouse sensitivity (DIASettings, kept at once, written to disk when the slider is let go) and Back.
 * The other buttons only call the delegates given by the HUD.
 */
class SDIAMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDIAMenu) {}
		SLATE_EVENT(FSimpleDelegate, OnContinue)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnNewGame)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows the menu in the given mode; bHasSave says whether Continue is offered (the main menu only). */
	void Open(EDIAMenuMode Mode, bool bHasSave);
	void Close();

	const FDIAMenuModel& GetModel() const { return Model; }

	/** Leaves the settings for the buttons (Back, Esc), saving the settings; false when the settings were not up. */
	bool BackFromSettings();

private:
	FReply HandleContinue();
	FReply HandleResume();
	FReply HandleNewGame();
	FReply HandleYes();
	FReply HandleNo();
	FReply HandleQuit();
	FReply HandleSettings();
	FReply HandleBack();
	/** A button of the menu's width that is only there while Visible says so. */
	TSharedRef<SWidget> BuildButton(const FText& Label, float Width, FOnClicked OnClicked, TFunction<bool()> Visible);

	FSimpleDelegate OnContinue;
	FSimpleDelegate OnResume;
	FSimpleDelegate OnNewGame;
	FSimpleDelegate OnQuit;

	FDIAMenuModel Model;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, AsphaltHud::PanelCornerRadius};
	FButtonStyle ButtonStyle;
};
