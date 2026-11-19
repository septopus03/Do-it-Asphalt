// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Fixed layout of the asphalt plant site around the lot (world coordinates in cm, origin at the lot centre). */
namespace AsphaltLayout
{
	/** Position of the asphalt plant; it faces +X. */
	inline const FVector2D PlantPos{-2000.0, 0.0};

	/** Centre and radius of the area where the player refills material. */
	inline const FVector2D RefillCenter{-1600.0, 0.0};
	inline constexpr double RefillRadiusCm = 200.0;

	inline const FVector2D PlayerStart{-1600.0, 0.0};

	/** Radius of the fence ring around the site. */
	inline constexpr double SiteRadiusCm = 2600.0;

	/** Outer end of a road: ToWorld(RoadAnchor) + RoadDirection * 300. */
	ASPHALTCORE_API FVector2D RoadEnd(int32 Road);

	/** Kiosk beside the road end: RoadEnd + Right * 220, where Right = (-Dir.Y, Dir.X). */
	ASPHALTCORE_API FVector2D KioskPos(int32 Road);

	/** Gate in the fence line: RoadDirection * SiteRadiusCm. */
	ASPHALTCORE_API FVector2D GatePos(int32 Road);

	/** Centre of the exit trigger (radius 150): RoadDirection(5) * (SiteRadiusCm + 150). */
	ASPHALTCORE_API FVector2D ExitTrigger();

	/** Spawn point of special objects on the lot: ToWorld(RoadAnchor) - RoadDirection * 300. */
	ASPHALTCORE_API FVector2D SpecialSpawn(int32 Road);
}
