// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAHudOverlay.h"
#include "Game/DIAWorldSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	/** A horizontal bar: dim track and a fill whose width follows Fraction (0..1). */
	TSharedRef<SWidget> MakeBar(TAttribute<float> Fraction, const FLinearColor& Fill, float Width)
	{
		return SNew(SBox)
			.WidthOverride(Width)
			.HeightOverride(AsphaltHud::BarHeight)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(AsphaltHud::BarTrackColor)
				]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride_Lambda([Fraction, Width]() { return FMath::Clamp(Fraction.Get(), 0.f, 1.f) * Width; })
					[
						SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Fill)
					]
				]
			];
	}
}

void SDIAHudOverlay::Construct(const FArguments& InArgs)
{
	Hud = InArgs._Hud;
	Subsystem = InArgs._Subsystem;

	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
		StateHandle = S->OnStateChanged.AddSP(this, &SDIAHudOverlay::HandleStateChanged);
	}

	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(AsphaltHud::ScreenMargin)
		[
			BuildTopLeft()
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(AsphaltHud::ScreenMargin)
		[
			BuildTopRight()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, AsphaltHud::ToastTop, 0.f, 0.f)
		[
			BuildToasts()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, AsphaltHud::BottomMargin)
		[
			BuildBottom()
		]
	];
}

void SDIAHudOverlay::Unbind()
{
	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		S->OnStateChanged.Remove(StateHandle);
	}
	StateHandle.Reset();
}

void SDIAHudOverlay::HandleStateChanged()
{
	// Fires every tick; the model builds new texts only when a shown value changed.
	if (const UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
	}
}

TSharedRef<SWidget> SDIAHudOverlay::BuildTopLeft()
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(AsphaltHud::PanelColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Credits; }), AsphaltHud::BigFontSize, AsphaltHud::CreditsColor)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f, 0.f, 0.f, 4.f)
			[
				AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "CreditsCaption", "credits"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildTopRight()
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(AsphaltHud::PanelColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Progress; }), AsphaltHud::BigFontSize, AsphaltHud::TextColor)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ProgressCaption", "cells paved"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 6.f, 0.f, 0.f)
			[
				MakeBar(TAttribute<float>::CreateLambda([this]() { return Model.GetView().ProgressFraction; }), AsphaltHud::ProgressColor, AsphaltHud::ProgressBarWidth)
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildBottom()
{
	TWeakObjectPtr<ADIAHUD> WeakHud = Hud;
	return SNew(SVerticalBox)
		// Contextual prompt (hidden while empty).
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::PanelColor)
			.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
			.Visibility_Lambda([WeakHud]() { return WeakHud.IsValid() && !WeakHud->GetPrompt().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([WeakHud]() { return WeakHud.IsValid() ? WeakHud->GetPrompt() : FText::GetEmpty(); }), AsphaltHud::MediumFontSize, AsphaltHud::PromptColor)
			]
		]
		;
}

TSharedRef<SWidget> SDIAHudOverlay::BuildToasts()
{
	TWeakObjectPtr<ADIAHUD> WeakHud = Hud;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (int32 Index = 0; Index < AsphaltHud::MaxToasts; ++Index)
	{
		auto Alpha = [WeakHud, Index]() { return WeakHud.IsValid() ? WeakHud->GetToasts().GetAlpha(Index) : 0.f; };
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor_Lambda([Alpha]() { FLinearColor C = AsphaltHud::PanelColor; C.A *= Alpha(); return FSlateColor(C); })
			.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
			.Visibility_Lambda([Alpha]() { return Alpha() > 0.f ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(STextBlock)
				.Text_Lambda([WeakHud, Index]() { return WeakHud.IsValid() ? WeakHud->GetToasts().GetText(Index) : FText::GetEmpty(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::MediumFontSize))
				.ColorAndOpacity_Lambda([Alpha]() { FLinearColor C = AsphaltHud::TextColor; C.A = Alpha(); return FSlateColor(C); })
				.ShadowOffset(AsphaltHud::ShadowOffset)
				.ShadowColorAndOpacity_Lambda([Alpha]() { FLinearColor C = AsphaltHud::ShadowColor; C.A *= Alpha(); return C; })
			]
		];
	}
	return Box;
}
