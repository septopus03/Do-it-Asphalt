// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The win screen (GDD chapter 9): a dimmed backdrop that takes the mouse, the title "Parking lot complete!", the four
 * statistics (time, asphalt poured, money earned, upgrades bought) and the buttons New Game and Quit. The widget tree is built
 * once, hidden; Show takes a snapshot of the statistics into FDIAWinModel (the labels read it through attribute lambdas) and
 * makes it visible. The buttons only call the delegates given by the HUD.
 */
class SDIAWinScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDIAWinScreen) {}
		SLATE_EVENT(FSimpleDelegate, OnNewGame)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Reads the statistics from the game and shows the screen. */
	void Show(const FAsphaltGame& Game);

	const FDIAWinModel& GetModel() const { return Model; }

private:
	FReply HandleNewGame();
	FReply HandleQuit();
	TSharedRef<SWidget> BuildStatRow(const FText& Label, TFunction<FText()> Value, const FLinearColor& ValueColor);
	TSharedRef<SWidget> BuildButton(const FText& Label, FOnClicked OnClicked);

	FSimpleDelegate OnNewGame;
	FSimpleDelegate OnQuit;

	FDIAWinModel Model;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, AsphaltHud::PanelCornerRadius};
	FSlateRoundedBoxBrush RowBrush{FLinearColor::White, AsphaltHud::RowCornerRadius};
	FButtonStyle ButtonStyle;
};
