// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAWorldSubsystem.h"
#include "DoItAsphalt.h"
#include "EngineUtils.h"
#include "Game/DIASaveGame.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "LotBalance.h"
#include "Misc/CommandLine.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "Player/DIACharacter.h"

namespace
{
	/** Longest slice of real time given to the core at once, so that a hitch or a pause cannot cause a jump. */
	constexpr double MaxTickSeconds = 1.0;
}

namespace
{
	/** The slot name set from code (tests); empty while there is none. */
	FString& SlotOverride()
	{
		static FString Name;
		return Name;
	}

	/** The command line switch -DIASlot=<name> (screenshots), read once; empty if absent. */
	const FString& SlotFromCommandLine()
	{
		static const FString Name = []()
		{
			FString Value;
			FParse::Value(FCommandLine::Get(), TEXT("-DIASlot="), Value);
			return Value;
		}();
		return Name;
	}

	/** Every engine save file starts with these four bytes; anything else is not one. */
	constexpr uint8 SaveFileMagic[] = {'G', 'V', 'A', 'S'};

	/**
	 * Our own footer after the engine's save: these four bytes and the CRC of everything before them. The engine's reader trusts the
	 * sizes in the file and can bring the game down on a file that a power loss cut short; a file whose footer is missing or wrong is
	 * never given to it.
	 */
	constexpr uint8 SaveFooterMagic[] = {'D', 'I', 'A', 'E'};
	constexpr int32 SaveFooterBytes = sizeof(SaveFooterMagic) + sizeof(uint32);

	/**
	 * The core's blob inside the named slot; false (and an empty array) if the slot is missing or holds something else.
	 * Only a blob that the core accepts counts, so that what this returns can always be loaded.
	 */
	bool ReadSlotBlob(const FString& Slot, TArray<uint8>& OutBlob)
	{
		OutBlob.Reset();
		ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
		// No file at all is the usual case (a first start) and is not worth a warning in the log.
		if (!SaveSystem || !SaveSystem->DoesSaveGameExist(*Slot, DIASave::UserIndex))
		{
			return false;
		}
		// The engine can bring the whole game down on a damaged file (a write that was cut short leaves exactly that),
		// so the file is checked here first: footer, CRC and the engine's magic.
		TArray<uint8> File;
		if (!SaveSystem->LoadGame(false, *Slot, DIASave::UserIndex, File) || File.Num() < SaveFooterBytes + UE_ARRAY_COUNT(SaveFileMagic))
		{
			return false;
		}
		const int32 BodyBytes = File.Num() - SaveFooterBytes;
		uint32 StoredCrc = 0;
		FMemory::Memcpy(&StoredCrc, File.GetData() + BodyBytes + sizeof(SaveFooterMagic), sizeof(StoredCrc));
		if (FMemory::Memcmp(File.GetData() + BodyBytes, SaveFooterMagic, sizeof(SaveFooterMagic)) != 0
			|| StoredCrc != FCrc::MemCrc32(File.GetData(), BodyBytes)
			|| FMemory::Memcmp(File.GetData(), SaveFileMagic, sizeof(SaveFileMagic)) != 0)
		{
			return false;
		}
		File.SetNum(BodyBytes);
		const UDIASaveGame* Save = Cast<UDIASaveGame>(UGameplayStatics::LoadGameFromMemory(File));
		if (!Save)
		{
			return false;
		}
		FLotGame Scratch;
		if (!Scratch.LoadFromBytes(Save->Blob))
		{
			return false;
		}
		OutBlob = Save->Blob;
		return true;
	}

	/** The blob of the slot, or, if the slot is damaged (a write that was cut short), of its backup. */
	bool ReadSavedBlob(TArray<uint8>& OutBlob)
	{
		if (ReadSlotBlob(UDIAWorldSubsystem::SlotName(), OutBlob))
		{
			return true;
		}
		if (UGameplayStatics::DoesSaveGameExist(UDIAWorldSubsystem::SlotName(), DIASave::UserIndex))
		{
			UE_LOG(LogDoItAsphalt, Warning, TEXT("The save slot %s is damaged; trying its backup"), *UDIAWorldSubsystem::SlotName());
		}
		return ReadSlotBlob(UDIAWorldSubsystem::BackupSlotName(), OutBlob);
	}
}

bool UDIAWorldSubsystem::WriteSlot(const FString& Slot, const TArray<uint8>& Blob)
{
	UDIASaveGame* Save = NewObject<UDIASaveGame>(GetTransientPackage());
	Save->Blob = Blob;
	TArray<uint8> File;
	ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
	if (!SaveSystem || !UGameplayStatics::SaveGameToMemory(Save, File))
	{
		return false;
	}
	const uint32 Crc = FCrc::MemCrc32(File.GetData(), File.Num());
	File.Append(SaveFooterMagic, sizeof(SaveFooterMagic));
	File.Append(reinterpret_cast<const uint8*>(&Crc), sizeof(Crc));
	return SaveSystem->SaveGame(false, *Slot, DIASave::UserIndex, File);
}

FString UDIAWorldSubsystem::SlotName()
{
	if (!SlotOverride().IsEmpty())
	{
		return SlotOverride();
	}
	return SlotFromCommandLine().IsEmpty() ? FString(DIASave::DefaultSlotName) : SlotFromCommandLine();
}

FString UDIAWorldSubsystem::BackupSlotName()
{
	return SlotName() + DIASave::BackupSuffix;
}

void UDIAWorldSubsystem::SetSlotNameOverride(const FString& Name)
{
	SlotOverride() = Name;
}

bool UDIAWorldSubsystem::HasSave() const
{
	TArray<uint8> Blob;
	return ReadSavedBlob(Blob);
}

bool UDIAWorldSubsystem::LoadGame()
{
	TArray<uint8> Blob;
	// LoadFromBytes checks everything before it changes anything, so a failure leaves the game as it is.
	if (!ReadSavedBlob(Blob) || !Game.LoadFromBytes(Blob))
	{
		return false;
	}
	AutosaveTimer = 0.0;
	// The actors take their positions from the core first; the state broadcast that follows then finds nothing left to do.
	OnGameReset.Broadcast();
	OnStateChanged.Broadcast();
	return true;
}

void UDIAWorldSubsystem::WritePlayerPositions()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FLotWorldState& State = Game.World();
	TActorIterator<ADIACharacter> Character(World);
	if (Character)
	{
		const FVector Location = Character->GetActorLocation();
		State.PlayerPos = FVector2D(Location.X, Location.Y);
		State.PlayerYawDeg = Character->GetActorRotation().Yaw;
	}
}

void UDIAWorldSubsystem::SaveGame()
{
	// Any save starts the countdown to the next autosave again.
	AutosaveTimer = 0.0;
	WritePlayerPositions();
	const TArray<uint8> Blob = Game.SaveToBytes();
	// The slot is written in place, so a write that is cut short (power loss) leaves it damaged. The backup is only written
	// after the slot was, so one of the two always holds a complete save.
	if (!WriteSlot(SlotName(), Blob) || !WriteSlot(BackupSlotName(), Blob))
	{
		UE_LOG(LogDoItAsphalt, Warning, TEXT("The game could not be saved to the slot %s"), *SlotName());
	}
}

void UDIAWorldSubsystem::NewGame()
{
	Game = FLotGame();
	// Both: a backup that stayed would bring the old game back.
	UGameplayStatics::DeleteGameInSlot(SlotName(), DIASave::UserIndex);
	UGameplayStatics::DeleteGameInSlot(BackupSlotName(), DIASave::UserIndex);
	AutosaveTimer = 0.0;
	OnGameReset.Broadcast();
	OnStateChanged.Broadcast();
}

void UDIAWorldSubsystem::Autosave()
{
	if (bAutosaveEnabled)
	{
		SaveGame();
	}
}

void UDIAWorldSubsystem::Dispatch(const FLotEvents& Events)
{
	if (Events.CompletedCells.Num() > 0)
	{
		OnCellsCompleted.Broadcast(Events.CompletedCells);
	}
	for (const int32 Entry : Events.CompletedEntries)
	{
		OnEntryCompleted.Broadcast(Entry);
	}
	for (const ELotUnlock Unlock : Events.Unlocked)
	{
		OnUnlocked.Broadcast(Unlock);
	}
	if (Events.bExitOpened)
	{
		OnExitOpened.Broadcast();
	}
	OnStateChanged.Broadcast();
}

bool UDIAWorldSubsystem::BuyItem(ELotItem Item)
{
	if (!Game.BuyItem(Item))
	{
		return false;
	}
	OnItemBought.Broadcast(Item);
	OnStateChanged.Broadcast();
	Autosave();
	return true;
}

bool UDIAWorldSubsystem::BuyPlant()
{
	if (!Game.BuyPlant())
	{
		return false;
	}
	OnStateChanged.Broadcast();
	Autosave();
	return true;
}

void UDIAWorldSubsystem::Win()
{
	if (Game.HasWon())
	{
		return;
	}
	Game.SetWon();
	if (!Game.HasWon())
	{
		return;
	}
	OnWon.Broadcast();
	OnStateChanged.Broadcast();
	Autosave();
}

void UDIAWorldSubsystem::RegisterInteractable(AActor* Actor, IDIAInteractable* Interactable)
{
	if (!Actor || !Interactable)
	{
		return;
	}
	UnregisterInteractable(Actor);
	Interactables.Add({Actor, Interactable});
}

void UDIAWorldSubsystem::UnregisterInteractable(const AActor* Actor)
{
	Interactables.RemoveAll([Actor](const FInteractableEntry& Entry) { return !Entry.Actor.IsValid() || Entry.Actor.Get() == Actor; });
}

IDIAInteractable* UDIAWorldSubsystem::FindInteractable(const ADIACharacter* Who, AActor** OutActor) const
{
	if (OutActor)
	{
		*OutActor = nullptr;
	}
	if (!Who)
	{
		return nullptr;
	}
	// What the player holds in the hands comes first: E lets it go, whatever else is in range and however far its origin is.
	for (const FInteractableEntry& Entry : Interactables)
	{
		if (Entry.Actor.IsValid() && Entry.Interactable->IsHeldByPlayer())
		{
			if (OutActor)
			{
				*OutActor = Entry.Actor.Get();
			}
			return Entry.Interactable;
		}
	}
	const FVector Point = Who->GetActorLocation();
	IDIAInteractable* Best = nullptr;
	double BestDistance = LotBalance::InteractionRangeCm;
	for (const FInteractableEntry& Entry : Interactables)
	{
		AActor* Actor = Entry.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		// Distance to the collision envelope; an actor without collision is measured to its origin.
		FVector Closest;
		double Distance = Actor->ActorGetDistanceToCollision(Point, ECC_Pawn, Closest);
		if (Distance < 0.0)
		{
			Distance = FVector::Dist(Point, Actor->GetActorLocation());
		}
		if (Distance > BestDistance)
		{
			continue;
		}
		if (Entry.Interactable->GetPrompt(Who).IsEmpty())
		{
			continue;
		}
		Best = Entry.Interactable;
		BestDistance = Distance;
		if (OutActor)
		{
			*OutActor = Actor;
		}
	}
	return Best;
}

void UDIAWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const double Seconds = FMath::Min(static_cast<double>(DeltaTime), MaxTickSeconds);
	Dispatch(Game.Tick(Seconds));

	// The game time of an autosave: a paused game (the menu) does not tick, so it never counts.
	AutosaveTimer += Seconds;
	if (AutosaveTimer >= LotBalance::AutosaveIntervalSeconds)
	{
		AutosaveTimer = 0.0;
		Autosave();
	}
}

TStatId UDIAWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDIAWorldSubsystem, STATGROUP_Tickables);
}

bool UDIAWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// The game runs in game and PIE worlds; the editor's own world must not simulate it.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
