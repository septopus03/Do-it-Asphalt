// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAAudio.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIASettings.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "Player/DIACharacter.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "World/DIAPaverCar.h"

const TCHAR* DIAAudio::Name(ESound Sound)
{
	static const TCHAR* const Names[static_cast<int32>(ESound::Count)] = {
		TEXT("Ambient_loop"), TEXT("Plant_loop"), TEXT("Engine_loop"), TEXT("Paver_loop"), TEXT("Pour_loop"), TEXT("Fill_loop"), TEXT("Station_loop"),
		TEXT("Scoop"), TEXT("StepA"), TEXT("StepB"), TEXT("StepC"), TEXT("Thud"), TEXT("Click"), TEXT("Buy"), TEXT("Unlock"), TEXT("Win"), TEXT("Toast")};
	return Names[FMath::Clamp(static_cast<int32>(Sound), 0, static_cast<int32>(ESound::Count) - 1)];
}

float DIAAudio::EngineVolume(float SpeedFraction)
{
	return FMath::Lerp(0.35f, 0.85f, FMath::Clamp(FMath::Abs(SpeedFraction), 0.f, 1.f));
}

float DIAAudio::EnginePitch(float SpeedFraction)
{
	return FMath::Lerp(0.75f, 1.9f, FMath::Clamp(FMath::Abs(SpeedFraction), 0.f, 1.f));
}

bool UDIAAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only a game that can be heard: not tests or servers (no renderer), not a world that is not played.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FApp::CanEverRender() && GEngine && GEngine->UseSound();
}

void UDIAAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UDIAWorldSubsystem>();
	Super::Initialize(Collection);

	for (int32 I = 0; I < static_cast<int32>(DIAAudio::ESound::Count); ++I)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/SW_%s"), DIAAudio::Name(static_cast<DIAAudio::ESound>(I)));
		Sounds.Add(LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}
	Loops.SetNum(static_cast<int32>(DIAAudio::ESound::Count));

	// What stands in the world is heard from near: full volume close, quiet by the far end.
	Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(DIAAudio::InnerRadiusCm, 0.0, 0.0);
	Settings.FalloffDistance = DIAAudio::FalloffCm;
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;

	GameSubsystem = UDIAWorldSubsystem::Get(this);
	if (UDIAWorldSubsystem* Game = GameSubsystem.Get())
	{
		BoughtHandle = Game->OnItemBought.AddUObject(this, &UDIAAudioSubsystem::HandleBought);
		UnlockHandle = Game->OnUnlocked.AddUObject(this, &UDIAAudioSubsystem::HandleUnlocked);
		WonHandle = Game->OnWon.AddUObject(this, &UDIAAudioSubsystem::HandleWon);
	}
}

void UDIAAudioSubsystem::Deinitialize()
{
	if (UDIAWorldSubsystem* Game = GameSubsystem.Get())
	{
		Game->OnItemBought.Remove(BoughtHandle);
		Game->OnUnlocked.Remove(UnlockHandle);
		Game->OnWon.Remove(WonHandle);
	}
	for (FDIALoop& Loop : Loops)
	{
		if (Loop.Component)
		{
			Loop.Component->Stop();
		}
	}
	Loops.Reset();
	Super::Deinitialize();
}

float UDIAAudioSubsystem::Sfx() const
{
	return DIASettings::GetFloat(DIASettings::EFloat::SfxVolume);
}

float UDIAAudioSubsystem::Ambience() const
{
	return DIASettings::GetFloat(DIASettings::EFloat::AmbientVolume);
}

void UDIAAudioSubsystem::Drive(DIAAudio::ESound Which, float Target, float Volume, float Pitch, float DeltaTime, const FVector* At)
{
	USoundBase* Wave = Sound(Which);
	if (!Wave || !Loops.IsValidIndex(static_cast<int32>(Which)))
	{
		return;
	}
	FDIALoop& Loop = Loops[static_cast<int32>(Which)];
	const float Step = DIAAudio::FadePerSecond * DeltaTime;
	Loop.Level = FMath::Clamp(Loop.Level + FMath::Clamp(Target - Loop.Level, -Step, Step), 0.f, 1.f);
	const float Level = Loop.Level * Volume;

	if (Level <= 0.001f)
	{
		if (Loop.Component && Loop.Component->IsPlaying())
		{
			Loop.Component->Stop();
		}
		return;
	}
	if (!Loop.Component)
	{
		UWorld* World = GetWorld();
		if (At)
		{
			Loop.Component = UGameplayStatics::SpawnSoundAtLocation(World, Wave, *At, FRotator::ZeroRotator, Level, Pitch, 0.f, Attenuation, nullptr, /*bAutoDestroy*/ false);
			Loop.b3D = true;
		}
		else
		{
			Loop.Component = UGameplayStatics::SpawnSound2D(World, Wave, Level, Pitch, 0.f, nullptr, /*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ false);
		}
		if (!Loop.Component)
		{
			return;
		}
	}
	if (At && Loop.b3D)
	{
		Loop.Component->SetWorldLocation(*At);
	}
	Loop.Component->SetVolumeMultiplier(Level);
	Loop.Component->SetPitchMultiplier(Pitch);
	if (!Loop.Component->IsPlaying())
	{
		Loop.Component->Play();
	}
}

void UDIAAudioSubsystem::PlayOnce(DIAAudio::ESound Which, float Volume, float Pitch)
{
	if (USoundBase* Wave = Sound(Which))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Wave, Volume * Sfx(), Pitch);
	}
}

void UDIAAudioSubsystem::HandleBought(ELotItem Item)
{
	PlayOnce(DIAAudio::ESound::Buy);
}

void UDIAAudioSubsystem::HandleUnlocked(ELotUnlock Unlock)
{
	PlayOnce(DIAAudio::ESound::Unlock);
}

void UDIAAudioSubsystem::HandleWon()
{
	PlayOnce(DIAAudio::ESound::Win);
}

void UDIAAudioSubsystem::TickHand(const ADIACharacter* Character, float DeltaTime, bool bCarPours)
{
	const bool bHandPour = Character && Character->IsPouringNow();
	const bool bPour = bHandPour || bCarPours;
	const bool bFill = Character && Character->IsFillingNow();
	const UDIAWorldSubsystem* Game = GameSubsystem.Get();
	const bool bSpoon = bFill && Game && Game->GetGame().GetHand() == ELotHand::Bowl;

	Drive(DIAAudio::ESound::Pour, bPour ? 1.f : 0.f, (bHandPour ? 0.8f : 0.9f) * Sfx(), bHandPour ? 1.f : 0.8f, DeltaTime);
	Drive(DIAAudio::ESound::Fill, bFill && !bSpoon ? 1.f : 0.f, 0.7f * Sfx(), 1.f, DeltaTime);

	// The spoon: one squelch per stroke.
	if (bSpoon)
	{
		ScoopTimer += DeltaTime;
		if (ScoopTimer >= LotBalance::ScoopCycleSeconds)
		{
			ScoopTimer = 0.f;
			PlayOnce(DIAAudio::ESound::Scoop, 0.8f, FMath::FRandRange(0.92f, 1.08f));
		}
	}
	else
	{
		ScoopTimer = 0.f;
	}

	// Picking something up or letting it go.
	if (Game)
	{
		const int32 Held = static_cast<int32>(Game->GetGame().GetHeld());
		if (LastHeld >= 0 && Held != LastHeld)
		{
			PlayOnce(DIAAudio::ESound::Thud, 0.6f, Held == static_cast<int32>(ELotMovable::Count) ? 0.85f : 1.1f);
		}
		LastHeld = Held;
	}
}

void UDIAAudioSubsystem::TickFootsteps(const ADIACharacter* Character, float DeltaTime)
{
	const UDIAWorldSubsystem* Game = GameSubsystem.Get();
	if (!Character || !Game || Game->GetGame().World().bPlayerInCar)
	{
		StepTravelled = 0.f;
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return;
	}
	const float Speed = Character->GetVelocity().Size2D();
	if (Speed < 30.f)
	{
		StepTravelled = 0.f;
		return;
	}
	StepTravelled += Speed * DeltaTime;
	const float Stride = Speed > 400.f ? DIAAudio::StepDistanceRunCm : DIAAudio::StepDistanceWalkCm;
	if (StepTravelled >= Stride)
	{
		StepTravelled = 0.f;
		const DIAAudio::ESound Steps[3] = {DIAAudio::ESound::StepA, DIAAudio::ESound::StepB, DIAAudio::ESound::StepC};
		PlayOnce(Steps[NextStep % 3], 0.55f, FMath::FRandRange(0.9f, 1.1f));
		NextStep = (NextStep + 1 + FMath::RandRange(0, 1)) % 3;
	}
}

bool UDIAAudioSubsystem::TickVehicles(float DeltaTime)
{
	const UDIAWorldSubsystem* GameSub = GameSubsystem.Get();
	UWorld* World = GetWorld();
	if (!GameSub || !World)
	{
		return false;
	}
	const FLotGame& Game = GameSub->GetGame();

	// The car: the engine runs while somebody sits in it; the doors thud when that changes.
	TActorIterator<ADIAPaverCar> CarIt(World);
	const ADIAPaverCar* Car = CarIt ? *CarIt : nullptr;
	const bool bDriven = Car && Car->HasDriver();
	if (bDriven != bWasInCar)
	{
		PlayOnce(DIAAudio::ESound::Thud, 0.9f, 0.8f);
		bWasInCar = bDriven;
	}
	const float SpeedFraction = Car ? static_cast<float>(Car->GetSpeed() / LotBalance::CarSpeedCmS) : 0.f;
	const FVector CarAt = Car ? Car->GetActorLocation() : FVector::ZeroVector;
	Drive(DIAAudio::ESound::Engine, bDriven ? 1.f : 0.f, DIAAudio::EngineVolume(SpeedFraction) * Sfx(), DIAAudio::EnginePitch(SpeedFraction), DeltaTime);

	// The car pours its tank out: the same stream as at the hand.
	const bool bCarPour = bDriven && Car->IsPouring() && Game.GetCarTank() > 0.0;

	// A vehicle that stands at a station and takes asphalt: the pump of the station sounds there.
	const bool bCarLoading = bDriven && Game.IsAtStation(Game.World().CarPos) && Game.GetCarTank() < Game.GetCarTankCapacity() - 1.0 && Game.GetPlantStock() > 0.0;
	const bool bPaverLoading = Game.Owns(ELotItem::AutoPaver) && Game.GetAutoPaverPhase() == ELotPaverPhase::Loading;
	const FVector PaverAt(Game.GetAutoPaverPos().X, Game.GetAutoPaverPos().Y, 60.0);
	const FVector At = bCarLoading ? CarAt : PaverAt;
	Drive(DIAAudio::ESound::Station, (bCarLoading || bPaverLoading) ? 1.f : 0.f, 0.8f * Sfx(), 1.f, DeltaTime, &At);

	// The auto paver's motor while it drives or paves.
	const ELotPaverPhase Phase = Game.GetAutoPaverPhase();
	const bool bPaverRuns = Game.Owns(ELotItem::AutoPaver) && Game.GetAutoPaverSector() != INDEX_NONE && Phase != ELotPaverPhase::Idle && Phase != ELotPaverPhase::Loading;
	Drive(DIAAudio::ESound::Paver, bPaverRuns ? 1.f : 0.f, 0.7f * Sfx(), Phase == ELotPaverPhase::Working ? 0.9f : 1.15f, DeltaTime, &PaverAt);
	return bCarPour;
}

void UDIAAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UDIAWorldSubsystem* Game = GameSubsystem.Get();
	if (!Game)
	{
		return;
	}
	// The wind and the distant road, and the hum of the plant (heard from where it stands).
	Drive(DIAAudio::ESound::Ambient, 1.f, 0.6f * Ambience(), 1.f, DeltaTime);
	const FVector PlantAt(LotLayout::PlantPos.X, LotLayout::PlantPos.Y, 300.0);
	Drive(DIAAudio::ESound::Plant, 1.f, 0.9f * Ambience(), 1.f, DeltaTime, &PlantAt);

	const ADIACharacter* Character = ADIACharacter::GetLocal(this);
	const bool bCarPours = TickVehicles(DeltaTime);
	TickHand(Character, DeltaTime, bCarPours);
	TickFootsteps(Character, DeltaTime);
}
