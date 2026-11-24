// Copyright Do it Asphalt. All Rights Reserved.

#include "Player/DIAInput.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	/** Adds a mapping of a key to an action; Swizzle turns a 1D key into the Y axis, Negate flips the listed axes. */
	void MapAxisKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegateX, bool bNegateY)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bSwizzle)
		{
			// The default order YXZ moves the key's value from X to Y.
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
		}
		if (bNegateX || bNegateY)
		{
			UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Context);
			Negate->bX = bNegateX;
			Negate->bY = bNegateY;
			Negate->bZ = false;
			Mapping.Modifiers.Add(Negate);
		}
	}
}

FDIAInput FDIAInput::Create(UObject* Outer)
{
	FDIAInput Input;
	Input.Context = NewObject<UInputMappingContext>(Outer, TEXT("IMC_DIA"));
	Input.Move = MakeAction(Outer, TEXT("IA_DIA_Move"), EInputActionValueType::Axis2D);
	Input.Look = MakeAction(Outer, TEXT("IA_DIA_Look"), EInputActionValueType::Axis2D);
	Input.Use = MakeAction(Outer, TEXT("IA_DIA_Use"), EInputActionValueType::Boolean);
	Input.WorkView = MakeAction(Outer, TEXT("IA_DIA_WorkView"), EInputActionValueType::Boolean);
	Input.NextHand = MakeAction(Outer, TEXT("IA_DIA_NextHand"), EInputActionValueType::Boolean);
	Input.Hand1 = MakeAction(Outer, TEXT("IA_DIA_Hand1"), EInputActionValueType::Boolean);
	Input.Hand2 = MakeAction(Outer, TEXT("IA_DIA_Hand2"), EInputActionValueType::Boolean);
	Input.Hand3 = MakeAction(Outer, TEXT("IA_DIA_Hand3"), EInputActionValueType::Boolean);
	Input.Hand4 = MakeAction(Outer, TEXT("IA_DIA_Hand4"), EInputActionValueType::Boolean);
	Input.Fill = MakeAction(Outer, TEXT("IA_DIA_Fill"), EInputActionValueType::Boolean);
	Input.SendPaver = MakeAction(Outer, TEXT("IA_DIA_SendPaver"), EInputActionValueType::Boolean);
	Input.PaverMap = MakeAction(Outer, TEXT("IA_DIA_PaverMap"), EInputActionValueType::Boolean);
	Input.Sprint = MakeAction(Outer, TEXT("IA_DIA_Sprint"), EInputActionValueType::Boolean);
	Input.Interact = MakeAction(Outer, TEXT("IA_DIA_Interact"), EInputActionValueType::Boolean);
	Input.CheckView = MakeAction(Outer, TEXT("IA_DIA_CheckView"), EInputActionValueType::Boolean);
	Input.Pause = MakeAction(Outer, TEXT("IA_DIA_Pause"), EInputActionValueType::Boolean);

	// Esc has to close the pause menu while the game is paused.
	Input.Pause->bTriggerWhenPaused = true;

	UInputMappingContext* Context = Input.Context;

	// Move: X = right, Y = forward.
	MapAxisKey(Context, Input.Move, EKeys::W, /*Swizzle*/ true, false, false);
	MapAxisKey(Context, Input.Move, EKeys::S, /*Swizzle*/ true, false, /*NegateY*/ true);
	MapAxisKey(Context, Input.Move, EKeys::D, false, false, false);
	MapAxisKey(Context, Input.Move, EKeys::A, false, /*NegateX*/ true, false);

	// Look: the mouse reports up as positive Y; the controller's pitch input wants the opposite.
	MapAxisKey(Context, Input.Look, EKeys::Mouse2D, false, false, /*NegateY*/ true);

	Context->MapKey(Input.Use, EKeys::LeftMouseButton);
	Context->MapKey(Input.WorkView, EKeys::RightMouseButton);
	Context->MapKey(Input.NextHand, EKeys::Q);
	Context->MapKey(Input.Hand1, EKeys::One);
	Context->MapKey(Input.Hand2, EKeys::Two);
	Context->MapKey(Input.Hand3, EKeys::Three);
	Context->MapKey(Input.Hand4, EKeys::Four);
	Context->MapKey(Input.Fill, EKeys::R);
	Context->MapKey(Input.SendPaver, EKeys::H);
	Context->MapKey(Input.PaverMap, EKeys::M);
	Context->MapKey(Input.Sprint, EKeys::LeftShift);
	Context->MapKey(Input.Interact, EKeys::E);
	Context->MapKey(Input.CheckView, EKeys::V);
	Context->MapKey(Input.Pause, EKeys::Escape);
	return Input;
}
