// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "GameFramework/Actor.h"
#include "World/DIAInteractable.h"
#include "DIAPortableStation.generated.h"

class ADIACharacter;
class UBoxComponent;
class UDIAWorldSubsystem;
class UStaticMeshComponent;

/** Handling of the portable station (the refill radius is AsphaltLayout::RefillRadiusCm; positions are in the character's frame). */
namespace DIAStation
{
	/** While it is carried the station hangs this far behind the character's centre. */
	inline constexpr double CarryBehindCm = 100.0;
	/** Putting it down places its centre this far in front of the character's centre. */
	inline constexpr double PutDownAheadCm = 150.0;
	/** The free-space test lifts the station's box this high, so that the ground it stands on does not count as blocking. */
	inline constexpr double ClearanceLiftCm = 2.0;
}

/**
 * The portable refill station: a tank on wheels that the player picks up and puts down with E. A placed station refills
 * the backpack within AsphaltLayout::RefillRadiusCm (the character does that, from the plant's stock); a carried one does
 * not. The core owns the state (FAsphaltWorldState::StationPos / bStationCarried); this actor only mirrors it: Interact
 * writes the core and then the actor follows, and when the core changes in another way (a loaded save) the actor catches
 * up on the next state change. The game mode spawns it once the special is owned.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAPortableStation : public AActor, public IDIAInteractable
{
	GENERATED_BODY()

public:
	ADIAPortableStation();

	// IDIAInteractable
	virtual FText GetPrompt(const ADIACharacter* By) const override;
	/** Picks the station up, or puts it down in front of the player if there is room (otherwise a toast says so). */
	virtual void Interact(ADIACharacter* By) override;

	/** Hanging on a character (collision off), as opposed to standing on the ground. */
	bool IsCarried() const;

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UBoxComponent* GetCollision() const { return Collision; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** The character of the first local player; null while there is none. */
	ADIACharacter* GetLocalCharacter() const;
	/** Makes the actor match the core: carried or standing at StationPos. Runs on every state change, so it must return early when nothing differs. */
	void SyncFromCore();
	void CarryBy(ADIACharacter* Carrier);
	void PlaceOnGround(const FVector2D& Where, double YawDeg);
	/** True when nothing blocks the station's box standing at Where (the station and the character do not count). */
	bool HasRoomAt(const FVector2D& Where, double YawDeg, const ADIACharacter* By) const;
	void ShowNoRoomToast(const ADIACharacter* By) const;

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** The prompt text of the state it was built for; the prompt is queried every frame. */
	mutable bool bPromptCarried = false;
	mutable FText CachedPrompt;

	FDelegateHandle StateHandle;
};
