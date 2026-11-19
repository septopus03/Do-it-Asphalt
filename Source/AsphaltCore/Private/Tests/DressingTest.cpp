// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltDressing.h"
#include "AsphaltLayout.h"
#include "HexMap.h"

DIA_TEST(FDressingTest, "DoItAsphalt.Core.Dressing")

namespace
{
	/** Largest angle in degrees that a lamp may sit away from the exact corner of the lot (the corner by the plant slides). */
	constexpr double DressingTestMaxLampSlideDeg = 25.0;
	/** Tolerance of the distance of a lamp from the lot's outline, in cm. */
	constexpr double DressingTestLampToleranceCm = 0.5;
	constexpr double DressingTestYawToleranceDeg = 1e-6;

	double DressingTestSegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / AB.SizeSquared(), 0.0, 1.0);
		return FVector2D::Distance(P, A + AB * T);
	}
}

bool FDressingTest::RunTest(const FString& Parameters)
{
	const FHexMap Map;
	const TArray<FDressingItem> Items = AsphaltDressing::Build(Map);

	// Counts per kind.
	int32 Counts[4] = {0, 0, 0, 0};
	for (const FDressingItem& Item : Items)
	{
		++Counts[static_cast<int32>(Item.Kind)];
	}
	TestEqual(TEXT("total"), Items.Num(), 64);
	TestEqual(TEXT("cones"), Counts[static_cast<int32>(EDressingKind::Cone)], 30);
	TestEqual(TEXT("barrels"), Counts[static_cast<int32>(EDressingKind::Barrel)], 20);
	TestEqual(TEXT("lamps"), Counts[static_cast<int32>(EDressingKind::Lamp)], 6);
	TestEqual(TEXT("dirt piles"), Counts[static_cast<int32>(EDressingKind::DirtPile)], 8);

	// Deterministic: a second call, even with a new map, gives exactly the same items.
	const FHexMap OtherMap;
	const TArray<FDressingItem> Again = AsphaltDressing::Build(OtherMap);
	bool bSame = Again.Num() == Items.Num();
	for (int32 I = 0; bSame && I < Items.Num(); ++I)
	{
		bSame = Again[I].Kind == Items[I].Kind && Again[I].Pos == Items[I].Pos && Again[I].YawDeg == Items[I].YawDeg;
	}
	TestTrue(TEXT("two calls give the same result"), bSame);

	// Every constraint, checked for every item.
	TArray<FVector2D> Spots;
	Spots.Add(AsphaltLayout::RefillCenter);
	Spots.Add(AsphaltLayout::PlayerStart);
	for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
	{
		Spots.Add(AsphaltLayout::KioskPos(Road));
		Spots.Add(AsphaltLayout::GatePos(Road));
		Spots.Add(AsphaltLayout::SpecialSpawn(Road));
	}

	int32 OnCell = 0;
	int32 NearCell = 0;
	int32 NearSpot = 0;
	int32 NearPlant = 0;
	int32 NearRoad = 0;
	int32 TooFar = 0;
	int32 TooClose = 0;
	int32 BadYaw = 0;
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		const FDressingItem& Item = Items[I];
		if (Map.FindCell(AsphaltHex::FromWorld(Item.Pos)) != INDEX_NONE)
		{
			++OnCell;
		}
		// 150 cm from the outline of a cell is the circumradius (the side) plus 150 cm from its centre.
		for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
		{
			if (FVector2D::Distance(Item.Pos, AsphaltHex::ToWorld(Map.GetCell(Cell).Coord)) <= 150.0 + AsphaltHex::SideCm)
			{
				++NearCell;
				break;
			}
		}
		for (const FVector2D& Spot : Spots)
		{
			if (FVector2D::Distance(Item.Pos, Spot) <= 400.0)
			{
				++NearSpot;
				break;
			}
		}
		if (FVector2D::Distance(Item.Pos, AsphaltLayout::PlantPos) <= 700.0)
		{
			++NearPlant;
		}
		for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
		{
			if (DressingTestSegmentDistance(Item.Pos, AsphaltLayout::RoadEnd(Road), AsphaltLayout::GatePos(Road)) <= 250.0)
			{
				++NearRoad;
				break;
			}
		}
		if (Item.Pos.Size() > AsphaltLayout::SiteRadiusCm - 100.0)
		{
			++TooFar;
		}
		for (int32 J = I + 1; J < Items.Num(); ++J)
		{
			if (FVector2D::Distance(Item.Pos, Items[J].Pos) < 120.0)
			{
				++TooClose;
			}
		}
		if (!FMath::IsFinite(Item.YawDeg) || Item.YawDeg < 0.0 || Item.YawDeg >= 360.0)
		{
			++BadYaw;
		}
	}
	TestEqual(TEXT("items on a cell"), OnCell, 0);
	TestEqual(TEXT("items within 150 cm of a cell"), NearCell, 0);
	TestEqual(TEXT("items within 400 cm of the refill centre, a kiosk, a gate, a special spawn or the player start"), NearSpot, 0);
	TestEqual(TEXT("items within 700 cm of the plant"), NearPlant, 0);
	TestEqual(TEXT("items within 250 cm of a road axis"), NearRoad, 0);
	TestEqual(TEXT("items beyond SiteRadius - 100"), TooFar, 0);
	TestEqual(TEXT("pairs closer than 120 cm"), TooClose, 0);
	TestEqual(TEXT("items with a yaw outside 0..360"), BadYaw, 0);

	// Lamps: 250 cm beyond the outline of the lot at (or, by the plant, beside) a corner, with the arm towards the lot.
	const double CornerCentreRadius = AsphaltHex::ToWorld(AsphaltHex::Directions[0] * FHexMap::LotRadius).Size();
	const double ExpectedRadius = CornerCentreRadius + AsphaltHex::SideCm * FMath::Sqrt(3.0) * 0.5 + 250.0;
	int32 CornersWithLamp = 0;
	for (int32 Corner = 0; Corner < 6; ++Corner)
	{
		const FVector2D CornerCentre = AsphaltHex::ToWorld(AsphaltHex::Directions[Corner] * FHexMap::LotRadius);
		int32 Near = 0;
		for (const FDressingItem& Item : Items)
		{
			if (Item.Kind != EDressingKind::Lamp)
			{
				continue;
			}
			const double Between = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(CornerCentre.GetSafeNormal(), Item.Pos.GetSafeNormal()), -1.0, 1.0)));
			Near += Between <= DressingTestMaxLampSlideDeg;
		}
		CornersWithLamp += Near == 1;
	}
	TestEqual(TEXT("every corner of the lot has exactly one lamp"), CornersWithLamp, 6);
	for (const FDressingItem& Item : Items)
	{
		if (Item.Kind != EDressingKind::Lamp)
		{
			continue;
		}
		TestEqual(TEXT("lamp radius: 250 cm beyond the lot's outline"), Item.Pos.Size(), ExpectedRadius, DressingTestLampToleranceCm);
		// The lamp's arm (local +Y, turned by the yaw) points at the middle of the lot.
		const double Yaw = FMath::DegreesToRadians(Item.YawDeg);
		const FVector2D Arm(-FMath::Sin(Yaw), FMath::Cos(Yaw));
		TestEqual(TEXT("lamp faces the lot"), FVector2D::DotProduct(Arm, -Item.Pos.GetSafeNormal()), 1.0, DressingTestYawToleranceDeg);
	}
	return true;
}
