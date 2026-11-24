// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "LotBalance.h"
#include "World/DIAInteractable.h"
#include "World/DIAToolPath.h"
#include "DIAMovable.generated.h"

class ADIACharacter;
class UBoxComponent;
class UStaticMeshComponent;

/** Handling of the movables in the world: model-sized and presentation constants only (speeds and ranges are LotBalance's). */
namespace DIAMovable
{
	/** The free-space test lifts the footprint this high, so that the ground it stands on does not count as blocking. */
	inline constexpr double ClearanceLiftCm = 2.0;
	/** How far the actor may be from the core's position before it is moved there; the core's number is the truth. */
	inline constexpr double SyncToleranceCm = 0.5;
	inline constexpr double SyncToleranceDeg = 0.5;
	/** Content below this shows nothing. */
	inline constexpr double LevelMinLitres = 1e-4;
}

/**
 * How each kind is held: presentation, not game numbers. Ahead is the origin's distance in front of the player along the player's
 * facing (negative = behind), Yaw the turn against the player's yaw, Tilt the pitch about Y. A forward-held kind has its back edge
 * HandleClearanceCm in front of the player (the capsule is 34 cm wide); the trolley and the cart are pushed in front of the player, the
 * station is pulled behind.
 *
 * Every pose keeps the footprint clear of the player's capsule (34 cm), so that letting go needs no move. The station is pulled behind
 * the player with its handle (the model's -X end) towards the player.
 */
namespace DIAMovableHold
{
	struct FHold
	{
		double AheadCm;
		double YawDeg;
		double TiltDeg;
	};

	inline constexpr double HandleClearanceCm = 45.0;
	/** The trolley's handle (50 cm behind its origin) is HandleClearanceCm in front of the player. */
	inline constexpr double TrolleyAheadCm = HandleClearanceCm + 50.0;
	/** The station's origin behind the player: its near end (60 cm from the origin) 45 cm behind the player's centre. */
	inline constexpr double StationBehindCm = 105.0;

	/** The pose of the kind while held. */
	constexpr FHold Of(ELotMovable Kind)
	{
		switch (Kind)
		{
		case ELotMovable::Trolley: return {TrolleyAheadCm, 0.0, 0.0};
		case ELotMovable::Station: return {-StationBehindCm, 180.0, 0.0};
		case ELotMovable::Cart:
		default: return {HandleClearanceCm + 65.0, 0.0, 0.0};
		}
	}
}

/**
 * The trolley, the cart and the portable station as objects of the world (one actor per owned kind; the game mode
 * keeps the set). The core owns the position and who holds it (FLotWorldState::Movables), this actor only mirrors it: Interact grabs
 * or lets go through FLotGame, and the actor follows the core on every state change and after a loaded or restarted game. While
 * held it follows the player every tick (presentation only, collision off): the trolley and the cart in front of the player with their
 * handle towards the player, the station behind. Letting go writes the pose the actor has into the core, if there is room there.
 *
 * Behaviour that belongs to the object lives here, so that it runs in one place: a standing trolley or cart in the refill zone fills
 * itself, and the held trolley or cart lays a strip while Use is held and it moves. The level shows as a disc in the tub.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAMovable : public AActor, public IDIAInteractable
{
	GENERATED_BODY()

public:
	ADIAMovable();

	/** Chooses the kind: collision, model and level display follow it. Called right after the spawn (before BeginPlay). */
	void Init(ELotMovable InKind);
	ELotMovable GetKind() const { return Kind; }

	// IDIAInteractable
	virtual FText GetPrompt(const ADIACharacter* By) const override;
	/** Grabs the movable, or lets it go where it is if there is room (otherwise a toast says so). */
	virtual void Interact(ADIACharacter* By) override;

	/** Lets go of the held movable at the pose it has; false (nothing written, a toast shown) if the footprint is not free there. */
	bool TryRelease(ADIACharacter* By);
	/**
	 * True if the movable could be set down at the pose it has now: the very test a release makes (walls, mall, plant, the car and other
	 * movables block; the movable itself and the character, By or the player's, do not).
	 */
	bool CanBePutDownHere(const ADIACharacter* By = nullptr) const;
	/** The core says the player holds this one. */
	bool IsHeld() const;
	virtual bool IsHeldByPlayer() const override { return IsHeld(); }

	/** Where the outlet opens, on the ground plane, in the world. The origin for kinds without a mouth. */
	FVector2D GetMouthPoint() const;

	/** Distance in the ground plane from the point to the footprint (0 inside it); the footprint of a standing movable. */
	double GetDistanceToFootprint(FVector2D Point) const;
	bool ContainsPoint(FVector2D Point) const { return GetDistanceToFootprint(Point) <= 0.0; }

	/** The source the bucket can be filled from at the point: a standing, owned trolley or cart whose footprint contains it. False elsewhere. */
	static bool FindTapAt(const UWorld* World, FVector2D Point, ELotSource& OutSource);

	UBoxComponent* GetCollision() const { return Collision; }
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	/** The disc in the tub of the cart or the trolley. */
	UStaticMeshComponent* GetLevelMesh() const { return LevelMesh; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** The character that grabbed it, else the player's, else any character in the world; null if there is none. */
	ADIACharacter* GetPlayer() const;
	/** Makes the actor match the core: held (collision off) or standing at the core's pose. Runs on every state change, so it returns early when nothing differs. */
	void SyncFromCore();
	/** A game was loaded or started: the strokes start afresh (nothing is laid between the old and the new place), then the actor follows the core. */
	void HandleGameReset();
	void PlaceStanding(FVector2D Where, double YawDeg);
	void FollowPlayer(const ADIACharacter* Player);
	/** True when nothing blocks the footprint standing at Where (the movable itself and the character do not count). */
	bool HasRoomAt(FVector2D Where, double YawDeg, const ADIACharacter* By) const;
	void ShowNoRoomToast(const ADIACharacter* By) const;
	/**
	 * The path of the held cart's mouth in this tick while Use is down: from where it was to where it is now.
	 * False (and no path) on the first tick of a press, when it did not move, and when it moved further than a walk at SpeedCmS allows
	 * (a teleport, a hitch, a spin): the path then starts again where it is. The bound is the larger of this tick's and the previous
	 * tick's length, so that one long frame followed by a short one does not drop a segment of an honest walk.
	 */
	bool NextToolPath(const ADIACharacter* Player, float DeltaSeconds, double SpeedCmS, FVector2D& OutFrom, FVector2D& OutTo);
	/** The held cart lays a strip of asphalt along the path of its mouth. */
	void WorkTheStrip(UDIAWorldSubsystem& Subsystem, const ADIACharacter* Player, float DeltaSeconds);
	/** The held actor ticks after this character and its movement (null: after nobody), so that its pose is this frame's. */
	void TickAfter(ADIACharacter* Character);
	/** Shows the content of the cart or the trolley; the work is skipped while it did not change. */
	void UpdateLevel();

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LevelMesh;

	ELotMovable Kind = ELotMovable::Cart;
	/** The character that grabbed it, or the one found for it; looked up again once it is gone. */
	mutable TWeakObjectPtr<ADIACharacter> Holder;

	/** The actor shows the held pose (collision off, tilted), as opposed to standing. */
	bool bShownHeld = false;
	/** The path of the held cart's mouth while Use is down. */
	FDIAToolPath ToolPath;
	/** The character this actor was ordered to tick after. */
	TWeakObjectPtr<ADIACharacter> TickedAfter;
	/** Content the level shows now, to skip the work when nothing changed. */
	double ShownLitres = -1.0;

	/** The prompt for the state it was built for; the prompt is queried every frame. */
	mutable int32 PromptKey = -1;
	mutable FText CachedPrompt;

	FDIAStateSubscription StateSub;
};
