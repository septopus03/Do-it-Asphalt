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

	/** Old road behind the end of a road (origin at the centre of the near edge, runs towards +X) and the barrier that closes it (origin at the centre of the footprint). */
	inline const FVector RoadStubCm(1200.0, 340.0, 5.0);
	inline const FVector BarrierCm(30.0, 340.0, 100.0);

	/** Thickness of the flat disc that marks the refill zone. */
	inline constexpr double RefillMarkerThicknessCm = 1.0;
}
