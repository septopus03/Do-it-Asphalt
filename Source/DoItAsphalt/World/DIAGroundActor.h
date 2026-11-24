// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "LotBalance.h"
#include "LotGrid.h"
#include "LotMap.h"
#include "DIAGroundActor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

/** Rendering constants of the ground (nothing here is a game rule; those live in LotBalance / LotLayout). */
namespace DIAGround
{
	/** Side of the textured plot: exactly the area the coverage grid describes. */
	inline constexpr double PlotSizeCm = 2.0 * AsphaltLot::HalfExtentCm;
	/** The untextured soil around the plot reaches this far from the centre (112 m). */
	inline constexpr double SurroundHalfExtentCm = 11200.0;
	/** The surround lies this much below the plot so that the two never fight over the same depth. */
	inline constexpr double SurroundDropCm = 2.0;
	/** Thickness of the collision slab below Z = 0. */
	inline constexpr double CollisionDepthCm = 100.0;
	/** An upload of at least this many texels is logged at Log level; smaller ones only at VeryVerbose. */
	inline constexpr int64 BulkyUploadTexels = 100000;
	/** A sector's side in cm: the rectangle the auto paver's outline is drawn around. */
	inline constexpr double SectorSizeCm = AsphaltLot::SectorSide * AsphaltLot::CellCm;
	/** The width of an outline drawn as strips (the second and third robot's), the same as the material's one for the first. */
	inline constexpr double OutlineWidthCm = 10.0;
}

/**
 * The ground: one 200 x 200 m plane whose material shows soil, the cell grid lines and the asphalt (CoverageTex,
 * mirrored from FLotGrid as an R8G8 texture: R = raw or done, G = done), surrounded by plain soil. The actor listens
 * to the world subsystem and uploads only the changed rectangles of the coverage texture (the whole of it when a
 * game is loaded or restarted). Texture X follows world X and texture row Y follows world Y, exactly like the grid.
 * The actor keeps a CPU copy of the texture, because a -nullrhi run cannot read a GPU texture back.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGroundActor : public AActor
{
	GENERATED_BODY()

public:
	ADIAGroundActor();

	/** Uploads the rectangles of the grid changed since the last call (the whole grid if bFull). */
	void SyncCoverage(bool bFull = false);

	/** Reads the CPU copy of the coverage texture. False if the texel is outside the texture or there is no copy yet. */
	bool GetCoverageTexel(int32 X, int32 Y, uint8& OutR, uint8& OutG) const;

	/**
	 * The check view: 1 turns every part of the lot without finished asphalt (bare or raw) orange, 0 shows the ground as it is.
	 * Applied to the dynamic material (the value is kept for a material that is made later).
	 */
	void SetMissingHighlight(float Amount);
	float GetMissingHighlight() const { return MissingHighlight; }

	/**
	 * The outline of every robot's sector, set when a robot's sector changes. The first robot's is the material's vector parameter
	 * SectorRect (min X, min Y, max X, max Y in world cm; a zero rectangle, no sector, draws nothing); the material has one such
	 * parameter only, so the second and third robot's are four thin strips each, lying on the ground.
	 */
	void SyncSectorRect();

	/** The rectangle (min X, min Y, max X, max Y) the robot's outline shows now; all zero if it has no sector. */
	FVector4 GetPaverOutline(int32 Paver) const;
	/** One of the four strips of the second or third robot's outline (null for the first robot, whose outline is in the material). */
	const UStaticMeshComponent* GetOutlineStrip(int32 Paver, int32 Side) const;

	UTexture2D* GetCoverageTexture() const { return CoverageTexture; }
	UMaterialInstanceDynamic* GetGroundMaterial() const { return GroundMaterial; }
	/** How often the whole texture was uploaded since play began (a loaded or restarted game does that once). */
	int32 GetNumFullUploads() const { return NumFullUploads; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStateChanged();
	/** A game was loaded or started: the texture shows the whole new grid. */
	void HandleGameReset();

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlotMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SurroundMesh;

	UPROPERTY()
	TObjectPtr<UTexture2D> CoverageTexture;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroundMaterial;

	/** What the GPU texture holds: interleaved R, G per texel, row by row. */
	TArray<uint8> Mirror;

	int32 NumFullUploads = 0;
	/** The sector each robot's outline shows now (-2: nothing was set yet). */
	static_assert(MaxAutoPavers == 3, "ShownSector and ShownRect list three initialisers: extend them with MaxAutoPavers");
	int32 ShownSector[MaxAutoPavers] = {-2, -2, -2};
	FVector4 ShownRect[MaxAutoPavers] = {FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0)};
	/** Four strips per robot but the first (south, north, west, east); they are made in BeginPlay and shown while the robot has a sector. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OutlineStrips;
	float MissingHighlight = 0.f;

	FDIAStateSubscription StateSub;
};
