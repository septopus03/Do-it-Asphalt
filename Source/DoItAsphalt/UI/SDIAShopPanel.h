// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/SCompoundWidget.h"

class ADIAHUD;
class UDIAWorldSubsystem;

/**
 * The shop: a scrolling list of the plant and the eleven items, each with its value, its price and a button (Buy,
 * Locked with the reason, Owned). The widget tree is built once; the texts and the buttons' state are read from
 * FDIAShopModel through attribute lambdas, and the model is refreshed on OnStateChanged but only changes when a shown value
 * changed. The panel is shown and hidden by the HUD (Visible / Collapsed); only the panel itself takes the mouse, the game
 * keeps running underneath. The buttons never take keyboard focus, so Tab keeps closing the shop.
 */
class SDIAShopPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDIAShopPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ADIAHUD>, Hud)
		SLATE_ARGUMENT(TWeakObjectPtr<UDIAWorldSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Stops listening to the game; called before the widget is removed. */
	void Unbind();

	const FDIAShopModel& GetModel() const { return Model; }

private:
	void HandleStateChanged();
	FReply HandleBuy(int32 Row);
	TSharedRef<SWidget> BuildRow(int32 Row);

	TWeakObjectPtr<ADIAHUD> Hud;
	TWeakObjectPtr<UDIAWorldSubsystem> Subsystem;
	FDelegateHandle StateHandle;

	FDIAShopModel Model;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, AsphaltHud::PanelCornerRadius};
	FSlateRoundedBoxBrush RowBrush{FLinearColor::White, AsphaltHud::RowCornerRadius};
	FButtonStyle ButtonStyle;
};
