// Copyright Do it Asphalt. All Rights Reserved.

// Console commands for debugging and for automatic screenshots. Not compiled into Shipping builds.
#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif
#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "DoItAsphalt.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/DIAWorldSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace DIACheats
{
	/** Height of the top view camera above the centre of the site. */
	constexpr double TopViewHeightCm = 4500.0;
	/** Longest time ShotAndQuit waits for shaders and assets to finish compiling, in seconds. */
	constexpr double CompileTimeoutSeconds = 300.0;
	/** Quiet time after compiling finished before the picture is taken (frames need to show the final materials). */
	constexpr double SettleSeconds = 1.0;
	/** Longest time ShotAndQuit waits for the PNG to appear on disk. */
	constexpr double WriteTimeoutSeconds = 30.0;

	/** The console passes the world only when it knows one; during start-up fall back to the running game world. */
	UWorld* ResolveWorld(UWorld* World)
	{
		if (World && World->GetSubsystem<UDIAWorldSubsystem>())
		{
			return World;
		}
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) &&
					Context.World() && Context.World()->GetSubsystem<UDIAWorldSubsystem>())
				{
					return Context.World();
				}
			}
		}
		return nullptr;
	}

	UDIAWorldSubsystem* GetSubsystem(UWorld* World)
	{
		UWorld* Resolved = ResolveWorld(World);
		if (!Resolved)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("DIA cheat: no game world with a UDIAWorldSubsystem"));
			return nullptr;
		}
		return Resolved->GetSubsystem<UDIAWorldSubsystem>();
	}

	bool Usage(bool bOk, const TCHAR* Text)
	{
		if (!bOk)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("Usage: %s"), Text);
		}
		return bOk;
	}

	void Give(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 2, TEXT("dia.Give <money> <shards>")))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->GetGame().DebugGive(FCString::Atod(*Args[0]), FCString::Atoi(*Args[1]));
			S->Dispatch(FAsphaltEvents());
		}
	}

	void Complete(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1, TEXT("dia.Complete <n>  (completes the first n incomplete cells by index)")))
		{
			return;
		}
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S)
		{
			return;
		}
		FAsphaltGame& Game = S->GetGame();
		int32 Remaining = FCString::Atoi(*Args[0]);
		FAsphaltEvents Events;
		for (int32 Cell = 0; Cell < Game.GetMap().Num() && Remaining > 0; ++Cell)
		{
			if (!Game.GetGrid().IsCellComplete(Cell))
			{
				Events.Append(Game.DebugCompleteCell(Cell));
				--Remaining;
			}
		}
		S->Dispatch(Events);
	}

	void CompleteRoad(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1, TEXT("dia.CompleteRoad <0..5>")))
		{
			return;
		}
		const int32 Road = FCString::Atoi(*Args[0]);
		if (!Usage(Road >= 0 && Road < FHexMap::NumRoads, TEXT("dia.CompleteRoad <0..5>")))
		{
			return;
		}
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S)
		{
			return;
		}
		FAsphaltGame& Game = S->GetGame();
		FAsphaltEvents Events;
		for (const int32 Cell : Game.GetMap().GetRoadCells(Road))
		{
			if (!Game.GetGrid().IsCellComplete(Cell))
			{
				Events.Append(Game.DebugCompleteCell(Cell));
			}
		}
		S->Dispatch(Events);
	}

	void SetLevels(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 3, TEXT("dia.SetLevels <backpack> <tool> <plant>")))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			FAsphaltGame& Game = S->GetGame();
			Game.DebugSetLevel(EAsphaltUpgrade::Backpack, FCString::Atoi(*Args[0]));
			Game.DebugSetLevel(EAsphaltUpgrade::Tool, FCString::Atoi(*Args[1]));
			Game.DebugSetLevel(EAsphaltUpgrade::Plant, FCString::Atoi(*Args[2]));
			S->Dispatch(FAsphaltEvents());
		}
	}

	void Special(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Usage_ = TEXT("dia.Special <Sprint|TwoHands|PortableStation|AutoPaver|PaverCar>");
		if (!Usage(Args.Num() == 1, Usage_))
		{
			return;
		}
		static const TCHAR* Names[] = {TEXT("Sprint"), TEXT("TwoHands"), TEXT("PortableStation"), TEXT("AutoPaver"), TEXT("PaverCar")};
		static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EAsphaltSpecial::Count), "one name per special");
		int32 Found = INDEX_NONE;
		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			if (Args[0].Equals(Names[I], ESearchCase::IgnoreCase))
			{
				Found = I;
			}
		}
		if (!Usage(Found != INDEX_NONE, Usage_))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			const EAsphaltSpecial Special = static_cast<EAsphaltSpecial>(Found);
			S->GetGame().DebugGrantSpecial(Special);
			S->OnSpecialBought.Broadcast(Special);
			S->Dispatch(FAsphaltEvents());
		}
	}

	APlayerController* GetPlayerController(UWorld* World)
	{
		UWorld* Resolved = ResolveWorld(World);
		APlayerController* PC = Resolved ? Resolved->GetFirstPlayerController() : nullptr;
		if (!PC)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("DIA cheat: there is no player controller"));
		}
		return PC;
	}

	void Teleport(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() >= 2 && Args.Num() <= 4, TEXT("dia.Teleport <x> <y> [yaw] [pitch]")))
		{
			return;
		}
		APlayerController* PC = GetPlayerController(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Teleport: the player has no pawn"));
			return;
		}
		const double X = FCString::Atod(*Args[0]);
		const double Y = FCString::Atod(*Args[1]);
		const double Yaw = Args.Num() >= 3 ? FCString::Atod(*Args[2]) : 0.0;
		const double Pitch = Args.Num() >= 4 ? FCString::Atod(*Args[3]) : 0.0;

		// Keep the pawn's height: whatever stands on the ground already has the right one. The pawn turns by yaw
		// only; the camera (control rotation) also gets the pitch.
		Pawn->TeleportTo(FVector(X, Y, Pawn->GetActorLocation().Z), FRotator(0.0, Yaw, 0.0));
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.0));
		PC->SetViewTarget(Pawn);

		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->GetGame().World().PlayerPos = FVector2D(X, Y);
			S->GetGame().World().PlayerYawDeg = Yaw;
		}
	}

	TWeakObjectPtr<ACameraActor> TopViewCamera;

	void TopView(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = GetPlayerController(World);
		if (!PC)
		{
			return;
		}
		UWorld* PlayWorld = PC->GetWorld();
		if (!TopViewCamera.IsValid() || TopViewCamera->GetWorld() != PlayWorld)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TopViewCamera = PlayWorld->SpawnActor<ACameraActor>(FVector(0.0, 0.0, TopViewHeightCm), FRotator(-90.0, 0.0, 0.0), Params);
		}
		if (TopViewCamera.IsValid())
		{
			PC->SetViewTarget(TopViewCamera.Get());
		}
	}

	bool StillCompiling()
	{
#if WITH_EDITOR
		return (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) ||
			FAssetCompilingManager::Get().GetNumRemainingAssets() > 0;
#else
		return false;
#endif
	}

	/** Waits <seconds>, then for the compilers to go quiet, saves Saved/Screenshots/<name>.png and quits. */
	void ShotAndQuit(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 2, TEXT("dia.ShotAndQuit <seconds> <name>")))
		{
			return;
		}
		const double Delay = FMath::Max(0.0, FCString::Atod(*Args[0]));
		// Mouse movement while the window opens would turn the camera and spoil the picture; a shot run ignores look input.
		if (APlayerController* PC = GetPlayerController(World))
		{
			PC->SetIgnoreLookInput(true);
		}
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / (Args[1] + TEXT(".png")));

		struct FState
		{
			double Elapsed = 0.0;
			double QuietFor = 0.0;
			double WaitedForFile = 0.0;
			bool bRequested = false;
		};
		TSharedRef<FState> State = MakeShared<FState>();

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State, Delay, Path](float Dt)
		{
			State->Elapsed += Dt;
			if (!State->bRequested)
			{
				if (State->Elapsed < Delay)
				{
					return true;
				}
				// Materials that are still compiling would show up as the default grey material.
				State->QuietFor = StillCompiling() ? 0.0 : State->QuietFor + Dt;
				if (State->QuietFor < SettleSeconds && State->Elapsed < Delay + CompileTimeoutSeconds)
				{
					return true;
				}
				IFileManager::Get().Delete(*Path, false, true, true);
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
				FScreenshotRequest::RequestScreenshot(Path, false, false);
				State->bRequested = true;
				UE_LOG(LogDoItAsphalt, Log, TEXT("dia.ShotAndQuit: screenshot requested: %s"), *Path);
				return true;
			}

			State->WaitedForFile += Dt;
			const bool bWritten = IFileManager::Get().FileExists(*Path);
			if (bWritten || State->WaitedForFile > WriteTimeoutSeconds)
			{
				UE_LOG(LogDoItAsphalt, Log, TEXT("dia.ShotAndQuit: %s, quitting"), bWritten ? TEXT("screenshot written") : TEXT("screenshot NOT written"));
				// A short grace period lets the writer thread finish closing the file.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
				{
					FPlatformMisc::RequestExit(false);
					return false;
				}), 0.5f);
				return false;
			}
			return true;
		}));
	}

	// The registrations keep the commands alive for the lifetime of the module.
	FAutoConsoleCommandWithWorldAndArgs GiveCommand(TEXT("dia.Give"), TEXT("dia.Give <money> <shards>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Give));
	FAutoConsoleCommandWithWorldAndArgs CompleteCommand(TEXT("dia.Complete"), TEXT("dia.Complete <n>: completes the first n incomplete cells"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Complete));
	FAutoConsoleCommandWithWorldAndArgs CompleteRoadCommand(TEXT("dia.CompleteRoad"), TEXT("dia.CompleteRoad <0..5>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CompleteRoad));
	FAutoConsoleCommandWithWorldAndArgs SetLevelsCommand(TEXT("dia.SetLevels"), TEXT("dia.SetLevels <backpack> <tool> <plant>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetLevels));
	FAutoConsoleCommandWithWorldAndArgs SpecialCommand(TEXT("dia.Special"), TEXT("dia.Special <Sprint|TwoHands|PortableStation|AutoPaver|PaverCar>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Special));
	FAutoConsoleCommandWithWorldAndArgs TeleportCommand(TEXT("dia.Teleport"), TEXT("dia.Teleport <x> <y> [yaw] [pitch]"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Teleport));
	FAutoConsoleCommandWithWorldAndArgs TopViewCommand(TEXT("dia.TopView"), TEXT("dia.TopView: camera 45 m above the centre, looking straight down"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TopView));
	FAutoConsoleCommandWithWorldAndArgs ShotAndQuitCommand(TEXT("dia.ShotAndQuit"), TEXT("dia.ShotAndQuit <seconds> <name>: saves Saved/Screenshots/<name>.png and quits"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShotAndQuit));
}

#endif // !UE_BUILD_SHIPPING
