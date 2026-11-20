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
	/** The paver truck fills its tank when its centre is within RefillRadiusCm plus this of a refill point. */
	inline constexpr double CarRefillExtraRadiusCm = 200.0;

	inline const FVector2D PlayerStart{-1600.0, 0.0};
	/** The player starts facing the plant (-X), so that the camera behind the player looks at it from the lot side instead of sitting inside it. */
	inline constexpr double PlayerStartYawDeg = 180.0;

	/** Radius of the fence ring around the site. */
	inline constexpr double SiteRadiusCm = 2600.0;

	/** The road end lies this far beyond the road anchor. */
	inline constexpr double RoadEndOutCm = 300.0;
	/** Special objects spawn this far before the road anchor (on the lot side). */
	inline constexpr double SpecialSpawnBackCm = 300.0;
	/** The kiosk stands this far to the right of the road end. */
	inline constexpr double KioskSideCm = 220.0;
	/** Radius of the exit trigger sphere. */
	inline constexpr double ExitTriggerRadiusCm = 150.0;

	/** Outer end of a road: ToWorld(RoadAnchor) + RoadDirection * RoadEndOutCm. */
	ASPHALTCORE_API FVector2D RoadEnd(int32 Road);

	/** Kiosk beside the road end: RoadEnd + Right * KioskSideCm, where Right = (-Dir.Y, Dir.X). */
	ASPHALTCORE_API FVector2D KioskPos(int32 Road);

	/** Gate in the fence line: RoadDirection * SiteRadiusCm. */
	ASPHALTCORE_API FVector2D GatePos(int32 Road);

	/** Centre of the exit trigger (radius ExitTriggerRadiusCm): RoadDirection(5) * (SiteRadiusCm + ExitTriggerRadiusCm). */
	ASPHALTCORE_API FVector2D ExitTrigger();

	/** Spawn point of special objects on the lot: ToWorld(RoadAnchor) - RoadDirection * SpecialSpawnBackCm. */
	ASPHALTCORE_API FVector2D SpecialSpawn(int32 Road);
}
