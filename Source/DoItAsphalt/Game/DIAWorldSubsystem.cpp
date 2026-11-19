// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAWorldSubsystem.h"

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
