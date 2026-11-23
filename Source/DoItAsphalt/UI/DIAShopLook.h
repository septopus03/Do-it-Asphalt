// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/DIAFormat.h"

/** How a shop card looks in one of its states: the colours of the sign plate and what the button shows. */
struct FDIAShopLook
{
	/** The plate of the card and the ring line inside it. */
	FLinearColor Plate;
	FLinearColor Ring;
	/** The name and the value; may carry an alpha (it lies over Plate). */
	FLinearColor Text;
	/** The round tile in front of the name and the letter on it; the fill may carry an alpha (it lies over Plate). */
	FLinearColor TileFill;
	FLinearColor TileText;
	FLinearColor Price;
	/** Whether the button is shown (an owned card shows OWNED instead), and whether it can be pressed. */
	bool bShowsButton = false;
	bool bButtonEnabled = false;
	FText ButtonLabel;
};

namespace AsphaltHud
{
	/** The look of a card in the given state. */
	FDIAShopLook ShopLook(EDIAShopState State);
}
