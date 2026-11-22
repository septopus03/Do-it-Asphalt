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

namespace
{
	/** The composite font of the weight's file in Dir, or null when the file is missing. */
	TSharedPtr<const FCompositeFont> LoadComposite(const FString& Dir, EDIAFontWeight Weight)
	{
		const FString Path = Dir / AsphaltHud::FontFileName(Weight);
		if (!FPaths::FileExists(Path))
		{
			return nullptr;
		}
		return MakeShared<FStandaloneCompositeFont>(FName("Default"), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}

	FSlateFontInfo FromComposite(const TSharedPtr<const FCompositeFont>& Composite, int32 Size)
	{
		if (!Composite.IsValid())
		{
			return FCoreStyle::GetDefaultFontStyle("BoldCondensed", Size);
		}
		return FSlateFontInfo(Composite.ToSharedRef(), static_cast<float>(Size));
	}
}

FSlateFontInfo AsphaltHud::FontFromDir(const FString& Dir, EDIAFontWeight Weight, int32 Size)
{
	return FromComposite(LoadComposite(Dir, Weight), Size);
}

FSlateFontInfo AsphaltHud::Font(EDIAFontWeight Weight, int32 Size)
{
	// Resolved once per weight (the file check and the composite font), then reused by every text block; Slate runs on the game thread.
	static TSharedPtr<const FCompositeFont> Composites[3];
	static bool Resolved[3] = {false, false, false};
	const int32 Index = FMath::Clamp(static_cast<int32>(Weight), 0, 2);
	if (!Resolved[Index])
	{
		Composites[Index] = LoadComposite(FPaths::ProjectContentDir() / TEXT("Slate/Fonts"), static_cast<EDIAFontWeight>(Index));
		Resolved[Index] = true;
	}
	return FromComposite(Composites[Index], Size);
}
