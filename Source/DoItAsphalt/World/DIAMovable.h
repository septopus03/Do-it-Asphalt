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
 * HandleClearanceCm in front of the player (the capsule is 34 cm wide); the suitcase is pulled behind the player, turned the other
 * way round and tilted about its wheel axis.
 *
 * Every pose keeps the footprint clear of the player's capsule (34 cm), so that letting go needs no move. The finisher's origin (the
 * middle of its bar) is 1.65 m ahead: its rear edge, 1.22 m behind the origin, is then 43 cm ahead of the player's centre. The station
 * is pulled behind the player with its handle (the model's -X end) towards the player.
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
	/** The suitcase's origin (below its wheel axis) is this far behind the player; its handle then lies right behind the player's back. */
	inline constexpr double SuitcaseBehindCm = 62.0;
	inline constexpr double SuitcaseTiltDeg = 25.0;
	/** The finisher's origin (the middle of its bar) ahead of the player: its rear edge (122 cm behind the origin) 43 cm in front of the player's centre. */
	inline constexpr double FinisherAheadCm = 165.0;
	/** The station's origin behind the player: its near end (60 cm from the origin) 45 cm behind the player's centre. */
	inline constexpr double StationBehindCm = 105.0;

	/** The pose of the kind while held. */
	constexpr FHold Of(ELotMovable Kind)
	{
		switch (Kind)
		{
		case ELotMovable::Suitcase: return {-SuitcaseBehindCm, 180.0, SuitcaseTiltDeg};
		case ELotMovable::Station: return {-StationBehindCm, 180.0, 0.0};
		case ELotMovable::Finisher: return {FinisherAheadCm, 0.0, 0.0};
		case ELotMovable::Cart:
		default: return {HandleClearanceCm + 65.0, 0.0, 0.0};
		}
	}
}

/**
 * The suitcase, the cart, the portable station and the finisher as objects of the world (one actor per owned kind; the game mode
 * keeps the set). The core owns the position and who holds it (FLotWorldState::Movables), this actor only mirrors it: Interact grabs
 * or lets go through FLotGame, and the actor follows the core on every state change and after a loaded or restarted game. While
 * held it follows the player every tick (presentation only, collision off): the suitcase behind the player tilted about its wheel
 * axis with the handle towards the player, the others in front with their handle towards the player. Letting go writes the pose the
 * actor has into the core, if there is room there.
 *
 * Behaviour that belongs to the object lives here, so that it runs in one place: a standing suitcase or cart in the refill zone fills
 * itself, a standing suitcase doses raw asphalt while the player holds Flap beside it, and the held cart lays a raw strip while Use is
 * held and it moves. The level shows as a disc in the cart's tub and as a column in the suitcase's sight strip.
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

	/** Where the flap or the outlet opens, on the ground plane, in the world. The origin for kinds without a mouth. */
	FVector2D GetMouthPoint() const;

	/** Distance in the ground plane from the point to the footprint (0 inside it); the footprint of a standing movable. */
	double GetDistanceToFootprint(FVector2D Point) const;
	bool ContainsPoint(FVector2D Point) const { return GetDistanceToFootprint(Point) <= 0.0; }

	/** The source the bucket can be filled from at the point: a standing, owned suitcase or cart whose footprint contains it. False elsewhere. */
	static bool FindTapAt(const UWorld* World, FVector2D Point, ELotSource& OutSource);

	UBoxComponent* GetCollision() const { return Collision; }
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	/** The disc in the cart's tub or the column in the suitcase's sight strip. */
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
	 * The path of the held cart's mouth or the finisher's bar in this tick while Use is down: from where it was to where it is now.
	 * False (and no path) on the first tick of a press, when it did not move, and when it moved further than a walk at SpeedCmS allows
	 * (a teleport, a hitch, a spin): the path then starts again where it is. The bound is the larger of this tick's and the previous
	 * tick's length, so that one long frame followed by a short one does not drop a segment of an honest walk.
	 */
	bool NextToolPath(const ADIACharacter* Player, float DeltaSeconds, double SpeedCmS, FVector2D& OutFrom, FVector2D& OutTo);
	/** The held cart lays a raw strip, the held finisher finishes the raw asphalt under its bar. */
	void WorkTheStrip(UDIAWorldSubsystem& Subsystem, const ADIACharacter* Player, float DeltaSeconds);
	/** The held actor ticks after this character and its movement (null: after nobody), so that its pose is this frame's. */
	void TickAfter(ADIACharacter* Character);
	/** Shows the content of the cart or the suitcase; the work is skipped while it did not change. */
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
	/** The path of the held cart's mouth or the finisher's bar while Use is down. */
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
