// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace AsphaltHud
{
	/** Bold text with a drop shadow so that it stays legible on any ground: the one text builder of the HUD, the shop and the win screen. */
	inline TSharedRef<STextBlock> MakeShadowedText(TAttribute<FText> Text, int32 Size, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", Size))
			.ColorAndOpacity(Color)
			.ShadowOffset(ShadowOffset)
			.ShadowColorAndOpacity(ShadowColor);
	}

	/** A button of the given width with the bold button label: the one button builder of the win screen and the menu. */
	inline TSharedRef<SBox> MakeMenuButton(const FText& Label, float Width, FOnClicked OnClicked, const FButtonStyle* ButtonStyle)
	{
		return SNew(SBox).WidthOverride(Width)
			[
				SNew(SButton)
				.ButtonStyle(ButtonStyle)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(0.f, ScreenButtonPaddingV))
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", MediumFontSize))
					.ColorAndOpacity(ButtonTextColor)
				]
			];
	}

	/** A dimmed backdrop over the whole screen with the content on a rounded panel in the middle: the scaffold of the win screen and the menu. */
	inline TSharedRef<SOverlay> MakeScreenPanel(const FSlateBrush* PanelBrush, float ContentWidth, const TSharedRef<SWidget>& Content)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(ScreenBackdropColor)
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush)
				.BorderBackgroundColor(ShopPanelColor)
				.Padding(ScreenPanelPadding)
				[
					SNew(SBox).WidthOverride(ContentWidth)
					[
						Content
					]
				]
			];
	}
}
