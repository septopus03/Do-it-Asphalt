// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotLayout.h"
#include "LotMap.h"

DIA_TEST(FLotHandSpoonStrokesTest, "DoItAsphalt.Lot.Hand.SpoonStrokes")
DIA_TEST(FLotHandScoopIsFasterTest, "DoItAsphalt.Lot.Hand.ScoopIsFaster")
DIA_TEST(FLotHandScoopNeedsStockTest, "DoItAsphalt.Lot.Hand.ScoopNeedsStock")
DIA_TEST(FLotHandSwapDelayTest, "DoItAsphalt.Lot.Hand.SwapDelay")
DIA_TEST(FLotHandPourSmallStepsTest, "DoItAsphalt.Lot.Hand.PourSmallSteps")
DIA_TEST(FLotHandNothingLeavesOnOccupiedTest, "DoItAsphalt.Lot.Hand.NothingLeavesOnOccupied")
DIA_TEST(FLotHandFirstSquareTest, "DoItAsphalt.Lot.Hand.FirstSquare")
DIA_TEST(FLotHandBucketDumpTest, "DoItAsphalt.Lot.Hand.BucketDump")
DIA_TEST(FLotHandDebugFillTest, "DoItAsphalt.Lot.Hand.DebugFill")

namespace
{
	constexpr double Tolerance = 1e-6;
	/** One texel's price: the accuracy of anything measured in poured litres. */
	constexpr double TexelTolerance = FLotGrid::LitresPerTexel;

	/** Fills the bowl in hand from the plant stock. */
	void FillBowl(FLotGame& G)
	{
		G.ScoopFromTrough(10.0);
	}

	/** Centre of an ordinary lot cell away from the edges of the car park. */
	FVector2D MiddleOfLot()
	{
		return AsphaltLot::CellCentre(AsphaltLot::CellIndex(40, 40));
	}
}

bool FLotHandSpoonStrokesTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("new game holds the bowl"), G.GetHand(), ELotHand::Bowl);
	TestEqual(TEXT("bowl starts empty"), G.GetHandLitres(ELotHand::Bowl), 0.0);
	TestEqual(TEXT("bowl capacity"), G.GetHandCapacity(ELotHand::Bowl), 2.5);
	TestEqual(TEXT("no stroke yet"), G.ScoopFromTrough(LotBalance::ScoopCycleSeconds - 0.01), 0.0);
	TestNearlyEqual(TEXT("first stroke"), G.ScoopFromTrough(0.02), 0.25, Tolerance);
	TestNearlyEqual(TEXT("stock after the stroke"), G.GetPlantStock(), 79.75, Tolerance);
	G.ScoopFromTrough(9.0 * LotBalance::ScoopCycleSeconds + 0.1);
	TestNearlyEqual(TEXT("bowl full after ten strokes"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
	TestEqual(TEXT("full bowl takes nothing"), G.ScoopFromTrough(1.0), 0.0);
	TestNearlyEqual(TEXT("stock"), G.GetPlantStock(), 77.5, Tolerance);
	return true;
}

bool FLotHandScoopIsFasterTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugGrantItem(ELotItem::Scoop);
	TestNearlyEqual(TEXT("stroke 1"), G.ScoopFromTrough(0.6), 1.0, Tolerance);
	TestNearlyEqual(TEXT("stroke 2"), G.ScoopFromTrough(0.6), 1.0, Tolerance);
	TestNearlyEqual(TEXT("stroke 3 fills the bowl"), G.ScoopFromTrough(0.6), 0.5, Tolerance);
	TestEqual(TEXT("bowl full"), G.ScoopFromTrough(0.6), 0.0);
	return true;
}

bool FLotHandScoopNeedsStockTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugSetPlantStock(0.0);
	TestEqual(TEXT("empty stock"), G.ScoopFromTrough(1.0), 0.0);
	G.DebugSetPlantStock(0.1);
	TestNearlyEqual(TEXT("only what the stock has"), G.ScoopFromTrough(0.6), 0.1, Tolerance);
	TestEqual(TEXT("stock used up"), G.GetPlantStock(), 0.0);
	return true;
}

bool FLotHandSwapDelayTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	FillBowl(G);
	TestFalse(TEXT("no bucket"), G.SelectHand(ELotHand::Bucket));
	TestFalse(TEXT("no scoop"), G.CanSelectHand(ELotHand::Scoop));
	G.DebugGrantItem(ELotItem::Scoop);
	TestTrue(TEXT("the scoop once bought"), G.CanSelectHand(ELotHand::Scoop));
	TestTrue(TEXT("same item at once"), G.SelectHand(ELotHand::Bowl));
	TestFalse(TEXT("same item costs no time"), G.IsHandBusy());

	TestTrue(TEXT("take the scoop"), G.SelectHand(ELotHand::Scoop));
	TestTrue(TEXT("swap takes time"), G.IsHandBusy());
	TestFalse(TEXT("no second swap meanwhile"), G.SelectHand(ELotHand::Bowl));
	G.Tick(1.5);
	TestFalse(TEXT("swap done"), G.IsHandBusy());
	TestEqual(TEXT("scoop in hand"), G.GetHand(), ELotHand::Scoop);

	TestTrue(TEXT("back to the bowl"), G.SelectHand(ELotHand::Bowl));
	TestTrue(TEXT("busy again"), G.IsHandBusy());
	TestNearlyEqual(TEXT("the bowl kept its content"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
	const FVector2D Aim = MiddleOfLot();
	const FLotEvents Busy = G.PourHand(Aim, Aim + FVector2D(-100.0, 0.0), 1.0);
	TestEqual(TEXT("busy hands pour nothing"), Busy.LitresPoured, 0.0);
	TestNearlyEqual(TEXT("bowl untouched"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
	G.Tick(1.5);
	const FLotEvents Free = G.PourHand(Aim, Aim + FVector2D(-100.0, 0.0), 1.0);
	TestTrue(TEXT("free hands pour"), Free.LitresPoured > 0.0);

	FLotGame Quick;
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		Quick.DebugCompleteCell(AsphaltLot::EntryCell(0, I));
	}
	TestTrue(TEXT("quick swap unlocked"), Quick.IsUnlocked(ELotUnlock::QuickSwap));
	Quick.DebugGrantItem(ELotItem::Scoop);
	TestTrue(TEXT("quick swap selects"), Quick.SelectHand(ELotHand::Scoop));
	TestFalse(TEXT("quick swap costs no time"), Quick.IsHandBusy());
	return true;
}

bool FLotHandPourSmallStepsTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	FillBowl(G);
	FVector2D Aim = AsphaltLot::CellCentre(AsphaltLot::CellIndex(20, 20));
	double Poured = 0.0;
	for (int32 I = 0; I < 60; ++I)
	{
		Poured += G.PourHand(Aim, Aim + FVector2D(-100.0, 0.0), 1.0 / 60.0).LitresPoured;
		Aim.X += 8.0;
	}
	TestNearlyEqual(TEXT("one litre in one second"), Poured, 1.0, TexelTolerance);
	TestNearlyEqual(TEXT("bowl lost what was poured"), G.GetHandLitres(ELotHand::Bowl), 2.5 - Poured, Tolerance);
	TestNearlyEqual(TEXT("total litres"), G.GetTotalLitres(), Poured, Tolerance);
	return true;
}

bool FLotHandNothingLeavesOnOccupiedTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	FillBowl(G);
	const FVector2D Done = MiddleOfLot();
	G.GetGrid().Stamp(Done - FVector2D(40.0, 0.0), Done + FVector2D(40.0, 0.0), 80.0, 100.0);
	const FVector2D Targets[2] = {Done, FVector2D(5000.0, 5000.0)};
	const TCHAR* Names[2] = {TEXT("finished texels"), TEXT("off the lot")};
	for (int32 I = 0; I < 2; ++I)
	{
		const FLotEvents E = G.PourHand(Targets[I], Targets[I] + FVector2D(-100.0, 0.0), 1.0);
		TestEqual(FString::Printf(TEXT("%s: nothing poured"), Names[I]), E.LitresPoured, 0.0);
		TestNearlyEqual(FString::Printf(TEXT("%s: bowl unchanged"), Names[I]), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
	}
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("bad aim"), G.PourHand(FVector2D(NaN, 0.0), Done, 1.0).LitresPoured, 0.0);
	TestEqual(TEXT("bad Dt"), G.PourHand(MiddleOfLot() + FVector2D(0.0, 300.0), Done, NaN).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("bowl unchanged"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
	return true;
}

bool FLotHandFirstSquareTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	const int32 Cell = LotLayout::FirstCell();
	const FVector2D Centre = AsphaltLot::CellCentre(Cell);
	// 5 x 5 aim points; none is closer than PourRadiusCm[Bowl] to the edge of the cell, so no pour leaves it.
	const double Offsets[5] = {-35.0, -17.5, 0.0, 17.5, 35.0};

	for (int32 BowlNo = 0; BowlNo < 4; ++BowlNo)
	{
		FillBowl(G);
		TestNearlyEqual(TEXT("bowl filled"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
		for (int32 Pass = 0; Pass < 100 && G.GetHandLitres(ELotHand::Bowl) > 0.0 && !G.GetGrid().IsCellComplete(Cell); ++Pass)
		{
			for (double Y : Offsets)
			{
				for (double X : Offsets)
				{
					const FVector2D Aim = Centre + FVector2D(X, Y);
					G.PourHand(Aim, Aim + FVector2D(-100.0, 0.0), 1.0);
				}
			}
		}
	}
	TestTrue(TEXT("first cell complete"), G.GetGrid().IsCellComplete(Cell));
	TestEqual(TEXT("only that cell"), G.GetCompletedCells(), 1);
	TestNearlyEqual(TEXT("credits: ten for the cell, one a litre"), G.GetCredits(), 10.0 + 9.5, Tolerance);
	TestNearlyEqual(TEXT("half a litre left in the fourth bowl"), G.GetHandLitres(ELotHand::Bowl), 0.5, TexelTolerance);
	TestNearlyEqual(TEXT("litres used"), G.GetTotalLitres(), 9.5, TexelTolerance);
	return true;
}

bool FLotHandBucketDumpTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	TestEqual(TEXT("no bucket yet"), G.FillBucket(ELotSource::Plant, 4.0), 0.0);
	G.DebugGrantItem(ELotItem::Bucket);
	TestNearlyEqual(TEXT("plant fills 2.5 l/s"), G.FillBucket(ELotSource::Plant, 4.0), 10.0, Tolerance);
	TestNearlyEqual(TEXT("bucket full"), G.GetHandLitres(ELotHand::Bucket), 10.0, Tolerance);
	TestNearlyEqual(TEXT("stock"), G.GetPlantStock(), 70.0, Tolerance);
	TestEqual(TEXT("full bucket takes nothing"), G.FillBucket(ELotSource::Plant, 1.0), 0.0);

	const FVector2D Aim = MiddleOfLot();
	const FVector2D From = Aim + FVector2D(-100.0, 0.0);
	TestTrue(TEXT("take the bucket"), G.SelectHand(ELotHand::Bucket));
	TestEqual(TEXT("busy hands dump nothing"), G.DumpBucket(Aim, From).LitresPoured, 0.0);
	G.Tick(1.5);
	TestEqual(TEXT("off the lot nothing leaves"), G.DumpBucket(FVector2D(5000.0, 5000.0), From).LitresPoured, 0.0);
	TestNearlyEqual(TEXT("bucket unchanged"), G.GetHandLitres(ELotHand::Bucket), 10.0, Tolerance);

	const FLotEvents E = G.DumpBucket(Aim, From);
	TestTrue(TEXT("cell complete"), G.GetGrid().IsCellComplete(AsphaltLot::CellAt(Aim)));
	TestNearlyEqual(TEXT("credits: ten for the cell, one a litre"), E.CreditsEarned, 10.0 + E.LitresPoured, Tolerance);
	TestNearlyEqual(TEXT("half a litre stays"), G.GetHandLitres(ELotHand::Bucket), 0.5, TexelTolerance);
	const FLotEvents Again = G.DumpBucket(Aim, From);
	TestEqual(TEXT("complete cell takes nothing"), Again.LitresPoured, 0.0);
	TestNearlyEqual(TEXT("bucket keeps the rest"), G.GetHandLitres(ELotHand::Bucket), 0.5, TexelTolerance);

	FLotGame Other;
	Other.DebugGrantItem(ELotItem::Bucket);
	Other.SelectHand(ELotHand::Bucket);
	TestEqual(TEXT("busy hands fill nothing"), Other.FillBucket(ELotSource::Plant, 1.0), 0.0);
	Other.Tick(LotBalance::SwapSeconds);
	TestNearlyEqual(TEXT("bucket in hand fills at the dispense rate"), Other.FillBucket(ELotSource::Plant, 1.0), LotBalance::DispenseRateLPerS(0), Tolerance);
	TestNearlyEqual(TEXT("... and holds it"), Other.GetHandLitres(ELotHand::Bucket), LotBalance::DispenseRateLPerS(0), Tolerance);
	return true;
}

bool FLotHandDebugFillTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	G.DebugFillHand();
	TestNearlyEqual(TEXT("bowl filled to capacity"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);
	TestEqual(TEXT("the stock is not touched"), G.GetPlantStock(), LotBalance::PlantStartStockL);

	G.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), G.SelectHand(ELotHand::Bucket));
	TestEqual(TEXT("the bucket is empty"), G.GetHandLitres(ELotHand::Bucket), 0.0);
	G.DebugFillHand();
	TestNearlyEqual(TEXT("bucket filled to capacity"), G.GetHandLitres(ELotHand::Bucket), 10.0, Tolerance);
	TestNearlyEqual(TEXT("the bowl keeps its own content"), G.GetHandLitres(ELotHand::Bowl), 2.5, Tolerance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
