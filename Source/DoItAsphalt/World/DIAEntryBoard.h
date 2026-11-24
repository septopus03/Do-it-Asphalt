// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LotBalance.h"
#include "DIAEntryBoard.generated.h"

class UBoxComponent;
class UDIAWorldSubsystem;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Where the boards stand and how their text is laid out (nothing here is a game rule). */
namespace DIABoard
{
	/** The entries 1 and 2 face the shopping centre: their board is the text on the centre's sign, not a kiosk. */
	inline bool HasKiosk(int32 Entry) { return Entry != 1 && Entry != 2; }

	/** Where the board of an entry stands: the kiosk at LotLayout::BoardPos turned to the car park, or the text just in front of the mall's sign. */
	DOITASPHALT_API FTransform BoardTransform(int32 Entry);

	/** The text of a mall sign stands this far in front of the sign. */
	inline constexpr double SignTextStandOffCm = 3.0;
	/** The text of a kiosk: centre of the white board on its screen side (+X of the model, the text just in front of it), and the area it may fill. */
	inline const FVector KioskTextCm(0.6, 0.0, 190.0);
	inline const FVector2D KioskTextAreaCm(54.0, 28.0);
	/** Share of a line's height that the glyph cell takes, and the width of a character relative to its height. */
	inline constexpr double LineSpacing = 1.15;
	inline constexpr double CharAspect = 0.6;
	/** The size of the text component at which the text (lines split at newlines) fills the area (width x height, cm) as far as it can; the shop signs use it too. */
	DOITASPHALT_API double FitTextSize(const FString& Text, FVector2D AreaCm);
	/** Colours: amber on the screen of a kiosk, dark on the white sign of the mall, green once the step is unlocked. */
	inline const FColor KioskTextColor(210, 80, 0);
	inline const FColor SignTextColor(58, 63, 71);
	inline const FColor UnlockedTextColor(40, 140, 50);
}

/**
 * The board of one entry segment (Entry 0..5): what the segment's unlock step opens, how many of its 12 cells are done, how many
 * cells of the whole car park the step needs, and UNLOCKED once the step is open. A kiosk (SM_Kiosk) stands beside the entries 0,
 * 3, 4 and 5; the entries 1 and 2, which lie before the shopping centre, get their text on the centre's sign board. The text is
 * rewritten when the game reports a completed cell, entry or unlock step, or a new or loaded game - not every frame.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAEntryBoard : public AActor
{
	GENERATED_BODY()

public:
	ADIAEntryBoard();

	/** The entry segment (0..5); set before BeginPlay (spawn deferred). */
	int32 Entry = 0;

	/** The text as the game state now reads: name of the step, "Entry n / 12", "Cells n / threshold", or UNLOCKED instead of the last two. */
	FText GetBoardText() const;

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UBoxComponent* GetCollision() const { return Collision; }
	UTextRenderComponent* GetTextComponent() const { return Text; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Writes the text into the component, but only if it differs from what is shown. */
	void Refresh();
	void OnCells(const TArray<int32>& Cells) { Refresh(); }
	void OnEntry(int32 CompletedEntry) { Refresh(); }
	void OnUnlock(ELotUnlock Unlock) { Refresh(); }

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Text;

	FDelegateHandle CellsHandle;
	FDelegateHandle EntryHandle;
	FDelegateHandle UnlockHandle;
	FDelegateHandle ResetHandle;
};
