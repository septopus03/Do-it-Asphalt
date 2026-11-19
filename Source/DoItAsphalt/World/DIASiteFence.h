// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DIASiteFence.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** The ring around the site: a circle of AsphaltLayout::SiteRadiusCm with a gap at each of the six roads. */
namespace DIASite
{
	/** The invisible wall is cut from this many equal slots of the circle (every 5 degrees, the first centred on +X). */
	inline constexpr int32 WallSlots = 72;
	/** Width of the gap at each road, across the road; the barriers and the exit gate fill it. */
	inline constexpr double GapWidthCm = 380.0;
	/** The wall is thick enough that nothing tunnels through it and tall enough that nothing jumps over it. */
	inline constexpr double WallThicknessCm = 40.0;
	inline constexpr double WallHeightCm = 400.0;
	/** Slivers of a slot left over by a gap that are shorter than this are dropped. */
	inline constexpr double MinWallArcDeg = 0.05;

	/** A stretch of the circle in degrees, counter-clockwise; EndDeg may exceed 360 where the stretch crosses +X. */
	struct FArc
	{
		double StartDeg = 0.0;
		double EndDeg = 0.0;

		double LengthDeg() const { return EndDeg - StartDeg; }
		double MidDeg() const { return (StartDeg + EndDeg) * 0.5; }
	};

	/** Half the angle of a gap as seen from the middle of the lot. */
	DOITASPHALT_API double GapHalfAngleDeg();
	/** The six stretches of the circle between the gaps, sorted by their start angle. */
	DOITASPHALT_API TArray<FArc> FenceArcs();
	/** The wall: every slot of WallSlots that is not wholly inside a gap, cut where a gap begins or ends. */
	DOITASPHALT_API TArray<FArc> WallArcs();
}

/**
 * The boundary of the site (GDD chapter 5: the player cannot leave except by the exit): a ring of fence panels (SM_Fence,
 * one every 2 m along the circle) and, in the same places, an invisible wall of box colliders that blocks pawns but not the
 * camera. Both leave a 380 cm gap at each road; the barriers (ADIARoadActor) close five of them and the exit gate the sixth.
 * Built once in BeginPlay; the fence has no state.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIASiteFence : public AActor
{
	GENERATED_BODY()

public:
	ADIASiteFence();

	int32 GetNumPanels() const { return Panels.Num(); }
	UStaticMeshComponent* GetPanel(int32 Index) const { return Panels.IsValidIndex(Index) ? Panels[Index].Get() : nullptr; }
	int32 GetNumWallSegments() const { return Walls.Num(); }
	UBoxComponent* GetWallSegment(int32 Index) const { return Walls.IsValidIndex(Index) ? Walls[Index].Get() : nullptr; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildPanels();
	void BuildWall();

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Walls;
};
