// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/SCompoundWidget.h"

class ADIAHUD;
class UDIAWorldSubsystem;

/**
 * The HUD layout: credits top left, progress and the next unlock step top right, the vessel in the hand bottom right, the
 * plant's stock (in the refill zone) and the contextual prompt bottom centre, toasts top centre, and the check view's
 * arrow and distance below them while V is held. It owns no game state: the
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
	TSharedRef<SWidget> BuildHand();
	TSharedRef<SWidget> BuildCheckView();
	TSharedRef<SWidget> BuildToasts();

	TWeakObjectPtr<ADIAHUD> Hud;
	TWeakObjectPtr<UDIAWorldSubsystem> Subsystem;
	FDelegateHandle StateHandle;

	FDIAHudModel Model;
	FSlateRoundedBoxBrush PanelBrush{FLinearColor::White, AsphaltHud::HudPanelCornerRadius};
};
