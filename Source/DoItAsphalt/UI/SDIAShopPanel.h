// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"

class ADIAHUD;
class UDIAWorldSubsystem;

/**
 * The buy button. A double click is swallowed: Slate would deliver its second press as an ordinary press, and the button
 * would click twice (SButton::OnMouseButtonDoubleClick treats it as a single click). Here the second press never starts, so the
 * second release finds no press and clicks nothing: one click sequence buys at most one thing.
 */
class SDIAShopButton : public SButton
{
public:
	using FArguments = SButton::FArguments;

	void Construct(const FArguments& InArgs) { SButton::Construct(InArgs); }

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
};

/**
 * The shop: a black panel with a hazard-striped top edge and a grid of sign plates for the plant and the ten items. A card is a
 * plate in the colours of its state (AsphaltHud::ShopLook) with a round tile, the name, what it gives, the price and a button
 * (Buy, Locked with the reason); an item that is owned stays where it is and shows OWNED with a tick instead of the button.
 * The widget tree is built once; the texts and the buttons' state are read from
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

	/** Reads the game again and forgets a press that is still held; the HUD calls it when the shop opens. */
	void Refresh();

private:
	void HandleStateChanged();
	void HandlePressed(int32 Row);
	FReply HandleBuy(int32 Row);
	TSharedRef<SWidget> BuildCard(int32 Row);

	TWeakObjectPtr<ADIAHUD> Hud;
	TWeakObjectPtr<UDIAWorldSubsystem> Subsystem;
	FDelegateHandle StateHandle;

	FDIAShopModel Model;
	/** What the button that is held down would buy, named by item (or plant level), not by row: the rows move when something is bought. */
	FDIAShopTarget Pressed;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, AsphaltHud::ShopPanelRadius};
	/** The round tile in front of a card's name. */
	FSlateRoundedBoxBrush TileBrush{FLinearColor::White, AsphaltHud::ShopIconSize / 2.f};
	FButtonStyle ButtonStyle;
};
