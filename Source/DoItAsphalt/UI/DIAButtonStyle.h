// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/DIAHudStyle.h"

namespace AsphaltHud
{
	/** The orange rounded button of the shop and the win screen (the style keeps its own copies of the brushes). */
	inline FButtonStyle MakeButtonStyle()
	{
		return FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(ButtonColor, ButtonCornerRadius))
			.SetHovered(FSlateRoundedBoxBrush(ButtonHoverColor, ButtonCornerRadius))
			.SetPressed(FSlateRoundedBoxBrush(ButtonPressedColor, ButtonCornerRadius))
			.SetDisabled(FSlateRoundedBoxBrush(ButtonDisabledColor, ButtonCornerRadius))
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
	}

	/** The sign button: black, a lighter hover and a darker press; disabled is see-through with a thin white outline. */
	inline FButtonStyle MakeSignButtonStyle()
	{
		const FSlateRoundedBoxBrush Disabled(FLinearColor::Transparent, 4.f, FLinearColor(FColor(0xFF, 0xFF, 0xFF, 0x66)), 2.f);
		return FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(SignBlack, 4.f))
			.SetHovered(FSlateRoundedBoxBrush(SignButtonHover, 4.f))
			.SetPressed(FSlateRoundedBoxBrush(SignButtonPressed, 4.f))
			.SetDisabled(Disabled)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
	}
}
