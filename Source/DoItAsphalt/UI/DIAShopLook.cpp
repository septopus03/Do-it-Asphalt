// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAShopLook.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"

FDIAShopLook AsphaltHud::ShopLook(EDIAShopState State)
{
	FDIAShopLook Look;
	switch (State)
	{
	case EDIAShopState::Buy:
		Look.Plate = SignYellow;
		Look.Ring = SignBlack;
		Look.Text = SignBlack;
		Look.TileFill = SignBlack;
		Look.TileText = SignYellow;
		Look.Price = SignBlack;
		Look.bShowsButton = true;
		Look.bButtonEnabled = true;
		Look.ButtonLabel = NSLOCTEXT("DIAHud", "ShopBuy", "BUY");
		break;
	case EDIAShopState::NoCredits:
		Look.Plate = SignCardLack;
		Look.Ring = Faded(SignWhite, 0.45f);
		Look.Text = SignWhite;
		Look.TileFill = SignWhite;
		Look.TileText = SignBlack;
		Look.Price = SignLackPrice;
		Look.bShowsButton = true;
		Look.ButtonLabel = NSLOCTEXT("DIAHud", "ShopBuy", "BUY");
		break;
	case EDIAShopState::Locked:
		Look.Plate = SignCardLocked;
		Look.Ring = Faded(SignWhite, 0.18f);
		Look.Text = SignDimText;
		Look.TileFill = Faded(SignWhite, 0.22f);
		Look.TileText = SignWhite;
		Look.Price = SignDimText;
		Look.bShowsButton = true;
		Look.ButtonLabel = NSLOCTEXT("DIAHud", "ShopLocked", "LOCKED");
		break;
	case EDIAShopState::Owned:
	default:
		Look.Plate = SignBlack;
		Look.Ring = SignWhite;
		Look.Text = SignWhite;
		Look.TileFill = SignWhite;
		Look.TileText = SignBlack;
		Look.Price = SignWhite;
		break;
	}
	return Look;
}
