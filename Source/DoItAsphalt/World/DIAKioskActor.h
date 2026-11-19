// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsphaltGame.h"
#include "GameFramework/Actor.h"
#include "World/DIAInteractable.h"
#include "DIAKioskActor.generated.h"

class UBoxComponent;
class UDIAWorldSubsystem;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Placement of the pieces of SM_Kiosk and SM_Lock, relative to the kiosk's origin (the centre of its footprint, on the ground). */
namespace DIAKiosk
{
	/** The model's white board: its face is at X = 0 and faces +X; the text sits just in front of it, centred at Z = 190 cm. */
	inline const FVector BoardTextCm(0.6, 0.0, 190.0);
	/** Free area of the board (60 x 30 cm) that the name may fill. */
	inline constexpr double BoardTextWidthCm = 54.0;
	inline constexpr double BoardTextMaxHeightCm = 14.0;
	/** The lock hangs in front of the screen (the screen's centre is about X = 9, Z = 138 on the sloping panel). */
	inline const FVector LockCm(16.0, 0.0, 140.0);
}

/**
 * A kiosk at the end of a road (AsphaltLayout::KioskPos), turned towards the road, which sells one special. It is locked
 * (shows a padlock in front of the screen) until its road is complete. The name of the special is written on the board.
 * The kiosk is solid for the player; the interaction range is measured to its collision box.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAKioskActor : public AActor, public IDIAInteractable
{
	GENERATED_BODY()

public:
	ADIAKioskActor();

	/** Which special this kiosk sells; use SetSpecial to change it after BeginPlay. */
	EAsphaltSpecial Special = EAsphaltSpecial::Sprint;
	void SetSpecial(EAsphaltSpecial NewSpecial);

	/** Road whose completion unlocks the kiosk. */
	int32 GetRoad() const;
	/** The road is not complete yet, so the padlock shows. */
	bool IsLocked() const;

	// IDIAInteractable
	virtual FText GetPrompt(const ADIACharacter* By) const override;
	virtual void Interact(ADIACharacter* By) override;

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UStaticMeshComponent* GetLockMesh() const { return LockMesh; }
	UTextRenderComponent* GetBoardText() const { return BoardText; }
	UBoxComponent* GetCollision() const { return Collision; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UDIAWorldSubsystem* GetSubsystem() const;
	/** Re-reads the game; returns at once unless the progress, ownership or affordability changed (it runs every tick and on every prompt query). */
	void Refresh() const;
	void ApplySpecialName();

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LockMesh;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> BoardText;

	/** What the prompt and the padlock currently show. */
	mutable int32 ShownProgress = INDEX_NONE;
	mutable bool bShownOwned = false;
	mutable bool bShownAffordable = false;
	mutable FText CachedPrompt;

	FDelegateHandle StateHandle;
	bool bBegun = false;
};
