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
}
