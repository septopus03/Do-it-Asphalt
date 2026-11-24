// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAControlsLegend.h"

#define LOCTEXT_NAMESPACE "DIAControlsLegend"

namespace
{
	using DIAControlsLegend::FBinding;
	using DIAControlsLegend::FCap;
	using DIAControlsLegend::FLine;

	FBinding Bind(const FKey& Key, TObjectPtr<UInputAction> FDIAInput::* Action)
	{
		FBinding Binding;
		Binding.Key = Key;
		Binding.Action = Action;
		return Binding;
	}

	FCap Cap(const FText& Label, std::initializer_list<FBinding> Bindings)
	{
		FCap Result;
		Result.Label = Label;
		Result.Bindings = TArray<FBinding>(Bindings);
		return Result;
	}

	FCap Cap(const TCHAR* Label, const FKey& Key, TObjectPtr<UInputAction> FDIAInput::* Action)
	{
		return Cap(FText::FromString(Label), {Bind(Key, Action)});
	}

	FLine Line(std::initializer_list<FCap> Caps, const FText& Action)
	{
		FLine Result;
		Result.Caps = TArray<FCap>(Caps);
		Result.Action = Action;
		return Result;
	}

	/** The mouse is one cap: its movement is the look action. */
	FCap MouseCap()
	{
		return Cap(LOCTEXT("KeyMouse", "Mouse"), {Bind(EKeys::Mouse2D, &FDIAInput::Look)});
	}
}

FText DIAControlsLegend::Title()
{
	return LOCTEXT("Title", "Controls");
}

const TArray<DIAControlsLegend::FLine>& DIAControlsLegend::OnFoot()
{
	static const TArray<FLine> Lines = []()
	{
		TArray<FLine> Result;
		Result.Add(Line({Cap(TEXT("W"), EKeys::W, &FDIAInput::Move), Cap(TEXT("A"), EKeys::A, &FDIAInput::Move), Cap(TEXT("S"), EKeys::S, &FDIAInput::Move), Cap(TEXT("D"), EKeys::D, &FDIAInput::Move)},
			LOCTEXT("ActionMove", "move")));
		Result.Add(Line({Cap(LOCTEXT("KeyShift", "Shift"), {Bind(EKeys::LeftShift, &FDIAInput::Sprint)})}, LOCTEXT("ActionSprint", "sprint")));
		Result.Add(Line({MouseCap()}, LOCTEXT("ActionLook", "look")));
		Result.Add(Line({Cap(LOCTEXT("KeyLmb", "LMB"), {Bind(EKeys::LeftMouseButton, &FDIAInput::Use)})}, LOCTEXT("ActionScoop", "scoop / pour")));
		Result.Add(Line({Cap(LOCTEXT("KeyRmb", "RMB"), {Bind(EKeys::RightMouseButton, &FDIAInput::WorkView)})}, LOCTEXT("ActionWorkView", "work view")));
		Result.Add(Line({Cap(TEXT("Q"), EKeys::Q, &FDIAInput::NextHand),
				Cap(LOCTEXT("KeyHands", "1-4"), {Bind(EKeys::One, &FDIAInput::Hand1), Bind(EKeys::Two, &FDIAInput::Hand2), Bind(EKeys::Three, &FDIAInput::Hand3), Bind(EKeys::Four, &FDIAInput::Hand4)})},
			LOCTEXT("ActionTools", "next / pick tool")));
		Result.Add(Line({Cap(TEXT("R"), EKeys::R, &FDIAInput::Fill)}, LOCTEXT("ActionFill", "fill")));
		Result.Add(Line({Cap(TEXT("E"), EKeys::E, &FDIAInput::Interact)}, LOCTEXT("ActionUse", "use / shop")));
		Result.Add(Line({Cap(TEXT("H"), EKeys::H, &FDIAInput::SendPaver)}, LOCTEXT("ActionSendRobot", "send robot")));
		Result.Add(Line({Cap(TEXT("M"), EKeys::M, &FDIAInput::PaverMap)}, LOCTEXT("ActionMap", "paver map")));
		Result.Add(Line({Cap(TEXT("V"), EKeys::V, &FDIAInput::CheckView)}, LOCTEXT("ActionCheck", "check view")));
		Result.Add(Line({Cap(LOCTEXT("KeyEsc", "Esc"), {Bind(EKeys::Escape, &FDIAInput::Pause)})}, LOCTEXT("ActionPause", "pause")));
		return Result;
	}();
	return Lines;
}

const TArray<DIAControlsLegend::FLine>& DIAControlsLegend::InCar()
{
	// At the wheel W and S are the throttle, A and D the steering (ADIAPlayerController::OnMove), the button pours the bar, E gets out and
	// the paver map still opens; the hand keys, sprint, fill, the work view, the check view and H do nothing there.
	static const TArray<FLine> Lines = []()
	{
		TArray<FLine> Result;
		Result.Add(Line({Cap(TEXT("W"), EKeys::W, &FDIAInput::Move), Cap(TEXT("S"), EKeys::S, &FDIAInput::Move)}, LOCTEXT("ActionThrottle", "throttle")));
		Result.Add(Line({Cap(TEXT("A"), EKeys::A, &FDIAInput::Move), Cap(TEXT("D"), EKeys::D, &FDIAInput::Move)}, LOCTEXT("ActionSteer", "steer")));
		Result.Add(Line({MouseCap()}, LOCTEXT("ActionLookCar", "look")));
		Result.Add(Line({Cap(LOCTEXT("KeyLmbCar", "LMB"), {Bind(EKeys::LeftMouseButton, &FDIAInput::Use)})}, LOCTEXT("ActionPourCar", "pour")));
		Result.Add(Line({Cap(TEXT("E"), EKeys::E, &FDIAInput::Interact)}, LOCTEXT("ActionGetOut", "get out")));
		Result.Add(Line({Cap(TEXT("M"), EKeys::M, &FDIAInput::PaverMap)}, LOCTEXT("ActionMapCar", "paver map")));
		Result.Add(Line({Cap(LOCTEXT("KeyEscCar", "Esc"), {Bind(EKeys::Escape, &FDIAInput::Pause)})}, LOCTEXT("ActionPauseCar", "pause")));
		return Result;
	}();
	return Lines;
}

#undef LOCTEXT_NAMESPACE
