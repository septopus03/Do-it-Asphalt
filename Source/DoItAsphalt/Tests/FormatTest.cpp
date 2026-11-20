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

DIA_TEST(FFormatTest, "DoItAsphalt.Game.Format.Text")

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
	Is(TEXT("litres 3.24 / 5"), DIAFormat::Litres(3.24, 5.0), TEXT("3.2 / 5 l"));
	Is(TEXT("litres 200 / 200"), DIAFormat::Litres(200.0, 200.0), TEXT("200 / 200 l"));
	Is(TEXT("litres 4.99 / 5 is not shown as full"), DIAFormat::Litres(4.99, 5.0), TEXT("4.9 / 5 l"));

	// Progress: cells done / total and the percentage rounded down (100 % only when really done).
	Is(TEXT("progress 137 / 30,000"), DIAFormat::Progress(137, AsphaltLot::NumCells), TEXT("137 / 30,000 · 0%"));
	Is(TEXT("progress 15,000 / 30,000"), DIAFormat::Progress(15000, AsphaltLot::NumCells), TEXT("15,000 / 30,000 · 50%"));
	Is(TEXT("progress 30,000 / 30,000"), DIAFormat::Progress(AsphaltLot::NumCells, AsphaltLot::NumCells), TEXT("30,000 / 30,000 · 100%"));
	Is(TEXT("progress 29,999 is not 100"), DIAFormat::Progress(AsphaltLot::NumCells - 1, AsphaltLot::NumCells), TEXT("29,999 / 30,000 · 99%"));
	Is(TEXT("progress without cells does not divide by zero"), DIAFormat::Progress(0, 0), TEXT("0 / 0 · 0%"));

	Is(TEXT("count"), DIAFormat::Count(1234567), TEXT("1,234,567"));
	Is(TEXT("duration 754"), DIAFormat::Duration(754.0), TEXT("12:34"));
	Is(TEXT("duration 3725"), DIAFormat::Duration(3725.0), TEXT("1:02:05"));
	Is(TEXT("litres poured"), DIAFormat::LitresPoured(12345.9), TEXT("12,345 l"));
	Is(TEXT("items owned"), DIAFormat::Items(8, 16), TEXT("8 / 16"));

	// Toast and prompt texts.
	Is(TEXT("toast credits"), DIAFormat::CreditsToast(30.0), TEXT("+30 credits"));
	Is(TEXT("toast one credit"), DIAFormat::CreditsToast(1.0), TEXT("+1 credit"));
	Is(TEXT("toast entry"), DIAFormat::EntryToast(2), TEXT("Entry 3 complete"));
	Is(TEXT("toast exit"), DIAFormat::ExitToast(), TEXT("The exit is open!"));
	Is(TEXT("item name"), DIAFormat::ItemName(ELotItem::AutoPaver), TEXT("Auto Paver"));
	Is(TEXT("item toast"), DIAFormat::ItemBoughtToast(ELotItem::Scoop), TEXT("Scoop bought"));
	Is(TEXT("unlock name"), DIAFormat::UnlockName(ELotUnlock::Sprint), TEXT("Sprint"));
	Is(TEXT("car item name"), DIAFormat::ItemName(ELotItem::Car), TEXT("Paver Car"));
	Is(TEXT("car upgrade name"), DIAFormat::ItemName(ELotItem::CarWideBar), TEXT("Wide Bar"));
	Is(TEXT("car unlock name"), DIAFormat::UnlockName(ELotUnlock::Car), TEXT("Paver Car"));
	Is(TEXT("hand name"), DIAFormat::HandName(ELotHand::Trowel), TEXT("Trowel"));
	TestEqual(TEXT("unlock toast"), DIAFormat::UnlockToast(ELotUnlock::Sprint).ToString(), FString(TEXT("Unlocked: Sprint\n")) + DIAFormat::UnlockEffect(ELotUnlock::Sprint).ToString());
	Is(TEXT("nearest unfinished"), DIAFormat::NearestUnfinished(4180.0), TEXT("Nearest unfinished: 42 m"));
	Is(TEXT("nearest unfinished rounds to the nearest metre"), DIAFormat::NearestUnfinished(4260.0), TEXT("Nearest unfinished: 43 m"));
	// The arrow: 0 = straight ahead, 90 = to the right (+Y when looking along +X), -90 = to the left, 180 = behind.
	TestEqual(TEXT("arrow ahead"), DIAFormat::ArrowDegrees(FVector2D(0.0, 0.0), FVector2D(500.0, 0.0), 0.0), 0.0, 1e-6);
	TestEqual(TEXT("arrow right"), DIAFormat::ArrowDegrees(FVector2D(0.0, 0.0), FVector2D(0.0, 500.0), 0.0), 90.0, 1e-6);
	TestEqual(TEXT("arrow left"), DIAFormat::ArrowDegrees(FVector2D(0.0, 0.0), FVector2D(0.0, -500.0), 0.0), -90.0, 1e-6);
	TestEqual(TEXT("arrow follows the view yaw"), DIAFormat::ArrowDegrees(FVector2D(0.0, 0.0), FVector2D(0.0, 500.0), 90.0), 0.0, 1e-6);
	TestEqual(TEXT("arrow behind"), FMath::Abs(DIAFormat::ArrowDegrees(FVector2D(0.0, 0.0), FVector2D(-500.0, 0.0), 0.0)), 180.0, 1e-6);
	{
		// The boards of the entry segments: name, entry cells, cells of the car park; UNLOCKED once the step is open.
		FLotGame Game;
		Is(TEXT("board of a new game"), DIAFormat::EntryBoard(Game, 0), TEXT("QUICK SWAP\nEntry 0 / 12\nCells 0 / 10"));
		Is(TEXT("board of the exit"), DIAFormat::EntryBoard(Game, 5), TEXT("EXIT\nEntry 0 / 12\nCells 0 / 30,000"));
		Game.DebugCompleteCells(5);
		Is(TEXT("cells count for every board"), DIAFormat::EntryBoard(Game, 2), TEXT("STRIP WORK\nEntry 0 / 12\nCells 5 / 350"));
		// 900 cells by index fill the first rows, which hold no cell of the entries 1 to 5; entry 3 gets 11 of its 12 cells.
		Game.DebugCompleteCells(900);
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry - 1; ++I)
		{
			Game.DebugCompleteCell(AsphaltLot::EntryCell(3, I));
		}
		Is(TEXT("the cells never read above the threshold"), DIAFormat::EntryBoard(Game, 3), TEXT("AUTO PAVER\nEntry 11 / 12\nCells 800 / 800"));
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			Game.DebugCompleteCell(AsphaltLot::EntryCell(1, I));
		}
		Is(TEXT("an unlocked step"), DIAFormat::EntryBoard(Game, 1), TEXT("SPRINT\nUNLOCKED"));
		TestTrue(TEXT("no board for a missing entry"), DIAFormat::EntryBoard(Game, 6).IsEmpty());
	}
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

DIA_TEST(FFormatNamesTest, "DoItAsphalt.Game.Format.Names")

bool FFormatNamesTest::RunTest(const FString& Parameters)
{
	for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
	{
		TestFalse(*FString::Printf(TEXT("item %d has a name"), I), DIAFormat::ItemName(static_cast<ELotItem>(I)).IsEmpty());
	}
	for (int32 I = 0; I < static_cast<int32>(ELotUnlock::Count); ++I)
	{
		const ELotUnlock Unlock = static_cast<ELotUnlock>(I);
		TestFalse(*FString::Printf(TEXT("unlock %d has a name"), I), DIAFormat::UnlockName(Unlock).IsEmpty());
		TestFalse(*FString::Printf(TEXT("unlock %d says what it opens"), I), DIAFormat::UnlockEffect(Unlock).IsEmpty());
	}
	for (int32 I = 0; I < static_cast<int32>(ELotHand::Count); ++I)
	{
		TestFalse(*FString::Printf(TEXT("hand %d has a name"), I), DIAFormat::HandName(static_cast<ELotHand>(I)).IsEmpty());
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

DIA_TEST(FHudCreditsToastTest, "DoItAsphalt.Game.HudCreditsToast")
bool FHudCreditsToastTest::RunTest(const FString& Parameters)
{
	const double Cell = LotBalance::CreditsPerCell;
	const auto CreditsToasts = [](const FDIAToasts& Toasts)
	{
		int32 N = 0;
		for (int32 I = 0; I < Toasts.Num(); ++I)
		{
			N += Toasts.GetText(I).ToString().Contains(TEXT("credit")) ? 1 : 0;
		}
		return N;
	};

	// Hand pouring: one cell still shows its own amount.
	FDIAToasts Toasts;
	Toasts.AddCredits(Cell);
	TestEqual(TEXT("one cell"), Toasts.Num(), 1);
	TestEqual(TEXT("one cell shows its credits"), Toasts.GetText(0).ToString(), DIAFormat::CreditsToast(Cell).ToString());

	// A stream of cells within the hold time is one toast with the running total.
	for (int32 I = 0; I < 9; ++I)
	{
		Toasts.Advance(0.3f);
		Toasts.AddCredits(Cell);
	}
	TestEqual(TEXT("exactly one credits toast"), Toasts.Num(), 1);
	TestEqual(TEXT("running total"), Toasts.GetText(0).ToString(), DIAFormat::CreditsToast(10 * Cell).ToString());
	TestEqual(TEXT("fully visible after the last addition"), Toasts.GetAlpha(0), 1.f);

	// Another toast between the cells keeps its full hold time while the cells keep coming.
	Toasts.Add(FText::FromString(TEXT("Entry")));
	for (int32 I = 0; I < 8; ++I)
	{
		Toasts.Advance(0.25f);
		Toasts.AddCredits(Cell);
	}
	TestEqual(TEXT("still one credits toast"), CreditsToasts(Toasts), 1);
	TestEqual(TEXT("two toasts"), Toasts.Num(), 2);
	TestEqual(TEXT("the credits toast keeps its slot"), Toasts.GetText(1).ToString(), FString(TEXT("Entry")));
	TestEqual(TEXT("the other toast shows the total"), Toasts.GetText(0).ToString(), DIAFormat::CreditsToast(18 * Cell).ToString());
	TestEqual(TEXT("the entry toast is still fully visible after 2 s"), Toasts.GetAlpha(1), 1.f);
	Toasts.Advance(AsphaltHud::ToastHoldSeconds - 2.0f + 0.1f);
	TestEqual(TEXT("the entry toast is fading only after its hold time"), Toasts.GetAlpha(1) < 1.f, true);

	// A credits toast never pushes out another toast: with all places taken it is not shown.
	FDIAToasts Full;
	for (int32 I = 0; I < AsphaltHud::MaxToasts; ++I)
	{
		Full.Add(FText::FromString(FString::FromInt(I)));
	}
	Full.AddCredits(Cell);
	TestEqual(TEXT("no places left"), Full.Num(), AsphaltHud::MaxToasts);
	TestEqual(TEXT("no credits toast shown"), CreditsToasts(Full), 0);
	TestEqual(TEXT("the oldest other toast stays"), Full.GetText(0).ToString(), FString(TEXT("0")));

	// A new toast takes the place of the credits toast rather than another toast.
	FDIAToasts Mixed;
	Mixed.Add(FText::FromString(TEXT("A")));
	Mixed.AddCredits(Cell);
	Mixed.Add(FText::FromString(TEXT("B")));
	Mixed.Add(FText::FromString(TEXT("C")));
	TestEqual(TEXT("limit kept"), Mixed.Num(), AsphaltHud::MaxToasts);
	TestEqual(TEXT("the credits toast went first"), CreditsToasts(Mixed), 0);
	TestEqual(TEXT("A stays"), Mixed.GetText(0).ToString(), FString(TEXT("A")));

	// After the credits toast has expired the next cells start a new total.
	FDIAToasts Expiring;
	Expiring.AddCredits(5 * Cell);
	Expiring.Advance(AsphaltHud::ToastHoldSeconds + AsphaltHud::ToastFadeSeconds + 0.1f);
	TestEqual(TEXT("expired"), Expiring.Num(), 0);
	Expiring.AddCredits(Cell);
	TestEqual(TEXT("new total starts from its own amount"), Expiring.GetText(0).ToString(), DIAFormat::CreditsToast(Cell).ToString());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
