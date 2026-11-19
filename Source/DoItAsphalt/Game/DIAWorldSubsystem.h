// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "World/DIAInteractable.h"
#include "DIAWorldSubsystem.generated.h"

/**
 * Owns the one FAsphaltGame of the running world, ticks it and turns the events of every game operation into
 * delegates. Actors and widgets never talk to the core's events directly, they listen here.
 */
UCLASS()
class DOITASPHALT_API UDIAWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCells, const TArray<int32>&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoad, int32);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpecial, EAsphaltSpecial);

	/** The subsystem of the world the object lives in; null for an object without a world. Replaces the lookup every actor repeated. */
	static UDIAWorldSubsystem* Get(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	}

	FAsphaltGame& GetGame() { return Game; }
	const FAsphaltGame& GetGame() const { return Game; }

	/** Pours along A->B from the backpack (or the paver car's tank) and dispatches the outcome. */
	void Pour(FVector2D A, FVector2D B, bool bFromCar);

	/**
	 * Broadcasts what happened: OnCellsCompleted once if any cell completed, OnRoadCompleted per completed road,
	 * OnExitOpened if the exit opened, and always OnStateChanged last (the plant keeps producing, so even an event-less
	 * tick changes what the HUD shows).
	 */
	void Dispatch(const FAsphaltEvents& Events);

	/** Buys the next level of an upgrade; broadcasts OnStateChanged on success. */
	bool Buy(EAsphaltUpgrade U);
	/** Buys a special; broadcasts OnSpecialBought and OnStateChanged on success. */
	bool BuySpecial(EAsphaltSpecial S);

	/**
	 * Marks the game as won (the player reached the exit gate): the core ignores it while the exit is closed, and nothing
	 * happens (no OnWon) then or if the game was won already. Otherwise broadcasts OnWon once and OnStateChanged.
	 */
	void Win();

	// Save slot. The game is stored as one blob (FAsphaltGame::SaveToBytes) in the slot SlotName().
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
	/** Writes the game to the slot and its backup; the position of the player (or of the truck, if the player drives it) is taken from the world first. */
	void SaveGame();
	/** Starts over: a fresh core, the slot and its backup are deleted, and OnGameReset and OnStateChanged are broadcast (the actors go to the layout's positions). */
	void NewGame();

	/**
	 * The game saves itself only while this is on (every AsphaltSave interval of game time, after each purchase, when it is won and
	 * when the world ends). The HUD turns it on when the player leaves the main menu, so that closing the menu never overwrites the save
	 * and tests never touch a slot unless they ask for it.
	 */
	void SetAutosaveEnabled(bool bEnabled) { bAutosaveEnabled = bEnabled; }
	bool IsAutosaveEnabled() const { return bAutosaveEnabled; }
	/** SaveGame if the autosave is on. */
	void Autosave();

	/** Interactables (kiosks, later the station and the car) register themselves while they are in the world. */
	void RegisterInteractable(AActor* Actor, IDIAInteractable* Interactable);
	void UnregisterInteractable(const AActor* Actor);
	/**
	 * The nearest registered interactable whose collision envelope is within DIAInteraction::RangeCm of the character and whose
	 * prompt is not empty; null if there is none. Runs every frame: no allocation, a handful of distance queries.
	 */
	IDIAInteractable* FindInteractable(const ADIACharacter* Who, AActor** OutActor = nullptr) const;

	/** Anything the HUD shows changed. */
	FSimpleMulticastDelegate OnStateChanged;
	FOnCells OnCellsCompleted;
	FOnRoad OnRoadCompleted;
	FOnSpecial OnSpecialBought;
	FSimpleMulticastDelegate OnExitOpened;
	/** The game was won; fires once per game. */
	FSimpleMulticastDelegate OnWon;
	/** The whole game was replaced (LoadGame, NewGame): actors re-read their positions and equipment from the core. */
	FSimpleMulticastDelegate OnGameReset;

	// UTickableWorldSubsystem
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

	/** Writes the player's position (the truck's, while driving) from the actors into the core, where a save finds it. */
	void WritePlayerPositions();

	FAsphaltGame Game;
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
