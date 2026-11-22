// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAHudFont.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

FString AsphaltHud::FontFileName(EDIAFontWeight Weight)
{
	switch (Weight)
	{
	case EDIAFontWeight::SemiBold:
		return TEXT("BarlowCondensed-SemiBold.ttf");
	case EDIAFontWeight::ExtraBold:
		return TEXT("BarlowCondensed-ExtraBold.ttf");
	default:
		return TEXT("BarlowCondensed-Bold.ttf");
	}
}

FSlateFontInfo AsphaltHud::FontFromDir(const FString& Dir, EDIAFontWeight Weight, int32 Size)
{
	const FString Path = Dir / FontFileName(Weight);
	if (!FPaths::FileExists(Path))
	{
		return FCoreStyle::GetDefaultFontStyle("BoldCondensed", Size);
	}
	const TSharedRef<const FCompositeFont> Composite = MakeShared<FStandaloneCompositeFont>(FName("Default"), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	return FSlateFontInfo(Composite, static_cast<float>(Size));
}

FSlateFontInfo AsphaltHud::Font(EDIAFontWeight Weight, int32 Size)
{
	return FontFromDir(FPaths::ProjectContentDir() / TEXT("Slate/Fonts"), Weight, Size);
}
