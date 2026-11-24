// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LotBalance.h"
#include "World/DIAInteractable.h"
#include "DIAShopPoint.generated.h"

namespace DIAShopPoint
{
	/** The panel closes when the player is further than this (on the ground plane) from the point of the building it shows. */
	inline constexpr double CloseRangeCm = 300.0;
}

/**
 * The buy point of one shop building (LotLayout::ShopPoint): E here opens that building's panel. It has no collision (the world
 * subsystem then measures the range to its origin) and stands at the height of the player's capsule centre, so that the distance
 * to a player standing exactly on the point is zero and the point is then the nearest interactable (it wins over the car in the
 * garage's bay); a little way off, the nearest interactable wins, as everywhere.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAShopPoint : public AActor, public IDIAInteractable
{
	GENERATED_BODY()

public:
	ADIAShopPoint();

	/** Which building; set before BeginPlay (spawn deferred). */
	ELotShop Kind = ELotShop::Tools;

	/** Where the point of a building stands: LotLayout::ShopPoint at the height of the player's capsule centre. */
	static FTransform MakeTransform(ELotShop Shop);

	// IDIAInteractable
	virtual FText GetPrompt(const ADIACharacter* By) const override;
	virtual void Interact(ADIACharacter* By) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
