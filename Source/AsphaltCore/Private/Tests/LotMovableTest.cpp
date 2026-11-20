// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotLayout.h"
#include "LotMap.h"

DIA_TEST(FLotMovableSpawnOnBuyTest, "DoItAsphalt.Lot.Movable.SpawnOnBuy")
DIA_TEST(FLotMovableGrabRulesTest, "DoItAsphalt.Lot.Movable.GrabRules")
DIA_TEST(FLotMovableReleaseTest, "DoItAsphalt.Lot.Movable.Release")
DIA_TEST(FLotMovableCartFillTest, "DoItAsphalt.Lot.Movable.CartFill")
DIA_TEST(FLotMovableCartLays20CellsTest, "DoItAsphalt.Lot.Movable.CartLays20Cells")
DIA_TEST(FLotMovableFinisherCompletesStripTest, "DoItAsphalt.Lot.Movable.FinisherCompletesStrip")
DIA_TEST(FLotMovableSuitcaseDoseTest, "DoItAsphalt.Lot.Movable.SuitcaseDose")
DIA_TEST(FLotMovableTrowelFinishesDoseTest, "DoItAsphalt.Lot.Movable.TrowelFinishesDose")
DIA_TEST(FLotMovableBucketFromSuitcaseTest, "DoItAsphalt.Lot.Movable.BucketFromSuitcase")
DIA_TEST(FLotMovableDoseCarriesRestTest, "DoItAsphalt.Lot.Movable.DoseCarriesRest")
DIA_TEST(FLotMovableLitresConservedTest, "DoItAsphalt.Lot.Movable.LitresConserved")

namespace
{
	constexpr double Tolerance = 1e-6;
	constexpr double TexelTolerance = FLotGrid::LitresPerTexel;
	const double NaN = std::numeric_limits<double>::quiet_NaN();

	/** An ordinary lot cell away from the edges. */
	int32 MiddleCell(int32 Col = 40, int32 Row = 40)
	{
		return AsphaltLot::CellIndex(Col, Row);
	}

	/** Axis of a strip over Count cells of one row, starting at column Col; the stamp's end pads reach the cell edges exactly. */
	void RowStrip(int32 Col, int32 Row, int32 Count, FVector2D& A, FVector2D& B)
	{
		A = AsphaltLot::CellCentre(MiddleCell(Col, Row)) + FVector2D(-AsphaltLot::CellCm * 0.5 + LotBalance::StampPadCm, 0.0);
		B = AsphaltLot::CellCentre(MiddleCell(Col + Count - 1, Row)) + FVector2D(AsphaltLot::CellCm * 0.5 - LotBalance::StampPadCm, 0.0);
	}

	/** Game with the item owned and plant level 5 so that containers can be filled quickly. */
	void Equip(FLotGame& G, ELotItem Item)
	{
		G.DebugGrantItem(Item);
		G.DebugSetPlantLevel(5);
	}

	/** Fills a vessel from the plant until it is full. */
	void FillFull(FLotGame& G, ELotMovable Movable)
	{
		G.DebugSetPlantStock(G.GetPlantStorage());
		G.FillMovable(Movable, 1000.0);
	}

	int32 RawTexelsInRow(const FLotGame& G, int32 Col, int32 Count, int32 Row)
	{
		int32 Sum = 0;
		for (int32 I = 0; I < Count; ++I)
		{
			Sum += G.GetGrid().GetCellRaw(MiddleCell(Col + I, Row));
		}
		return Sum;
	}
}

bool FLotMovableSpawnOnBuyTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGive(1000000.0);
	TestTrue(TEXT("buy the cart"), G.BuyItem(ELotItem::Cart));
	TestTrue(TEXT("cart is owned"), G.OwnsMovable(ELotMovable::Cart));
	TestFalse(TEXT("suitcase is not"), G.OwnsMovable(ELotMovable::Suitcase));
	TestEqual(TEXT("cart spawn"), G.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos, LotLayout::ItemSpawn(ELotItem::Cart));
	TestFalse(TEXT("cart is not held"), G.World().Movables[static_cast<int32>(ELotMovable::Cart)].bHeld);
	TestEqual(TEXT("cart yaw"), G.World().Movables[static_cast<int32>(ELotMovable::Cart)].YawDeg, 0.0);
	G.DebugGrantItem(ELotItem::Station);
	TestEqual(TEXT("granted station spawn"), G.World().Movables[static_cast<int32>(ELotMovable::Station)].Pos, LotLayout::ItemSpawn(ELotItem::Station));
	TestTrue(TEXT("station is owned"), G.OwnsMovable(ELotMovable::Station));
	TestFalse(TEXT("invalid movable is not owned"), G.OwnsMovable(ELotMovable::Count));
	return true;
}

bool FLotMovableGrabRulesTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestFalse(TEXT("not owned"), G.Grab(ELotMovable::Cart));
	TestEqual(TEXT("nothing held"), G.GetHeld(), ELotMovable::Count);
	G.DebugGrantItem(ELotItem::Cart);
	G.DebugGrantItem(ELotItem::Suitcase);
	G.DebugGrantItem(ELotItem::Bucket);
	G.DebugGrantItem(ELotItem::Scoop);
	TestFalse(TEXT("invalid movable"), G.Grab(ELotMovable::Count));
	G.ScoopFromTrough(10.0);
	const double BowlBefore = G.GetHandLitres(ELotHand::Bowl);
	const double StockBefore = G.GetPlantStock();
	TestTrue(TEXT("grab the cart"), G.Grab(ELotMovable::Cart));
	TestEqual(TEXT("cart held"), G.GetHeld(), ELotMovable::Cart);
	TestTrue(TEXT("flag set"), G.World().Movables[static_cast<int32>(ELotMovable::Cart)].bHeld);
	TestFalse(TEXT("no second one"), G.Grab(ELotMovable::Suitcase));
	TestFalse(TEXT("not the same again"), G.Grab(ELotMovable::Cart));

	const FVector2D Aim = AsphaltLot::CellCentre(MiddleCell());
	const FVector2D From = Aim + FVector2D(-100.0, 0.0);
	TestEqual(TEXT("no pour while holding"), G.PourHand(Aim, From, 1.0).LitresPoured, 0.0);
	TestEqual(TEXT("no scooping while holding"), G.ScoopFromTrough(10.0), 0.0);
	TestNearlyEqual(TEXT("bowl untouched"), G.GetHandLitres(ELotHand::Bowl), BowlBefore, Tolerance);
	TestNearlyEqual(TEXT("stock untouched"), G.GetPlantStock(), StockBefore, Tolerance);
	TestEqual(TEXT("no bucket filling while holding"), G.FillBucket(ELotSource::Plant, 1.0), 0.0);
	TestEqual(TEXT("no trowel while holding"), G.Trowel(Aim, Aim + FVector2D(100.0, 0.0)).LitresPoured, 0.0);
	TestEqual(TEXT("no dumping while holding"), G.DumpBucket(Aim, From).LitresPoured, 0.0);
	TestEqual(TEXT("nothing was poured"), G.GetTotalLitres(), 0.0);

	G.Release(FVector2D(10.0, 20.0), 30.0);
	TestEqual(TEXT("hands free again"), G.GetHeld(), ELotMovable::Count);
	TestTrue(TEXT("scooping works again"), G.FillBucket(ELotSource::Plant, 1.0) > 0.0);

	FLotGame InCar;
	InCar.DebugGrantItem(ELotItem::Cart);
	InCar.World().bPlayerInCar = true;
	TestFalse(TEXT("no grabbing from the car"), InCar.Grab(ELotMovable::Cart));
	return true;
}

bool FLotMovableReleaseTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::Cart);
	const int32 Cart = static_cast<int32>(ELotMovable::Cart);
	G.Release(FVector2D(1.0, 2.0), 3.0);
	TestEqual(TEXT("release with nothing held changes nothing"), G.World().Movables[Cart].Pos, LotLayout::ItemSpawn(ELotItem::Cart));
	TestTrue(TEXT("grab"), G.Grab(ELotMovable::Cart));
	G.Release(FVector2D(100.0, -200.0), 45.0);
	TestEqual(TEXT("put down"), G.World().Movables[Cart].Pos, FVector2D(100.0, -200.0));
	TestEqual(TEXT("yaw"), G.World().Movables[Cart].YawDeg, 45.0);
	TestFalse(TEXT("not held"), G.World().Movables[Cart].bHeld);
	TestEqual(TEXT("nothing held"), G.GetHeld(), ELotMovable::Count);

	TestTrue(TEXT("grab again"), G.Grab(ELotMovable::Cart));
	G.Release(FVector2D(NaN, 5.0), 90.0);
	TestEqual(TEXT("bad position keeps the old one"), G.World().Movables[Cart].Pos, FVector2D(100.0, -200.0));
	TestEqual(TEXT("and still lets go"), G.GetHeld(), ELotMovable::Count);
	TestFalse(TEXT("flag cleared"), G.World().Movables[Cart].bHeld);
	return true;
}

bool FLotMovableCartFillTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("not owned: nothing moves"), G.FillMovable(ELotMovable::Cart, 10.0), 0.0);
	G.DebugGrantItem(ELotItem::Cart);
	TestEqual(TEXT("capacity"), G.GetMovableCapacity(ELotMovable::Cart), 200.0);
	TestEqual(TEXT("empty at first"), G.GetMovableLitres(ELotMovable::Cart), 0.0);
	TestNearlyEqual(TEXT("ten seconds at level 0"), G.FillMovable(ELotMovable::Cart, 10.0), 25.0, Tolerance);
	TestNearlyEqual(TEXT("stock"), G.GetPlantStock(), 55.0, Tolerance);
	TestNearlyEqual(TEXT("cart"), G.GetMovableLitres(ELotMovable::Cart), 25.0, Tolerance);
	TestEqual(TEXT("bad Dt"), G.FillMovable(ELotMovable::Cart, -1.0), 0.0);
	TestEqual(TEXT("NaN Dt"), G.FillMovable(ELotMovable::Cart, NaN), 0.0);
	TestEqual(TEXT("station has no content"), G.FillMovable(ELotMovable::Station, 10.0), 0.0);
	G.DebugGrantItem(ELotItem::Station);
	TestEqual(TEXT("owned station still has none"), G.FillMovable(ELotMovable::Station, 10.0), 0.0);
	TestEqual(TEXT("station litres"), G.GetMovableLitres(ELotMovable::Station), 0.0);

	G.DebugSetPlantStock(0.0);
	TestEqual(TEXT("empty stock gives nothing"), G.FillMovable(ELotMovable::Cart, 10.0), 0.0);

	FLotGame Full;
	Equip(Full, ELotItem::Cart);
	FillFull(Full, ELotMovable::Cart);
	TestNearlyEqual(TEXT("never above the capacity"), Full.GetMovableLitres(ELotMovable::Cart), 200.0, Tolerance);
	Full.DebugSetPlantStock(Full.GetPlantStorage());
	const double Before = Full.GetPlantStock();
	TestEqual(TEXT("full cart takes nothing"), Full.FillMovable(ELotMovable::Cart, 100.0), 0.0);
	TestEqual(TEXT("stock untouched"), Full.GetPlantStock(), Before);

	FLotGame Held;
	Held.DebugGrantItem(ELotItem::Cart);
	Held.Grab(ELotMovable::Cart);
	TestTrue(TEXT("a held cart can be filled"), Held.FillMovable(ELotMovable::Cart, 1.0) > 0.0);
	return true;
}

bool FLotMovableCartLays20CellsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Cart);
	FillFull(G, ELotMovable::Cart);
	FVector2D A, B;
	RowStrip(10, 40, 20, A, B);

	TestEqual(TEXT("not held: nothing is laid"), G.LayCartStrip(A, B).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("cart kept its content"), G.GetMovableLitres(ELotMovable::Cart), 200.0, Tolerance);
	G.Grab(ELotMovable::Cart);
	const FLotEvents Events = G.LayCartStrip(A, B);
	TestEqual(TEXT("raw texels"), RawTexelsInRow(G, 10, 20, 40), 8000);
	TestNearlyEqual(TEXT("litres poured"), Events.LitresPoured, 200.0, TexelTolerance);
	TestNearlyEqual(TEXT("the cart lost what was laid"), G.GetMovableLitres(ELotMovable::Cart), 200.0 - Events.LitresPoured, Tolerance);
	TestTrue(TEXT("empty cart"), G.GetMovableLitres(ELotMovable::Cart) < TexelTolerance);
	TestEqual(TEXT("raw earns nothing"), G.GetCredits(), 0.0);
	TestEqual(TEXT("no complete cells"), G.GetCompletedCells(), 0);
	TestEqual(TEXT("no cells reported"), Events.CompletedCells.Num(), 0);
	TestNearlyEqual(TEXT("total litres booked"), G.GetTotalLitres(), Events.LitresPoured, Tolerance);

	G.Release(A, 0.0);
	TestEqual(TEXT("empty cart lays nothing"), G.LayCartStrip(A, B).LitresPoured, 0.0);
	return true;
}

bool FLotMovableFinisherCompletesStripTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Cart);
	Equip(G, ELotItem::Finisher);
	FillFull(G, ELotMovable::Cart);
	FVector2D A, B;
	RowStrip(10, 40, 20, A, B);

	G.Grab(ELotMovable::Cart);
	G.LayCartStrip(A, B);
	G.Release(A, 0.0);
	TestEqual(TEXT("finisher not held: nothing happens"), G.FinisherPass(A, B).CompletedCells.Num(), 0);
	TestEqual(TEXT("still nothing complete"), G.GetCompletedCells(), 0);

	TestTrue(TEXT("grab the finisher"), G.Grab(ELotMovable::Finisher));
	const FLotEvents Events = G.FinisherPass(A, B);
	TestEqual(TEXT("cells reported"), Events.CompletedCells.Num(), 20);
	TestEqual(TEXT("complete cells"), G.GetCompletedCells(), 20);
	TestEqual(TEXT("credits"), G.GetCredits(), 200.0);
	TestEqual(TEXT("credits in the event"), Events.CreditsEarned, 200.0);
	TestEqual(TEXT("finishing costs nothing"), Events.LitresPoured, 0.0);
	TestEqual(TEXT("second pass finds nothing"), G.FinisherPass(A, B).CompletedCells.Num(), 0);
	TestEqual(TEXT("credits paid once"), G.GetCredits(), 200.0);

	FVector2D C, D;
	RowStrip(10, 60, 20, C, D);
	TestEqual(TEXT("empty ground: nothing"), G.FinisherPass(C, D).CompletedCells.Num(), 0);
	TestEqual(TEXT("credits unchanged"), G.GetCredits(), 200.0);
	return true;
}

bool FLotMovableSuitcaseDoseTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Suitcase);
	FillFull(G, ELotMovable::Suitcase);
	TestNearlyEqual(TEXT("full suitcase"), G.GetMovableLitres(ELotMovable::Suitcase), 60.0, Tolerance);
	TestEqual(TEXT("suitcase capacity"), G.GetMovableCapacity(ELotMovable::Suitcase), 60.0);
	const FVector2D Mouth = AsphaltLot::CellCentre(MiddleCell());
	const FVector2D From = Mouth + FVector2D(-100.0, 0.0);

	G.Grab(ELotMovable::Suitcase);
	TestEqual(TEXT("a held suitcase does not dose"), G.DoseSuitcase(Mouth, From, 5.0).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("content kept"), G.GetMovableLitres(ELotMovable::Suitcase), 60.0, Tolerance);
	G.Release(Mouth, 0.0);

	const FLotEvents Events = G.DoseSuitcase(Mouth, From, 5.0);
	TestNearlyEqual(TEXT("ten litres dosed"), Events.LitresPoured, 10.0, TexelTolerance);
	TestEqual(TEXT("400 raw texels"), G.GetGrid().GetCellRaw(MiddleCell()), 400);
	TestEqual(TEXT("no done texels"), G.GetGrid().GetCellDone(MiddleCell()), 0);
	TestNearlyEqual(TEXT("suitcase has 50 l"), G.GetMovableLitres(ELotMovable::Suitcase), 50.0, TexelTolerance);
	TestEqual(TEXT("raw earns nothing"), G.GetCredits(), 0.0);

	const double Before = G.GetMovableLitres(ELotMovable::Suitcase);
	const FLotEvents Again = G.DoseSuitcase(Mouth, From, 5.0);
	TestEqual(TEXT("a full cell takes nothing"), Again.LitresPoured, 0.0);
	TestEqual(TEXT("suitcase unchanged"), G.GetMovableLitres(ELotMovable::Suitcase), Before);

	TestEqual(TEXT("off the lot"), G.DoseSuitcase(FVector2D(1.0e6, 0.0), From, 5.0).LitresPoured, 0.0);
	TestEqual(TEXT("NaN mouth"), G.DoseSuitcase(FVector2D(NaN, 0.0), From, 5.0).LitresPoured, 0.0);
	TestEqual(TEXT("bad Dt"), G.DoseSuitcase(Mouth, From, -1.0).LitresPoured, 0.0);
	TestEqual(TEXT("suitcase still unchanged"), G.GetMovableLitres(ELotMovable::Suitcase), Before);

	// A cell that was full must not leave a stale budget for the next one.
	const FVector2D Next = AsphaltLot::CellCentre(MiddleCell(41, 40));
	const FLotEvents Fresh = G.DoseSuitcase(Next, From, 1.0);
	TestNearlyEqual(TEXT("a fresh cell gets exactly one second"), Fresh.LitresPoured, 2.0, TexelTolerance);

	FLotGame Unowned;
	TestEqual(TEXT("not owned"), Unowned.DoseSuitcase(Mouth, From, 5.0).LitresPoured, 0.0);
	return true;
}

bool FLotMovableTrowelFinishesDoseTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Suitcase);
	FillFull(G, ELotMovable::Suitcase);
	const int32 Cell = MiddleCell();
	const FVector2D Mouth = AsphaltLot::CellCentre(Cell);
	G.DoseSuitcase(Mouth, Mouth + FVector2D(-100.0, 0.0), 5.0);
	TestFalse(TEXT("raw cell is not complete"), G.GetGrid().IsCellComplete(Cell));

	G.SelectHand(ELotHand::Trowel);
	G.Tick(2.0);
	const FVector2D A = Mouth + FVector2D(-AsphaltLot::CellCm * 0.5 + LotBalance::StampPadCm, 0.0);
	const FVector2D B = Mouth + FVector2D(AsphaltLot::CellCm * 0.5 - LotBalance::StampPadCm, 0.0);
	FLotEvents Events;
	for (int32 I = 0; I < 4; ++I)
	{
		// The trowel is 30 cm wide: sweep four lanes over the cell.
		const FVector2D Lane(0.0, (I - 1.5) * LotBalance::TrowelWidthCm);
		Events.Append(G.Trowel(A + Lane, B + Lane));
	}
	TestTrue(TEXT("the cell is complete"), G.GetGrid().IsCellComplete(Cell));
	TestEqual(TEXT("one cell reported"), Events.CompletedCells.Num(), 1);
	TestEqual(TEXT("ten credits"), G.GetCredits(), 10.0);
	return true;
}

bool FLotMovableBucketFromSuitcaseTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Suitcase);
	G.DebugGrantItem(ELotItem::Bucket);
	TestEqual(TEXT("empty suitcase gives nothing"), G.FillBucket(ELotSource::Suitcase, 1.0), 0.0);
	FillFull(G, ELotMovable::Suitcase);
	TestNearlyEqual(TEXT("one second of tapping"), G.FillBucket(ELotSource::Suitcase, 1.0), 5.0, Tolerance);
	TestNearlyEqual(TEXT("bucket content"), G.GetHandLitres(ELotHand::Bucket), 5.0, Tolerance);
	TestNearlyEqual(TEXT("suitcase lost it"), G.GetMovableLitres(ELotMovable::Suitcase), 55.0, Tolerance);
	TestNearlyEqual(TEXT("only what fits"), G.FillBucket(ELotSource::Suitcase, 10.0), 5.0, Tolerance);
	TestEqual(TEXT("full bucket takes nothing"), G.FillBucket(ELotSource::Suitcase, 1.0), 0.0);
	TestNearlyEqual(TEXT("suitcase"), G.GetMovableLitres(ELotMovable::Suitcase), 50.0, Tolerance);

	TestEqual(TEXT("cart not owned"), G.FillBucket(ELotSource::Cart, 1.0), 0.0);
	G.DebugGrantItem(ELotItem::Cart);
	TestEqual(TEXT("empty cart gives nothing"), G.FillBucket(ELotSource::Cart, 1.0), 0.0);

	FLotGame H;
	Equip(H, ELotItem::Suitcase);
	H.DebugGrantItem(ELotItem::Bucket);
	FillFull(H, ELotMovable::Suitcase);
	H.Grab(ELotMovable::Suitcase);
	TestEqual(TEXT("a held suitcase cannot be tapped"), H.FillBucket(ELotSource::Suitcase, 1.0), 0.0);

	FLotGame K;
	Equip(K, ELotItem::Suitcase);
	K.DebugGrantItem(ELotItem::Bucket);
	FillFull(K, ELotMovable::Suitcase);
	K.SelectHand(ELotHand::Bucket);
	K.Tick(LotBalance::SwapSeconds);
	TestNearlyEqual(TEXT("the bucket in hand is tapped too"), K.FillBucket(ELotSource::Suitcase, 1.0), 5.0, Tolerance);
	TestNearlyEqual(TEXT("... and holds it"), K.GetHandLitres(ELotHand::Bucket), 5.0, Tolerance);
	return true;
}

bool FLotMovableDoseCarriesRestTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Suitcase);
	FillFull(G, ELotMovable::Suitcase);
	const FVector2D Mouth = AsphaltLot::CellCentre(MiddleCell());
	const FVector2D From = Mouth + FVector2D(-100.0, 0.0);
	// 60 calls of 1/60 s earn 2 l; every call alone is below one texel's price at most of the time.
	double Poured = 0.0;
	for (int32 I = 0; I < 60; ++I)
	{
		Poured += G.DoseSuitcase(Mouth, From, 1.0 / 60.0).LitresPoured;
	}
	TestNearlyEqual(TEXT("two litres in one second"), Poured, 2.0, TexelTolerance);
	TestNearlyEqual(TEXT("suitcase lost exactly what was laid"), G.GetMovableLitres(ELotMovable::Suitcase), 60.0 - Poured, Tolerance);
	TestEqual(TEXT("raw texels match the litres"), G.GetGrid().GetCellRaw(MiddleCell()), FMath::RoundToInt32(Poured / FLotGrid::LitresPerTexel));
	return true;
}

bool FLotMovableLitresConservedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	for (ELotItem Item : {ELotItem::Suitcase, ELotItem::Cart, ELotItem::Finisher, ELotItem::Bucket})
	{
		G.DebugGrantItem(Item);
	}
	G.DebugSetPlantLevel(5);
	G.DebugSetPlantStock(0.0);

	const double Rate = LotBalance::PlantRateLPerMin[5] / 60.0;
	double Produced = 0.0;
	auto Whole = [&G]()
	{
		double Sum = G.GetPlantStock() + G.GetTotalLitres();
		for (ELotMovable Movable : {ELotMovable::Suitcase, ELotMovable::Cart})
		{
			Sum += G.GetMovableLitres(Movable);
		}
		for (ELotHand Item : {ELotHand::Bowl, ELotHand::Scoop, ELotHand::Bucket})
		{
			Sum += G.GetHandLitres(Item);
		}
		return Sum;
	};
	auto Check = [&](const TCHAR* What)
	{
		TestNearlyEqual(What, Whole(), Produced, 1e-6);
	};
	auto Produce = [&](double Seconds)
	{
		G.Tick(Seconds);
		Produced += Rate * Seconds;
	};

	Produce(10.0);
	Check(TEXT("after production"));
	G.ScoopFromTrough(3.0);
	Check(TEXT("after scooping"));
	G.FillMovable(ELotMovable::Suitcase, 2.0);
	G.FillMovable(ELotMovable::Cart, 3.0);
	Check(TEXT("after filling"));
	G.FillBucket(ELotSource::Suitcase, 1.0);
	G.FillBucket(ELotSource::Plant, 0.5);
	Check(TEXT("after tapping"));

	const FVector2D Mouth = AsphaltLot::CellCentre(MiddleCell());
	for (int32 I = 0; I < 90; ++I)
	{
		G.DoseSuitcase(Mouth, Mouth + FVector2D(-100.0, 0.0), 1.0 / 60.0);
		Check(TEXT("during dosing"));
	}
	G.DoseSuitcase(AsphaltLot::CellCentre(MiddleCell(42, 40)), Mouth, 7.0);
	Check(TEXT("after the big dose"));

	FVector2D A, B;
	RowStrip(10, 50, 6, A, B);
	G.Grab(ELotMovable::Cart);
	G.LayCartStrip(A, B);
	Check(TEXT("after the cart strip"));
	G.LayCartStrip(A, B);
	Check(TEXT("after a repeated strip"));
	G.Release(A, 0.0);
	G.Grab(ELotMovable::Finisher);
	G.FinisherPass(A, B);
	Check(TEXT("after finishing"));
	G.Release(A, 0.0);
	Produce(30.0);
	G.FillMovable(ELotMovable::Cart, 100.0);
	Check(TEXT("after refilling"));

	TestTrue(TEXT("litres are on the ground"), G.GetTotalLitres() > 0.0);
	TestTrue(TEXT("no negative content"), G.GetMovableLitres(ELotMovable::Suitcase) >= 0.0 && G.GetMovableLitres(ELotMovable::Cart) >= 0.0 && G.GetPlantStock() >= 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
