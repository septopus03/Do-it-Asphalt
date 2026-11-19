// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "AsphaltLayout.h"

/** Where asphalt can be taken from the plant: shared by everything that refills (the character's backpack, the truck's tank). */
namespace DIARefill
{
	/**
	 * True within AsphaltLayout::RefillRadiusCm (plus ExtraRadiusCm) of the plant's refill point, or of the portable station once it is
	 * owned and put down (a carried one fills nothing). The extra radius lets a large body count by its centre.
	 */
	inline bool IsInZone(const FAsphaltGame& Game, const FVector2D& Position, double ExtraRadiusCm = 0.0)
	{
		const double Radius = AsphaltLayout::RefillRadiusCm + ExtraRadiusCm;
		if (FVector2D::Distance(Position, AsphaltLayout::RefillCenter) <= Radius)
		{
			return true;
		}
		const FAsphaltWorldState& State = Game.World();
		return Game.HasSpecial(EAsphaltSpecial::PortableStation) && !State.bStationCarried &&
			FVector2D::Distance(Position, State.StationPos) <= Radius;
	}
}
