// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The player's settings, kept outside the save of the game (they belong to the player, not to a lot). Sliders (EFloat), switches (EToggle)
 * and the graphics quality (0..3). Values live in the game-user ini; a Get reads them once, a Set changes the value in memory at once (a
 * slider calls it on every move) and Save writes them to disk (the menu calls it when a slider is let go and when the settings are left).
 * Apply hands the ones the engine owns (window mode, vsync, scalability, master volume) to the engine; the game calls it at the start and
 * the menu after every change.
 */
namespace DIASettings
{
	enum class EFloat : uint8
	{
		MouseSensitivity,
		FieldOfView,
		MasterVolume,
		SfxVolume,
		AmbientVolume,
		Count
	};

	enum class EToggle : uint8
	{
		InvertY,
		Fullscreen,
		VSync,
		Count
	};

	struct FFloatSpec
	{
		const TCHAR* Key;
		float Min;
		float Max;
		float Default;
		/** The step of the slider. */
		float Step;
	};

	DOITASPHALT_API const FFloatSpec& Spec(EFloat Setting);
	DOITASPHALT_API const TCHAR* ToggleKey(EToggle Setting);
	DOITASPHALT_API bool ToggleDefault(EToggle Setting);

	inline constexpr int32 NumQualityLevels = 4;
	inline constexpr int32 DefaultQuality = 2;

	/** Value limited to the range of the setting; anything that is not a number gives its default. */
	DOITASPHALT_API float ClampFloat(EFloat Setting, float Value);
	DOITASPHALT_API float GetFloat(EFloat Setting);
	/** Sets the value (clamped); does not touch the disk. */
	DOITASPHALT_API void SetFloat(EFloat Setting, float Value);
	DOITASPHALT_API bool GetToggle(EToggle Setting);
	DOITASPHALT_API void SetToggle(EToggle Setting, bool bOn);
	/** Graphics quality 0 (Low) .. 3 (Epic). */
	DOITASPHALT_API int32 GetQuality();
	DOITASPHALT_API void SetQuality(int32 Level);
	DOITASPHALT_API FText QualityName(int32 Level);

	// The mouse sensitivity, as it was the only setting once: a factor on the look input, 1 being the feel the game had before.
	inline constexpr float MinMouseSensitivity = 0.1f;
	inline constexpr float MaxMouseSensitivity = 3.f;
	inline constexpr float DefaultMouseSensitivity = 1.f;
	DOITASPHALT_API float ClampMouseSensitivity(float Value);
	DOITASPHALT_API float GetMouseSensitivity();
	DOITASPHALT_API void SetMouseSensitivity(float Value);

	/** Horizontal field of view of the walking view in degrees (the work view and the car follow it). */
	inline float GetFieldOfView() { return GetFloat(EFloat::FieldOfView); }

	/** Hands the engine-owned settings to the engine. Does nothing without a window (tests, servers) or in the editor, where the window is not the game's. */
	DOITASPHALT_API void Apply();
	/** Writes the settings to the game-user ini. */
	DOITASPHALT_API void Save();
	/** Back to the defaults in memory (the menu's Defaults button); Apply and Save follow. */
	DOITASPHALT_API void ResetToDefaults();
	/** Forgets the values in memory so that the next Get reads the ini again (tests). */
	DOITASPHALT_API void ResetCache();
}
