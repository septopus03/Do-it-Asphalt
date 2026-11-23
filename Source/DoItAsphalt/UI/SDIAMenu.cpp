// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAMenu.h"
#include "Game/DIASettings.h"
#include "Widgets/Layout/SScrollBox.h"
#include "UI/DIAButtonStyle.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
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
	ButtonStyle = AsphaltHud::MakeSignButtonStyle();
	SliderStyle = AsphaltHud::MakeSliderStyle();

	// Hidden until Open; once open, the whole screen takes the mouse, so that nothing behind it can be clicked.
	SetVisibility(EVisibility::Collapsed);
	const TSharedRef<SWidget> Content =
		SNew(SVerticalBox)
		// The title shrinks to fit when it is wider than the plate ("Do it Asphalt" at 60 is).
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap * 2.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return Model.ShowsQuestion() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::DownOnly)
				.HAlign(HAlign_Center)
				[
					AsphaltHud::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model.GetTitle(); }), EDIAFontWeight::ExtraBold, AsphaltHud::MenuTitleFont, AsphaltHud::SignBlack)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap * 2.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return Model.ShowsQuestion() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				MakeQuestionText()
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuContinue", "Continue"), AsphaltHud::MenuContentWidth, EDIAMenuButton::Continue,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleContinue), [this]() { return Model.ShowsContinue(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuResume", "Resume"), AsphaltHud::MenuContentWidth, EDIAMenuButton::Resume,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleResume), [this]() { return Model.ShowsResume(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuNewGame", "New Game"), AsphaltHud::MenuContentWidth, EDIAMenuButton::NewGame,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleNewGame), [this]() { return Model.ShowsButtons(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuDevStart", "Dev Start"), AsphaltHud::MenuContentWidth, EDIAMenuButton::None,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleDevStart), [this]() { return Model.ShowsDevStart(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuSettings", "Settings"), AsphaltHud::MenuContentWidth, EDIAMenuButton::None,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleSettings), [this]() { return Model.ShowsButtons(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuQuit", "Quit"), AsphaltHud::MenuContentWidth, EDIAMenuButton::None,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleQuit), [this]() { return Model.ShowsButtons(); })
		]
		// The question's two answers take the place of the buttons.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, AsphaltHud::MenuGap, 0.f)
			[
				BuildButton(NSLOCTEXT("DIAHud", "MenuYes", "Yes"), AsphaltHud::MenuQuestionButtonWidth, EDIAMenuButton::Yes,
					FOnClicked::CreateSP(this, &SDIAMenu::HandleYes), [this]() { return Model.ShowsQuestion(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				BuildButton(NSLOCTEXT("DIAHud", "MenuNo", "No"), AsphaltHud::MenuQuestionButtonWidth, EDIAMenuButton::None,
					FOnClicked::CreateSP(this, &SDIAMenu::HandleNo), [this]() { return Model.ShowsQuestion(); })
			]
		]
		// The settings take the place of the buttons: a scrolling list of sliders and switches, then Defaults and Back.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return Model.ShowsSettings() ? EVisibility::Visible : EVisibility::Collapsed; })
			.MaxDesiredHeight(AsphaltHud::MenuSettingsMaxHeight)
			[
				SNew(SScrollBox)
				.ScrollBarVisibility(EVisibility::Collapsed)
				+ SScrollBox::Slot()
				[
					BuildSettingsList()
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuDefaults", "Defaults"), AsphaltHud::MenuContentWidth, EDIAMenuButton::None,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleDefaults), [this]() { return Model.ShowsSettings(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildButton(NSLOCTEXT("DIAHud", "MenuBack", "Back"), AsphaltHud::MenuContentWidth, EDIAMenuButton::None,
				FOnClicked::CreateSP(this, &SDIAMenu::HandleBack), [this]() { return Model.ShowsSettings(); })
		];
	ChildSlot
	[
		AsphaltHud::MakeScreenPanel(AsphaltHud::MenuContentWidth, Content)
	];
}

TSharedRef<SWidget> SDIAMenu::MakeQuestionText()
{
	// The question is wider than the plate at this size: it wraps and is centred.
	const TSharedRef<STextBlock> Text = AsphaltHud::MakeText(FDIAMenuModel::GetQuestion(), EDIAFontWeight::Bold, AsphaltHud::MenuQuestionFont, AsphaltHud::SignBlack);
	Text->SetWrapTextAt(AsphaltHud::MenuContentWidth);
	Text->SetJustification(ETextJustify::Center);
	return Text;
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

FReply SDIAMenu::HandleDefaults()
{
	DIASettings::ResetToDefaults();
	DIASettings::Apply();
	DIASettings::Save();
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAMenu::MakeSliderRow(const FText& Label, DIASettings::EFloat Setting, TFunction<FText(float)> Format)
{
	const DIASettings::FFloatSpec& Spec = DIASettings::Spec(Setting);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				AsphaltHud::MakeText(Label, EDIAFontWeight::Bold, AsphaltHud::MenuSettingCaptionFont, AsphaltHud::SignBlack)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[
				AsphaltHud::MakeText(
					TAttribute<FText>::CreateLambda([Setting, Format]() { return Format(DIASettings::GetFloat(Setting)); }),
					EDIAFontWeight::ExtraBold, AsphaltHud::MenuSettingValueFont, AsphaltHud::SignBlack)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, AsphaltHud::MenuGap)
		[
			SNew(SBox).HeightOverride(AsphaltHud::MenuSettingSliderHeight)
			[
				SNew(SSlider)
				.Style(&SliderStyle)
				.MinValue(Spec.Min)
				.MaxValue(Spec.Max)
				.StepSize(Spec.Step)
				.Value_Lambda([Setting]() { return DIASettings::GetFloat(Setting); })
				.OnValueChanged_Lambda([Setting](float Value)
				{
					DIASettings::SetFloat(Setting, Value);
					// The master volume and the like are heard while the slider moves.
					DIASettings::Apply();
				})
				.OnMouseCaptureEnd_Lambda([]() { DIASettings::Save(); })
			]
		];
}

TSharedRef<SWidget> SDIAMenu::MakeChoiceRow(const FText& Label, TFunction<FText()> Value, TFunction<void()> OnClick)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			AsphaltHud::MakeText(Label, EDIAFontWeight::Bold, AsphaltHud::MenuSettingCaptionFont, AsphaltHud::SignBlack)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 0.f, AsphaltHud::MenuGap)
		[
			AsphaltHud::MakeMenuButton(TAttribute<FText>::CreateLambda([Value]() { return Value(); }), AsphaltHud::MenuChoiceButtonWidth,
				FOnClicked::CreateLambda([OnClick]()
				{
					OnClick();
					DIASettings::Apply();
					DIASettings::Save();
					return FReply::Handled();
				}), &ButtonStyle)
		];
}

TSharedRef<SWidget> SDIAMenu::BuildSettingsList()
{
	const auto Percent = [](float V) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f))); };
	const auto OnOff = [](DIASettings::EToggle T)
	{
		return TFunction<FText()>([T]() { return DIASettings::GetToggle(T) ? NSLOCTEXT("DIAHud", "On", "On") : NSLOCTEXT("DIAHud", "Off", "Off"); });
	};
	const auto Flip = [](DIASettings::EToggle T)
	{
		return TFunction<void()>([T]() { DIASettings::SetToggle(T, !DIASettings::GetToggle(T)); });
	};
	using DIASettings::EFloat;
	using DIASettings::EToggle;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSliderRow(NSLOCTEXT("DIAHud", "MenuSensitivityLabel", "Mouse sensitivity"), EFloat::MouseSensitivity,
				[](float V) { return FText::FromString(FString::Printf(TEXT("%.2f"), V)); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChoiceRow(NSLOCTEXT("DIAHud", "MenuInvertY", "Invert Y axis"), OnOff(EToggle::InvertY), Flip(EToggle::InvertY))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSliderRow(NSLOCTEXT("DIAHud", "MenuFov", "Field of view"), EFloat::FieldOfView,
				[](float V) { return FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(V))); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSliderRow(NSLOCTEXT("DIAHud", "MenuMaster", "Master volume"), EFloat::MasterVolume, Percent)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSliderRow(NSLOCTEXT("DIAHud", "MenuSfx", "Effects volume"), EFloat::SfxVolume, Percent)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSliderRow(NSLOCTEXT("DIAHud", "MenuAmbient", "Ambience volume"), EFloat::AmbientVolume, Percent)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChoiceRow(NSLOCTEXT("DIAHud", "MenuFullscreen", "Fullscreen"), OnOff(EToggle::Fullscreen), Flip(EToggle::Fullscreen))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChoiceRow(NSLOCTEXT("DIAHud", "MenuVsync", "V-Sync"), OnOff(EToggle::VSync), Flip(EToggle::VSync))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChoiceRow(NSLOCTEXT("DIAHud", "MenuQuality", "Graphics quality"),
				[]() { return DIASettings::QualityName(DIASettings::GetQuality()); },
				[]() { DIASettings::SetQuality((DIASettings::GetQuality() + 1) % DIASettings::NumQualityLevels); })
		];
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

TSharedRef<SWidget> SDIAMenu::BuildButton(const FText& Label, float Width, EDIAMenuButton Which, FOnClicked OnClicked, TFunction<bool()> Visible)
{
	const TSharedRef<SBox> Button = AsphaltHud::MakeMenuButton(Label, Width, OnClicked, &ButtonStyle,
		TAttribute<bool>::CreateLambda([this, Which]() { return Model.IsPrimary(Which); }));
	Button->SetVisibility(TAttribute<EVisibility>::CreateLambda([Visible = MoveTemp(Visible)]() { return Visible() ? EVisibility::Visible : EVisibility::Collapsed; }));
	return Button;
}
