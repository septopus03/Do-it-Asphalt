// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** The shared colours, sizes and timings of the HUD (most paddings and two gauge-track colours live in the widgets). Slate units (the viewport's DPI scaling applies on top). */
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
	inline const FLinearColor ScreenBackdropColor{0.f, 0.f, 0.f, 0.72f};

	// Hazard signage palette: yellow and black signs. Written as sRGB bytes; Slate blends gamma-encoded values.
	inline const FLinearColor SignYellow{FColor(0xFF, 0xC6, 0x1A, 0xFF)};
	inline const FLinearColor SignBlack{FColor(0x11, 0x12, 0x14, 0xFF)};
	inline const FLinearColor SignWhite{FColor(0xFF, 0xFF, 0xFF, 0xFF)};
	inline const FLinearColor SignOrange{FColor(0xFF, 0x7A, 0x1A, 0xFF)};
	inline const FLinearColor SignOrangeText{FColor(0xFF, 0x9A, 0x4D, 0xFF)};
	inline const FLinearColor SignLackPrice{FColor(0xFF, 0x9A, 0x80, 0xFF)};
	inline const FLinearColor SignCardLack{FColor(0x2C, 0x2D, 0x31, 0xFF)};
	inline const FLinearColor SignCardLocked{FColor(0x1D, 0x1E, 0x21, 0xFF)};
	inline const FLinearColor SignStripeDark{FColor(0x4A, 0x3F, 0x14, 0xFF)};
	inline const FLinearColor SignButtonHover{FColor(0x2C, 0x2D, 0x31, 0xFF)};
	inline const FLinearColor SignButtonPressed{FColor(0x00, 0x00, 0x00, 0xFF)};
	/** Secondary text: white at 70 % (lies over a dark surface; see Over). */
	inline const FLinearColor SignDimText{1.f, 1.f, 1.f, 0.70f};

	/** Top laid over the opaque Bottom by Top.A, blended in sRGB space (as Slate does); the result is opaque. */
	inline FLinearColor Over(const FLinearColor& Top, const FLinearColor& Bottom)
	{
		const FColor T = Top.ToFColor(true);
		const FColor B = Bottom.ToFColor(true);
		const float A = FMath::Clamp(Top.A, 0.f, 1.f);
		const auto Mix = [A](uint8 Tc, uint8 Bc) { return static_cast<uint8>(FMath::RoundToInt(Tc * A + Bc * (1.f - A))); };
		return FLinearColor(FColor(Mix(T.R, B.R), Mix(T.G, B.G), Mix(T.B, B.B), 255));
	}

	/** WCAG contrast ratio of two colours (1 to 21), from their linear components. */
	inline float ContrastRatio(const FLinearColor& A, const FLinearColor& B)
	{
		const auto Luminance = [](const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; };
		const float La = Luminance(A);
		const float Lb = Luminance(B);
		return (FMath::Max(La, Lb) + 0.05f) / (FMath::Min(La, Lb) + 0.05f);
	}

	// Sizes.
	inline constexpr float ScreenMargin = 28.f;
	inline constexpr float BarHeight = 18.f;
	inline constexpr float ProgressBarWidth = 440.f;
	/** Distance from the bottom edge of the screen to the lowest widget. */
	inline constexpr float BottomMargin = 26.f;
	/** The win screen's two buttons. */
	inline constexpr float WinButtonWidth = 230.f;
	/** The question buttons of the menu. */
	inline constexpr float MenuQuestionButtonWidth = 170.f;
	/** The crosshair in the middle of the screen: a white dot (inner) in a dark ring (outer), in pixels. */
	inline constexpr float CrosshairOuterSize = 10.f;
	inline constexpr float CrosshairInnerSize = 5.f;
	inline const FLinearColor CrosshairColor{1.f, 1.f, 1.f, 0.95f};
	inline const FLinearColor CrosshairRingColor{0.f, 0.f, 0.f, 0.6f};
	/** The check view's arrow and text, and their distance from the top edge. */
	inline constexpr int32 CheckArrowFontSize = 36;
	inline constexpr float CheckTop = 268.f;
	/** The shop: a grid of cards. Columns, the height of a card, the gap, the tile and the button. */
	inline constexpr int32 ShopColumns = 4;
	inline constexpr float ShopCardHeight = 172.f;
	inline constexpr float ShopCardGap = 12.f;
	inline constexpr float ShopIconSize = 46.f;
	inline constexpr float ShopButtonWidth = 108.f;
	/** The inventory bar at the bottom of the screen: the size of a slot, the gap between slots, the height of a slot's fill gauge. */
	inline constexpr float HotbarSlotWidth = 140.f;
	inline constexpr float HotbarSlotHeight = 94.f;
	inline constexpr float HotbarGap = 10.f;
	inline constexpr float HotbarGaugeHeight = 9.f;
	inline constexpr int32 HotbarNameFontSize = 20;
	/** Distance of the toasts from the top edge. */
	inline constexpr float ToastTop = 110.f;

	// The signage HUD (the mockup's CSS pixels are Slate units; font sizes are CSS px x 0.75).
	/** The width of the ring line of a plate. */
	inline constexpr float HudRingWidth = 3.f;
	/** Font sizes (Slate points) of the HUD texts. */
	inline constexpr int32 HudCreditsFont = 46;
	inline constexpr int32 HudCreditsCaptionFont = 17;
	inline constexpr int32 HudProgressFont = 33;
	inline constexpr int32 HudProgressCaptionFont = 16;
	inline constexpr int32 HudNextTagFont = 14;
	inline constexpr int32 HudNextFont = 17;
	inline constexpr int32 HudToastFont = 24;
	inline constexpr int32 HudCheckFont = 24;
	inline constexpr int32 HudRefillCaptionFont = 16;
	inline constexpr int32 HudRefillFont = 23;
	inline constexpr int32 HudPromptFont = 26;
	inline constexpr int32 HudHandFont = 20;
	inline constexpr int32 HotbarKeyFont = 15;
	/** The toast's text wraps at this width. */
	inline constexpr float ToastWrapWidth = 900.f;
	inline constexpr float ToastGap = 8.f;
	/** The check view's diamond: its side, the ring width of its plate, and the gap to the text plate. */
	inline constexpr float CheckDiamondSize = 74.f;
	inline constexpr float CheckDiamondRing = 4.f;
	inline constexpr float CheckGap = 30.f;
	/** The progress bar: the width of its yellow frame and the band of the stripes in its track. */
	inline constexpr float ProgressFrameWidth = 2.f;
	inline constexpr float ProgressStripeBand = 8.f;
	/** The stripes under the inventory bar: height and band. */
	inline constexpr float HotbarStripeHeight = 12.f;
	inline constexpr float HotbarStripeBand = 12.f;
	/** The prompt's key plate is this wide. */
	inline constexpr float PromptKeyWidth = 58.f;
	/** The gap between the stacked plates at the bottom of the screen. */
	inline constexpr float BottomStackGap = 10.f;
	inline constexpr float PromptLineGap = 8.f;
	/** The key square in a hotbar slot. */
	inline constexpr float HotbarKeySize = 26.f;

	// The signage shop (CSS pixels of the mockup are Slate units; font sizes are CSS px x 0.75).
	/** The panel: its width, its corner radius, and the height and band of the stripes along its top edge. */
	inline constexpr float ShopPanelWidth = 1300.f;
	inline constexpr float ShopPanelRadius = 8.f;
	inline constexpr float ShopStripeHeight = 20.f;
	inline constexpr float ShopStripeBand = 16.f;
	/** The panel's padding (top, sides, bottom) and the gap between its header, grid and hint. */
	inline constexpr float ShopContentTop = 22.f;
	inline constexpr float ShopContentSide = 36.f;
	inline constexpr float ShopContentBottom = 26.f;
	inline constexpr float ShopSectionGap = 22.f;
	/** A card: the padding of its content (CSS, measured from inside the border), the gap between the tile and the text, the button height. */
	inline constexpr float ShopCardPaddingH = 18.f;
	inline constexpr float ShopCardPaddingV = 16.f;
	inline constexpr float ShopTileGap = 12.f;
	inline constexpr float ShopButtonHeight = 48.f;
	/** The tick in front of OWNED: the side of its box (the tick itself is drawn in a 24-unit square) and its line width. */
	inline constexpr float ShopTickSize = 26.f;
	inline constexpr float ShopTickLine = 4.f;
	/** Font sizes of the shop. */
	inline constexpr int32 ShopTitleFont = 57;
	inline constexpr int32 ShopSubtitleFont = 17;
	inline constexpr int32 ShopCreditsFont = 44;
	inline constexpr int32 ShopCreditsCaptionFont = 17;
	inline constexpr int32 ShopNameFont = 22;
	inline constexpr int32 ShopValueTextFont = 16;
	inline constexpr int32 ShopTileFont = 21;
	inline constexpr int32 ShopPriceFont = 26;
	inline constexpr int32 ShopBuyFont = 20;
	inline constexpr int32 ShopOffButtonFont = 17;
	inline constexpr int32 ShopOwnedFont = 21;
	inline constexpr int32 ShopHintFont = 17;

	// The signage menu and win screen (CSS pixels of the mockup are Slate units; font sizes are CSS px x 0.75, the brief wins where it names one).
	/** The plate of a screen: the width of its ring, and the padding inside it (the mockup's padding less one ring width, as in the HUD). */
	inline constexpr float ScreenRingWidth = 4.f;
	inline constexpr float ScreenPlatePadH = 36.f;
	inline constexpr float ScreenPlatePadTop = 30.f;
	inline constexpr float ScreenPlatePadBottom = 40.f;
	/** The menu: the inner width of its plate, the gap between buttons and between the title and the first button, and the button height. */
	inline constexpr float MenuContentWidth = 352.f;
	inline constexpr float MenuGap = 12.f;
	inline constexpr float MenuButtonHeight = 64.f;
	inline constexpr int32 MenuTitleFont = 60;
	inline constexpr int32 MenuQuestionFont = 20;
	inline constexpr int32 MenuButtonFont = 24;
	/** The settings: the caption and the value on one line, and the slider below (its height, the bar and the handle). */
	inline constexpr int32 MenuSettingCaptionFont = 20;
	inline constexpr int32 MenuSettingValueFont = 30;
	inline constexpr float MenuSettingSliderHeight = 44.f;
	inline constexpr float SliderBarThickness = 8.f;
	inline constexpr float SliderThumbWidth = 14.f;
	inline constexpr float SliderThumbHeight = 34.f;
	/** The win screen: the inner width of its plate, the stripes along the top, the title, the rows and the buttons. */
	inline constexpr float WinContentWidth = 472.f;
	inline constexpr float WinStripeHeight = 20.f;
	inline constexpr float WinStripeBand = 16.f;
	inline constexpr int32 WinTitleFont = 48;
	inline constexpr int32 WinLabelFont = 20;
	inline constexpr int32 WinValueFont = 26;
	inline constexpr float WinRowLine = 3.f;
	inline constexpr float WinRowPadV = 9.f;
	inline constexpr float WinTitleGapTop = 26.f;
	inline constexpr float WinTitleGapBottom = 18.f;
	inline constexpr float WinButtonsGapTop = 14.f;
	inline constexpr float WinButtonsPadTop = 20.f;
}
