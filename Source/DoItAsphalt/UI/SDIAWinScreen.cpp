// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAWinScreen.h"
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

void SDIAWinScreen::Construct(const FArguments& InArgs)
{
	OnNewGame = InArgs._OnNewGame;
	OnQuit = InArgs._OnQuit;
	ButtonStyle = AsphaltHud::MakeButtonStyle();

	// Hidden until Show; once shown, the whole screen takes the mouse, so that nothing behind it can be clicked.
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
				SNew(SBox).WidthOverride(AsphaltHud::WinPanelWidth)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::WinTitleSpacing)
					[
						AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "WinTitle", "Parking lot complete!"), AsphaltHud::BigFontSize, AsphaltHud::WinTitleColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::WinRowSpacing)
					[
						BuildStatRow(NSLOCTEXT("DIAHud", "WinTime", "Time"), [this]() { return Model.GetView().Time; }, AsphaltHud::TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::WinRowSpacing)
					[
						BuildStatRow(NSLOCTEXT("DIAHud", "WinPoured", "Asphalt poured"), [this]() { return Model.GetView().Poured; }, AsphaltHud::TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::WinRowSpacing)
					[
						BuildStatRow(NSLOCTEXT("DIAHud", "WinMoney", "Money earned"), [this]() { return Model.GetView().Money; }, AsphaltHud::MoneyColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::WinTitleSpacing)
					[
						BuildStatRow(NSLOCTEXT("DIAHud", "WinUpgrades", "Upgrades bought"), [this]() { return Model.GetView().Upgrades; }, AsphaltHud::TextColor)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, AsphaltHud::WinButtonsSpacing, 0.f)
						[
							BuildButton(NSLOCTEXT("DIAHud", "WinNewGame", "New Game"), FOnClicked::CreateSP(this, &SDIAWinScreen::HandleNewGame))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							BuildButton(NSLOCTEXT("DIAHud", "WinQuit", "Quit"), FOnClicked::CreateSP(this, &SDIAWinScreen::HandleQuit))
						]
					]
				]
			]
		]
	];
}

void SDIAWinScreen::Show(const FAsphaltGame& Game)
{
	Model.Update(Game);
	SetVisibility(EVisibility::Visible);
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

TSharedRef<SWidget> SDIAWinScreen::BuildStatRow(const FText& Label, TFunction<FText()> Value, const FLinearColor& ValueColor)
{
	return SNew(SBorder)
		.BorderImage(&RowBrush)
		.BorderBackgroundColor(AsphaltHud::ShopRowColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::WinLabelWidth)
				[
					AsphaltHud::MakeShadowedText(Label, AsphaltHud::MediumFontSize, AsphaltHud::CaptionColor)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda(MoveTemp(Value)), AsphaltHud::MediumFontSize, ValueColor)
			]
		];
}

TSharedRef<SWidget> SDIAWinScreen::BuildButton(const FText& Label, FOnClicked OnClicked)
{
	return SNew(SBox).WidthOverride(AsphaltHud::WinButtonWidth)
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
