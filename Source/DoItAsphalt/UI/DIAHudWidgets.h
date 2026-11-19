// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAHudStyle.h"
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
}
