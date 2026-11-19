// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "Subsystems/WorldSubsystem.h"
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

	/** Anything the HUD shows changed. */
	FSimpleMulticastDelegate OnStateChanged;
	FOnCells OnCellsCompleted;
	FOnRoad OnRoadCompleted;
	FOnSpecial OnSpecialBought;
	FSimpleMulticastDelegate OnExitOpened;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	FAsphaltGame Game;
};
