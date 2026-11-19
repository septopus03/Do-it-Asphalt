// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Axial coordinates of a cell on the pointy-top hexagonal grid (cube coordinate S = -Q - R is implicit). */
struct ASPHALTCORE_API FHexCoord
{
	int32 Q = 0;
	int32 R = 0;
};

inline FHexCoord operator+(FHexCoord A, FHexCoord B)
{
	return FHexCoord{A.Q + B.Q, A.R + B.R};
}

inline FHexCoord operator*(FHexCoord A, int32 K)
{
	return FHexCoord{A.Q * K, A.R * K};
}

inline bool operator==(FHexCoord A, FHexCoord B)
{
	return A.Q == B.Q && A.R == B.R;
}

inline uint32 GetTypeHash(FHexCoord C)
{
	return HashCombineFast(::GetTypeHash(C.Q), ::GetTypeHash(C.R));
}

namespace AsphaltHex
{
	/** Side length of one cell in centimetres; chosen so that the cell area is 1 m^2. */
	inline constexpr double SideCm = 62.0403;

	/** The six neighbour offsets: {1,0},{1,-1},{0,-1},{-1,0},{-1,1},{0,1}. */
	ASPHALTCORE_API extern const FHexCoord Directions[6];

	/** Number of steps between two cells: (|dq| + |dr| + |dq+dr|) / 2. */
	ASPHALTCORE_API int32 Distance(FHexCoord A, FHexCoord B);

	/** World position (cm) of the cell centre: X = sqrt(3) * Side * (Q + R/2), Y = 1.5 * Side * R. */
	ASPHALTCORE_API FVector2D ToWorld(FHexCoord C);

	/** Cell containing the world position (cm), using cube rounding. */
	ASPHALTCORE_API FHexCoord FromWorld(FVector2D P);

	/** Distance (cm) from the world position to the nearest edge of the cell containing it. */
	ASPHALTCORE_API double EdgeDistanceCm(FVector2D P);
}
