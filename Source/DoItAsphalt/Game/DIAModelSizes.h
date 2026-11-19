// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Sizes of the models in cm (docs/03-3d-modely.md, chapter 4), used for the placeholder shape that the game shows
 * until the real model exists. They are model dimensions, not game rules. Boxes are X (forward) x Y (right) x Z (up).
 */
namespace DIAModelSizes
{
	/** Backpack S, M, L, XL by tier; the tank extends from the back plane towards -X. */
	inline const FVector BackpackCm[4] = {
		FVector(16.0, 26.0, 32.0),
		FVector(22.0, 32.0, 42.0),
		FVector(28.0, 38.0, 52.0),
		FVector(34.0, 44.0, 62.0),
	};

	/** Helmet; origin at the centre of its lower edge. */
	inline const FVector HelmetCm(28.0, 24.0, 16.0);

	/** Spreader placeholder: a flat strip of this depth (X) and height (Z); its width is the tool width. */
	inline constexpr double SpreaderDepthCm = 8.0;
	inline constexpr double SpreaderHeightCm = 8.0;

	/** Body of the plant (without the outlet); origin at the centre of the footprint on the ground. */
	inline const FVector PlantCm(600.0, 400.0, 500.0);

	/** Plant addons: the second silo (from plant level 5) and the lying tank (from level 10); origin at the centre of the footprint. */
	inline const FVector PlantAddonACm(160.0, 160.0, 550.0);
	inline const FVector PlantAddonBCm(240.0, 180.0, 300.0);

	/** Kiosk (origin at the centre of the footprint, on the ground) and the padlock shown on a locked one (origin at its centre). */
	inline const FVector KioskCm(60.0, 80.0, 220.0);
	inline const FVector LockCm(10.0, 30.0, 40.0);

	/** Portable station (tank on two wheels, origin at the centre of the footprint, on the ground). */
	inline const FVector StationCm(120.0, 80.0, 130.0);

	/** Automated paver (low tracked robot, front +X with the spreader bar; origin at the centre of the footprint, on the ground). */
	inline const FVector AutoPaverCm(100.0, 80.0, 60.0);

	/**
	 * Paver truck (cab in front +X, tank, four wheels, the spreader bar at the rear X = -2.3 m just above the ground, the door on -Y;
	 * origin at the centre of the footprint, on the ground). The whole model is 4.6 x 3.0 x 2.4 m; the body without the bar is 2.2 m
	 * wide and is what the collision box and the placeholder box measure. The placeholder bar is a thin flat strip as wide as the real one.
	 */
	inline const FVector PaverCarCm(460.0, 300.0, 240.0);
	inline const FVector PaverCarBodyCm(460.0, 220.0, 240.0);
	inline const FVector PaverCarBarCm(10.0, 300.0, 10.0);

	/** Old road behind the end of a road (origin at the centre of the near edge, runs towards +X) and the barrier that closes it (origin at the centre of the footprint). */
	inline const FVector RoadStubCm(1200.0, 340.0, 5.0);
	inline const FVector BarrierCm(30.0, 340.0, 100.0);

	/**
	 * Fence panel (origin at the centre of the footprint, on the ground; panels tile along Y at the 2 m pitch), the frame of the exit gate
	 * (origin at the centre of the footprint, on the ground; the passage is 340 cm wide) and its barrier arm (origin at the end at the
	 * hinge, the arm runs towards +Y and the game turns it about the X axis; the box measures the arm itself).
	 */
	inline const FVector FenceCm(8.0, 200.0, 180.0);
	inline const FVector ExitGateFrameCm(40.0, 380.0, 300.0);
	inline const FVector ExitGateBarrierCm(10.0, 340.0, 12.0);

	/** Thickness of the flat disc that marks the refill zone. */
	inline constexpr double RefillMarkerThicknessCm = 1.0;

	/**
	 * Site dressing (all origins on the ground): cone, barrel and dirt pile at the centre of the footprint; the floodlight at the foot of
	 * its pole, the arm runs towards +Y (the envelope is X +-20, Y -20..100). The pole is the only solid part of the floodlight.
	 */
	inline const FVector ConeCm(30.0, 30.0, 50.0);
	inline const FVector BarrelCm(60.0, 60.0, 90.0);
	inline const FVector DirtPileCm(200.0, 200.0, 100.0);
	inline const FVector LampCm(40.0, 120.0, 500.0);
	inline const FVector LampCentreCm(0.0, 40.0, 250.0);
	inline const FVector LampPoleCm(40.0, 40.0, 500.0);
}
