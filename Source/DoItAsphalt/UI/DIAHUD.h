// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/DIAFormat.h"
#include "DIAHUD.generated.h"

class SDIAHudOverlay;
class UDIAWorldSubsystem;

/**
 * The HUD actor: creates the Slate overlay in the game viewport (not when nothing can render, e.g. in tests), keeps the
 * contextual prompt and the toasts, and turns the game's events into toasts. The game state stays in the subsystem.
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADIAHUD();

	/** Contextual prompt above the backpack bar; empty text hides it. */
	void SetPrompt(const FText& Text);
	/** A short message at the top of the screen; it fades after AsphaltHud::ToastHoldSeconds. */
	void ShowToast(const FText& Text);

	const FText& GetPrompt() const { return Prompt; }
	const FDIAToasts& GetToasts() const { return Toasts; }
	bool HasOverlay() const { return Overlay.IsValid(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void CreateOverlay(UDIAWorldSubsystem* Subsystem);
	void DestroyOverlay();

	void HandleCellsCompleted(const TArray<int32>& Cells);
	void HandleRoadCompleted(int32 Road);
	void HandleSpecialBought(EAsphaltSpecial Special);
	void HandleExitOpened();

	FText Prompt;
	FDIAToasts Toasts;
	TSharedPtr<SDIAHudOverlay> Overlay;

	TWeakObjectPtr<UDIAWorldSubsystem> BoundSubsystem;
	FDelegateHandle CellsHandle;
	FDelegateHandle RoadHandle;
	FDelegateHandle SpecialHandle;
	FDelegateHandle ExitHandle;
};
