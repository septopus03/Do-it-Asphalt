// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIASettings.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* const Section = TEXT("DoItAsphalt.Settings");
	const TCHAR* const MouseKey = TEXT("MouseSensitivity");

	/** Negative means "not read yet". */
	float CachedMouseSensitivity = -1.f;
}

float DIASettings::ClampMouseSensitivity(float Value)
{
	if (!FMath::IsFinite(Value))
	{
		return DefaultMouseSensitivity;
	}
	return FMath::Clamp(Value, MinMouseSensitivity, MaxMouseSensitivity);
}

float DIASettings::GetMouseSensitivity()
{
	if (CachedMouseSensitivity < 0.f)
	{
		float Stored = DefaultMouseSensitivity;
		if (GConfig)
		{
			GConfig->GetFloat(Section, MouseKey, Stored, GGameUserSettingsIni);
		}
		CachedMouseSensitivity = ClampMouseSensitivity(Stored);
	}
	return CachedMouseSensitivity;
}

void DIASettings::SetMouseSensitivity(float Value)
{
	CachedMouseSensitivity = ClampMouseSensitivity(Value);
}

void DIASettings::Save()
{
	if (GConfig)
	{
		GConfig->SetFloat(Section, MouseKey, GetMouseSensitivity(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void DIASettings::ResetCache()
{
	CachedMouseSensitivity = -1.f;
}
