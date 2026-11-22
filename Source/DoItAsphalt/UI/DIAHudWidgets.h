// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAHudFont.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace AsphaltHud
{
	/** The colour with its alpha multiplied by Alpha (clamped to 0..1). */
	inline FLinearColor Faded(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * FMath::Clamp(Alpha, 0.f, 1.f));
	}

	/**
	 * A sign plate: a rounded plate with a thin inset ring line, holding Content. Three nested borders (plate, ring, plate); every
	 * layer's alpha is multiplied by Opacity.
	 */
	inline TSharedRef<SWidget> MakePlate(TAttribute<FLinearColor> Plate, TAttribute<FLinearColor> Ring, const FMargin& Padding, const TSharedRef<SWidget>& Content, float RingWidth = 3.f, TAttribute<float> Opacity = 1.f)
	{
		static const FSlateRoundedBoxBrush OuterBrush(FLinearColor::White, 6.f);
		static const FSlateRoundedBoxBrush RingBrush(FLinearColor::White, 3.f);
		static const FSlateRoundedBoxBrush InnerBrush(FLinearColor::White, 1.f);

		const TAttribute<FSlateColor> PlateColor = TAttribute<FSlateColor>::CreateLambda([Plate, Opacity]() { return FSlateColor(Faded(Plate.Get(), Opacity.Get())); });
		const TAttribute<FSlateColor> RingColor = TAttribute<FSlateColor>::CreateLambda([Ring, Opacity]() { return FSlateColor(Faded(Ring.Get(), Opacity.Get())); });

		return SNew(SBorder)
			.BorderImage(&OuterBrush)
			.BorderBackgroundColor(PlateColor)
			.Padding(FMargin(RingWidth))
			[
				SNew(SBorder)
				.BorderImage(&RingBrush)
				.BorderBackgroundColor(RingColor)
				.Padding(FMargin(RingWidth))
				[
					SNew(SBorder)
					.BorderImage(&InnerBrush)
					.BorderBackgroundColor(PlateColor)
					.Padding(Padding)
					[
						Content
					]
				]
			];
	}

	/** Upper-case text in the HUD typeface, without a shadow. */
	inline TSharedRef<STextBlock> MakeText(TAttribute<FText> Text, EDIAFontWeight Weight, int32 Size, TAttribute<FSlateColor> Color)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Weight, Size))
			.ColorAndOpacity(Color)
			.TransformPolicy(ETextTransformPolicy::ToUpper);
	}

	/** A black sign button of the given width: the one button builder of the menu and the win screen. The label is yellow while bPrimary says so, otherwise white. */
	inline TSharedRef<SBox> MakeMenuButton(const FText& Label, float Width, FOnClicked OnClicked, const FButtonStyle* ButtonStyle, TAttribute<bool> bPrimary = false)
	{
		const TAttribute<FSlateColor> LabelColor = TAttribute<FSlateColor>::CreateLambda([bPrimary]() { return FSlateColor(bPrimary.Get() ? SignYellow : SignWhite); });
		return SNew(SBox).WidthOverride(Width).HeightOverride(MenuButtonHeight)
			[
				SNew(SButton)
				.ButtonStyle(ButtonStyle)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(0.f))
				.OnClicked(OnClicked)
				[
					MakeText(Label, EDIAFontWeight::ExtraBold, MenuButtonFont, LabelColor)
				]
			];
	}

	/** A dimmed backdrop over the whole screen with the content on a yellow sign plate (ring 4) in the middle: the scaffold of the win screen and the menu. */
	inline TSharedRef<SOverlay> MakeScreenPanel(float ContentWidth, const TSharedRef<SWidget>& Content)
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
				MakePlate(SignYellow, SignBlack, FMargin(ScreenPlatePadH, ScreenPlatePadTop, ScreenPlatePadH, ScreenPlatePadBottom),
					SNew(SBox).WidthOverride(ContentWidth)
					[
						Content
					], ScreenRingWidth)
			];
	}
}
