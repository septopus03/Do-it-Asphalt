// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Player/DIAInput.h"

class UInputAction;

/**
 * The controls legend (a sign plate at the bottom left of the HUD): the one table of the keys it shows. Every key cap names the keys it stands for
 * and the action of FDIAInput each of them must trigger, so that a test can compare the legend with the real bindings (DIAInput.cpp).
 */
namespace DIAControlsLegend
{
	/** One key of the real bindings and the action of FDIAInput that it triggers. */
	struct FBinding
	{
		FKey Key;
		TObjectPtr<UInputAction> FDIAInput::* Action = nullptr;
	};

	/** A key cap: its label ("W", "Shift", "1-4") and the keys it stands for (a cap like "1-4" covers four of them). */
	struct FCap
	{
		FText Label;
		TArray<FBinding> Bindings;
	};

	/** A line of the legend: the caps, then the action in a few words. */
	struct FLine
	{
		TArray<FCap> Caps;
		FText Action;
	};

	/** The title of the plate. */
	DOITASPHALT_API FText Title();
	/** The lines while the player is on foot. */
	DOITASPHALT_API const TArray<FLine>& OnFoot();
	/** The lines while the player sits in the car: only the keys that do something at the wheel. */
	DOITASPHALT_API const TArray<FLine>& InCar();
}
