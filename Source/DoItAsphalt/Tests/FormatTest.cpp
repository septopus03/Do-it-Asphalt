// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltBalance.h"
#include "AsphaltGame.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Engine/World.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"

DIA_TEST(FFormatTest, "DoItAsphalt.Game.Format")

bool FFormatTest::RunTest(const FString& Parameters)
{
	auto Is = [this](const TCHAR* What, const FText& Actual, const TCHAR* Expected)
	{
		TestEqual(What, Actual.ToString(), FString(Expected));
	};

	// Money: whole units, rounded down, thousands separated.
	Is(TEXT("money 1234.7"), DIAFormat::Money(1234.7), TEXT("1,234"));
	Is(TEXT("money 0"), DIAFormat::Money(0.0), TEXT("0"));
	Is(TEXT("money 999.99"), DIAFormat::Money(999.99), TEXT("999"));
	Is(TEXT("money 1234567"), DIAFormat::Money(1234567.0), TEXT("1,234,567"));
	Is(TEXT("money negative never shows a minus"), DIAFormat::Money(-3.0), TEXT("0"));

	// Litres: one decimal at most, rounded down so that "full" really means full.
	Is(TEXT("litres 3.24 / 5"), DIAFormat::Litres(3.24, 5.0), TEXT("3.2 / 5 L"));
	Is(TEXT("litres 200 / 200"), DIAFormat::Litres(200.0, 200.0), TEXT("200 / 200 L"));
	Is(TEXT("litres 0 / 5"), DIAFormat::Litres(0.0, 5.0), TEXT("0 / 5 L"));
	Is(TEXT("litres 4.99 / 5 is not shown as full"), DIAFormat::Litres(4.99, 5.0), TEXT("4.9 / 5 L"));
	Is(TEXT("litres 42 / 60"), DIAFormat::Litres(42.0, 60.0), TEXT("42 / 60 L"));

	// Progress: cells done / total and the percentage rounded down (100 % only when really done).
	Is(TEXT("progress 137 / 499"), DIAFormat::Progress(137, 499), TEXT("137 / 499 · 27%"));
	Is(TEXT("progress 0 / 499"), DIAFormat::Progress(0, 499), TEXT("0 / 499 · 0%"));
	Is(TEXT("progress 499 / 499"), DIAFormat::Progress(499, 499), TEXT("499 / 499 · 100%"));
	Is(TEXT("progress 498 / 499 is not 100"), DIAFormat::Progress(498, 499), TEXT("498 / 499 · 99%"));
	Is(TEXT("progress without cells does not divide by zero"), DIAFormat::Progress(0, 0), TEXT("0 / 0 · 0%"));

	// Price: money, shards (singular and plural) and the maximum level.
	Is(TEXT("price 25"), DIAFormat::Price(25.0, false), TEXT("25"));
	Is(TEXT("price 600"), DIAFormat::Price(600.0, false), TEXT("600"));
	Is(TEXT("price 1,200"), DIAFormat::Price(1200.0, false), TEXT("1,200"));
	Is(TEXT("price 4 shards"), DIAFormat::Price(4.0, true), TEXT("4 shards"));
	Is(TEXT("price 1 shard"), DIAFormat::Price(1.0, true), TEXT("1 shard"));
	Is(TEXT("price MAX"), DIAFormat::Price(-1.0, false), TEXT("MAX"));
	Is(TEXT("price MAX for shards too"), DIAFormat::Price(-1.0, true), TEXT("MAX"));

	// Toast and prompt texts.
	Is(TEXT("toast 1 shard"), DIAFormat::ShardsToast(1), TEXT("+1 shard"));
	Is(TEXT("toast 137 shards"), DIAFormat::ShardsToast(137), TEXT("+137 shards"));
	Is(TEXT("toast road"), DIAFormat::RoadToast(2), TEXT("Road 3 complete"));
	Is(TEXT("toast exit"), DIAFormat::ExitToast(), TEXT("The exit is open!"));
	Is(TEXT("special name"), DIAFormat::SpecialName(EAsphaltSpecial::PortableStation), TEXT("Portable Station"));
	Is(TEXT("special toast"), DIAFormat::SpecialToast(EAsphaltSpecial::Sprint), TEXT("Sprint unlocked"));
	return true;
}

DIA_TEST(FHudModelTest, "DoItAsphalt.Game.HudModel")

bool FHudModelTest::RunTest(const FString& Parameters)
{
	FAsphaltGame Game;
	for (int32 Cell = 0; Cell < 137; ++Cell)
	{
		Game.DebugCompleteCell(Cell);
	}
	// Completing cells paid out already; top up to exactly 1234.7 money and 17 shards.
	Game.DebugGive(1234.7 - Game.GetMoney(), 17 - Game.GetShards());
	// Far from every refill point.
	Game.World().PlayerPos = FVector2D(0.0, 0.0);

	FDIAHudModel Model;
	TestTrue(TEXT("the first update builds the view"), Model.Update(Game));
	TestEqual(TEXT("money text"), Model.GetView().Money.ToString(), FString(TEXT("1,234")));
	TestEqual(TEXT("shards text"), Model.GetView().Shards.ToString(), FString(TEXT("17")));
	TestEqual(TEXT("progress text"), Model.GetView().Progress.ToString(), FString(TEXT("137 / 499 · 27%")));
	TestEqual(TEXT("backpack text"), Model.GetView().Backpack.ToString(), FString(TEXT("0 / 5 L")));
	TestFalse(TEXT("no stock far from the plant"), Model.GetView().bShowStock);
	TestEqual(TEXT("progress bar"), Model.GetView().ProgressFraction, 137.f / 499.f, 1e-4f);
	TestEqual(TEXT("empty backpack bar"), Model.GetView().BackpackFraction, 0.f);

	// The plant keeps producing every tick; far from the plant the shown values do not change, so nothing is rebuilt.
	const int32 Rebuilds = Model.GetRebuildCount();
	for (int32 I = 0; I < 50; ++I)
	{
		Game.Tick(0.016);
		TestFalse(TEXT("unchanged display is not rebuilt"), Model.Update(Game));
	}
	TestEqual(TEXT("no rebuild during production far from the plant"), Model.GetRebuildCount(), Rebuilds);

	// Near the plant the stock is shown: 15 l/min is 0.25 l/s, so a tenth of a litre per 0.4 s changes the text.
	Game.World().PlayerPos = AsphaltLayout::RefillCenter + FVector2D(AsphaltHud::StockRadiusCm - 1.0, 0.0);
	TestTrue(TEXT("entering the radius changes the view"), Model.Update(Game));
	TestTrue(TEXT("stock shown within the radius"), Model.GetView().bShowStock);
	const FString StockBefore = Model.GetView().Stock.ToString();
	TestTrue(TEXT("stock text is litres of the plant storage"), StockBefore.EndsWith(FString::Printf(TEXT("/ %d L"), FMath::RoundToInt(Game.GetPlantStorage()))));
	int32 Changes = 0;
	for (int32 I = 0; I < 100; ++I)
	{
		Game.Tick(0.1);
		Changes += Model.Update(Game) ? 1 : 0;
	}
	// 10 s of production is 2.5 l: about 25 tenth-litre changes, far fewer than the 100 updates.
	TestTrue(TEXT("stock text follows production"), Changes >= 20 && Changes <= 30);
	TestNotEqual(TEXT("stock text moved"), Model.GetView().Stock.ToString(), StockBefore);

	// Just outside the radius the stock disappears again.
	Game.World().PlayerPos = AsphaltLayout::RefillCenter + FVector2D(AsphaltHud::StockRadiusCm + 1.0, 0.0);
	TestTrue(TEXT("leaving the radius changes the view"), Model.Update(Game));
	TestFalse(TEXT("no stock outside the radius"), Model.GetView().bShowStock);

	// The paver car shows its tank instead of the backpack.
	Game.DebugGrantSpecial(EAsphaltSpecial::PaverCar);
	Game.World().bPlayerInCar = true;
	TestTrue(TEXT("getting into the car changes the view"), Model.Update(Game));
	TestEqual(TEXT("car tank text"), Model.GetView().Backpack.ToString(), FString::Printf(TEXT("0 / %d L"), FMath::RoundToInt(AsphaltBalance::CarTankL)));
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
