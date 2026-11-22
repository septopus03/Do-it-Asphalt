// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LotGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "World/DIAInteractable.h"
#include "DIAWorldSubsystem.generated.h"

/**
 * Owns the one FLotGame of the running world, ticks it and turns the events of every game operation into
 * delegates. Actors and widgets never talk to the core's events directly, they listen here.
 */
UCLASS()
class DOITASPHALT_API UDIAWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCells, const TArray<int32>&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntry, int32);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnlock, ELotUnlock);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnItem, ELotItem);

	/** The subsystem of the world the object lives in; null for an object without a world. Replaces the lookup every actor repeated. */
	static UDIAWorldSubsystem* Get(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	}

	FLotGame& GetGame() { return Game; }
	const FLotGame& GetGame() const { return Game; }

	/**
	 * Broadcasts what happened: OnCellsCompleted once if any cell completed, OnEntryCompleted per completed entry segment,
	 * OnUnlocked per unlock step, OnExitOpened if the exit opened, and always OnStateChanged last (the plant keeps producing,
	 * so even an event-less tick changes what the HUD shows). Every change of the game goes through an FLotGame method
	 * and its events come here.
	 */
	void Dispatch(const FLotEvents& Events);

	/** Buys an item; broadcasts OnItemBought and OnStateChanged on success (and autosaves). */
	bool BuyItem(ELotItem Item);
	/** Buys the next plant level; broadcasts OnStateChanged on success (and autosaves). */
	bool BuyPlant();

	/**
	 * Marks the game as won (the player reached the exit gate): the core ignores it while the exit is closed, and nothing
	 * happens (no OnWon) then or if the game was won already. Otherwise broadcasts OnWon once and OnStateChanged.
	 */
	void Win();

	// Save slot. The game is stored as one blob (FLotGame::SaveToBytes) in the slot SlotName().
	/** "DoItAsphalt_Slot0" unless overridden by SetSlotNameOverride or the command line switch -DIASlot=<name>. */
	static FString SlotName();
	/** Writes the blob into the named slot as a save file with the footer that ReadSlotBlob checks; false if it could not be written. */
	static bool WriteSlot(const FString& Slot, const TArray<uint8>& Blob);
	/** The slot's backup: SlotName() with "_bak" added. Every save writes the slot and then the backup; a damaged slot is read from it. */
	static FString BackupSlotName();
	/** Tests and screenshots use their own slot so that they never touch the real save; an empty name removes the override. */
	static void SetSlotNameOverride(const FString& Name);
	/** The slot (or, if that is damaged or missing, its backup) holds a blob that loads into a scratch game. */
	bool HasSave() const;
	/**
	 * Replaces the game with the saved one and broadcasts OnGameReset and OnStateChanged. Returns false and leaves the current game
	 * untouched if there is no valid save.
	 */
	bool LoadGame();
	/**
	 * Writes the game to the slot and its backup. The positions of the player (the car's while driving) and of the car are taken from the
	 * world first, and a held movable is saved as put down where it is: the running game does not change, the player keeps holding it.
	 */
	void SaveGame();
	/** Starts over: a fresh core, the slot and its backup are deleted, and OnGameReset and OnStateChanged are broadcast (the actors go to the layout's positions). */
	void NewGame();
	/**
	 * Dev Start: NewGame, then credits for everything in the shop (DevStartCredits) and every unlock step open up to the car (the exit
	 * stays closed): the cells and the entry segments the steps ask for are completed. For trying the whole game without playing it.
	 */
	void DevStart();
	static constexpr double DevStartCredits = 150000.0;

	/**
	 * The game saves itself only while this is on (every LotBalance::AutosaveIntervalSeconds of game time, after each purchase, when it is won and
	 * when the world ends). The HUD turns it on when the player leaves the main menu, so that closing the menu never overwrites the save
	 * and tests never touch a slot unless they ask for it.
	 */
	void SetAutosaveEnabled(bool bEnabled) { bAutosaveEnabled = bEnabled; }
	bool IsAutosaveEnabled() const { return bAutosaveEnabled; }
	/** SaveGame if the autosave is on. */
	void Autosave();

	/** Interactables (kiosks, the station, the car) register themselves while they are in the world. */
	void RegisterInteractable(AActor* Actor, IDIAInteractable* Interactable);
	void UnregisterInteractable(const AActor* Actor);
	/**
	 * The nearest registered interactable whose collision envelope is within LotBalance::InteractionRangeCm of the character and whose
	 * prompt is not empty; null if there is none. Runs every frame: no allocation, a handful of distance queries.
	 */
	IDIAInteractable* FindInteractable(const ADIACharacter* Who, AActor** OutActor = nullptr) const;

	/** Anything the HUD shows changed. */
	FSimpleMulticastDelegate OnStateChanged;
	FOnCells OnCellsCompleted;
	FOnEntry OnEntryCompleted;
	FOnUnlock OnUnlocked;
	FOnItem OnItemBought;
	FSimpleMulticastDelegate OnExitOpened;
	/** The game was won; fires once per game. */
	FSimpleMulticastDelegate OnWon;
	/** The whole game was replaced (LoadGame, NewGame): actors re-read their positions and equipment from the core. */
	FSimpleMulticastDelegate OnGameReset;

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FInteractableEntry
	{
		TWeakObjectPtr<AActor> Actor;
		IDIAInteractable* Interactable = nullptr;
	};
	TArray<FInteractableEntry> Interactables;

	/** Writes the player's position (the car's while driving) and the car's pose from the actors into the core, where a save finds them. */
	void WritePlayerPositions();
	/** The blob of the game as it is saved: a held movable put down where its actor is, on a copy. */
	TArray<uint8> MakeSaveBlob() const;

	FLotGame Game;
	bool bAutosaveEnabled = false;
	double AutosaveTimer = 0.0;
};

/**
 * The OnStateChanged handle of an actor that mirrors the core, with the OnGameReset handle for one that also has to take its
 * positions from a loaded or restarted game at once: Bind in BeginPlay, Unbind in EndPlay (both are idempotent, and Bind drops
 * an earlier binding first). It keeps the add/remove/reset sequence in one place.
 */
struct FDIAStateSubscription
{
	template <typename T>
	void Bind(UDIAWorldSubsystem* Subsystem, T* Object, void (T::*OnState)(), void (T::*OnReset)() = nullptr)
	{
		Unbind(Subsystem);
		if (Subsystem)
		{
			Handle = Subsystem->OnStateChanged.AddUObject(Object, OnState);
			if (OnReset)
			{
				ResetHandle = Subsystem->OnGameReset.AddUObject(Object, OnReset);
			}
		}
	}

	void Unbind(UDIAWorldSubsystem* Subsystem)
	{
		if (Subsystem)
		{
			if (Handle.IsValid())
			{
				Subsystem->OnStateChanged.Remove(Handle);
			}
			if (ResetHandle.IsValid())
			{
				Subsystem->OnGameReset.Remove(ResetHandle);
			}
		}
		Handle.Reset();
		ResetHandle.Reset();
	}

	FDelegateHandle Handle;
	FDelegateHandle ResetHandle;
};
