// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAMenu.h"
#include "Game/DIASettings.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAButtonStyle.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
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
	OnDevStart = InArgs._OnDevStart;
	OnQuit = InArgs._OnQuit;
	ButtonStyle = AsphaltHud::MakeButtonStyle();

	// Hidden until Open; once open, the whole screen takes the mouse, so that nothing behind it can be clicked.
	SetVisibility(EVisibility::Collapsed);
	const TSharedRef<SWidget> Content =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuTitleSpacing)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return Model.ShowsQuestion() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetTitle(); }), AsphaltHud::BigFontSize, AsphaltHud::ScreenTitleColor)
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
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuDevStart", "Dev Start"), AsphaltHud::MenuButtonWidth,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleDevStart), [this]() { return Model.ShowsDevStart(); })
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuSettings", "Settings"), AsphaltHud::MenuButtonWidth,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleSettings), [this]() { return Model.ShowsButtons(); })
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
		// The settings take the place of the buttons: the label with the value, the slider, Back.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return Model.ShowsSettings() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::MenuButtonSpacing)
				[
					AsphaltHud::MakeShadowedText(
						TAttribute<FText>::CreateLambda([]()
						{
							return FText::Format(NSLOCTEXT("DIAHud", "MenuSensitivity", "Mouse sensitivity: {0}"),
								FText::FromString(FString::Printf(TEXT("%.2f"), DIASettings::GetMouseSensitivity())));
						}),
						AsphaltHud::MediumFontSize, AsphaltHud::TextColor)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(AsphaltHud::MenuButtonWidth).HeightOverride(AsphaltHud::MenuSliderHeight)
					[
						SNew(SSlider)
						.MinValue(DIASettings::MinMouseSensitivity)
						.MaxValue(DIASettings::MaxMouseSensitivity)
						.StepSize(0.05f)
						.Value_Lambda([]() { return DIASettings::GetMouseSensitivity(); })
						.OnValueChanged_Lambda([](float Value) { DIASettings::SetMouseSensitivity(Value); })
						.OnMouseCaptureEnd_Lambda([]() { DIASettings::Save(); })
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuBack", "Back"), AsphaltHud::MenuButtonWidth,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleBack), [this]() { return Model.ShowsSettings(); })
		];
	ChildSlot
	[
		AsphaltHud::MakeScreenPanel(&PanelBrush, AsphaltHud::MenuPanelWidth, Content)
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

FReply SDIAMenu::HandleDevStart()
{
	if (Model.NeedsNewGameConfirmation())
	{
		Model.bConfirming = true;
		Model.bConfirmingDevStart = true;
	}
	else
	{
		OnDevStart.ExecuteIfBound();
	}
	return FReply::Handled();
}

FReply SDIAMenu::HandleYes()
{
	const bool bDevStart = Model.bConfirmingDevStart;
	Model.bConfirming = false;
	Model.bConfirmingDevStart = false;
	(bDevStart ? OnDevStart : OnNewGame).ExecuteIfBound();
	return FReply::Handled();
}

FReply SDIAMenu::HandleNo()
{
	Model.bConfirming = false;
	Model.bConfirmingDevStart = false;
	return FReply::Handled();
}

FReply SDIAMenu::HandleSettings()
{
	Model.OpenSettings();
	return FReply::Handled();
}

FReply SDIAMenu::HandleBack()
{
	BackFromSettings();
	return FReply::Handled();
}

bool SDIAMenu::BackFromSettings()
{
	if (!Model.ShowsSettings())
	{
		return false;
	}
	Model.CloseSettings();
	DIASettings::Save();
	return true;
}

FReply SDIAMenu::HandleQuit()
{
	OnQuit.ExecuteIfBound();
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAMenu::BuildButton(const FText& Label, float Width, FOnClicked OnClicked, TFunction<bool()> Visible)
{
	const TSharedRef<SBox> Button = AsphaltHud::MakeMenuButton(Label, Width, OnClicked, &ButtonStyle);
	Button->SetVisibility(TAttribute<EVisibility>::CreateLambda([Visible = MoveTemp(Visible)]() { return Visible() ? EVisibility::Visible : EVisibility::Collapsed; }));
	return Button;
}
