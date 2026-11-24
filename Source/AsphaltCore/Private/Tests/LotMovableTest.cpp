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
DIA_TEST(FLotMovableTrolleyLaysStripTest, "DoItAsphalt.Lot.Movable.TrolleyLaysStrip")
DIA_TEST(FLotMovableBucketFromTrolleyTest, "DoItAsphalt.Lot.Movable.BucketFromTrolley")
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

	int32 DoneTexelsInRow(const FLotGame& G, int32 Col, int32 Count, int32 Row)
	{
		int32 Sum = 0;
		for (int32 I = 0; I < Count; ++I)
		{
			Sum += G.GetGrid().GetCellDone(MiddleCell(Col + I, Row));
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
	TestFalse(TEXT("trolley is not"), G.OwnsMovable(ELotMovable::Trolley));
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
	G.DebugGrantItem(ELotItem::Trolley);
	G.DebugGrantItem(ELotItem::Bucket);
	G.DebugGrantItem(ELotItem::Canister);
	TestFalse(TEXT("invalid movable"), G.Grab(ELotMovable::Count));
	G.ScoopFromTrough(10.0);
	const double BowlBefore = G.GetHandLitres(ELotHand::Bowl);
	const double StockBefore = G.GetPlantStock();
	TestTrue(TEXT("grab the cart"), G.Grab(ELotMovable::Cart));
	TestEqual(TEXT("cart held"), G.GetHeld(), ELotMovable::Cart);
	TestTrue(TEXT("flag set"), G.World().Movables[static_cast<int32>(ELotMovable::Cart)].bHeld);
	TestFalse(TEXT("no second one"), G.Grab(ELotMovable::Trolley));
	TestFalse(TEXT("not the same again"), G.Grab(ELotMovable::Cart));

	const FVector2D Aim = AsphaltLot::CellCentre(MiddleCell());
	const FVector2D From = Aim + FVector2D(-100.0, 0.0);
	TestEqual(TEXT("no pour while holding"), G.PourHand(Aim, From, 1.0).LitresPoured, 0.0);
	TestEqual(TEXT("no scooping while holding"), G.ScoopFromTrough(10.0), 0.0);
	TestNearlyEqual(TEXT("bowl untouched"), G.GetHandLitres(ELotHand::Bowl), BowlBefore, Tolerance);
	TestNearlyEqual(TEXT("stock untouched"), G.GetPlantStock(), StockBefore, Tolerance);
	TestEqual(TEXT("no bucket filling while holding"), G.FillVessel(ELotHand::Bucket, ELotSource::Plant, 1.0), 0.0);
	TestEqual(TEXT("no dumping while holding"), G.DumpVessel(Aim, From).LitresPoured, 0.0);
	TestEqual(TEXT("nothing was poured"), G.GetTotalLitres(), 0.0);

	G.Release(FVector2D(10.0, 20.0), 30.0);
	TestEqual(TEXT("hands free again"), G.GetHeld(), ELotMovable::Count);
	TestTrue(TEXT("scooping works again"), G.FillVessel(ELotHand::Bucket, ELotSource::Plant, 1.0) > 0.0);

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

	TestEqual(TEXT("not held: nothing is laid"), G.LayStrip(A, B).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("cart kept its content"), G.GetMovableLitres(ELotMovable::Cart), 200.0, Tolerance);
	G.Grab(ELotMovable::Cart);
	const FLotEvents Events = G.LayStrip(A, B);
	TestEqual(TEXT("done texels"), DoneTexelsInRow(G, 10, 20, 40), 8000);
	// Each cell is done at 380 of its 400 texels (9.5 l); the free rest of a completed cell is not paid for.
	TestNearlyEqual(TEXT("litres poured"), Events.LitresPoured, 20 * 9.5, TexelTolerance);
	TestNearlyEqual(TEXT("the cart lost what was laid"), G.GetMovableLitres(ELotMovable::Cart), 200.0 - Events.LitresPoured, Tolerance);
	TestNearlyEqual(TEXT("ten litres are left in the cart"), G.GetMovableLitres(ELotMovable::Cart), 10.0, TexelTolerance);
	TestEqual(TEXT("20 complete cells"), G.GetCompletedCells(), 20);
	TestEqual(TEXT("20 cells reported"), Events.CompletedCells.Num(), 20);
	TestNearlyEqual(TEXT("credits: a litre each and ten a cell"), G.GetCredits(), Events.LitresPoured * LotBalance::CreditsPerLitre + 20 * LotBalance::CreditsPerCell, Tolerance);
	TestNearlyEqual(TEXT("total litres booked"), G.GetTotalLitres(), Events.LitresPoured, Tolerance);

	TestEqual(TEXT("a second pass over finished cells lays nothing"), G.LayStrip(A, B).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("the cart keeps its rest"), G.GetMovableLitres(ELotMovable::Cart), 10.0, TexelTolerance);

	G.Release(A, 0.0);
	TestEqual(TEXT("not held again: nothing is laid"), G.LayStrip(A, B).LitresPoured, 0.0);
	return true;
}

bool FLotMovableTrolleyLaysStripTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Trolley);
	FillFull(G, ELotMovable::Trolley);
	TestNearlyEqual(TEXT("full trolley"), G.GetMovableLitres(ELotMovable::Trolley), 80.0, Tolerance);
	TestEqual(TEXT("trolley capacity"), G.GetMovableCapacity(ELotMovable::Trolley), 80.0);
	FVector2D A, B;
	RowStrip(10, 40, 10, A, B);

	TestEqual(TEXT("a standing trolley lays nothing"), G.LayStrip(A, B).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("content kept"), G.GetMovableLitres(ELotMovable::Trolley), 80.0, Tolerance);
	G.Grab(ELotMovable::Trolley);
	const FLotEvents Events = G.LayStrip(A, B);
	// 45 cm of the 100 cm of a cell: nine texel rows of twenty, 4.5 l a cell, none of them complete.
	TestNearlyEqual(TEXT("litres of ten cells of the narrow strip"), Events.LitresPoured, 10 * 4.5, 10 * TexelTolerance * 20);
	TestEqual(TEXT("no cell is complete"), G.GetCompletedCells(), 0);
	TestNearlyEqual(TEXT("the trolley lost what was laid"), G.GetMovableLitres(ELotMovable::Trolley), 80.0 - Events.LitresPoured, Tolerance);
	TestNearlyEqual(TEXT("credits: a litre each"), G.GetCredits(), Events.LitresPoured * LotBalance::CreditsPerLitre, Tolerance);
	TestEqual(TEXT("a second pass over the same strip lays nothing"), G.LayStrip(A, B).LitresPoured, 0.0);

	// The tank is the budget: a long strip stops when the trolley is empty.
	FVector2D C, D;
	RowStrip(10, 50, 60, C, D);
	const FLotEvents Long = G.LayStrip(C, D);
	TestNearlyEqual(TEXT("the rest of the trolley"), Long.LitresPoured, 80.0 - Events.LitresPoured, TexelTolerance);
	TestNearlyEqual(TEXT("empty"), G.GetMovableLitres(ELotMovable::Trolley), 0.0, TexelTolerance);

	FLotGame Unowned;
	TestEqual(TEXT("not owned"), Unowned.LayStrip(A, B).LitresPoured, 0.0);
	return true;
}

bool FLotMovableBucketFromTrolleyTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	Equip(G, ELotItem::Trolley);
	G.DebugGrantItem(ELotItem::Bucket);
	TestEqual(TEXT("empty trolley gives nothing"), G.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 1.0), 0.0);
	FillFull(G, ELotMovable::Trolley);
	TestNearlyEqual(TEXT("one second of tapping"), G.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 1.0), 5.0, Tolerance);
	TestNearlyEqual(TEXT("bucket content"), G.GetHandLitres(ELotHand::Bucket), 5.0, Tolerance);
	TestNearlyEqual(TEXT("trolley lost it"), G.GetMovableLitres(ELotMovable::Trolley), 75.0, Tolerance);
	TestNearlyEqual(TEXT("only what fits"), G.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 10.0), 5.0, Tolerance);
	TestEqual(TEXT("full bucket takes nothing"), G.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 1.0), 0.0);
	TestNearlyEqual(TEXT("trolley"), G.GetMovableLitres(ELotMovable::Trolley), 70.0, Tolerance);

	TestEqual(TEXT("cart not owned"), G.FillVessel(ELotHand::Bucket, ELotSource::Cart, 1.0), 0.0);
	G.DebugGrantItem(ELotItem::Cart);
	TestEqual(TEXT("empty cart gives nothing"), G.FillVessel(ELotHand::Bucket, ELotSource::Cart, 1.0), 0.0);

	FLotGame H;
	Equip(H, ELotItem::Trolley);
	H.DebugGrantItem(ELotItem::Bucket);
	FillFull(H, ELotMovable::Trolley);
	H.Grab(ELotMovable::Trolley);
	TestEqual(TEXT("a held trolley cannot be tapped"), H.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 1.0), 0.0);

	FLotGame K;
	Equip(K, ELotItem::Trolley);
	K.DebugGrantItem(ELotItem::Bucket);
	FillFull(K, ELotMovable::Trolley);
	K.SelectHand(ELotHand::Bucket);
	K.Tick(LotBalance::SwapSeconds);
	TestNearlyEqual(TEXT("the bucket in hand is tapped too"), K.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 1.0), 5.0, Tolerance);
	TestNearlyEqual(TEXT("... and holds it"), K.GetHandLitres(ELotHand::Bucket), 5.0, Tolerance);
	return true;
}

bool FLotMovableLitresConservedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	for (ELotItem Item : {ELotItem::Trolley, ELotItem::Cart, ELotItem::Station, ELotItem::Bucket})
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
		for (ELotMovable Movable : {ELotMovable::Trolley, ELotMovable::Cart})
		{
			Sum += G.GetMovableLitres(Movable);
		}
		for (ELotHand Item : {ELotHand::Bowl, ELotHand::Canister, ELotHand::Bucket})
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
	G.FillMovable(ELotMovable::Trolley, 2.0);
	G.FillMovable(ELotMovable::Cart, 3.0);
	Check(TEXT("after filling"));
	G.FillVessel(ELotHand::Bucket, ELotSource::Trolley, 1.0);
	G.FillVessel(ELotHand::Bucket, ELotSource::Plant, 0.5);
	Check(TEXT("after tapping"));

	FVector2D A, B;
	RowStrip(30, 40, 8, A, B);
	G.Grab(ELotMovable::Trolley);
	G.LayStrip(A, B);
	Check(TEXT("after the trolley strip"));
	G.Release(A, 0.0);

	RowStrip(10, 50, 6, A, B);
	G.Grab(ELotMovable::Cart);
	G.LayStrip(A, B);
	Check(TEXT("after the cart strip"));
	G.LayStrip(A, B);
	Check(TEXT("after a repeated strip"));
	G.Release(A, 0.0);
	Produce(30.0);
	G.FillMovable(ELotMovable::Cart, 100.0);
	Check(TEXT("after refilling"));

	TestTrue(TEXT("litres are on the ground"), G.GetTotalLitres() > 0.0);
	TestTrue(TEXT("no negative content"), G.GetMovableLitres(ELotMovable::Trolley) >= 0.0 && G.GetMovableLitres(ELotMovable::Cart) >= 0.0 && G.GetPlantStock() >= 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
