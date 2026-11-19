// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "HexCoord.h"

DIA_TEST(FHexAreaTest, "DoItAsphalt.Core.Hex.Area")
DIA_TEST(FHexDistanceTest, "DoItAsphalt.Core.Hex.Distance")
DIA_TEST(FHexToWorldTest, "DoItAsphalt.Core.Hex.ToWorld")
DIA_TEST(FHexRoundTripTest, "DoItAsphalt.Core.Hex.RoundTrip")
DIA_TEST(FHexEdgeDistanceTest, "DoItAsphalt.Core.Hex.EdgeDistance")
DIA_TEST(FHexOperatorsTest, "DoItAsphalt.Core.Hex.Operators")

bool FHexAreaTest::RunTest(const FString& Parameters)
{
	// A regular hexagon with side s has area (3 * sqrt(3) / 2) * s^2; the cell must be 1 m^2 = 10000 cm^2.
	TestEqual(TEXT("area"), 1.5 * FMath::Sqrt(3.0) * AsphaltHex::SideCm * AsphaltHex::SideCm, 10000.0, 1.0);
	return true;
}

bool FHexDistanceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("d1"), AsphaltHex::Distance({0, 0}, {12, -6}), 12);
	TestEqual(TEXT("d2"), AsphaltHex::Distance({13, -7}, {0, 0}), 13);
	TestEqual(TEXT("d3"), AsphaltHex::Distance({-3, 5}, {2, -1}), 6);
	return true;
}

bool FHexToWorldTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("x"), AsphaltHex::ToWorld({1, 0}).Equals(FVector2D(107.457, 0.0), 0.01));
	TestTrue(TEXT("y"), AsphaltHex::ToWorld({0, 1}).Equals(FVector2D(53.7285, 93.0605), 0.01));
	return true;
}

bool FHexRoundTripTest::RunTest(const FString& Parameters)
{
	int32 Failures = 0;
	for (int32 Q = -15; Q <= 15; ++Q)
	{
		for (int32 R = -15; R <= 15; ++R)
		{
			const FHexCoord C{Q, R};
			const FVector2D Center = AsphaltHex::ToWorld(C);
			if (!(AsphaltHex::FromWorld(Center) == C))
			{
				++Failures;
			}
			if (!(AsphaltHex::FromWorld(Center + FVector2D(30, 20)) == C))
			{
				++Failures;
			}
		}
	}
	TestEqual(TEXT("round-trip failures"), Failures, 0);
	return true;
}

bool FHexEdgeDistanceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("center"), AsphaltHex::EdgeDistanceCm(AsphaltHex::ToWorld({3, -2})), 53.7285, 0.01);
	TestEqual(TEXT("edge"), AsphaltHex::EdgeDistanceCm(AsphaltHex::ToWorld({3, -2}) + FVector2D(53.7285, 0)), 0.0, 0.01);
	return true;
}

bool FHexOperatorsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("add"), FHexCoord{1, 2} + FHexCoord{3, -5} == FHexCoord{4, -3});
	TestTrue(TEXT("mul"), FHexCoord{2, -3} * 4 == FHexCoord{8, -12});
	TestTrue(TEXT("neq"), !(FHexCoord{1, 2} == FHexCoord{2, 1}));
	TestEqual(TEXT("hash equal"), GetTypeHash(FHexCoord{7, -3}), GetTypeHash(FHexCoord{7, -3}));
	TestTrue(TEXT("hash differs"), GetTypeHash(FHexCoord{1, 2}) != GetTypeHash(FHexCoord{2, 1}));

	// Directions are the six unit neighbours at distance 1 and sum to zero.
	FHexCoord Sum;
	for (const FHexCoord& D : AsphaltHex::Directions)
	{
		TestEqual(TEXT("direction distance"), AsphaltHex::Distance({0, 0}, D), 1);
		Sum = Sum + D;
	}
	TestTrue(TEXT("directions sum to zero"), Sum == FHexCoord{0, 0});
	return true;
}
