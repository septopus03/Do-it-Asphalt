// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Every colour, size and timing of the HUD in one place. Slate units (the viewport's DPI scaling applies on top). */
namespace AsphaltHud
{
	// Behaviour.
	/** A toast stays fully visible this long, then fades out. */
	inline constexpr float ToastHoldSeconds = 2.0f;
	inline constexpr float ToastFadeSeconds = 0.5f;
	/** Toasts shown at once; a burst drops the oldest. */
	inline constexpr int32 MaxToasts = 3;
	/** Z order of the overlay in the game viewport. */
	inline constexpr int32 ViewportZOrder = 10;
	/** The shop panel lies above the HUD. */
	inline constexpr int32 ShopZOrder = 11;
	/** The check view looks for the nearest unfinished cell at most this often (the search scans every cell). */
	inline constexpr float CheckRefreshSeconds = 0.5f;
	/** The win screen lies above everything. */
	inline constexpr int32 WinZOrder = 12;
	/** The main and pause menu lie above everything. */
	inline constexpr int32 MenuZOrder = 13;

	// Colours.
	inline const FLinearColor TextColor{0.97f, 0.97f, 0.97f, 1.f};
	inline const FLinearColor CaptionColor{0.78f, 0.80f, 0.84f, 1.f};
	inline const FLinearColor ShadowColor{0.f, 0.f, 0.f, 0.85f};
	inline const FLinearColor PanelColor{0.04f, 0.05f, 0.07f, 0.62f};
	inline const FLinearColor CreditsColor{1.f, 0.86f, 0.30f, 1.f};
	inline const FLinearColor ProgressColor{0.55f, 0.88f, 0.45f, 1.f};
	inline const FLinearColor BarTrackColor{1.f, 1.f, 1.f, 0.16f};
	inline const FLinearColor PromptColor{1.f, 0.93f, 0.55f, 1.f};
	inline const FLinearColor ShopPanelColor{0.04f, 0.05f, 0.07f, 0.94f};
	inline const FLinearColor ShopRowColor{1.f, 1.f, 1.f, 0.07f};
	/** Price colours of a row that is owned, or locked (dim), and of one that is on offer but too expensive. */
	inline const FLinearColor ShopDimColor{0.78f, 0.80f, 0.84f, 0.55f};
	inline const FLinearColor ShopLackColor{1.f, 0.45f, 0.40f, 1.f};
	inline const FLinearColor ButtonColor{1.f, 0.62f, 0.15f, 1.f};
	inline const FLinearColor ButtonHoverColor{1.f, 0.74f, 0.36f, 1.f};
	inline const FLinearColor ButtonPressedColor{0.82f, 0.46f, 0.08f, 1.f};
	inline const FLinearColor ButtonDisabledColor{1.f, 1.f, 1.f, 0.10f};
	inline const FLinearColor ScreenBackdropColor{0.f, 0.f, 0.f, 0.72f};
	inline const FLinearColor ScreenTitleColor{1.f, 0.62f, 0.15f, 1.f};
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
	inline constexpr float BarHeight = 12.f;
	inline constexpr float ProgressBarWidth = 300.f;
	/** Distance from the bottom edge of the screen to the lowest widget. */
	inline constexpr float BottomMargin = 28.f;
	inline constexpr float ButtonCornerRadius = 8.f;
	/** Corner radii: the big panels (win screen, menu), their rows, and the smaller HUD panels (stats, toasts). */
	inline constexpr float PanelCornerRadius = 12.f;
	inline constexpr float RowCornerRadius = 8.f;
	inline constexpr float HudPanelCornerRadius = 8.f;
	/** The full-screen panels (win screen and menu): the padding inside the panel and above and below a button's text. */
	inline constexpr float ScreenPanelPadding = 36.f;
	inline constexpr float ScreenButtonPaddingV = 10.f;
	/** The win screen: the panel's inner width, the statistics' label column and the two buttons. */
	inline constexpr float WinPanelWidth = 640.f;
	inline constexpr float WinLabelWidth = 320.f;
	inline constexpr float WinTitleSpacing = 28.f;
	inline constexpr float WinRowSpacing = 8.f;
	inline constexpr float WinButtonsSpacing = 28.f;
	inline constexpr float WinButtonWidth = 240.f;
	/** The menu: the panel's inner width, the title's distance to the buttons, the gap between buttons, and the width of a button. */
	inline constexpr float MenuPanelWidth = 520.f;
	inline constexpr float MenuTitleSpacing = 30.f;
	inline constexpr float MenuButtonSpacing = 14.f;
	inline constexpr float MenuButtonWidth = 300.f;
	inline constexpr float MenuQuestionButtonWidth = 140.f;
	/** The check view's arrow and text, and their distance from the top edge. */
	inline const FLinearColor CheckColor{1.f, 0.62f, 0.15f, 1.f};
	inline constexpr int32 CheckArrowFontSize = 48;
	inline constexpr float CheckTop = 250.f;
	/** The shop: the font of a row, the columns (name with its reason, value, price, button), the gaps, the padding and the tallest the list grows. */
	inline constexpr int32 ShopFontSize = 22;
	inline constexpr float ShopNameWidth = 260.f;
	inline constexpr float ShopValueWidth = 300.f;
	inline constexpr float ShopPriceWidth = 100.f;
	inline constexpr float ShopButtonWidth = 120.f;
	inline constexpr float ShopRowSpacing = 6.f;
	inline constexpr float ShopPadding = 26.f;
	inline constexpr float ShopMaxListHeight = 760.f;
	/** After a purchase the shop refuses another for this long: a double click is one click sequence and buys one thing. */
	inline constexpr float ShopClickGuardSeconds = 0.4f;
	/** Distance of the toasts from the top edge. */
	inline constexpr float ToastTop = 110.f;
}
