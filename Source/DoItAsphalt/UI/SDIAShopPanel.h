// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
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

	/**
	 * A click as the mouse makes it, press and then release: OnPressed, then OnClicked. A real click needs the hover and the mouse capture
	 * of the Slate application, which a headless test does not have, so tests click through this. For tests only: no production code calls it.
	 * Nothing happens, and false is returned, while the button is disabled (as for the mouse).
	 */
	bool SimulateClick()
	{
		if (!IsEnabled())
		{
			return false;
		}
		Press();
		ExecuteOnClick();
		Release();
		return true;
	}
};

/**
 * The shop: a black panel with a hazard-striped top edge and a grid of sign plates for the goods of one building (Tools, Robots or Garage). The Robots
 * building has its own layout instead of the grid: four columns, the station in the first, and in each of the other three one robot
 * with its Speed and Capacity cards below it (the space of those two is reserved, and empty but for a hint, until the robot is owned). A card is a
 * plate in the colours of its state (AsphaltHud::ShopLook) with a round tile, the name, what it gives, the price and a button
 * (Buy, Locked with the reason); above them a strip chooses which owned tool or vessel is in the hands (only one at a time); an item that is owned stays where it is and shows OWNED with a tick instead of the button.
 * The widget tree is built once; the texts and the buttons' state are read from
 * FDIAShopModel through attribute lambdas, and the model is refreshed on OnStateChanged but only changes when a shown value
 * changed. The panel is shown and hidden by the HUD (Visible / Collapsed); only the panel itself takes the mouse, the game
 * keeps running underneath. The buttons never take keyboard focus, so E and Esc, which close the shop, keep working; E opens it at the buy point of a building (ADIAShopPoint) and the panel closes by itself when the player walks away.
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

	/** Shows one building's goods: the grid is rebuilt with the rows of the shop only, in model order, four to a line, and the title changes. The "in hand" strip is part of Tools only. */
	void SetShop(ELotShop Shop);
	ELotShop GetShop() const { return Shop; }
	/** True while the "IN HAND" strip is shown (the Tools building only). */
	bool IsHandStripShown() const { return Shop == ELotShop::Tools; }
	/** The model rows that are on the grid now, in the order of the cards. */
	const TArray<int32>& GetShownRows() const { return ShownRows; }

	/** Reads the game again and forgets a press that is still held; the HUD calls it when the shop opens. */
	void Refresh();

	/** True while the shop shows the Robots layout (the station and one column per robot) instead of the uniform grid. */
	bool IsRobotsLayout() const { return Shop == ELotShop::Robots; }
	/** The Robots layout: the Speed or Capacity card of a robot as the widget tree has it (Visible while the robot is owned, Hidden but still taking its room otherwise); null outside the layout. */
	TSharedPtr<SWidget> FindUpgradeCard(int32 Paver, ELotPaverUpgrade Upgrade) const;
	/** The Robots layout: the dim hint that stands in the reserved room of a robot that is not owned; null outside the layout. */
	TSharedPtr<SWidget> FindUpgradeHint(int32 Paver) const;
	/** The buy button of the card of a model row; null if the row has no card now. */
	TSharedPtr<SDIAShopButton> FindBuyButton(int32 Row) const;

private:
	void HandleStateChanged();
	void HandlePressed(int32 Row);
	FReply HandleBuy(int32 Row);
	TSharedRef<SWidget> BuildCard(int32 Row, float Height);
	/** The Robots layout from ShownRows. */
	TSharedRef<SWidget> BuildRobotsLayout();
	/** The "in hand" strip above the cards: one button for each tool or vessel that is owned; the one in use is lit. */
	TSharedRef<SWidget> BuildHandSelector();
	FReply HandleSelectHand(ELotHand Hand);

	/** Puts the cards of Shop on the grid. */
	void FillGrid();

	TWeakObjectPtr<ADIAHUD> Hud;
	TWeakObjectPtr<UDIAWorldSubsystem> Subsystem;
	ELotShop Shop = ELotShop::Tools;
	/** The grid of Tools and Garage; the Robots layout is built in its place. */
	TSharedPtr<SUniformGridPanel> Grid;
	/** Holds the grid or the Robots layout. */
	TSharedPtr<SBox> Body;
	TArray<int32> ShownRows;
	/** The buy button of every card that is on the panel, by model row. */
	TMap<int32, TSharedPtr<SDIAShopButton>> BuyButtons;
	/** The Robots layout: each robot's upgrade cards (by the model row of the upgrade) and the hint of each robot. */
	TMap<int32, TSharedPtr<SWidget>> UpgradeCards;
	TSharedPtr<SWidget> UpgradeHints[MaxAutoPavers];
	FDelegateHandle StateHandle;

	FDIAShopModel Model;
	/** What the button that is held down would buy, named by item (or plant level), not by row: the rows move when something is bought. */
	FDIAShopTarget Pressed;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, AsphaltHud::ShopPanelRadius};
	/** The round tile in front of a card's name. */
	FSlateRoundedBoxBrush TileBrush{FLinearColor::White, AsphaltHud::ShopIconSize / 2.f};
	FButtonStyle ButtonStyle;
};
