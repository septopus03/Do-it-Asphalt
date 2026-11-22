// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Misc/Paths.h"
#include "UI/DIAHudFont.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "UI/DIAShopLook.h"
#include "UI/SDIAHazardStripes.h"

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

DIA_TEST(FUiFontSharedCompositeTest, "DoItAsphalt.Game.Ui.Font.SharedComposite")

bool FUiFontSharedCompositeTest::RunTest(const FString& Parameters)
{
	const FSlateFontInfo A = AsphaltHud::Font(EDIAFontWeight::Bold, 20);
	const FSlateFontInfo B = AsphaltHud::Font(EDIAFontWeight::Bold, 30);
	const FSlateFontInfo C = AsphaltHud::Font(EDIAFontWeight::ExtraBold, 20);
	TestTrue(TEXT("a weight shares one composite font"), A.CompositeFont == B.CompositeFont);
	TestTrue(TEXT("the composite font exists"), A.CompositeFont.IsValid());
	TestTrue(TEXT("another weight has another composite font"), A.CompositeFont != C.CompositeFont);
	TestEqual(TEXT("size of the first"), A.Size, 20.f);
	TestEqual(TEXT("size of the second"), B.Size, 30.f);
	return true;
}

DIA_TEST(FUiFadedTest, "DoItAsphalt.Game.Ui.Plate.Faded")

bool FUiFadedTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltHud;
	const FLinearColor Half = Faded(SignYellow, 0.5f);
	TestEqual(TEXT("R"), Half.R, SignYellow.R);
	TestEqual(TEXT("G"), Half.G, SignYellow.G);
	TestEqual(TEXT("B"), Half.B, SignYellow.B);
	TestEqual(TEXT("A"), Half.A, 0.5f);
	TestEqual(TEXT("alpha is multiplied"), Faded(SignDimText, 0.5f).A, 0.35f, 0.001f);
	TestEqual(TEXT("above 1 is clamped"), Faded(SignDimText, 2.f).A, SignDimText.A);
	TestEqual(TEXT("below 0 is clamped"), Faded(SignDimText, -1.f).A, 0.f);
	return true;
}

DIA_TEST(FUiStripesTest, "DoItAsphalt.Game.Ui.Stripes.Starts")

bool FUiStripesTest::RunTest(const FString& Parameters)
{
	const TArray<float> S = AsphaltHud::HazardStripeStarts(100.f, 12.f, 12.f);
	if (!TestTrue(TEXT("there are stripes"), S.Num() > 1))
	{
		return false;
	}
	TestEqual(TEXT("the first starts one height to the left"), S[0], -12.f);
	for (int32 i = 0; i + 1 < S.Num(); ++i)
	{
		TestEqual(TEXT("step is two bands"), S[i + 1] - S[i], 24.f);
	}
	TestTrue(TEXT("the last starts inside"), S.Last() < 100.f);
	TestTrue(TEXT("the next would start outside"), S.Last() + 24.f >= 100.f);
	return true;
}

DIA_TEST(FUiStripesDegenerateTest, "DoItAsphalt.Game.Ui.Stripes.Degenerate")

bool FUiStripesDegenerateTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("no width"), AsphaltHud::HazardStripeStarts(0.f, 12.f, 12.f).Num(), 0);
	TestEqual(TEXT("no height"), AsphaltHud::HazardStripeStarts(100.f, 0.f, 12.f).Num(), 0);
	TestEqual(TEXT("no band"), AsphaltHud::HazardStripeStarts(100.f, 12.f, 0.f).Num(), 0);
	TestEqual(TEXT("negative band"), AsphaltHud::HazardStripeStarts(100.f, 12.f, -3.f).Num(), 0);
	TestEqual(TEXT("capped"), AsphaltHud::HazardStripeStarts(1.0e6f, 12.f, 0.001f).Num(), 4096);

	const TSharedRef<SDIAHazardStripes> Stripes = SNew(SDIAHazardStripes).Band(12.f).Height(20.f);
	Stripes->SlatePrepass(1.f);
	TestEqual(TEXT("desired height"), static_cast<float>(Stripes->GetDesiredSize().Y), 20.f);
	TestEqual(TEXT("desired width"), static_cast<float>(Stripes->GetDesiredSize().X), 0.f);
	return true;
}

DIA_TEST(FUiStripesThicknessTest, "DoItAsphalt.Game.Ui.Stripes.ThicknessFollowsScale")

bool FUiStripesThicknessTest::RunTest(const FString& Parameters)
{
	const float One = AsphaltHud::HazardStripeThickness(12.f, 1.f);
	TestEqual(TEXT("scale 1"), One, 12.f / FMath::Sqrt(2.f), 0.001f);
	TestEqual(TEXT("scale 2 doubles it"), AsphaltHud::HazardStripeThickness(12.f, 2.f), 2.f * One, 0.001f);
	TestEqual(TEXT("zero scale"), AsphaltHud::HazardStripeThickness(12.f, 0.f), 0.f);
	TestEqual(TEXT("negative scale"), AsphaltHud::HazardStripeThickness(12.f, -1.f), 0.f);
	return true;
}

DIA_TEST(FUiShopLookTest, "DoItAsphalt.Game.Ui.Shop.Look")

bool FUiShopLookTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltHud;
	const FDIAShopLook Buy = ShopLook(EDIAShopState::Buy);
	const FDIAShopLook NoCredits = ShopLook(EDIAShopState::NoCredits);
	const FDIAShopLook Locked = ShopLook(EDIAShopState::Locked);
	const FDIAShopLook Owned = ShopLook(EDIAShopState::Owned);

	TestEqual(TEXT("buy plate"), Buy.Plate, SignYellow);
	TestEqual(TEXT("buy ring"), Buy.Ring, SignBlack);
	TestTrue(TEXT("buy shows a button"), Buy.bShowsButton);
	TestTrue(TEXT("buy button is enabled"), Buy.bButtonEnabled);
	TestEqual(TEXT("buy label"), Buy.ButtonLabel.ToString(), FString(TEXT("BUY")));

	TestEqual(TEXT("no credits plate"), NoCredits.Plate, SignCardLack);
	TestEqual(TEXT("no credits price"), NoCredits.Price, SignLackPrice);
	TestTrue(TEXT("no credits shows a button"), NoCredits.bShowsButton);
	TestFalse(TEXT("no credits button is disabled"), NoCredits.bButtonEnabled);
	TestEqual(TEXT("no credits label"), NoCredits.ButtonLabel.ToString(), FString(TEXT("BUY")));

	TestEqual(TEXT("locked plate"), Locked.Plate, SignCardLocked);
	TestTrue(TEXT("locked shows a button"), Locked.bShowsButton);
	TestFalse(TEXT("locked button is disabled"), Locked.bButtonEnabled);
	TestEqual(TEXT("locked label"), Locked.ButtonLabel.ToString(), FString(TEXT("LOCKED")));

	TestEqual(TEXT("owned plate"), Owned.Plate, SignBlack);
	TestEqual(TEXT("owned ring"), Owned.Ring, SignWhite);
	TestFalse(TEXT("owned shows no button"), Owned.bShowsButton);
	TestTrue(TEXT("owned has no label"), Owned.ButtonLabel.IsEmpty());

	const FDIAShopLook Looks[] = {Buy, NoCredits, Locked, Owned};
	for (int32 A = 0; A < 4; ++A)
	{
		for (int32 B = A + 1; B < 4; ++B)
		{
			TestNotEqual(*FString::Printf(TEXT("plates %d and %d differ"), A, B), Looks[A].Plate, Looks[B].Plate);
		}
	}
	for (int32 I = 0; I < 4; ++I)
	{
		const FDIAShopLook& L = Looks[I];
		TestTrue(*FString::Printf(TEXT("text on plate %d"), I), ContrastRatio(Over(L.Text, L.Plate), L.Plate) >= 4.5f);
		TestTrue(*FString::Printf(TEXT("price on plate %d"), I), ContrastRatio(Over(L.Price, L.Plate), L.Plate) >= 4.5f);
		const FLinearColor Tile = Over(L.TileFill, L.Plate);
		TestTrue(*FString::Printf(TEXT("tile letter on tile %d"), I), ContrastRatio(Over(L.TileText, Tile), Tile) >= 4.5f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
