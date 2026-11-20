// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Engine/World.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotMap.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"

DIA_TEST(FFormatTest, "DoItAsphalt.Game.Format")

bool FFormatTest::RunTest(const FString& Parameters)
{
	auto Is = [this](const TCHAR* What, const FText& Actual, const TCHAR* Expected)
	{
		TestEqual(What, Actual.ToString(), FString(Expected));
	};

	// Credits: whole units, rounded down, thousands separated.
	Is(TEXT("credits 1234.7"), DIAFormat::Credits(1234.7), TEXT("1,234"));
	Is(TEXT("credits 0"), DIAFormat::Credits(0.0), TEXT("0"));
	Is(TEXT("credits 999.99"), DIAFormat::Credits(999.99), TEXT("999"));
	Is(TEXT("credits 1234567"), DIAFormat::Credits(1234567.0), TEXT("1,234,567"));
	Is(TEXT("credits negative never shows a minus"), DIAFormat::Credits(-3.0), TEXT("0"));

	// Litres: one decimal at most, rounded down so that "full" really means full.
	Is(TEXT("litres 3.24 / 5"), DIAFormat::Litres(3.24, 5.0), TEXT("3.2 / 5 L"));
	Is(TEXT("litres 200 / 200"), DIAFormat::Litres(200.0, 200.0), TEXT("200 / 200 L"));
	Is(TEXT("litres 4.99 / 5 is not shown as full"), DIAFormat::Litres(4.99, 5.0), TEXT("4.9 / 5 L"));

	// Progress: cells done / total and the percentage rounded down (100 % only when really done).
	Is(TEXT("progress 137 / 30,000"), DIAFormat::Progress(137, AsphaltLot::NumCells), TEXT("137 / 30,000 · 0%"));
	Is(TEXT("progress 15,000 / 30,000"), DIAFormat::Progress(15000, AsphaltLot::NumCells), TEXT("15,000 / 30,000 · 50%"));
	Is(TEXT("progress 30,000 / 30,000"), DIAFormat::Progress(AsphaltLot::NumCells, AsphaltLot::NumCells), TEXT("30,000 / 30,000 · 100%"));
	Is(TEXT("progress 29,999 is not 100"), DIAFormat::Progress(AsphaltLot::NumCells - 1, AsphaltLot::NumCells), TEXT("29,999 / 30,000 · 99%"));
	Is(TEXT("progress without cells does not divide by zero"), DIAFormat::Progress(0, 0), TEXT("0 / 0 · 0%"));

	Is(TEXT("count"), DIAFormat::Count(1234567), TEXT("1,234,567"));
	Is(TEXT("duration 754"), DIAFormat::Duration(754.0), TEXT("12:34"));
	Is(TEXT("duration 3725"), DIAFormat::Duration(3725.0), TEXT("1:02:05"));
	Is(TEXT("litres poured"), DIAFormat::LitresPoured(12345.9), TEXT("12,345 L"));
	Is(TEXT("items owned"), DIAFormat::Items(8, 16), TEXT("8 / 16"));

	// Toast and prompt texts.
	Is(TEXT("toast credits"), DIAFormat::CreditsToast(30.0), TEXT("+30 credits"));
	Is(TEXT("toast one credit"), DIAFormat::CreditsToast(1.0), TEXT("+1 credit"));
	Is(TEXT("toast entry"), DIAFormat::EntryToast(2), TEXT("Entry 3 complete"));
	Is(TEXT("toast exit"), DIAFormat::ExitToast(), TEXT("The exit is open!"));
	Is(TEXT("item name"), DIAFormat::ItemName(ELotItem::AutoPaver), TEXT("Auto Paver"));
	Is(TEXT("item toast"), DIAFormat::ItemBoughtToast(ELotItem::Scoop), TEXT("Scoop bought"));
	Is(TEXT("unlock name"), DIAFormat::UnlockName(ELotUnlock::Sprint), TEXT("Sprint"));
	Is(TEXT("unlock toast"), DIAFormat::UnlockToast(ELotUnlock::Sprint), TEXT("Sprint unlocked"));
	Is(TEXT("plant toast"), DIAFormat::PlantToast(3), TEXT("Plant level 3"));

	// Every item and unlock has a name.
	for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
	{
		TestFalse(*FString::Printf(TEXT("item %d has a name"), I), DIAFormat::ItemName(static_cast<ELotItem>(I)).IsEmpty());
	}
	for (int32 I = 0; I < static_cast<int32>(ELotUnlock::Count); ++I)
	{
		TestFalse(*FString::Printf(TEXT("unlock %d has a name"), I), DIAFormat::UnlockName(static_cast<ELotUnlock>(I)).IsEmpty());
	}
	return true;
}

DIA_TEST(FHudModelTest, "DoItAsphalt.Game.Hud.Model")

bool FHudModelTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	Game.DebugCompleteCells(137);

	FDIAHudModel Model;
	TestTrue(TEXT("the first update builds the view"), Model.Update(Game));
	TestEqual(TEXT("credits text"), Model.GetView().Credits.ToString(), FString(TEXT("1,370")));
	TestTrue(TEXT("progress text"), Model.GetView().Progress.ToString().StartsWith(TEXT("137 / 30,000")));
	TestEqual(TEXT("progress text in full"), Model.GetView().Progress.ToString(), FString(TEXT("137 / 30,000 · 0%")));
	TestEqual(TEXT("progress bar"), Model.GetView().ProgressFraction, 137.f / 30000.f, 1e-6f);

	// The plant keeps producing and the clock runs, but neither is shown: nothing is rebuilt.
	const int32 Rebuilds = Model.GetRebuildCount();
	for (int32 I = 0; I < 50; ++I)
	{
		Game.Tick(0.016);
		TestFalse(TEXT("unchanged display is not rebuilt"), Model.Update(Game));
	}
	TestEqual(TEXT("no rebuild during production"), Model.GetRebuildCount(), Rebuilds);

	// What is shown changes the view.
	Game.DebugGive(5.0);
	TestTrue(TEXT("more credits rebuild the view"), Model.Update(Game));
	TestEqual(TEXT("credits follow"), Model.GetView().Credits.ToString(), FString(TEXT("1,375")));
	Game.DebugCompleteCell(AsphaltLot::CellIndex(150, 50));
	TestTrue(TEXT("a completed cell rebuilds the view"), Model.Update(Game));
	TestEqual(TEXT("progress follows"), Model.GetView().Progress.ToString(), FString(TEXT("138 / 30,000 · 0%")));
	return true;
}

DIA_TEST(FWinModelTest, "DoItAsphalt.Game.WinModelFormat")

bool FWinModelTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAWinModel Model;
	TestTrue(TEXT("the first update builds the view"), Model.Update(Game));
	TestEqual(TEXT("no items yet"), Model.GetView().Items.ToString(), FString::Printf(TEXT("0 / %d"), FDIAWinModel::ItemsTotal()));
	TestEqual(TEXT("no credits earned yet"), Model.GetView().Credits.ToString(), FString(TEXT("0")));
	TestFalse(TEXT("nothing changed"), Model.Update(Game));

	// Credits earned are what the cells paid, not what is left in the purse.
	Game.DebugCompleteCells(200);
	Game.DebugGrantItem(ELotItem::Scoop);
	Game.DebugSetPlantLevel(2);
	TestEqual(TEXT("items and plant levels count"), FDIAWinModel::ItemsBought(Game), 3);
	TestTrue(TEXT("changes rebuild the view"), Model.Update(Game));
	TestEqual(TEXT("credits earned"), Model.GetView().Credits.ToString(), FString(TEXT("2,000")));
	TestEqual(TEXT("items text"), Model.GetView().Items.ToString(), FString::Printf(TEXT("3 / %d"), FDIAWinModel::ItemsTotal()));
	return true;
}

DIA_TEST(FHudToastsTest, "DoItAsphalt.Game.HudToasts")

bool FHudToastsTest::RunTest(const FString& Parameters)
{
	FDIAToasts Toasts;
	TestEqual(TEXT("starts empty"), Toasts.Num(), 0);

	Toasts.Add(FText::FromString(TEXT("A")));
	TestEqual(TEXT("one toast"), Toasts.Num(), 1);
	TestEqual(TEXT("fully visible at the start"), Toasts.GetAlpha(0), 1.f);

	Toasts.Advance(AsphaltHud::ToastHoldSeconds - 0.1f);
	TestEqual(TEXT("still fully visible just before 2 s"), Toasts.GetAlpha(0), 1.f);
	Toasts.Advance(0.1f + AsphaltHud::ToastFadeSeconds * 0.5f);
	TestEqual(TEXT("half faded"), Toasts.GetAlpha(0), 0.5f, 1e-3f);
	Toasts.Advance(AsphaltHud::ToastFadeSeconds * 0.5f + 0.01f);
	TestEqual(TEXT("gone after the fade"), Toasts.Num(), 0);

	// A burst keeps the newest toasts only; the oldest are dropped.
	for (int32 I = 0; I < AsphaltHud::MaxToasts + 2; ++I)
	{
		Toasts.Add(FText::FromString(FString::FromInt(I)));
	}
	TestEqual(TEXT("limited number of toasts"), Toasts.Num(), AsphaltHud::MaxToasts);
	TestEqual(TEXT("the oldest is dropped"), Toasts.GetText(0).ToString(), FString::FromInt(2));
	TestEqual(TEXT("the newest is last"), Toasts.GetText(AsphaltHud::MaxToasts - 1).ToString(), FString::FromInt(AsphaltHud::MaxToasts + 1));
	TestEqual(TEXT("out of range text is empty"), Toasts.GetText(99).ToString(), FString());
	TestEqual(TEXT("out of range alpha is zero"), Toasts.GetAlpha(99), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
