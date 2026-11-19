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

	FAsphaltGame Game;
};

/**
 * The OnStateChanged handle of an actor that mirrors the core: Bind in BeginPlay, Unbind in EndPlay (both are idempotent,
 * and Bind drops an earlier binding first). It keeps the add/remove/reset sequence in one place.
 */
struct FDIAStateSubscription
{
	template <typename T>
	void Bind(UDIAWorldSubsystem* Subsystem, T* Object, void (T::*Method)())
	{
		Unbind(Subsystem);
		if (Subsystem)
		{
			Handle = Subsystem->OnStateChanged.AddUObject(Object, Method);
		}
	}

	void Unbind(UDIAWorldSubsystem* Subsystem)
	{
		if (Handle.IsValid())
		{
			if (Subsystem)
			{
				Subsystem->OnStateChanged.Remove(Handle);
			}
			Handle.Reset();
		}
	}

	FDelegateHandle Handle;
};
