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
	/** The shop panel lies above the HUD. */
	inline constexpr int32 ShopZOrder = 11;
	/** The win screen lies above everything. */
	inline constexpr int32 WinZOrder = 12;
	/** The main and pause menu lie above everything. */
	inline constexpr int32 MenuZOrder = 13;

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
	inline const FLinearColor ShopPanelColor{0.04f, 0.05f, 0.07f, 0.94f};
	inline const FLinearColor ShopRowColor{1.f, 1.f, 1.f, 0.07f};
	inline const FLinearColor ButtonColor{1.f, 0.62f, 0.15f, 1.f};
	inline const FLinearColor ButtonHoverColor{1.f, 0.74f, 0.36f, 1.f};
	inline const FLinearColor ButtonPressedColor{0.82f, 0.46f, 0.08f, 1.f};
	inline const FLinearColor ButtonDisabledColor{1.f, 1.f, 1.f, 0.10f};
	inline const FLinearColor WinBackdropColor{0.f, 0.f, 0.f, 0.72f};
	inline const FLinearColor WinTitleColor{1.f, 0.62f, 0.15f, 1.f};
	inline const FLinearColor ButtonTextColor{0.08f, 0.06f, 0.03f, 1.f};
	inline const FLinearColor ButtonTextDisabledColor{0.78f, 0.80f, 0.84f, 0.45f};

	/** Offset of the text shadow. */
	inline const FVector2D ShadowOffset{1.0, 1.0};

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
	/** Columns of the shop: name, change (current -> next), price, button. */
	inline constexpr float ShopNameWidth = 170.f;
	inline constexpr float ShopChangeWidth = 430.f;
	inline constexpr float ShopPriceWidth = 150.f;
	inline constexpr float ShopButtonWidth = 120.f;
	inline constexpr float ShopRowSpacing = 10.f;
	inline constexpr float ShopPadding = 26.f;
	inline constexpr float ButtonCornerRadius = 8.f;
	/** Corner radii: the big panels (shop, win screen), their rows, and the smaller HUD panels (stats, toasts). */
	inline constexpr float PanelCornerRadius = 12.f;
	inline constexpr float RowCornerRadius = 8.f;
	inline constexpr float HudPanelCornerRadius = 8.f;
	/** The win screen: the panel's inner width, the statistics' label column and the two buttons. */
	inline constexpr float WinPanelWidth = 640.f;
	inline constexpr float WinLabelWidth = 320.f;
	inline constexpr float WinTitleSpacing = 28.f;
	inline constexpr float WinRowSpacing = 8.f;
	inline constexpr float WinButtonsSpacing = 28.f;
	inline constexpr float WinButtonWidth = 240.f;
	inline constexpr float WinButtonPaddingV = 10.f;
	inline constexpr float WinPanelPadding = 36.f;
	/** The menu: the panel's inner width, the title's distance to the buttons, the gap between buttons, and the width of a button. */
	inline constexpr float MenuPanelWidth = 520.f;
	inline constexpr float MenuTitleSpacing = 30.f;
	inline constexpr float MenuButtonSpacing = 14.f;
	inline constexpr float MenuButtonWidth = 300.f;
	inline constexpr float MenuQuestionButtonWidth = 140.f;
	/** Distance of the toasts from the top edge. */
	inline constexpr float ToastTop = 110.f;
}
