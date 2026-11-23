// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include <limits>
#include "AsphaltTest.h"
#include "Game/DIAAudio.h"
#include "Game/DIASettings.h"
#include "LotMap.h"
#include "Sound/SoundWave.h"
#include "UI/SDIAPaverMap.h"

DIA_TEST(FSettingsRangesTest, "DoItAsphalt.Game.Settings.Ranges")

bool FSettingsRangesTest::RunTest(const FString& Parameters)
{
	for (int32 I = 0; I < static_cast<int32>(DIASettings::EFloat::Count); ++I)
	{
		const DIASettings::EFloat Setting = static_cast<DIASettings::EFloat>(I);
		const DIASettings::FFloatSpec& Spec = DIASettings::Spec(Setting);
		TestTrue(*FString::Printf(TEXT("%s: the default is in the range"), Spec.Key), Spec.Default >= Spec.Min && Spec.Default <= Spec.Max);
		TestTrue(*FString::Printf(TEXT("%s: the step fits in the range"), Spec.Key), Spec.Step > 0.f && Spec.Step <= Spec.Max - Spec.Min);
		TestEqual(*FString::Printf(TEXT("%s: below the range gives the minimum"), Spec.Key), DIASettings::ClampFloat(Setting, Spec.Min - 10.f), Spec.Min);
		TestEqual(*FString::Printf(TEXT("%s: above the range gives the maximum"), Spec.Key), DIASettings::ClampFloat(Setting, Spec.Max + 10.f), Spec.Max);
		TestEqual(*FString::Printf(TEXT("%s: not a number gives the default"), Spec.Key), DIASettings::ClampFloat(Setting, std::numeric_limits<float>::quiet_NaN()), Spec.Default);
	}
	TestEqual(TEXT("the mouse keeps its old limits"), DIASettings::ClampMouseSensitivity(100.f), DIASettings::MaxMouseSensitivity);
	TestEqual(TEXT("and its old default"), DIASettings::Spec(DIASettings::EFloat::MouseSensitivity).Default, DIASettings::DefaultMouseSensitivity);
	TestTrue(TEXT("the field of view is a sane angle"), DIASettings::Spec(DIASettings::EFloat::FieldOfView).Min >= 60.f && DIASettings::Spec(DIASettings::EFloat::FieldOfView).Max <= 120.f);
	for (int32 Level = 0; Level < DIASettings::NumQualityLevels; ++Level)
	{
		TestFalse(*FString::Printf(TEXT("quality %d has a name"), Level), DIASettings::QualityName(Level).IsEmpty());
	}
	TestTrue(TEXT("the default quality is a level"), DIASettings::DefaultQuality >= 0 && DIASettings::DefaultQuality < DIASettings::NumQualityLevels);
	return true;
}

DIA_TEST(FSettingsChangeInMemoryTest, "DoItAsphalt.Game.Settings.ChangeInMemory")

bool FSettingsChangeInMemoryTest::RunTest(const FString& Parameters)
{
	DIASettings::ResetToDefaults();
	TestEqual(TEXT("defaults: field of view"), DIASettings::GetFieldOfView(), DIASettings::Spec(DIASettings::EFloat::FieldOfView).Default);
	DIASettings::SetFloat(DIASettings::EFloat::FieldOfView, 100.f);
	TestEqual(TEXT("a change is seen at once"), DIASettings::GetFieldOfView(), 100.f);
	DIASettings::SetFloat(DIASettings::EFloat::MasterVolume, 5.f);
	TestEqual(TEXT("a value out of range is clamped"), DIASettings::GetFloat(DIASettings::EFloat::MasterVolume), 1.f);
	const bool bInvert = DIASettings::GetToggle(DIASettings::EToggle::InvertY);
	DIASettings::SetToggle(DIASettings::EToggle::InvertY, !bInvert);
	TestEqual(TEXT("a switch flips"), DIASettings::GetToggle(DIASettings::EToggle::InvertY), !bInvert);
	DIASettings::SetQuality(99);
	TestTrue(TEXT("a quality out of range is limited"), DIASettings::GetQuality() < DIASettings::NumQualityLevels);

	// Back to the defaults for whoever runs next; nothing was written to the disk.
	DIASettings::ResetToDefaults();
	TestEqual(TEXT("the defaults are back"), DIASettings::GetToggle(DIASettings::EToggle::InvertY), DIASettings::ToggleDefault(DIASettings::EToggle::InvertY));
	TestEqual(TEXT("and the quality"), DIASettings::GetQuality(), DIASettings::DefaultQuality);
	DIASettings::ResetCache();
	return true;
}

DIA_TEST(FPaverMapSlotsTest, "DoItAsphalt.Game.PaverMap.Slots")

bool FPaverMapSlotsTest::RunTest(const FString& Parameters)
{
	TSet<FIntPoint> Seen;
	for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
	{
		const FIntPoint Slot = DIAPaverMap::SectorSlot(Sector);
		TestTrue(*FString::Printf(TEXT("sector %d is on the map"), Sector), Slot.X >= 0 && Slot.Y >= 0 && Slot.X < DIAPaverMap::SectorsPerSide && Slot.Y < DIAPaverMap::SectorsPerSide);
		TestEqual(*FString::Printf(TEXT("sector %d comes back from its slot"), Sector), DIAPaverMap::SectorAtSlot(Slot), Sector);
		TestFalse(*FString::Printf(TEXT("sector %d has a slot of its own"), Sector), Seen.Contains(Slot));
		Seen.Add(Slot);

		// The map is drawn from the sector's own place in the world, so a sector's centre lands inside its slot.
		const FVector2D On = DIAPaverMap::WorldToMap(AsphaltLot::SectorCentre(Sector));
		const float Cell = DIAPaverMap::MapSize / DIAPaverMap::SectorsPerSide;
		TestTrue(*FString::Printf(TEXT("sector %d sits in its slot"), Sector), FMath::FloorToInt(On.X / Cell) == Slot.X && FMath::FloorToInt(On.Y / Cell) == Slot.Y);
	}
	TestEqual(TEXT("75 sectors fill 75 slots"), Seen.Num(), AsphaltLot::NumSectors);
	TestEqual(TEXT("a slot outside the map is no sector"), DIAPaverMap::SectorAtSlot(FIntPoint(-1, 0)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("neither is one past the end"), DIAPaverMap::SectorAtSlot(FIntPoint(DIAPaverMap::SectorsPerSide, 0)), static_cast<int32>(INDEX_NONE));
	return true;
}

DIA_TEST(FAudioAssetsTest, "DoItAsphalt.Game.Audio.AllSoundsImported")

bool FAudioAssetsTest::RunTest(const FString& Parameters)
{
	for (int32 I = 0; I < static_cast<int32>(DIAAudio::ESound::Count); ++I)
	{
		const DIAAudio::ESound Which = static_cast<DIAAudio::ESound>(I);
		const FString Name = DIAAudio::Name(Which);
		const USoundWave* Wave = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/Audio/SW_%s.SW_%s"), *Name, *Name));
		if (TestNotNull(*FString::Printf(TEXT("%s is imported"), *Name), Wave))
		{
			TestEqual(*FString::Printf(TEXT("%s loops exactly when it is a loop"), *Name), Wave->bLooping != 0, Name.EndsWith(TEXT("_loop")));
		}
	}
	return true;
}

DIA_TEST(FAudioEngineTest, "DoItAsphalt.Game.Audio.EngineFollowsSpeed")

bool FAudioEngineTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a standing engine idles quietly"), DIAAudio::EngineVolume(0.f) > 0.f && DIAAudio::EngineVolume(0.f) < DIAAudio::EngineVolume(1.f));
	TestTrue(TEXT("the pitch rises with the speed"), DIAAudio::EnginePitch(1.f) > DIAAudio::EnginePitch(0.f));
	float Last = 0.f;
	for (int32 Step = 0; Step <= 10; ++Step)
	{
		const float Volume = DIAAudio::EngineVolume(Step / 10.f);
		TestTrue(*FString::Printf(TEXT("the volume never drops (%d / 10)"), Step), Volume >= Last);
		Last = Volume;
	}
	TestEqual(TEXT("a speed beyond the top is the top"), DIAAudio::EngineVolume(5.f), DIAAudio::EngineVolume(1.f));
	TestEqual(TEXT("reversing sounds like driving forward"), DIAAudio::EnginePitch(-1.f), DIAAudio::EnginePitch(1.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
