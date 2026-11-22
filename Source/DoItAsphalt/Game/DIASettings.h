// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The player's settings, kept outside the save of the game (they belong to the player, not to a lot). Only the mouse
 * sensitivity so far: a factor on the look input, 1 being the feel the game had before the setting existed. The value lives in
 * the game-user ini; Get reads it once, Set changes the value in memory at once (the slider calls it on every move) and
 * Save writes it to disk (the menu calls it when the slider is let go and when the settings are left).
 */
namespace DIASettings
{
	inline constexpr float MinMouseSensitivity = 0.1f;
	inline constexpr float MaxMouseSensitivity = 3.f;
	inline constexpr float DefaultMouseSensitivity = 1.f;

	/** Value limited to [MinMouseSensitivity, MaxMouseSensitivity]; anything that is not a number gives the default. */
	float ClampMouseSensitivity(float Value);

	float GetMouseSensitivity();
	/** Sets the sensitivity (clamped); does not touch the disk. */
	void SetMouseSensitivity(float Value);
	/** Writes the settings to the game-user ini. */
	void Save();
	/** Forgets the value in memory so that the next Get reads the ini again (tests). */
	void ResetCache();
}
