// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAMenu.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAButtonStyle.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SDIAMenu::Construct(const FArguments& InArgs)
{
	OnContinue = InArgs._OnContinue;
	OnResume = InArgs._OnResume;
	OnNewGame = InArgs._OnNewGame;
	OnQuit = InArgs._OnQuit;
	ButtonStyle = AsphaltHud::MakeButtonStyle();

	// Hidden until Open; once open, the whole screen takes the mouse, so that nothing behind it can be clicked.
	SetVisibility(EVisibility::Collapsed);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(AsphaltHud::WinBackdropColor)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::ShopPanelColor)
			.Padding(AsphaltHud::WinPanelPadding)
			[
				SNew(SBox).WidthOverride(AsphaltHud::MenuPanelWidth)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuTitleSpacing)
					[
						SNew(SBox)
						.Visibility_Lambda([this]() { return Model.ShowsQuestion() ? EVisibility::Collapsed : EVisibility::Visible; })
						[
							AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetTitle(); }), AsphaltHud::BigFontSize, AsphaltHud::WinTitleColor)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuTitleSpacing)
					[
						SNew(SBox)
						.Visibility_Lambda([this]() { return Model.ShowsQuestion() ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							AsphaltHud::MakeShadowedText(FDIAMenuModel::GetQuestion(), AsphaltHud::MediumFontSize, AsphaltHud::TextColor)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
					[
						BuildButton(NSLOCTEXT("DIAHud", "MenuContinue", "Continue"), AsphaltHud::MenuButtonWidth,
							FOnClicked::CreateSP(this, &SDIAMenu::HandleContinue), [this]() { return Model.ShowsContinue(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
					[
						BuildButton(NSLOCTEXT("DIAHud", "MenuResume", "Resume"), AsphaltHud::MenuButtonWidth,
							FOnClicked::CreateSP(this, &SDIAMenu::HandleResume), [this]() { return Model.ShowsResume(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
					[
						BuildButton(NSLOCTEXT("DIAHud", "MenuNewGame", "New Game"), AsphaltHud::MenuButtonWidth,
							FOnClicked::CreateSP(this, &SDIAMenu::HandleNewGame), [this]() { return Model.ShowsButtons(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						BuildButton(NSLOCTEXT("DIAHud", "MenuQuit", "Quit"), AsphaltHud::MenuButtonWidth,
							FOnClicked::CreateSP(this, &SDIAMenu::HandleQuit), [this]() { return Model.ShowsButtons(); })
					]
					// The question's two answers take the place of the buttons.
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, AsphaltHud::MenuButtonSpacing, 0.f)
						[
							BuildButton(NSLOCTEXT("DIAHud", "MenuYes", "Yes"), AsphaltHud::MenuQuestionButtonWidth,
								FOnClicked::CreateSP(this, &SDIAMenu::HandleYes), [this]() { return Model.ShowsQuestion(); })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							BuildButton(NSLOCTEXT("DIAHud", "MenuNo", "No"), AsphaltHud::MenuQuestionButtonWidth,
								FOnClicked::CreateSP(this, &SDIAMenu::HandleNo), [this]() { return Model.ShowsQuestion(); })
						]
					]
				]
			]
		]
	];
}

void SDIAMenu::Open(EDIAMenuMode Mode, bool bHasSave)
{
	Model.Open(Mode, bHasSave);
	SetVisibility(EVisibility::Visible);
}

void SDIAMenu::Close()
{
	Model.Close();
	SetVisibility(EVisibility::Collapsed);
}

FReply SDIAMenu::HandleContinue()
{
	OnContinue.ExecuteIfBound();
	return FReply::Handled();
}

FReply SDIAMenu::HandleResume()
{
	OnResume.ExecuteIfBound();
	return FReply::Handled();
}

FReply SDIAMenu::HandleNewGame()
{
	if (Model.NeedsNewGameConfirmation())
	{
		Model.bConfirming = true;
	}
	else
	{
		OnNewGame.ExecuteIfBound();
	}
	return FReply::Handled();
}

FReply SDIAMenu::HandleYes()
{
	Model.bConfirming = false;
	OnNewGame.ExecuteIfBound();
	return FReply::Handled();
}

FReply SDIAMenu::HandleNo()
{
	Model.bConfirming = false;
	return FReply::Handled();
}

FReply SDIAMenu::HandleQuit()
{
	OnQuit.ExecuteIfBound();
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAMenu::BuildButton(const FText& Label, float Width, FOnClicked OnClicked, TFunction<bool()> Visible)
{
	return SNew(SBox)
		.WidthOverride(Width)
		.Visibility_Lambda([Visible = MoveTemp(Visible)]() { return Visible() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SButton)
			.ButtonStyle(&ButtonStyle)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(0.f, AsphaltHud::WinButtonPaddingV))
			.OnClicked(OnClicked)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::MediumFontSize))
				.ColorAndOpacity(AsphaltHud::ButtonTextColor)
			]
		];
}
