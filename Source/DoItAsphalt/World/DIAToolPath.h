// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Numbers of the path a working tool (the cart's mouth, the finisher's bar, the car's bar) draws in one tick. */
namespace DIAToolPath
{
	/** Two points closer than this are the same point: a tool that does not move works nothing. */
	inline constexpr double MoveToleranceCm = 0.01;
	/** A point that moves further in one tick than the working speed allows (times this slack) was moved, not driven: a teleport, a spin. */
	inline constexpr double JumpSlack = 1.5;
}

/**
 * The path of a tool that works while it moves, from tick to tick: from where it was to where it is now. Used by everything that
 * lays asphalt by moving (the held cart and finisher, the paver car), so that all of them treat a standing tool, the first tick of
 * a stroke and a jump in the same way.
 */
struct FDIAToolPath
{
	/** The tool stops working (the button went up, it was let go, it was entered, the game was replaced): the next stroke starts afresh. */
	void Reset() { bActive = false; }
	bool IsActive() const { return bActive; }

	/**
	 * Feeds the tool's point of this tick. False (and no path) on the first tick of a stroke, when the point did not move, and when
	 * it moved further than a walk at SpeedCmS allows: the path then starts again where it is. The bound is the larger of this
	 * tick's and the previous tick's length, so that one long frame followed by a short one does not drop a segment of an honest move.
	 */
	bool Advance(FVector2D Current, float DeltaSeconds, double SpeedCmS, FVector2D& OutFrom, FVector2D& OutTo)
	{
		const float LastSeconds = PreviousSeconds;
		PreviousSeconds = DeltaSeconds;
		if (!bActive)
		{
			bActive = true;
			Previous = Current;
			return false;
		}
		if (Current.Equals(Previous, DIAToolPath::MoveToleranceCm))
		{
			return false;
		}
		if (FVector2D::Distance(Current, Previous) > SpeedCmS * FMath::Max(DeltaSeconds, LastSeconds) * DIAToolPath::JumpSlack)
		{
			Previous = Current;
			return false;
		}
		OutFrom = Previous;
		OutTo = Current;
		Previous = Current;
		return true;
	}

private:
	bool bActive = false;
	FVector2D Previous = FVector2D::ZeroVector;
	float PreviousSeconds = 0.0f;
};
