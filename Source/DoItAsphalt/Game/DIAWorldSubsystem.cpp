// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "Player/DIACharacter.h"

namespace
{
	/** Longest slice of real time given to the core at once, so that a hitch or a pause cannot cause a jump. */
	constexpr double MaxTickSeconds = 1.0;
}

void UDIAWorldSubsystem::Pour(FVector2D A, FVector2D B, bool bFromCar)
{
	Dispatch(bFromCar ? Game.PourFromCar(A, B) : Game.PourFromBackpack(A, B));
}

void UDIAWorldSubsystem::Dispatch(const FAsphaltEvents& Events)
{
	if (Events.CompletedCells.Num() > 0)
	{
		OnCellsCompleted.Broadcast(Events.CompletedCells);
	}
	for (const int32 Road : Events.CompletedRoads)
	{
		OnRoadCompleted.Broadcast(Road);
	}
	if (Events.bExitOpened)
	{
		OnExitOpened.Broadcast();
	}
	OnStateChanged.Broadcast();
}

bool UDIAWorldSubsystem::Buy(EAsphaltUpgrade U)
{
	if (!Game.Buy(U))
	{
		return false;
	}
	OnStateChanged.Broadcast();
	return true;
}

bool UDIAWorldSubsystem::BuySpecial(EAsphaltSpecial S)
{
	if (!Game.BuySpecial(S))
	{
		return false;
	}
	OnSpecialBought.Broadcast(S);
	OnStateChanged.Broadcast();
	return true;
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
	const FVector Point = Who->GetActorLocation();
	IDIAInteractable* Best = nullptr;
	double BestDistance = DIAInteraction::RangeCm;
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
	Dispatch(Game.Tick(FMath::Min(static_cast<double>(DeltaTime), MaxTickSeconds)));
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
