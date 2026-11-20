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
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "UI/DIAHUD.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace DIACheats
{
	/** Height of the top view camera above the centre of the lot when dia.TopView gets no height: the whole car park and its strip fit. */
	constexpr double TopViewDefaultHeightM = 260.0;
	/** Yaw of the top view camera: +X is up in the picture and +Y to the right, so the plant (towards -Y) is on the left and the mall quadrant at the top right. */
	constexpr double TopViewYawDeg = 0.0;
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
		if (!Usage(Args.Num() == 1, TEXT("dia.Give <credits>")))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->GetGame().DebugGive(FCString::Atod(*Args[0]));
			S->Dispatch(FLotEvents());
		}
	}

	void Complete(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1, TEXT("dia.Complete <n>  (completes the first n incomplete cells by index)")))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->Dispatch(S->GetGame().DebugCompleteCells(FCString::Atoi(*Args[0])));
		}
	}

	void CompleteEntry(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1, TEXT("dia.CompleteEntry <0..5>")))
		{
			return;
		}
		const int32 Entry = FCString::Atoi(*Args[0]);
		if (!Usage(Entry >= 0 && Entry < AsphaltLot::NumEntries, TEXT("dia.CompleteEntry <0..5>")))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			FLotEvents Events;
			for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
			{
				Events.Append(S->GetGame().DebugCompleteCell(AsphaltLot::EntryCell(Entry, I)));
			}
			S->Dispatch(Events);
		}
	}

	/** The names dia.Item accepts: the ELotItem values in order. */
	const TCHAR* const ItemNames[] = {TEXT("Scoop"), TEXT("Bucket"), TEXT("Suitcase"), TEXT("Cart"), TEXT("Station"), TEXT("Finisher"),
		TEXT("AutoPaver"), TEXT("AutoPaverFast"), TEXT("AutoPaverFaster"), TEXT("Car"), TEXT("CarWideBar")};
	static_assert(UE_ARRAY_COUNT(ItemNames) == static_cast<int32>(ELotItem::Count), "one name per item");

	void Item(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Usage_ = TEXT("dia.Item <Scoop|Bucket|Suitcase|Cart|Station|Finisher|AutoPaver|AutoPaverFast|AutoPaverFaster|Car|CarWideBar>");
		if (!Usage(Args.Num() == 1, Usage_))
		{
			return;
		}
		int32 Found = INDEX_NONE;
		for (int32 I = 0; I < UE_ARRAY_COUNT(ItemNames); ++I)
		{
			if (Args[0].Equals(ItemNames[I], ESearchCase::IgnoreCase))
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
			const ELotItem Granted = static_cast<ELotItem>(Found);
			S->GetGame().DebugGrantItem(Granted);
			S->OnItemBought.Broadcast(Granted);
			S->Dispatch(FLotEvents());
		}
	}

	void Plant(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1, TEXT("dia.Plant <0..5>")))
		{
			return;
		}
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->GetGame().DebugSetPlantLevel(FCString::Atoi(*Args[0]));
			S->Dispatch(FLotEvents());
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
		if (!Pawn->TeleportTo(FVector(X, Y, Pawn->GetActorLocation().Z), FRotator(0.0, Yaw, 0.0)))
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Teleport: the spot (%.0f, %.0f) is blocked, the player stays where it is"), X, Y);
		}
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.0));
		PC->SetViewTarget(Pawn);

		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->GetGame().World().PlayerPos = FVector2D(X, Y);
			S->GetGame().World().PlayerYawDeg = Yaw;
		}
	}

	ADIAHUD* GetHud(UWorld* World)
	{
		APlayerController* PC = GetPlayerController(World);
		ADIAHUD* Hud = PC ? Cast<ADIAHUD>(PC->GetHUD()) : nullptr;
		if (PC && !Hud)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("DIA cheat: the player has no ADIAHUD"));
		}
		return Hud;
	}

	void Prompt(const TArray<FString>& Args, UWorld* World)
	{
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->SetPrompt(FText::FromString(FString::Join(Args, TEXT(" "))));
		}
	}

	void Toast(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() >= 1, TEXT("dia.Toast <text>")))
		{
			return;
		}
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->ShowToast(FText::FromString(FString::Join(Args, TEXT(" "))));
		}
	}

	void SkipMenu(const TArray<FString>& Args, UWorld* World)
	{
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->SkipMenu();
		}
	}

	/** Puts the third-person camera <distance> cm behind the player, turned by <yawOffset> from the player's heading (for close-ups of the equipment). */
	void CameraCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() >= 1 && Args.Num() <= 3, TEXT("dia.Camera <distanceCm> [yawOffsetDeg] [pitchDeg]")))
		{
			return;
		}
		APlayerController* PC = GetPlayerController(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		USpringArmComponent* Boom = Pawn ? Pawn->FindComponentByClass<USpringArmComponent>() : nullptr;
		if (!Boom)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Camera: the player has no camera boom"));
			return;
		}
		Boom->TargetArmLength = static_cast<float>(FCString::Atod(*Args[0]));
		const double YawOffset = Args.Num() >= 2 ? FCString::Atod(*Args[1]) : 0.0;
		const double Pitch = Args.Num() >= 3 ? FCString::Atod(*Args[2]) : 0.0;
		PC->SetControlRotation(FRotator(Pitch, Pawn->GetActorRotation().Yaw + YawOffset, 0.0));
		PC->SetViewTarget(Pawn);
	}

	TWeakObjectPtr<ACameraActor> TopViewCamera;

	void TopView(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() <= 1, TEXT("dia.TopView [heightM=260]")))
		{
			return;
		}
		APlayerController* PC = GetPlayerController(World);
		if (!PC)
		{
			return;
		}
		const double HeightCm = (Args.Num() == 1 ? FCString::Atod(*Args[0]) : TopViewDefaultHeightM) * 100.0;
		UWorld* PlayWorld = PC->GetWorld();
		const FVector Where(0.0, 0.0, HeightCm);
		const FRotator Looking(-90.0, TopViewYawDeg, 0.0);
		if (!TopViewCamera.IsValid() || TopViewCamera->GetWorld() != PlayWorld)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TopViewCamera = PlayWorld->SpawnActor<ACameraActor>(Where, Looking, Params);
		}
		else
		{
			TopViewCamera->SetActorLocationAndRotation(Where, Looking);
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
				// bShowUI: the picture includes the Slate HUD.
				FScreenshotRequest::RequestScreenshot(Path, true, false);
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
	FAutoConsoleCommandWithWorldAndArgs GiveCommand(TEXT("dia.Give"), TEXT("dia.Give <credits>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Give));
	FAutoConsoleCommandWithWorldAndArgs CompleteCommand(TEXT("dia.Complete"), TEXT("dia.Complete <n>: completes the first n incomplete cells"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Complete));
	FAutoConsoleCommandWithWorldAndArgs CompleteEntryCommand(TEXT("dia.CompleteEntry"), TEXT("dia.CompleteEntry <0..5>: completes the 12 cells of an entry segment"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CompleteEntry));
	FAutoConsoleCommandWithWorldAndArgs ItemCommand(TEXT("dia.Item"), TEXT("dia.Item <name>: gives the player an item (Scoop, Bucket, Suitcase, Cart, Station, Finisher, AutoPaver, AutoPaverFast, AutoPaverFaster, Car, CarWideBar)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Item));
	FAutoConsoleCommandWithWorldAndArgs PlantCommand(TEXT("dia.Plant"), TEXT("dia.Plant <0..5>: sets the plant level"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Plant));
	FAutoConsoleCommandWithWorldAndArgs TeleportCommand(TEXT("dia.Teleport"), TEXT("dia.Teleport <x> <y> [yaw] [pitch]"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Teleport));
	FAutoConsoleCommandWithWorldAndArgs PromptCommand(TEXT("dia.Prompt"), TEXT("dia.Prompt <text>: sets the contextual prompt (no text hides it)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Prompt));
	FAutoConsoleCommandWithWorldAndArgs ToastCommand(TEXT("dia.Toast"), TEXT("dia.Toast <text>: shows a toast"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Toast));
	FAutoConsoleCommandWithWorldAndArgs SkipMenuCommand(TEXT("dia.SkipMenu"), TEXT("dia.SkipMenu: closes the main menu without choosing anything (the game starts as it is; screenshots use it)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkipMenu));
	FAutoConsoleCommandWithWorldAndArgs CameraConsoleCommand(TEXT("dia.Camera"), TEXT("dia.Camera <distanceCm> [yawOffsetDeg] [pitchDeg]: third-person camera distance and angle relative to the player's heading"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CameraCommand));
	FAutoConsoleCommandWithWorldAndArgs TopViewCommand(TEXT("dia.TopView"), TEXT("dia.TopView [heightM=260]: camera above the centre of the lot, looking straight down (+X up, +Y right)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TopView));
	FAutoConsoleCommandWithWorldAndArgs ShotAndQuitCommand(TEXT("dia.ShotAndQuit"), TEXT("dia.ShotAndQuit <seconds> <name>: saves Saved/Screenshots/<name>.png and quits"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShotAndQuit));
}

#endif // !UE_BUILD_SHIPPING
