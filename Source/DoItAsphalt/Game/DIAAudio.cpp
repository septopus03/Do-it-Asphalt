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

void FDIATrend::Update(double Now, float DeltaTime, bool bReset)
{
	constexpr double Epsilon = 1.0e-6;
	bJustRose = false;
	Down = FMath::Max(0.f, Down - DeltaTime);
	Up = FMath::Max(0.f, Up - DeltaTime);
	if (bReset || Last < 0.0)
	{
		Down = 0.f;
		Up = 0.f;
	}
	else if (Now < Last - Epsilon)
	{
		Down = HoldSeconds;
	}
	else if (Now > Last + Epsilon)
	{
		Up = HoldSeconds;
		bJustRose = true;
	}
	Last = Now;
}

void UDIAAudioSubsystem::TickHand(float DeltaTime, bool bCarPours)
{
	const UDIAWorldSubsystem* GameSub = GameSubsystem.Get();
	if (!GameSub)
	{
		return;
	}
	const FLotGame& Game = GameSub->GetGame();

	// The hand in use: a change of hand is not a change of litres.
	const int32 Hand = static_cast<int32>(Game.GetHand());
	HandTrend.Update(Game.GetHandLitres(Game.GetHand()), DeltaTime, Hand != LastHand);
	LastHand = Hand;

	// The movable in the hands (a cart or a trolley laying a strip gives its asphalt out) and all of them (a trough or a tap fills them).
	const int32 Held = static_cast<int32>(Game.GetHeld());
	const bool bHeldMovable = Held != static_cast<int32>(ELotMovable::Count);
	HeldTrend.Update(bHeldMovable ? Game.GetMovableLitres(Game.GetHeld()) : 0.0, DeltaTime, Held != LastHeldMovable);
	double MovablesTotal = 0.0;
	for (int32 I = 0; I < static_cast<int32>(ELotMovable::Count); ++I)
	{
		MovablesTotal += Game.GetMovableLitres(static_cast<ELotMovable>(I));
	}
	MovablesTrend.Update(MovablesTotal, DeltaTime);

	const bool bBowl = Game.GetHand() == ELotHand::Bowl;
	const bool bHandPour = !bHeldMovable && HandTrend.IsFalling();
	const bool bPour = bHandPour || bCarPours || (bHeldMovable && HeldTrend.IsFalling());
	// A vessel that gets asphalt is heard filling; the bowl only squelches once per spoonful.
	const bool bFill = (!bBowl && !bHeldMovable && HandTrend.IsRising()) || (MovablesTrend.IsRising() && !HeldTrend.IsFalling());

	Drive(DIAAudio::ESound::Pour, bPour ? 1.f : 0.f, (bHandPour ? 0.8f : 0.9f) * Sfx(), bHandPour ? 1.f : 0.8f, DeltaTime);
	Drive(DIAAudio::ESound::Fill, bFill ? 1.f : 0.f, 0.7f * Sfx(), 1.f, DeltaTime);

	// The spoon: one squelch for every spoonful that lands in the bowl.
	ScoopCooldown = FMath::Max(0.f, ScoopCooldown - DeltaTime);
	if (bBowl && !bHeldMovable && HandTrend.HasJustRisen() && ScoopCooldown <= 0.f)
	{
		ScoopCooldown = 0.25f;
		PlayOnce(DIAAudio::ESound::Scoop, 0.8f, FMath::FRandRange(0.92f, 1.08f));
	}

	// Picking something up or letting it go.
	if (LastHeldMovable >= 0 && Held != LastHeldMovable)
	{
		PlayOnce(DIAAudio::ESound::Thud, 0.6f, Held == static_cast<int32>(ELotMovable::Count) ? 0.85f : 1.1f);
	}
	LastHeldMovable = Held;
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

	// The car pours its tank out (the same stream as at the hand) only while the tank really empties; a car that stands at a station and
	// takes asphalt sounds the pump of the station, and only until the tank is full.
	CarTrend.Update(Game.GetCarTank(), DeltaTime);
	// The robots: the sound of the one nearest to the player plays (one source, as for the single robot before).
	const ADIACharacter* Listener = ADIACharacter::GetLocal(this);
	const FVector ListenerAt = Listener ? Listener->GetActorLocation() : FVector::ZeroVector;
	const auto Nearer = [&ListenerAt](int32 Best, const FVector& Candidate, const FVector& BestAt)
	{
		return Best == INDEX_NONE || FVector::DistSquared2D(Candidate, ListenerAt) < FVector::DistSquared2D(BestAt, ListenerAt);
	};
	int32 Loading = INDEX_NONE;
	int32 Running = INDEX_NONE;
	FVector LoadingAt = FVector::ZeroVector;
	FVector RunningAt = FVector::ZeroVector;
	for (int32 Paver = 0; Paver < Game.GetNumAutoPavers(); ++Paver)
	{
		PaverTrend[Paver].Update(Game.GetAutoPaverTank(Paver), DeltaTime);
		const FVector2D Pos = Game.GetAutoPaverPos(Paver);
		const FVector At(Pos.X, Pos.Y, 60.0);
		if (PaverTrend[Paver].IsRising() && Nearer(Loading, At, LoadingAt))
		{
			Loading = Paver;
			LoadingAt = At;
		}
		// The motor runs while the robot drives or paves.
		const ELotPaverPhase Phase = Game.GetAutoPaverPhase(Paver);
		const bool bRuns = Game.GetAutoPaverSector(Paver) != INDEX_NONE && Phase != ELotPaverPhase::Idle && Phase != ELotPaverPhase::Loading;
		if (bRuns && Nearer(Running, At, RunningAt))
		{
			Running = Paver;
			RunningAt = At;
		}
	}
	// While nothing runs or loads, the fading loops keep the place they had (not the world origin).
	if (Loading != INDEX_NONE)
	{
		LastLoadingAt = LoadingAt;
	}
	if (Running != INDEX_NONE)
	{
		LastRunningAt = RunningAt;
	}
	const bool bCarPour = CarTrend.IsFalling();
	const bool bCarLoading = CarTrend.IsRising();
	const bool bPaverLoading = Loading != INDEX_NONE;
	// The pump sounds where it last played: at the car while it loads, else at the robot that loads (and stays there while it fades).
	if (bCarLoading)
	{
		LastLoadingAt = CarAt;
	}
	const FVector At = LastLoadingAt;
	Drive(DIAAudio::ESound::Station, (bCarLoading || bPaverLoading) ? 1.f : 0.f, 0.8f * Sfx(), 1.f, DeltaTime, &At);

	const bool bPaverRuns = Running != INDEX_NONE;
	const ELotPaverPhase Phase = bPaverRuns ? Game.GetAutoPaverPhase(Running) : ELotPaverPhase::Idle;
	Drive(DIAAudio::ESound::Paver, bPaverRuns ? 1.f : 0.f, 0.7f * Sfx(), Phase == ELotPaverPhase::Working ? 0.9f : 1.15f, DeltaTime, &LastRunningAt);
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
	TickHand(DeltaTime, bCarPours);
	TickFootsteps(Character, DeltaTime);
}
