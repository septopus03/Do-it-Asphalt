// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"

struct FSlateBrush;

/**
 * The pictures of the things in the player's hand (/Game/Icons/ICON_<Name>, imported by Tools/UE/import_icons.py from
 * SourceAssets/Icons, made by Tools/ModelGen/_icons.py). The hotbar and the shop's "in hand" strip draw them. A picture that has
 * not been imported draws nothing, so the interface works without it.
 */
namespace DIAIcons
{
	/** The size the icons are drawn at, in Slate units (the pictures are 256 x 256). */
	inline constexpr float HandIconSize = 40.f;

	/** The asset name of the picture of a hand item, e.g. "ICON_Bucket". */
	DOITASPHALT_API const TCHAR* HandIconName(ELotHand Hand);

	/** The brush of a hand item's picture; never null, owned by the module. */
	DOITASPHALT_API const FSlateBrush* HandBrush(ELotHand Hand);
}
