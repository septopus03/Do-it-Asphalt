// Copyright Do it Asphalt. All Rights Reserved.

#include "HexCoord.h"

namespace AsphaltHex
{
	const FHexCoord Directions[6] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}};

	int32 Distance(FHexCoord A, FHexCoord B)
	{
		const int32 DQ = A.Q - B.Q;
		const int32 DR = A.R - B.R;
		return (FMath::Abs(DQ) + FMath::Abs(DR) + FMath::Abs(DQ + DR)) / 2;
	}

	FVector2D ToWorld(FHexCoord C)
	{
		const double Sqrt3 = FMath::Sqrt(3.0);
		return FVector2D(Sqrt3 * SideCm * (C.Q + C.R * 0.5), 1.5 * SideCm * C.R);
	}

	FHexCoord FromWorld(FVector2D P)
	{
		const double Sqrt3 = FMath::Sqrt(3.0);
		const double FracQ = (Sqrt3 / 3.0 * P.X - P.Y / 3.0) / SideCm;
		const double FracR = (2.0 / 3.0 * P.Y) / SideCm;
		const double FracS = -FracQ - FracR;

		// Cube rounding: round all three axes, then fix the one with the largest rounding error.
		double Q = FMath::RoundToDouble(FracQ);
		double R = FMath::RoundToDouble(FracR);
		const double S = FMath::RoundToDouble(FracS);

		const double DQ = FMath::Abs(Q - FracQ);
		const double DR = FMath::Abs(R - FracR);
		const double DS = FMath::Abs(S - FracS);

		if (DQ > DR && DQ > DS)
		{
			Q = -R - S;
		}
		else if (DR > DS)
		{
			R = -Q - S;
		}
		return FHexCoord{static_cast<int32>(Q), static_cast<int32>(R)};
	}

	double EdgeDistanceCm(FVector2D P)
	{
		const double Sqrt3 = FMath::Sqrt(3.0);
		const double Apothem = Sqrt3 * 0.5 * SideCm;
		const FVector2D Local = P - ToWorld(FromWorld(P));

		// Pointy-top hexagon: edge normals point at 0, 60, 120 ... degrees (opposite ones are covered by Abs).
		const double Along0 = FMath::Abs(Local.X);
		const double Along60 = FMath::Abs(0.5 * Local.X + 0.5 * Sqrt3 * Local.Y);
		const double Along120 = FMath::Abs(-0.5 * Local.X + 0.5 * Sqrt3 * Local.Y);
		const double Farthest = FMath::Max3(Along0, Along60, Along120);
		return FMath::Max(0.0, Apothem - Farthest);
	}
}
