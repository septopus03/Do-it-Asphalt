// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"
#include "LotMap.h"

/** State of one texel of the lot. The numeric values are what GetTexels() returns. */
enum class ELotTexel : uint8 { Free = 0, Raw = 1, Done = 2 };

/** Outcome of a pouring operation. */
struct FLotPour
{
	/** Litres paid for by the call. */
	double LitresUsed = 0.0;
	/** Texels the call paid for or (FinishRaw) converted itself; the free remainder of a completed cell is not counted. */
	int32 TexelsChanged = 0;
	/** Cells that became complete during the call, each reported exactly once per game. */
	TArray<int32> CompletedCells;
};

/**
 * What is on the ground of the whole car park, as a Size x Size texel grid (5 cm texels over +-10000 cm):
 * every texel is free, raw asphalt or finished asphalt. Texel (X, Y) covers world
 * [-10000 + X * 5, -10000 + (X + 1) * 5) x the same for Y; it belongs to the cell that contains its centre
 * (20 x 20 texels per cell, found by division). Texels outside the car park never change and cost nothing.
 *
 * Only a free texel can be changed (FinishRaw: a raw one), and each new texel costs LitresPerTexel. Once the
 * finished texels of a cell reach ThresholdTexels the cell is complete: all its remaining texels become
 * finished for free and the cell is reported. Raw asphalt never completes a cell on its own.
 */
class ASPHALTCORE_API FLotGrid
{
public:
	/** Texels per side of the grid. */
	static constexpr int32 Size = 4000;
	/** Side of a texel in centimetres. */
	static constexpr double TexelCm = 5.0;
	/** Texels in one cell. */
	static constexpr int32 TexelsPerCell = 400;
	/** Litres one texel costs. */
	static constexpr double LitresPerTexel = 0.025;
	/** Finished texels of a cell at which the cell is complete (95 %). */
	static constexpr int32 ThresholdTexels = 380;

	FLotGrid();

	/**
	 * Turns free texels of the oriented rectangle around the axis A->B (extended by StampPadCm at both ends, half
	 * width WidthCm / 2; A == B points the axis towards +X) into State (Raw or Done; Free changes nothing). Texels
	 * are taken in order of their projection on the axis from A, ties by texel index, until the litres run out.
	 */
	FLotPour Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres, ELotTexel State);

	/** Turns free texels within RadiusCm of Centre into Done, nearest to From first (ties by texel index). */
	FLotPour PourDisc(FVector2D Centre, double RadiusCm, double MaxLitres, FVector2D From);

	/** Turns free texels of one cell into State (Raw or Done), nearest to From first (ties by texel index). */
	FLotPour DepositIntoCell(int32 Cell, double MaxLitres, FVector2D From, ELotTexel State);

	/** Turns the raw texels of the stamp rectangle (as in Stamp) into Done for free; free texels stay free. */
	FLotPour FinishRaw(FVector2D A, FVector2D B, double WidthCm);

	/** Makes the whole cell Done; LitresUsed is the price of its free part, TexelsChanged the size of that part. */
	FLotPour CompleteCell(int32 Cell);

	/** Finished texels of the cell, O(1); 0 for an invalid cell. */
	int32 GetCellDone(int32 Cell) const;
	/** Raw texels of the cell, O(1); 0 for an invalid cell. */
	int32 GetCellRaw(int32 Cell) const;
	bool IsCellComplete(int32 Cell) const;
	int32 NumCompleteCells() const;

	/** State of a texel; Free outside the grid. */
	ELotTexel GetTexel(int32 X, int32 Y) const;
	/** Size * Size ELotTexel values, row after row (index Y * Size + X). */
	TConstArrayView<uint8> GetTexels() const;

	/**
	 * Appends the grid to Out in the save format: a bit per cell (lowest bit first) that is set for complete cells,
	 * the number of in-progress cells, then for each of them, in ascending order, its index and its 400 texels at two
	 * bits each (row after row inside the cell). Texels of complete cells and of untouched cells are not stored.
	 */
	void Write(TArray<uint8>& Out) const;
	/**
	 * Replaces the grid by the one in the front of In and sets OutBytesRead to the bytes it took. Anything beyond that
	 * is the caller's business. Returns false and leaves the grid and OutBytesRead untouched if the data is truncated,
	 * out of range, not in the canonical form that Write produces or contradicts itself. On success the counters are
	 * rebuilt and every sector is marked dirty, so the whole picture is drawn again.
	 */
	bool Read(TConstArrayView<uint8> In, int32& OutBytesRead);

	/** Empties Out, then appends the area changed since the last call, at most one rectangle per sector (texel coordinates, Max exclusive). */
	void ConsumeDirtyRects(TArray<FIntRect>& Out);

private:
	/** A free texel that may be turned, with the key that orders it. */
	struct FCandidate
	{
		double Key;
		int32 Index;
	};

	/** Collects the free car-park texels of the stamp rectangle in Candidates, keyed by projection on the axis. */
	void CollectStampCandidates(FVector2D A, FVector2D B, double WidthCm, ELotTexel WantedState);
	/** Sorts Candidates by key, then index. */
	void SortCandidates();
	/** Turns candidates (in their order) into State while Budget texels remain; returns the result. */
	FLotPour ApplyCandidates(int32 Budget, ELotTexel State);
	/** Changes one texel of Cell to To and updates counters and dirty rectangles; completes the cell when due. */
	void ChangeTexel(int32 X, int32 Y, int32 Cell, ELotTexel To, FLotPour& Result);
	/** Makes every texel of the cell Done for free, marks the cell dirty and reports it. */
	void FinishCell(int32 Cell, FLotPour& Result);
	void ExpandDirty(int32 Sector, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY);

	TArray<uint8> Texels;
	TArray<uint16> CellDone;
	TArray<uint16> CellRaw;
	int32 CompleteCount = 0;
	TArray<FIntRect> Dirty;
	/** Scratch buffer reused between calls so a call does not allocate. */
	TArray<FCandidate> Candidates;
};
