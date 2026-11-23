// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Sound/SlateSound.h"
#include "Sound/SoundBase.h"
#include "Styling/SlateTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UI/DIAHudStyle.h"

namespace AsphaltHud
{
	/** The sign button: black, a lighter hover and a darker press; disabled is see-through with a thin white outline. */
	inline FButtonStyle MakeSignButtonStyle()
	{
		const FSlateRoundedBoxBrush Disabled(FLinearColor::Transparent, 4.f, FLinearColor(FColor(0xFF, 0xFF, 0xFF, 0x66)), 2.f);
		// A press clicks (when the sound has been imported; a missing one is silent).
		FSlateSound Click;
		if (USoundBase* Wave = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SW_Click"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Click.SetResourceObject(Wave);
		}
		return FButtonStyle()
			.SetPressedSound(Click)
			.SetNormal(FSlateRoundedBoxBrush(SignBlack, 4.f))
			.SetHovered(FSlateRoundedBoxBrush(SignButtonHover, 4.f))
			.SetPressed(FSlateRoundedBoxBrush(SignButtonPressed, 4.f))
			.SetDisabled(Disabled)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
	}

	/** The slider of the settings: a black bar and a black handle. */
	inline FSliderStyle MakeSliderStyle()
	{
		const FSlateRoundedBoxBrush Bar(SignBlack, SliderBarThickness * 0.5f);
		const FSlateRoundedBoxBrush Thumb(SignBlack, 4.f, FVector2f(SliderThumbWidth, SliderThumbHeight));
		return FSliderStyle()
			.SetNormalBarImage(Bar)
			.SetHoveredBarImage(Bar)
			.SetDisabledBarImage(Bar)
			.SetNormalThumbImage(Thumb)
			.SetHoveredThumbImage(Thumb)
			.SetDisabledThumbImage(Thumb)
			.SetBarThickness(SliderBarThickness);
	}
}
