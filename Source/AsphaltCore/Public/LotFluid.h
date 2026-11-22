// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Hot asphalt that runs before it sets: a viscous shallow fluid on the texels of the car park.
 *
 * Every texel can hold asphalt, as a thickness in "texel volumes" (1 is the film a texel holds when the asphalt has settled) and
 * a heat (1 just poured, falling with time). Across each face between two texels flows a "pipe" that is pushed by the height
 * difference and keeps part of its flow from step to step (LotBalance::FluidMomentum). That inertia is what makes a heap that is
 * poured on the ground slosh outwards, overshoot and ripple, which is livelier than a real fluid of this thickness would be;
 * the rest is meant to be believable: only asphalt that stands thicker than the film runs, a flow can never take more than the
 * texel holds, the colder asphalt is the slower it runs, and what has cooled stops where it lies. The heat is carried by the
 * flow, so the front of a spreading puddle is as hot as the pour that feeds it.
 *
 * The fluid sees only the geometry of the car park (texels outside it, and the shopping-centre quadrant, are walls); it knows
 * nothing of raw or finished asphalt except the kind it was poured as, which it hands to the texels it covers. A texel that is
 * still free in the grid and holds LotBalance::FluidCoverUnits is reported through the callback of Step, once; the grid turns it
 * into ground. Asphalt that cools without ever covering a texel is lost, as the thin edge of a real puddle is.
 *
 * Memory is sparse: the fluid lives in tiles of 32 x 32 texels that exist only where there is asphalt (and next to it, where it
 * is about to flow). Stepping costs time in proportion to the tiles in use and nothing at all when there are none. All
 * arithmetic is deterministic (no randomness, no dependence on the order of the tiles), so the same pours always give the same
 * puddle.
 */
class ASPHALTCORE_API FLotFluid
{
public:
	/** Texels per side of the grid the fluid lives on; the grid asserts that it is its own size. */
	static constexpr int32 GridSide = 4000;
	static constexpr int32 TileShift = 5;
	static constexpr int32 TileSide = 1 << TileShift;
	static constexpr int32 TileTexels = TileSide * TileSide;
	static constexpr int32 TilesPerSide = (GridSide + TileSide - 1) / TileSide;

	FLotFluid();
	/** A copy owns its tiles (a game is copied to be saved). */
	FLotFluid(const FLotFluid& Other);
	FLotFluid& operator=(const FLotFluid& Other);
	FLotFluid(FLotFluid&&) = default;
	FLotFluid& operator=(FLotFluid&&) = default;

	/** Lays Units of hot asphalt of the given kind (1 raw, 2 finished) on a texel. False for a texel outside the car park or Units <= 0. */
	bool Add(int32 X, int32 Y, float Units, uint8 Kind);

	/**
	 * Advances the fluid by one step of LotBalance::FluidStepSeconds. Texels is the grid's texel array (GridSide x GridSide, 0 =
	 * free); OnSet(X, Y, Kind) is called for each free texel that now holds enough asphalt to set (the caller changes the grid).
	 */
	void Step(TConstArrayView<uint8> Texels, TFunctionRef<void(int32, int32, uint8)> OnSet);

	/** True while any asphalt is still hot enough to run. */
	bool IsActive() const { return NumUsedTiles > 0; }
	int32 GetNumTiles() const { return NumUsedTiles; }

	/** Thickness and heat (0..1) of a texel; 0 where there is nothing. */
	float GetUnits(int32 X, int32 Y) const;
	float GetHeat(int32 X, int32 Y) const;
	/** All the asphalt that is on the ground and still running, in texel volumes. */
	double GetTotalUnits() const;

	/** Takes everything away. */
	void Clear();

	/** True for a texel the fluid may enter: inside the grid and not in the shopping-centre quadrant. */
	static bool IsPourable(int32 X, int32 Y);

private:
	struct FTile
	{
		int32 Tx = 0;
		int32 Ty = 0;
		/** Thickness, and heat times thickness (heat is E / H, so that mixing two portions averages the heat by volume). */
		float H[TileTexels];
		float E[TileTexels];
		/** Flow across the face to the +X / +Y neighbour; negative flows the other way. */
		float Fx[TileTexels];
		float Fy[TileTexels];
		/** Share of the wanted outflow a texel can really give (<= 1). */
		float S[TileTexels];
		float Hn[TileTexels];
		float En[TileTexels];
		uint8 Kind[TileTexels];
		uint8 KindN[TileTexels];
	};

	/** The tile that holds texel (X, Y), or null where there is none. */
	const FTile* TileOf(int32 X, int32 Y) const;
	/** The tile at tile coordinates, or null where there is none (also outside the grid). */
	FTile* TileAt(int32 Tx, int32 Ty) const;
	/** Makes sure the tile exists (zeroed when new) and returns it; null outside the grid. */
	FTile* EnsureTile(int32 Tx, int32 Ty);
	void ReleaseTile(int32 Slot);
	/** Gives the neighbours of every tile whose edge holds asphalt a tile of their own, so that the flow has somewhere to go. */
	void GrowBorders();

	/** Tiles in use, by slot; a released slot holds null. */
	TArray<TUniquePtr<FTile>> Tiles;
	/** Slot in Tiles of each tile of the grid, -1 where there is none. */
	TArray<int32> TileTable;
	TArray<int32> FreeSlots;
	int32 NumUsedTiles = 0;
};
