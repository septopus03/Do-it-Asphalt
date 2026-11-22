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
		const TSharedRef<FStandaloneCompositeFont> Composite = MakeShared<FStandaloneCompositeFont>(FName("Default"), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		// Glyphs Barlow Condensed lacks (the arrow "→", the tick "✓") come from the engine's DroidSansFallback.
		const FString FallbackPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
		if (FPaths::FileExists(FallbackPath))
		{
			// A sub-typeface limited to the arrows and the dingbats (the tick): the plain fallback is not asked for these.
			FCompositeSubFont& Symbols = Composite->SubTypefaces.AddDefaulted_GetRef();
			Symbols.Typeface.Fonts.Emplace(FName("Symbols"), FallbackPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			Symbols.CharacterRanges.Add(FInt32Range(FInt32RangeBound::Inclusive(0x2190), FInt32RangeBound::Inclusive(0x21FF)));
			Symbols.CharacterRanges.Add(FInt32Range(FInt32RangeBound::Inclusive(0x2700), FInt32RangeBound::Inclusive(0x27BF)));
		}
		return Composite;
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
