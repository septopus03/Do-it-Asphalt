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
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "UI/DIAHUD.h"
#include "World/DIAMovable.h"
#include "World/DIAPaverCar.h"
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

	/** The names dia.Grab accepts: the ELotMovable values in order. */
	const TCHAR* const MovableNames[] = {TEXT("Suitcase"), TEXT("Cart"), TEXT("Station"), TEXT("Finisher")};
	static_assert(UE_ARRAY_COUNT(MovableNames) == static_cast<int32>(ELotMovable::Count), "one name per movable");

	/** The movable a name stands for; Count if it is none. */
	ELotMovable ParseMovable(const FString& Name)
	{
		for (int32 I = 0; I < UE_ARRAY_COUNT(MovableNames); ++I)
		{
			if (Name.Equals(MovableNames[I], ESearchCase::IgnoreCase))
			{
				return static_cast<ELotMovable>(I);
			}
		}
		return ELotMovable::Count;
	}

	/** Gives the player the movable (if not owned yet) and fills a suitcase or a cart to the top from the plant stock; the cheat is for looking at things. */
	void ProvideMovable(UDIAWorldSubsystem& S, ELotMovable Movable)
	{
		FLotGame& Game = S.GetGame();
		if (!Game.OwnsMovable(Movable))
		{
			const ELotItem Item = Movable == ELotMovable::Suitcase ? ELotItem::Suitcase : Movable == ELotMovable::Cart ? ELotItem::Cart
				: Movable == ELotMovable::Station ? ELotItem::Station : ELotItem::Finisher;
			Game.DebugGrantItem(Item);
			S.OnItemBought.Broadcast(Item);
		}
		Game.DebugSetPlantStock(Game.GetPlantStorage());
		for (int32 I = 0; I < 100 && Game.GetMovableLitres(Movable) < Game.GetMovableCapacity(Movable); ++I)
		{
			if (Game.FillMovable(Movable, 1.0) <= 0.0)
			{
				break;
			}
		}
	}

	void Grab(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Usage_ = TEXT("dia.Grab <Suitcase|Cart|Station|Finisher>");
		if (!Usage(Args.Num() == 1, Usage_))
		{
			return;
		}
		const ELotMovable Movable = ParseMovable(Args[0]);
		if (!Usage(Movable != ELotMovable::Count, Usage_))
		{
			return;
		}
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S)
		{
			return;
		}
		ProvideMovable(*S, Movable);
		for (TActorIterator<ADIAMovable> It(S->GetWorld()); It; ++It)
		{
			if (It->GetKind() == Movable)
			{
				It->Interact(Cast<ADIACharacter>(GetPlayerController(World) ? GetPlayerController(World)->GetPawn() : nullptr));
			}
		}
		if (S->GetGame().GetHeld() != Movable)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Grab: could not take it (something else is held, or the player sits in the car)"));
		}
	}

	void Drop(const TArray<FString>& Args, UWorld* World)
	{
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S || S->GetGame().GetHeld() == ELotMovable::Count)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Drop: nothing is held"));
			return;
		}
		APlayerController* PC = GetPlayerController(World);
		for (TActorIterator<ADIAMovable> It(S->GetWorld()); It; ++It)
		{
			if (It->IsHeld())
			{
				It->TryRelease(PC ? Cast<ADIACharacter>(PC->GetPawn()) : nullptr);
			}
		}
	}

	/** Lays a raw strip of the cart's width through the core, from the point towards +X, as if the cart had been pushed there; the cart stays where it was. */
	void RawStrip(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() >= 2 && Args.Num() <= 3, TEXT("dia.RawStrip <x> <y> [lengthM=4]")))
		{
			return;
		}
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S)
		{
			return;
		}
		FLotGame& Game = S->GetGame();
		if (Game.GetHeld() != ELotMovable::Count)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.RawStrip: something is held; let go of it first (dia.Drop)"));
			return;
		}
		ProvideMovable(*S, ELotMovable::Cart);
		const FLotMovable Before = Game.World().Movables[static_cast<int32>(ELotMovable::Cart)];
		const FVector2D From(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]));
		const double LengthCm = (Args.Num() == 3 ? FCString::Atod(*Args[2]) : 4.0) * 100.0;
		if (Game.Grab(ELotMovable::Cart))
		{
			S->Dispatch(Game.LayCartStrip(From, From + FVector2D(LengthCm, 0.0)));
			Game.Release(Before.Pos, Before.YawDeg);
			S->Dispatch(FLotEvents());
		}
	}

	/** Finishes the raw asphalt of a strip of the finisher's width through the core, from the point towards +X, as if the finisher had been guided there; the finisher stays where it was. */
	void FinishStrip(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() >= 2 && Args.Num() <= 3, TEXT("dia.FinishStrip <x> <y> [lengthM=4]")))
		{
			return;
		}
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S)
		{
			return;
		}
		FLotGame& Game = S->GetGame();
		if (Game.GetHeld() != ELotMovable::Count)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.FinishStrip: something is held; let go of it first (dia.Drop)"));
			return;
		}
		ProvideMovable(*S, ELotMovable::Finisher);
		const FLotMovable Before = Game.World().Movables[static_cast<int32>(ELotMovable::Finisher)];
		const FVector2D From(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]));
		const double LengthCm = (Args.Num() == 3 ? FCString::Atod(*Args[2]) : 4.0) * 100.0;
		if (Game.Grab(ELotMovable::Finisher))
		{
			S->Dispatch(Game.FinisherPass(From, From + FVector2D(LengthCm, 0.0)));
			Game.Release(Before.Pos, Before.YawDeg);
			S->Dispatch(FLotEvents());
		}
	}

	/** Gives the player the movable (if needed) and sets it down at the point, turned by the yaw; the actor follows the core. */
	void Place(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Usage_ = TEXT("dia.Place <Suitcase|Cart|Station|Finisher> <x> <y> [yaw]");
		if (!Usage(Args.Num() >= 3 && Args.Num() <= 4, Usage_))
		{
			return;
		}
		const ELotMovable Movable = ParseMovable(Args[0]);
		if (!Usage(Movable != ELotMovable::Count, Usage_))
		{
			return;
		}
		UDIAWorldSubsystem* S = GetSubsystem(World);
		if (!S)
		{
			return;
		}
		FLotGame& Game = S->GetGame();
		if (Game.GetHeld() != ELotMovable::Count)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Place: something is held; let go of it first (dia.Drop)"));
			return;
		}
		ProvideMovable(*S, Movable);
		if (Game.Grab(Movable))
		{
			Game.Release(FVector2D(FCString::Atod(*Args[1]), FCString::Atod(*Args[2])), Args.Num() == 4 ? FCString::Atod(*Args[3]) : 0.0);
			S->Dispatch(FLotEvents());
		}
	}

	/** Gets the player into the car wherever it stands: the same call as the E key makes (the player teleports to the car first). */
	void EnterCar(const TArray<FString>& Args, UWorld* World)
	{
		UDIAWorldSubsystem* S = GetSubsystem(World);
		APlayerController* PC = GetPlayerController(World);
		if (!S || !PC)
		{
			return;
		}
		if (PC->GetPawn() && PC->GetPawn()->IsA<ADIAPaverCar>())
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.EnterCar: the player drives the car already"));
			return;
		}
		ADIACharacter* Character = Cast<ADIACharacter>(PC->GetPawn());
		TActorIterator<ADIAPaverCar> Car(S->GetWorld());
		if (!Car)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.EnterCar: there is no car (dia.Item Car)"));
			return;
		}
		Car->Interact(Character);
		if (!Car->HasDriver())
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.EnterCar: could not get in (something is held, or the player is not on foot)"));
		}
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

	void SendPaver(const TArray<FString>& Args, UWorld* World)
	{
		// The same code path as the G key (it speaks through the toasts, and a menu or the shop swallows it just as it does the key).
		if (ADIAPlayerController* PC = Cast<ADIAPlayerController>(GetPlayerController(World)))
		{
			PC->OnSendPaver();
		}
	}

	void Save(const TArray<FString>& Args, UWorld* World)
	{
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->SaveGame();
		}
	}

	void Load(const TArray<FString>& Args, UWorld* World)
	{
		// The same call as Continue in the main menu.
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->ContinueGame();
		}
	}

	void NewGameCheat(const TArray<FString>& Args, UWorld* World)
	{
		// The same call as New Game in the menus.
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->NewGame();
		}
	}

	void SkipMenu(const TArray<FString>& Args, UWorld* World)
	{
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->SkipMenu();
		}
	}

	ADIACharacter* GetCharacter(UWorld* World)
	{
		APlayerController* PC = GetPlayerController(World);
		ADIACharacter* Character = PC ? Cast<ADIACharacter>(PC->GetPawn()) : nullptr;
		if (PC && !Character)
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("DIA cheat: the player is not an ADIACharacter"));
		}
		return Character;
	}

	/** Without arguments toggles between the view from the eyes and the external view; with them shows the external view <distance> cm behind the player (0 = from the eyes). */
	void CameraCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() <= 3, TEXT("dia.Camera [distanceCm] [yawOffsetDeg] [pitchDeg]")))
		{
			return;
		}
		ADIACharacter* Character = GetCharacter(World);
		APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (!PC)
		{
			return;
		}
		const double Distance = Args.Num() >= 1 ? FCString::Atod(*Args[0]) : (Character->IsExternalView() ? 0.0 : DIACharacter::CameraBoomLengthCm);
		if (Distance <= 0.0)
		{
			Character->SetExternalView(false);
		}
		else
		{
			Character->SetExternalView(true, static_cast<float>(Distance));
			const double YawOffset = Args.Num() >= 2 ? FCString::Atod(*Args[1]) : 0.0;
			const double Pitch = Args.Num() >= 3 ? FCString::Atod(*Args[2]) : 0.0;
			PC->SetControlRotation(FRotator(Pitch, Character->GetActorRotation().Yaw + YawOffset, 0.0));
		}
		PC->SetViewTarget(Character);
	}

	void WorkView(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1 && (Args[0] == TEXT("0") || Args[0] == TEXT("1")), TEXT("dia.WorkView <1|0>")))
		{
			return;
		}
		if (ADIACharacter* Character = GetCharacter(World))
		{
			Character->SetWorkView(Args[0] == TEXT("1"));
		}
	}

	void Shop(const TArray<FString>& Args, UWorld* World)
	{
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->ToggleShop();
		}
	}

	void CheckView(const TArray<FString>& Args, UWorld* World)
	{
		if (!Usage(Args.Num() == 1 && (Args[0] == TEXT("0") || Args[0] == TEXT("1")), TEXT("dia.CheckView <1|0>")))
		{
			return;
		}
		if (ADIAHUD* Hud = GetHud(World))
		{
			Hud->SetCheckView(Args[0] == TEXT("1"));
		}
	}

	/** The names dia.Hand accepts: the ELotHand values in order. */
	const TCHAR* const HandNames[] = {TEXT("Bowl"), TEXT("Scoop"), TEXT("Bucket"), TEXT("Trowel")};
	static_assert(UE_ARRAY_COUNT(HandNames) == static_cast<int32>(ELotHand::Count), "one name per hand item");

	void Hand(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* Usage_ = TEXT("dia.Hand <Bowl|Scoop|Bucket|Trowel>");
		if (!Usage(Args.Num() == 1, Usage_))
		{
			return;
		}
		int32 Found = INDEX_NONE;
		for (int32 I = 0; I < UE_ARRAY_COUNT(HandNames); ++I)
		{
			if (Args[0].Equals(HandNames[I], ESearchCase::IgnoreCase))
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
			const ELotHand Wanted = static_cast<ELotHand>(Found);
			// A tool the player has not bought yet is given first (the cheat is for looking at things).
			if (Wanted == ELotHand::Scoop)
			{
				S->GetGame().DebugGrantItem(ELotItem::Scoop);
			}
			else if (Wanted == ELotHand::Bucket)
			{
				S->GetGame().DebugGrantItem(ELotItem::Bucket);
			}
			if (!S->GetGame().SelectHand(Wanted))
			{
				UE_LOG(LogDoItAsphalt, Warning, TEXT("dia.Hand: the hands are busy or hold something"));
			}
			S->Dispatch(FLotEvents());
		}
	}

	void Fill(const TArray<FString>& Args, UWorld* World)
	{
		if (UDIAWorldSubsystem* S = GetSubsystem(World))
		{
			S->GetGame().DebugFillHand();
			S->Dispatch(FLotEvents());
		}
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
	FAutoConsoleCommandWithWorldAndArgs GrabCommand(TEXT("dia.Grab"), TEXT("dia.Grab <Suitcase|Cart|Station|Finisher>: gives the player the movable if needed, fills a suitcase or cart and takes it in the hands"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Grab));
	FAutoConsoleCommandWithWorldAndArgs DropCommand(TEXT("dia.Drop"), TEXT("dia.Drop: lets go of the held movable where it is, if there is room"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Drop));
	FAutoConsoleCommandWithWorldAndArgs RawStripCommand(TEXT("dia.RawStrip"), TEXT("dia.RawStrip <x> <y> [lengthM=4]: lays a raw strip of the cart's width from the point towards +X through the core (gives and fills the cart)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RawStrip));
	FAutoConsoleCommandWithWorldAndArgs FinishStripCommand(TEXT("dia.FinishStrip"), TEXT("dia.FinishStrip <x> <y> [lengthM=4]: finishes the raw asphalt of a strip of the finisher's width from the point towards +X through the core (gives the finisher)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FinishStrip));
	FAutoConsoleCommandWithWorldAndArgs PlaceCommand(TEXT("dia.Place"), TEXT("dia.Place <Suitcase|Cart|Station|Finisher> <x> <y> [yaw]: gives the movable if needed and sets it down at the point"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Place));
	FAutoConsoleCommandWithWorldAndArgs PlantCommand(TEXT("dia.Plant"), TEXT("dia.Plant <0..5>: sets the plant level"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Plant));
	FAutoConsoleCommandWithWorldAndArgs EnterCarCommand(TEXT("dia.EnterCar"), TEXT("dia.EnterCar: gets the player into the paver car (dia.Item Car first), like the E key beside it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EnterCar));
	FAutoConsoleCommandWithWorldAndArgs TeleportCommand(TEXT("dia.Teleport"), TEXT("dia.Teleport <x> <y> [yaw] [pitch]"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Teleport));
	FAutoConsoleCommandWithWorldAndArgs PromptCommand(TEXT("dia.Prompt"), TEXT("dia.Prompt <text>: sets the contextual prompt (no text hides it)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Prompt));
	FAutoConsoleCommandWithWorldAndArgs ToastCommand(TEXT("dia.Toast"), TEXT("dia.Toast <text>: shows a toast"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Toast));
	FAutoConsoleCommandWithWorldAndArgs SendPaverCommand(TEXT("dia.SendPaver"), TEXT("dia.SendPaver: sends the auto paver to the sector the player stands in (the G key)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SendPaver));
	FAutoConsoleCommandWithWorldAndArgs SaveCommand(TEXT("dia.Save"), TEXT("dia.Save: saves the game to the slot (the slot of a screenshot run is DIA_Shot)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Save));
	FAutoConsoleCommandWithWorldAndArgs LoadCommand(TEXT("dia.Load"), TEXT("dia.Load: loads the saved game, like Continue in the main menu"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Load));
	FAutoConsoleCommandWithWorldAndArgs NewGameConsoleCommand(TEXT("dia.NewGame"), TEXT("dia.NewGame: starts over, like New Game in the menus (the slot is deleted)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NewGameCheat));
	FAutoConsoleCommandWithWorldAndArgs SkipMenuCommand(TEXT("dia.SkipMenu"), TEXT("dia.SkipMenu: closes the main menu without choosing anything (the game starts as it is; screenshots use it)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkipMenu));
	FAutoConsoleCommandWithWorldAndArgs CameraConsoleCommand(TEXT("dia.Camera"), TEXT("dia.Camera [distanceCm] [yawOffsetDeg] [pitchDeg]: no arguments toggle between the view from the eyes and the external view; otherwise the external view that far behind the player at that angle (0 = from the eyes)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CameraCommand));
	FAutoConsoleCommandWithWorldAndArgs WorkViewConsoleCommand(TEXT("dia.WorkView"), TEXT("dia.WorkView <1|0>: holds the work view (zoomed, tilted down) or lets it go; screenshots use it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkView));
	FAutoConsoleCommandWithWorldAndArgs ShopConsoleCommand(TEXT("dia.Shop"), TEXT("dia.Shop: opens or closes the shop; screenshots use it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Shop));
	FAutoConsoleCommandWithWorldAndArgs CheckViewConsoleCommand(TEXT("dia.CheckView"), TEXT("dia.CheckView <1|0>: holds the check view (unfinished ground in orange, the way to the nearest unfinished cell) or lets it go; screenshots use it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CheckView));
	FAutoConsoleCommandWithWorldAndArgs HandConsoleCommand(TEXT("dia.Hand"), TEXT("dia.Hand <Bowl|Scoop|Bucket|Trowel>: takes the item into the hand (a scoop or bucket not owned yet is given)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Hand));
	FAutoConsoleCommandWithWorldAndArgs FillConsoleCommand(TEXT("dia.Fill"), TEXT("dia.Fill: fills the vessel in hand to its capacity"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Fill));
	FAutoConsoleCommandWithWorldAndArgs TopViewCommand(TEXT("dia.TopView"), TEXT("dia.TopView [heightM=260]: camera above the centre of the lot, looking straight down (+X up, +Y right)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TopView));
	FAutoConsoleCommandWithWorldAndArgs ShotAndQuitCommand(TEXT("dia.ShotAndQuit"), TEXT("dia.ShotAndQuit <seconds> <name>: saves Saved/Screenshots/<name>.png and quits"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShotAndQuit));
}

#endif // !UE_BUILD_SHIPPING
