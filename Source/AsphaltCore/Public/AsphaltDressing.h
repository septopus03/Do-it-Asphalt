// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FHexMap;

enum class EDressingKind : uint8
{
	Cone,
	Barrel,
	Lamp,
	DirtPile,
};

/** One piece of site dressing: what it is, where it stands on the ground (cm) and how it is turned (degrees about Z, 0..360). */
struct FDressingItem
{
	EDressingKind Kind = EDressingKind::Cone;
	FVector2D Pos = FVector2D::ZeroVector;
	double YawDeg = 0.0;
};

/**
 * The props that stand loosely between the lot and the fence (traffic cones, barrels, heaps of dirt and floodlights).
 * They are scenery only: the layout is a pure function of the map and a fixed seed, so every game and every test sees the same site.
 */
namespace AsphaltDressing
{
	inline constexpr int32 NumLamps = 6;
	inline constexpr int32 NumDirtPiles = 8;
	inline constexpr int32 NumBarrels = 20;
	inline constexpr int32 NumCones = 30;
	inline constexpr int32 NumItems = NumLamps + NumDirtPiles + NumBarrels + NumCones;

	/** A lamp stands this far beyond the edge of the lot, on the line from the centre through a corner of the lot. */
	inline constexpr double LampBeyondEdgeCm = 250.0;

	// What an item keeps away from (centre of the item to the thing; the bigger props add their own radius on top while placing).
	/** Distance from the outline of any map cell. */
	inline constexpr double CellClearCm = 150.0;
	/** Distance from the refill centre, the player start, and every kiosk, gate and special spawn point. */
	inline constexpr double SpotClearCm = 400.0;
	/** Distance from the plant. */
	inline constexpr double PlantClearCm = 700.0;
	/** Distance from the axis of each road between its end and its gate. */
	inline constexpr double RoadClearCm = 250.0;
	/** Distance from the fence circle inwards: nothing stands further out than SiteRadiusCm minus this. */
	inline constexpr double FenceMarginCm = 100.0;
	/** Distance between any two items. */
	inline constexpr double MinSpacingCm = 120.0;

	/** The layout: NumItems items, always the same for the same map (no random state outside this call). */
	ASPHALTCORE_API TArray<FDressingItem> Build(const FHexMap& Map);
}
