// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Every colour, size and timing of the HUD in one place. Slate units (the viewport's DPI scaling applies on top). */
namespace AsphaltHud
{
	// Behaviour.
	/** The plant's stock is shown while the player is within this distance of a refill point. */
	inline constexpr double StockRadiusCm = 600.0;
	/** A toast stays fully visible this long, then fades out. */
	inline constexpr float ToastHoldSeconds = 2.0f;
	inline constexpr float ToastFadeSeconds = 0.5f;
	/** Toasts shown at once; a burst drops the oldest. */
	inline constexpr int32 MaxToasts = 3;
	/** Z order of the overlay in the game viewport. */
	inline constexpr int32 ViewportZOrder = 10;

	// Colours.
	inline const FLinearColor TextColor{0.97f, 0.97f, 0.97f, 1.f};
	inline const FLinearColor CaptionColor{0.78f, 0.80f, 0.84f, 1.f};
	inline const FLinearColor ShadowColor{0.f, 0.f, 0.f, 0.85f};
	inline const FLinearColor PanelColor{0.04f, 0.05f, 0.07f, 0.62f};
	inline const FLinearColor MoneyColor{1.f, 0.86f, 0.30f, 1.f};
	inline const FLinearColor ShardColor{0.45f, 0.85f, 1.f, 1.f};
	inline const FLinearColor ProgressColor{0.55f, 0.88f, 0.45f, 1.f};
	inline const FLinearColor BackpackColor{1.f, 0.62f, 0.15f, 1.f};
	inline const FLinearColor StockColor{0.80f, 0.80f, 0.84f, 1.f};
	inline const FLinearColor BarTrackColor{1.f, 1.f, 1.f, 0.16f};
	inline const FLinearColor PromptColor{1.f, 0.93f, 0.55f, 1.f};

	// Sizes.
	inline constexpr int32 BigFontSize = 34;
	inline constexpr int32 MediumFontSize = 26;
	inline constexpr int32 CaptionFontSize = 17;
	inline constexpr float ScreenMargin = 24.f;
	inline constexpr float PanelPaddingH = 18.f;
	inline constexpr float PanelPaddingV = 10.f;
	inline constexpr float BackpackBarWidth = 360.f;
	inline constexpr float StockBarWidth = 260.f;
	inline constexpr float BarHeight = 12.f;
	inline constexpr float ProgressBarWidth = 300.f;
	/** Distance from the bottom edge of the screen to the lowest widget. */
	inline constexpr float BottomMargin = 28.f;
	/** Distance of the toasts from the top edge. */
	inline constexpr float ToastTop = 110.f;
}
