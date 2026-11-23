// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIASettings.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* const Section = TEXT("DoItAsphalt.Settings");
	const TCHAR* const QualityKey = TEXT("Quality");

	using DIASettings::EFloat;
	using DIASettings::EToggle;

	constexpr int32 NumFloats = static_cast<int32>(EFloat::Count);
	constexpr int32 NumToggles = static_cast<int32>(EToggle::Count);

	const DIASettings::FFloatSpec FloatSpecs[NumFloats] = {
		{TEXT("MouseSensitivity"), 0.1f, 3.f, 1.f, 0.05f},
		{TEXT("FieldOfView"), 70.f, 110.f, 90.f, 1.f},
		{TEXT("MasterVolume"), 0.f, 1.f, 0.8f, 0.05f},
		{TEXT("SfxVolume"), 0.f, 1.f, 1.f, 0.05f},
		{TEXT("AmbientVolume"), 0.f, 1.f, 0.7f, 0.05f},
	};
	static_assert(DIASettings::MinMouseSensitivity == 0.1f && DIASettings::MaxMouseSensitivity == 3.f && DIASettings::DefaultMouseSensitivity == 1.f, "the mouse constants are the first spec");

	const TCHAR* const ToggleKeys[NumToggles] = {TEXT("InvertY"), TEXT("Fullscreen"), TEXT("VSync")};
	const bool ToggleDefaults[NumToggles] = {false, false, true};

	/** NaN marks "not read yet". */
	float Floats[NumFloats];
	int32 Toggles[NumToggles];
	int32 Quality = -1;
	bool bLoaded = false;

	void Load()
	{
		if (bLoaded)
		{
			return;
		}
		bLoaded = true;
		for (int32 I = 0; I < NumFloats; ++I)
		{
			float Stored = FloatSpecs[I].Default;
			if (GConfig)
			{
				GConfig->GetFloat(Section, FloatSpecs[I].Key, Stored, GGameUserSettingsIni);
			}
			Floats[I] = DIASettings::ClampFloat(static_cast<EFloat>(I), Stored);
		}
		for (int32 I = 0; I < NumToggles; ++I)
		{
			bool Stored = ToggleDefaults[I];
			if (GConfig)
			{
				GConfig->GetBool(Section, ToggleKeys[I], Stored, GGameUserSettingsIni);
			}
			Toggles[I] = Stored ? 1 : 0;
		}
		int32 StoredQuality = DIASettings::DefaultQuality;
		if (GConfig)
		{
			GConfig->GetInt(Section, QualityKey, StoredQuality, GGameUserSettingsIni);
		}
		Quality = FMath::Clamp(StoredQuality, 0, DIASettings::NumQualityLevels - 1);
	}
}

const DIASettings::FFloatSpec& DIASettings::Spec(EFloat Setting)
{
	return FloatSpecs[FMath::Clamp(static_cast<int32>(Setting), 0, NumFloats - 1)];
}

const TCHAR* DIASettings::ToggleKey(EToggle Setting)
{
	return ToggleKeys[FMath::Clamp(static_cast<int32>(Setting), 0, NumToggles - 1)];
}

bool DIASettings::ToggleDefault(EToggle Setting)
{
	return ToggleDefaults[FMath::Clamp(static_cast<int32>(Setting), 0, NumToggles - 1)];
}

float DIASettings::ClampFloat(EFloat Setting, float Value)
{
	const FFloatSpec& S = Spec(Setting);
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, S.Min, S.Max) : S.Default;
}

float DIASettings::GetFloat(EFloat Setting)
{
	Load();
	return Floats[FMath::Clamp(static_cast<int32>(Setting), 0, NumFloats - 1)];
}

void DIASettings::SetFloat(EFloat Setting, float Value)
{
	Load();
	Floats[FMath::Clamp(static_cast<int32>(Setting), 0, NumFloats - 1)] = ClampFloat(Setting, Value);
}

bool DIASettings::GetToggle(EToggle Setting)
{
	Load();
	return Toggles[FMath::Clamp(static_cast<int32>(Setting), 0, NumToggles - 1)] != 0;
}

void DIASettings::SetToggle(EToggle Setting, bool bOn)
{
	Load();
	Toggles[FMath::Clamp(static_cast<int32>(Setting), 0, NumToggles - 1)] = bOn ? 1 : 0;
}

int32 DIASettings::GetQuality()
{
	Load();
	return Quality;
}

void DIASettings::SetQuality(int32 Level)
{
	Load();
	Quality = FMath::Clamp(Level, 0, NumQualityLevels - 1);
}

FText DIASettings::QualityName(int32 Level)
{
	switch (FMath::Clamp(Level, 0, NumQualityLevels - 1))
	{
	case 0: return NSLOCTEXT("DIASettings", "QualityLow", "Low");
	case 1: return NSLOCTEXT("DIASettings", "QualityMedium", "Medium");
	case 2: return NSLOCTEXT("DIASettings", "QualityHigh", "High");
	default: return NSLOCTEXT("DIASettings", "QualityEpic", "Epic");
	}
}

float DIASettings::ClampMouseSensitivity(float Value)
{
	return ClampFloat(EFloat::MouseSensitivity, Value);
}

float DIASettings::GetMouseSensitivity()
{
	return GetFloat(EFloat::MouseSensitivity);
}

void DIASettings::SetMouseSensitivity(float Value)
{
	SetFloat(EFloat::MouseSensitivity, Value);
}

void DIASettings::Apply()
{
	// The master volume is the engine's; it is also what a headless run does without a window.
	FApp::SetVolumeMultiplier(GetFloat(EFloat::MasterVolume));
	if (!FApp::CanEverRender() || !GEngine || GIsEditor)
	{
		return;
	}
	if (UGameUserSettings* User = GEngine->GetGameUserSettings())
	{
		User->SetFullscreenMode(GetToggle(EToggle::Fullscreen) ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		User->SetVSyncEnabled(GetToggle(EToggle::VSync));
		User->SetOverallScalabilityLevel(GetQuality());
		User->ApplySettings(false);
	}
}

void DIASettings::Save()
{
	if (GConfig)
	{
		Load();
		for (int32 I = 0; I < NumFloats; ++I)
		{
			GConfig->SetFloat(Section, FloatSpecs[I].Key, Floats[I], GGameUserSettingsIni);
		}
		for (int32 I = 0; I < NumToggles; ++I)
		{
			GConfig->SetBool(Section, ToggleKeys[I], Toggles[I] != 0, GGameUserSettingsIni);
		}
		GConfig->SetInt(Section, QualityKey, Quality, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void DIASettings::ResetToDefaults()
{
	Load();
	for (int32 I = 0; I < NumFloats; ++I)
	{
		Floats[I] = FloatSpecs[I].Default;
	}
	for (int32 I = 0; I < NumToggles; ++I)
	{
		Toggles[I] = ToggleDefaults[I] ? 1 : 0;
	}
	Quality = DefaultQuality;
}

void DIASettings::ResetCache()
{
	bLoaded = false;
}
