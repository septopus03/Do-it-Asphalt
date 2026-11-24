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
	/** The real spreader models are this much wider than the bar (W + 0.10 m): the wheels at the bar's ends stick out. The placeholder is exactly W wide. */
	inline constexpr double SpreaderEndExtraCm = 10.0;

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
	 * Paver truck, one model per tier (cab in front +X, tank, wheels, the spreader bar at the rear just above the ground, the door on -Y;
	 * origin at the centre of the footprint, on the ground), indexed by the car level 0..3 (SM_PaverCar, SM_PaverCar_T2..T4). PaverCarCm is
	 * the whole model (the width is the bar's); PaverCarBodyCm is the body without the bar, narrower, and is what the collision box and the
	 * placeholder box measure.
	 */
	inline const FVector PaverCarCm[4] = {FVector(460.0, 300.0, 240.0), FVector(560.0, 360.0, 270.0), FVector(680.0, 420.0, 300.0), FVector(820.0, 500.0, 340.0)};
	inline const FVector PaverCarBodyCm[4] = {FVector(460.0, 220.0, 240.0), FVector(560.0, 250.0, 270.0), FVector(680.0, 280.0, 300.0), FVector(820.0, 310.0, 340.0)};

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

	/**
	 * Asphalt-self models (docs/04-plan-zmien-asphalt-self.md, chapter 4.1; envelopes in Tools/ModelGen/manifest.json). Hand tools:
	 * the spoon has the origin at the centre of the handle (the working edge towards +X), the bowl and the vessels at the
	 * centre of the bottom.
	 */
	inline const FVector SpoonCm(20.0, 4.5, 1.8);
	inline const FVector BowlCm(25.0, 25.0, 9.0);
	/** Canister (open round pail with a pouring beak on +X and a grip on -X) and the tub (round basin with a lip on +X and a handle on each side); origin at the centre of the bottom. */
	inline const FVector CanisterCm(29.0, 23.0, 40.0);
	inline const FVector TubCm(51.0, 58.0, 22.0);
	inline const FVector BucketCm(29.0, 31.0, 38.5);
	/** Points of the bucket (model axes, cm): the bail handle at the top of the arc. */
	inline const FVector BucketBailCm(0.0, 0.0, 37.0);

	/**
	 * The trolley and the cart (origin at the centre of the footprint on the ground; handle at the back -X, the outlet mouth at the
	 * front +X; both are pushed towards +X).
	 */
	inline const FVector TrolleyCm(96.0, 52.0, 89.8);
	inline const FVector TrolleyHandleCm(-48.0, 0.0, 88.0);
	inline const FVector TrolleyMouthCm(43.5, 0.0, 0.0);
	inline const FVector CartCm(126.0, 67.0, 99.0);
	inline const FVector CartHandleCm(-63.0, 0.0, 97.0);
	inline const FVector CartMouthCm(58.5, 0.0, 6.0);

	/**
	 * Vehicle station (a gantry pad; origin at the centre of the pad footprint on the ground, vehicles drive through along X). The nozzle of the
	 * hopper hangs at NozzleCm over the middle of the lane.
	 */
	inline const FVector VehicleStationCm(800.0, 545.0, 460.0);
	inline const FVector VehicleStationNozzleCm(0.0, 0.0, 290.0);

	/** The trough (origin at the centre of the footprint on the ground). */
	inline const FVector TroughCm(128.0, 60.0, 50.0);

	/** Curb piece (origin at the centre of the footprint on the ground; pieces tile along Y, the bevel faces +X towards the lot). */
	inline const FVector CurbCm(15.0, 200.0, 12.0);

	/**
	 * Shopping mall (origin at the centre of the footprint on the ground; the facades face the lot on -X and -Y). The name sign on the
	 * -X facade is centred at MallSignCm in model axes and measures MallSignSizeCm (Y wide, Z high).
	 */
	inline const FVector MallCm(10000.0, 10000.0, 1430.0);
	inline const FVector MallSignCm(-5000.0, 500.0, 1060.0);
	inline const FVector2D MallSignSizeCm(1600.0, 400.0);

	/**
	 * The three shop buildings (envelopes from Tools/ModelGen/manifest.json; the origins are not the centres, see docs/03-3d-modely.md,
	 * chapter 4.5): the tool shop, the paver depot and the garage.
	 */
	inline const FVector ToolShopCm(353.0, 608.0, 400.0);
	inline const FVector PaverDepotCm(350.0, 620.0, 341.0);
	inline const FVector GarageCm(1215.0, 800.0, 559.0);
}
