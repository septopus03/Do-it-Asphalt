// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltLayout.h"
#include "AsphaltBalance.h"
#include "HexMap.h"

namespace AsphaltLayout
{
	FVector2D RoadEnd(int32 Road)
	{
		return AsphaltHex::ToWorld(FHexMap::RoadAnchor(Road)) + FHexMap::RoadDirection(Road) * RoadEndOutCm;
	}

	FVector2D KioskPos(int32 Road)
	{
		const FVector2D Dir = FHexMap::RoadDirection(Road);
		const FVector2D Right(-Dir.Y, Dir.X);
		return RoadEnd(Road) + Right * KioskSideCm;
	}

	FVector2D GatePos(int32 Road)
	{
		return FHexMap::RoadDirection(Road) * SiteRadiusCm;
	}

	FVector2D ExitTrigger()
	{
		return FHexMap::RoadDirection(AsphaltBalance::ExitRoad) * (SiteRadiusCm + ExitTriggerRadiusCm);
	}

	FVector2D SpecialSpawn(int32 Road)
	{
		return AsphaltHex::ToWorld(FHexMap::RoadAnchor(Road)) - FHexMap::RoadDirection(Road) * SpecialSpawnBackCm;
	}
}
