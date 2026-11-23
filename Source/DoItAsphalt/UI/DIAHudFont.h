// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/** The weights of the HUD's typeface (Barlow Condensed, shipped in Content/Slate/Fonts). */
enum class EDIAFontWeight : uint8
{
	SemiBold,
	Bold,
	ExtraBold
};

namespace AsphaltHud
{
	/** File name of a weight, e.g. "BarlowCondensed-Bold.ttf". */
	FString FontFileName(EDIAFontWeight Weight);

	/** The weight read from a file in Dir; the engine's bold condensed font when the file is missing. */
	FSlateFontInfo FontFromDir(const FString& Dir, EDIAFontWeight Weight, int32 Size);

	/** The weight from the project's Content/Slate/Fonts. */
	FSlateFontInfo Font(EDIAFontWeight Weight, int32 Size);
}
