// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotLayout.h"
#include "LotMap.h"

DIA_TEST(FLotLayoutShopsTest, "DoItAsphalt.Lot.Layout.Shops")
DIA_TEST(FLotLayoutGarageOnStationLaneTest, "DoItAsphalt.Lot.Layout.GarageOnStationLane")

namespace
{
	/** Completes the entry segment of the step and enough cells, then gives plenty of credits. */
	void OpenUnlock(FLotGame& G, ELotUnlock Unlock)
	{
		G.DebugGive(1000000.0);
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			G.DebugCompleteCell(AsphaltLot::EntryCell(static_cast<int32>(Unlock), I));
		}
		G.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(Unlock)]);
	}
}

/** The car leaves the garage straight through the vehicle station, so the bay axis must be the station's lane axis. */
bool FLotLayoutGarageOnStationLaneTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("garage bay axis is the station lane axis"), LotLayout::ShopPos(ELotShop::Garage).Y, LotLayout::VehicleStationPos.Y);
	TestTrue(TEXT("the garage lies west of the station"), LotLayout::ShopPos(ELotShop::Garage).X < LotLayout::VehicleStationPos.X);
	return true;
}

bool FLotLayoutShopsTest::RunTest(const FString& Parameters)
{
	constexpr double Tol = 0.01;
	TestTrue(TEXT("tools buy point"), LotLayout::ShopPoint(ELotShop::Tools).Equals(FVector2D(1030.0, -10278.0), Tol));
	TestTrue(TEXT("robots buy point"), LotLayout::ShopPoint(ELotShop::Robots).Equals(FVector2D(1555.0, -10365.0), Tol));
	TestTrue(TEXT("garage buy point"), LotLayout::ShopPoint(ELotShop::Garage).Equals(FVector2D(-2570.0, -10155.0), Tol));

	TestTrue(TEXT("dock 0"), LotLayout::PaverDock(0).Equals(FVector2D(2020.0, -10585.0), Tol));
	TestTrue(TEXT("dock 1"), LotLayout::PaverDock(1).Equals(FVector2D(1860.0, -10585.0), Tol));
	TestTrue(TEXT("dock 2"), LotLayout::PaverDock(2).Equals(FVector2D(1700.0, -10585.0), Tol));
	TestTrue(TEXT("dock 3 is zero"), LotLayout::PaverDock(3).IsZero());
	TestTrue(TEXT("dock -1 is zero"), LotLayout::PaverDock(-1).IsZero());

	for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
	{
		const double Y = LotLayout::ShopPos(static_cast<ELotShop>(I)).Y;
		TestTrue(*FString::Printf(TEXT("shop %d in the south strip"), I), Y > -AsphaltLot::HalfExtentCm - LotLayout::StripCm && Y < -AsphaltLot::HalfExtentCm);
	}

	TestTrue(TEXT("car spawn"), LotLayout::ItemSpawn(ELotItem::Car).Equals(FVector2D(-3000.0, -10450.0), Tol));
	TestTrue(TEXT("robot 2 spawns at dock 1"), LotLayout::ItemSpawn(ELotItem::AutoPaver2).Equals(LotLayout::PaverDock(1), Tol));
	TestTrue(TEXT("robot 3 spawns at dock 2"), LotLayout::ItemSpawn(ELotItem::AutoPaver3).Equals(LotLayout::PaverDock(2), Tol));
	FLotGame C;
	OpenUnlock(C, ELotUnlock::Car);
	C.World().CarYawDeg = 77.0;
	TestTrue(TEXT("car bought"), C.BuyItem(ELotItem::Car));
	TestTrue(TEXT("bought car at the garage"), C.World().CarPos.Equals(LotLayout::ItemSpawn(ELotItem::Car), Tol));
	TestEqual(TEXT("bought car yaw"), C.World().CarYawDeg, LotLayout::CarSpawnYawDeg);

	FLotGame P;
	OpenUnlock(P, ELotUnlock::AutoPaver);
	TestTrue(TEXT("robot bought"), P.BuyItem(ELotItem::AutoPaver));
	TestTrue(TEXT("bought robot at dock 0"), P.GetAutoPaverPos(0).Equals(LotLayout::PaverDock(0), Tol));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
