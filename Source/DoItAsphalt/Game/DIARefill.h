// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotGame.h"
#include "LotLayout.h"

/**
 * Where asphalt can be drawn from by hand: the trough of the plant and the portable station once it is set down. The
 * places are the trough's and the station's position in the core; the distances are LotLayout's (no number is kept here).
 */
namespace DIARefill
{
	/** True if the owned portable station stands on the lot (is not held). */
	inline bool IsStationPlaced(const FLotGame& Game)
	{
		return Game.OwnsMovable(ELotMovable::Station) && Game.GetHeld() != ELotMovable::Station;
	}

	/** True if the point is within Radius of the trough or of the set down station. */
	inline bool IsNearOutlet(const FLotGame& Game, FVector2D Point, double Radius)
	{
		if (FVector2D::Distance(Point, LotLayout::TroughPos) <= Radius)
		{
			return true;
		}
		return IsStationPlaced(Game) && FVector2D::Distance(Point, Game.World().Movables[static_cast<int32>(ELotMovable::Station)].Pos) <= Radius;
	}

	/** The aim point is within TroughReachCm of the trough or of the set down station: the bucket fills there, and the player may stand beside it. */
	inline bool IsAtOutlet(const FLotGame& Game, FVector2D Aim)
	{
		return IsNearOutlet(Game, Aim, LotLayout::TroughReachCm);
	}

	/** The position is within RefillRadiusCm of the same places: the standing trolley and cart fill there. */
	inline bool IsInZone(const FLotGame& Game, FVector2D Pos)
	{
		return IsNearOutlet(Game, Pos, LotLayout::RefillRadiusCm);
	}
}
