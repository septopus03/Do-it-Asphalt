// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAWinScreen.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAButtonStyle.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "UI/SDIAHazardStripes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SDIAWinScreen::Construct(const FArguments& InArgs)
{
	OnNewGame = InArgs._OnNewGame;
	OnQuit = InArgs._OnQuit;
	ButtonStyle = AsphaltHud::MakeSignButtonStyle();

	// Hidden until Show; once shown, the whole screen takes the mouse, so that nothing behind it can be clicked.
	SetVisibility(EVisibility::Collapsed);
	const TSharedRef<SWidget> Content =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SDIAHazardStripes)
			.Band(AsphaltHud::WinStripeBand)
			.Height(AsphaltHud::WinStripeHeight)
			.StripeColor(AsphaltHud::SignBlack)
			.GroundColor(AsphaltHud::SignYellow)
		]
		// The title shrinks to fit when it is wider than the plate (at 48 it is).
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, AsphaltHud::WinTitleGapTop, 0.f, AsphaltHud::WinTitleGapBottom)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			.HAlign(HAlign_Center)
			[
				AsphaltHud::MakeText(NSLOCTEXT("DIAHud", "WinTitle", "Parking lot complete!"), EDIAFontWeight::ExtraBold, AsphaltHud::WinTitleFont, AsphaltHud::SignBlack)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildStatRow(NSLOCTEXT("DIAHud", "WinTime", "Time"), [this]() { return Model.GetView().Time; })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildStatRow(NSLOCTEXT("DIAHud", "WinCells", "Cells paved"), [this]() { return Model.GetView().Cells; })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildStatRow(NSLOCTEXT("DIAHud", "WinPoured", "Asphalt poured"), [this]() { return Model.GetView().Poured; })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildStatRow(NSLOCTEXT("DIAHud", "WinCredits", "Credits earned"), [this]() { return Model.GetView().Credits; })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildStatRow(NSLOCTEXT("DIAHud", "WinItems", "Items bought"), [this]() { return Model.GetView().Items; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, AsphaltHud::WinButtonsGapTop, 0.f, 0.f)
		[
			MakeRule()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, AsphaltHud::WinButtonsPadTop, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, AsphaltHud::MenuGap, 0.f)
			[
				BuildButton(NSLOCTEXT("DIAHud", "WinNewGame", "New Game"), FOnClicked::CreateSP(this, &SDIAWinScreen::HandleNewGame), true)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				BuildButton(NSLOCTEXT("DIAHud", "WinQuit", "Quit"), FOnClicked::CreateSP(this, &SDIAWinScreen::HandleQuit), false)
			]
		];
	ChildSlot
	[
		AsphaltHud::MakeScreenPanel(AsphaltHud::WinContentWidth, Content)
	];
}

void SDIAWinScreen::Show(const FLotGame& Game)
{
	Model.Update(Game);
	SetVisibility(EVisibility::Visible);
}

void SDIAWinScreen::Hide()
{
	SetVisibility(EVisibility::Collapsed);
}

FReply SDIAWinScreen::HandleNewGame()
{
	OnNewGame.ExecuteIfBound();
	return FReply::Handled();
}

FReply SDIAWinScreen::HandleQuit()
{
	OnQuit.ExecuteIfBound();
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAWinScreen::MakeRule()
{
	return SNew(SBox).HeightOverride(AsphaltHud::WinRowLine)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(AsphaltHud::SignBlack)
		];
}

TSharedRef<SWidget> SDIAWinScreen::BuildStatRow(const FText& Label, TFunction<FText()> Value)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeRule()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, AsphaltHud::WinRowPadV)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				AsphaltHud::MakeText(Label, EDIAFontWeight::Bold, AsphaltHud::WinLabelFont, AsphaltHud::SignBlack)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[
				AsphaltHud::MakeText(TAttribute<FText>::CreateLambda(MoveTemp(Value)), EDIAFontWeight::ExtraBold, AsphaltHud::WinValueFont, AsphaltHud::SignBlack)
			]
		];
}

TSharedRef<SWidget> SDIAWinScreen::BuildButton(const FText& Label, FOnClicked OnClicked, bool bPrimary)
{
	return AsphaltHud::MakeMenuButton(Label, AsphaltHud::WinButtonWidth, OnClicked, &ButtonStyle, bPrimary);
}
