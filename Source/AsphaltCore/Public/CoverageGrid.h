// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexMap.h"

/** Outcome of a pouring operation. */
struct FPourResult
{
	double LitresUsed = 0.0;
	/** Cells that became complete during the operation, each reported exactly once per game. */
	TArray<int32> CompletedCells;
};

/**
 * Coverage of the site as a Size x Size texel mask (5 cm texels over +-1600 cm). A texel belongs to the map cell
 * that contains its centre; texels outside the map are never painted and never cost anything.
 *
 * A new texel of cell c costs LitresPerCell / GetCellTexelCount(c) litres. Once a cell reaches
 * CompletionThreshold coverage its remaining texels are filled for free and the cell is reported as completed.
 */
class ASPHALTCORE_API FCoverageGrid
{
public:
	static constexpr int32 Size = 640;
	static constexpr double TexelCm = 5.0;
	static constexpr double HalfExtentCm = 1600.0;

	explicit FCoverageGrid(const FHexMap& Map);

	/**
	 * Paints the oriented rectangle around the axis A->B (extended by StampPadCm at both ends, half width
	 * WidthCm / 2; A == B points the axis towards +X). Texels are processed row by row (Y outer, X inner) and
	 * painting stops as soon as the remaining litres cannot pay for the next new texel.
	 */
	FPourResult Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres);

	/** Covers uncovered texels of one cell in row-major order until the litres run out or the cell completes. */
	FPourResult DepositIntoCell(int32 Cell, double MaxLitres);

	/** Covers the whole cell; the result's LitresUsed is the price of the previously uncovered part. */
	FPourResult CompleteCell(int32 Cell);

	int32 GetCellTexelCount(int32 Cell) const;
	/** Covered fraction of the cell, 0..1. */
	double GetCellFill(int32 Cell) const;
	bool IsCellComplete(int32 Cell) const;
	int32 NumCompleteCells() const;
	/** Cell that owns the texel, INDEX_NONE if the texel belongs to no cell (or is out of range). */
	int32 GetTexelCell(int32 X, int32 Y) const;
	/** Size * Size texels, row-major, 0 (bare) or 255 (covered). */
	TConstArrayView<uint8> GetTexels() const;

	/** Returns the area changed since the last call (Max exclusive); false if nothing changed. */
	bool ConsumeDirtyRect(FIntRect& Out);

	/** Packs the mask into exactly Size * Size / 8 bytes (bit index = Y * Size + X, least significant bit first). */
	void WriteBits(TArray<uint8>& Out) const;
	/** Restores a mask from WriteBits output; false (and no change) on a wrong size. Rebuilds per-cell state. */
	bool ReadBits(TConstArrayView<uint8> In);

private:
	/** Paints one uncovered texel of Cell; returns true if the cell has now reached its completion threshold. */
	bool PaintTexel(int32 TexelIndex, int32 Cell);
	/** Fills the rest of the cell for free, marks it complete and reports it. */
	void FinishCell(int32 Cell, FPourResult& Result);
	void ExpandDirty(int32 MinX, int32 MinY, int32 MaxXInclusive, int32 MaxYInclusive);
	double TexelCost(int32 Cell) const;

	int32 NumCells = 0;
	/** Cell per texel, -1 if none. */
	TArray<int16> TexelCells;
	TArray<uint8> Texels;

	/** Texel indices of every cell in ascending (row-major) order, stored back to back. */
	TArray<int32> CellTexelIndices;
	TArray<int32> CellTexelStart;
	TArray<int32> CellTexelCount;
	TArray<int32> CellCovered;
	TArray<int32> CellThresholdTexels;
	TArray<FIntRect> CellBounds;
	TArray<uint8> CellComplete;
	int32 CompleteCount = 0;

	bool bDirty = false;
	FIntRect Dirty;
};
