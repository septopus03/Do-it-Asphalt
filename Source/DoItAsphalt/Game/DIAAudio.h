// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotBalance.h"
#include "Subsystems/WorldSubsystem.h"
#include "DIAAudio.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class UDIAWorldSubsystem;
class ADIACharacter;
class ADIAPaverCar;

/** One looping sound that fades to a target level; 3D ones sit at a place. */
USTRUCT()
struct FDIALoop
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAudioComponent> Component;
	float Level = 0.f;
	bool b3D = false;
};

/** The sounds of the game: names of the sound waves in /Game/Audio (SW_<Name>, made by Tools/Audio/gen_sounds.py) and the levels they play at. */
namespace DIAAudio
{
	/** The loops and one-shots; the index is the position in the table of the subsystem. */
	enum class ESound : uint8
	{
		Ambient,
		Plant,
		Engine,
		Paver,
		Pour,
		Fill,
		Station,
		Scoop,
		StepA,
		StepB,
		StepC,
		Thud,
		Click,
		Buy,
		Unlock,
		Win,
		Toast,
		Count
	};

	/** The asset name (without the SW_ prefix) of a sound. */
	DOITASPHALT_API const TCHAR* Name(ESound Sound);

	/** The engine of the car: volume and pitch follow the speed (a fraction of the top speed, 0..1) between these. */
	DOITASPHALT_API float EngineVolume(float SpeedFraction);
	DOITASPHALT_API float EnginePitch(float SpeedFraction);

	/** Distance in cm the player walks between two footsteps (the same on the run: the run covers more ground per step). */
	inline constexpr float StepDistanceWalkCm = 150.f;
	inline constexpr float StepDistanceRunCm = 230.f;
	/** A loop fades in and out at this rate (level per second). */
	inline constexpr float FadePerSecond = 6.f;
	/** The 3D loops (the plant, the paver, a station) are heard within this distance (cm) at full and fade out by FalloffCm. */
	inline constexpr float InnerRadiusCm = 400.f;
	inline constexpr float FalloffCm = 3500.f;
}

/**
 * Plays the sounds of the game from what the game says: the ambience, the hum of the plant, the engine of the car (louder and higher
 * with the speed), pouring and filling at the hand, the spoon in the trough, footsteps, the car doors, the paver's motor and the
 * stations while they load, and the jingles of a purchase, an unlock and the win. It reads the state each tick and owns nothing of it,
 * and it is only there while the game can make a sound at all (not in tests, servers or with -nosound). Volumes come from DIASettings
 * (effects and ambience; the master volume is the engine's). A sound that has not been imported is silent.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API UDIAAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDIAAudioSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	USoundBase* Sound(DIAAudio::ESound Which) const { return Sounds[static_cast<int32>(Which)]; }
	/** Moves a loop towards Target (0..1) times Volume at Pitch; starts it when it becomes audible and stops it when it is silent. At is where a 3D loop sounds. */
	void Drive(DIAAudio::ESound Which, float Target, float Volume, float Pitch, float DeltaTime, const FVector* At = nullptr);
	void PlayOnce(DIAAudio::ESound Which, float Volume = 1.f, float Pitch = 1.f);
	float Sfx() const;
	float Ambience() const;

	void HandleBought(ELotItem Item);
	void HandleUnlocked(ELotUnlock Unlock);
	void HandleWon();

	/** The hand's pouring, filling and the spoon; bCarPours adds the car's stream to the pouring loop. */
	void TickHand(const ADIACharacter* Character, float DeltaTime, bool bCarPours);
	void TickFootsteps(const ADIACharacter* Character, float DeltaTime);
	/** The car's engine, the stations and the paver's motor; returns whether the car pours now. */
	bool TickVehicles(float DeltaTime);

	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Sounds;
	UPROPERTY()
	TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY()
	TArray<FDIALoop> Loops;

	TWeakObjectPtr<UDIAWorldSubsystem> GameSubsystem;
	FDelegateHandle BoughtHandle;
	FDelegateHandle UnlockHandle;
	FDelegateHandle WonHandle;

	float StepTravelled = 0.f;
	int32 NextStep = 0;
	float ScoopTimer = 0.f;
	bool bWasInCar = false;
	int32 LastHeld = -1;
};
