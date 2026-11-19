// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoverageGrid.h"
#include "GameFramework/Actor.h"
#include "DIAGroundActor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

/** Rendering constants of the ground (nothing here is a game rule; those live in AsphaltBalance / AsphaltLayout). */
namespace DIAGround
{
	/** Side of the textured plot: exactly the area the coverage grid describes. */
	inline constexpr double PlotSizeCm = 2.0 * FCoverageGrid::HalfExtentCm;
	/** Edge length of the hex outline texture in texels. */
	inline constexpr int32 HexTexSize = 2048;
	/** A texel belongs to an outline if its centre is closer than this to the edge of its cell. */
	inline constexpr double HexLineHalfWidthCm = 1.5;
	/** The untextured soil around the plot reaches this far from the centre (60 m). */
	inline constexpr double SurroundHalfExtentCm = 6000.0;
	/** The surround lies this much below the plot so that the two never fight over the same depth. */
	inline constexpr double SurroundDropCm = 2.0;
	/** Thickness of the collision slab below Z = 0. */
	inline constexpr double CollisionDepthCm = 100.0;
}

/**
 * The ground: one 32 x 32 m plane whose material shows soil, the cell outlines (HexLinesTex, generated once) and
 * the asphalt (CoverageTex, mirrored from FCoverageGrid), surrounded by plain soil. The actor listens to the world
 * subsystem and uploads only the changed part of the coverage texture. Texture X follows world X and texture row Y
 * follows world Y, exactly like the grid.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAGroundActor : public AActor
{
	GENERATED_BODY()

public:
	ADIAGroundActor();

	/** Fills HexTexSize^2 texels: 255 where the texel centre lies in a map cell and within HexLineHalfWidthCm of its edge. */
	static void BuildHexLines(TArray<uint8>& OutTexels);

	/** Uploads the part of the grid changed since the last call (or the whole grid if bFull). */
	void SyncCoverage(bool bFull = false);

	UTexture2D* GetCoverageTexture() const { return CoverageTexture; }
	UTexture2D* GetHexLinesTexture() const { return HexLinesTexture; }
	UMaterialInstanceDynamic* GetGroundMaterial() const { return GroundMaterial; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStateChanged();

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlotMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SurroundMesh;

	UPROPERTY()
	TObjectPtr<UTexture2D> CoverageTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> HexLinesTexture;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroundMaterial;

	FDelegateHandle StateHandle;
};
