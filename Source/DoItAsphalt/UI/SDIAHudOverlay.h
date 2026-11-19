// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/DIAFormat.h"
#include "Widgets/SCompoundWidget.h"

class ADIAHUD;
class UDIAWorldSubsystem;

/**
 * The HUD layout (GDD chapter 11): money and shards top left, progress top right, the backpack (or car tank) bottom
 * centre with the plant's stock and the contextual prompt above it, toasts top centre. It owns no game state: the
 * numbers come from the world subsystem through FDIAHudModel, which is refreshed on OnStateChanged but only changes
 * what the widgets show when a displayed value changed; the widgets read the cached view through attribute lambdas.
 * Prompt and toasts are read from the ADIAHUD that created it. The widget is never hit-testable.
 */
class SDIAHudOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDIAHudOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ADIAHUD>, Hud)
		SLATE_ARGUMENT(TWeakObjectPtr<UDIAWorldSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Stops listening to the game; called before the widget is removed. */
	void Unbind();

	const FDIAHudModel& GetModel() const { return Model; }

private:
	void HandleStateChanged();

	TSharedRef<SWidget> BuildTopLeft();
	TSharedRef<SWidget> BuildTopRight();
	TSharedRef<SWidget> BuildBottom();
	TSharedRef<SWidget> BuildToasts();

	TWeakObjectPtr<ADIAHUD> Hud;
	TWeakObjectPtr<UDIAWorldSubsystem> Subsystem;
	FDelegateHandle StateHandle;

	FDIAHudModel Model;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, 8.f};
};
