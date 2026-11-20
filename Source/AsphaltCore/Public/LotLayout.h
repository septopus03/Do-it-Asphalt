// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"

/** Fixed positions around the car park (world coordinates in cm, origin at the lot centre). */
namespace LotLayout
{
	/** The trough and the plant outlet; the long side of the trough lies along X. */
	inline const FVector2D TroughPos{-50.0, -10080.0};
	/** The asphalt plant. */
	inline const FVector2D PlantPos{-50.0, -10420.0};
	/** The plant outlet (model +X) points towards +Y, to the car park. */
	inline constexpr double PlantYawDeg = 90.0;
	/** Refilling works within this radius of the trough and of a placed dispenser. */
	inline constexpr double RefillRadiusCm = 200.0;
	/** An aim point closer than this to TroughPos means scooping. */
	inline constexpr double TroughReachCm = 120.0;
	/** Where the player starts. */
	inline const FVector2D PlayerStart{-50.0, -9850.0};
	/** The player starts looking at the trough. */
	inline constexpr double PlayerStartYawDeg = -90.0;
	/** Width of the strip of ground beyond the outer sides of the car park. */
	inline constexpr double StripCm = 1200.0;
	/** Centre of the shopping-centre quadrant. */
	inline const FVector2D MallCentre{5000.0, 5000.0};
	/** Entry segment that leads to the exit. */
	inline constexpr int32 ExitEntry = 5;
	/** Radius of the exit trigger. */
	inline constexpr double ExitTriggerRadiusCm = 150.0;

	/** The first cell to pave: AsphaltLot::CellIndex(99, 0). */
	ASPHALTCORE_API int32 FirstCell();

	/** Gate in the strip beyond the exit entry: EntryCentre(ExitEntry) + Outward * StripCm. */
	ASPHALTCORE_API FVector2D GatePos();

	/** Centre of the exit trigger: GatePos + Outward * ExitTriggerRadiusCm. */
	ASPHALTCORE_API FVector2D ExitTrigger();

	/** Shop board beside an entry (sections 0, 3, 4, 5): EntryCentre + Outward * 150 + Right * 450, Right = (-Out.Y, Out.X). */
	ASPHALTCORE_API FVector2D BoardPos(int32 Entry);

	/** Where a bought item appears; items without a place of their own appear at PlayerStart. */
	ASPHALTCORE_API FVector2D ItemSpawn(ELotItem Item);
}
