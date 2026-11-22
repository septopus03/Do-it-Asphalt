// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Misc/Paths.h"
#include "UI/DIAHudFont.h"
#include "UI/DIAHudStyle.h"

DIA_TEST(FUiFontFilesTest, "DoItAsphalt.Game.Ui.Font.FilesExist")

bool FUiFontFilesTest::RunTest(const FString& Parameters)
{
	for (const EDIAFontWeight Weight : {EDIAFontWeight::SemiBold, EDIAFontWeight::Bold, EDIAFontWeight::ExtraBold})
	{
		const FString File = AsphaltHud::FontFileName(Weight);
		TestTrue(*FString::Printf(TEXT("%s exists"), *File), FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Slate/Fonts") / File));
	}
	const FSlateFontInfo Bold = AsphaltHud::Font(EDIAFontWeight::Bold, 20);
	TestNotEqual(TEXT("the shipped font is used, not the fallback"), Bold.TypefaceFontName, FName("BoldCondensed"));
	TestEqual(TEXT("size"), Bold.Size, 20.f);
	TestTrue(TEXT("the font is valid"), Bold.HasValidFont());
	return true;
}

DIA_TEST(FUiFontFallbackTest, "DoItAsphalt.Game.Ui.Font.FallsBackWhenMissing")

bool FUiFontFallbackTest::RunTest(const FString& Parameters)
{
	const FSlateFontInfo F = AsphaltHud::FontFromDir(TEXT("/no/such/dir"), EDIAFontWeight::ExtraBold, 33);
	TestEqual(TEXT("falls back to the engine font"), F.TypefaceFontName, FName("BoldCondensed"));
	TestEqual(TEXT("size"), F.Size, 33.f);
	TestTrue(TEXT("the fallback is valid"), F.HasValidFont());
	return true;
}

DIA_TEST(FUiPaletteContrastTest, "DoItAsphalt.Game.Ui.Palette.Contrast")

bool FUiPaletteContrastTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltHud;
	TestTrue(TEXT("black on yellow"), ContrastRatio(SignBlack, SignYellow) >= 4.5f);
	TestTrue(TEXT("white on black"), ContrastRatio(SignWhite, SignBlack) >= 4.5f);
	TestTrue(TEXT("yellow on black"), ContrastRatio(SignYellow, SignBlack) >= 4.5f);
	TestTrue(TEXT("orange text on black"), ContrastRatio(SignOrangeText, SignBlack) >= 4.5f);
	TestTrue(TEXT("lacking price on its card"), ContrastRatio(SignLackPrice, SignCardLack) >= 4.5f);
	TestTrue(TEXT("dim text on a locked card"), ContrastRatio(Over(SignDimText, SignCardLocked), SignCardLocked) >= 4.5f);
	TestEqual(TEXT("same colour is 1"), ContrastRatio(SignWhite, SignWhite), 1.f, 0.001f);
	TestEqual(TEXT("symmetric"), ContrastRatio(SignYellow, SignBlack), ContrastRatio(SignBlack, SignYellow));
	TestEqual(TEXT("Over is opaque"), Over(SignDimText, SignCardLocked).A, 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
