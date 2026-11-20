// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/DIAModelSizes.h"
#include "DIALotEdge.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/** Presentation constants of the car park's edge and the strip around it (nothing here is a game rule). */
namespace DIASite
{
	/** Length of one kerb piece and of one fence panel along the edge. */
	inline const double CurbPitchCm = DIAModelSizes::CurbCm.Y;
	inline const double FencePitchCm = DIAModelSizes::FenceCm.Y;
	/** The kerb stops this far either side of the trough, and of the entries 3, 4 and 5. */
	inline constexpr double CurbTroughGapHalfCm = 300.0;
	inline constexpr double CurbEntryGapHalfCm = 200.0;
	/** The fence stops this far either side of the gate. */
	inline constexpr double GateGapHalfCm = 200.0;

	/** White frame of an entry: width of the stripe, height of its underside above the ground, thickness. */
	inline constexpr double FrameWidthCm = 5.0;
	inline constexpr double FrameLiftCm = 0.5;
	inline constexpr double FrameThicknessCm = 0.2;

	/** Invisible walls: thickness (so that nothing tunnels through) and height. Their inner face is where the fence panels stand. */
	inline constexpr double WallThicknessCm = 100.0;
	inline constexpr double WallHeightCm = 600.0;

	/** A straight stretch along an edge: the pieces tile between Start and End, Outward points away from the car park. */
	struct FRun
	{
		FVector2D Start;
		FVector2D End;
		FVector2D Outward;

		double Length() const { return FVector2D::Distance(Start, End); }
		FVector2D Direction() const { return (End - Start).GetSafeNormal(); }
	};

	/** Whole pieces of the pitch that fit a stretch (at least one); they are spread evenly over it, a little stretched or squeezed. */
	inline int32 NumPieces(double LengthCm, double PitchCm)
	{
		return FMath::Max(1, FMath::RoundToInt(LengthCm / PitchCm));
	}

	/** Stretches of the car park's four outer sides that get a kerb: whole sides minus the gaps at the trough and the entries 3, 4, 5. */
	DOITASPHALT_API TArray<FRun> CurbRuns();
	/** Stretches of the fence along the outer edge of the strip: whole sides minus the gap at the gate. */
	DOITASPHALT_API TArray<FRun> FenceRuns();
}

/**
 * Everything at the edge of the car park that is not a machine: kerb (instanced SM_Curb, bevel towards the car park), fence
 * (instanced SM_Fence) along the outer edge of the strip, the invisible walls that keep the player on the site (along the
 * fence line and along the strip's edges at the shopping centre), the old road (SM_RoadStub) and SM_Barrier at the entries
 * 3, 4 and 5 (at the gate, the barrier stands in the fence gap until the gate replaces it), and the white frame of each of the
 * six entry segments on the ground. Only the walls and the barriers block the player; none of it stands on the car park.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIALotEdge : public AActor
{
	GENERATED_BODY()

public:
	ADIALotEdge();

	/** Kerb pieces and fence panels the actor builds (the tiling of CurbRuns / FenceRuns). */
	static int32 NumCurbs();
	static int32 NumFencePanels();

	UInstancedStaticMeshComponent* GetCurbs() const { return Curbs; }
	UInstancedStaticMeshComponent* GetFence() const { return Fence; }
	/** The white frames: four flat boxes for each entry, in the order of the entries. */
	UInstancedStaticMeshComponent* GetFrames() const { return Frames; }
	const TArray<TObjectPtr<UBoxComponent>>& GetWalls() const { return Walls; }
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetStubs() const { return Stubs; }
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetBarriers() const { return Barriers; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildCurbs();
	void BuildFence();
	void BuildFrames();
	void BuildWalls();
	void BuildRoadEnds();

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Curbs;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Fence;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Frames;

	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Walls;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Stubs;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Barriers;
};
