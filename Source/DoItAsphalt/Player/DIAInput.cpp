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
	Input.Pour = MakeAction(Outer, TEXT("IA_DIA_Pour"), EInputActionValueType::Boolean);
	Input.Sprint = MakeAction(Outer, TEXT("IA_DIA_Sprint"), EInputActionValueType::Boolean);
	Input.Interact = MakeAction(Outer, TEXT("IA_DIA_Interact"), EInputActionValueType::Boolean);
	Input.Shop = MakeAction(Outer, TEXT("IA_DIA_Shop"), EInputActionValueType::Boolean);
	Input.Pause = MakeAction(Outer, TEXT("IA_DIA_Pause"), EInputActionValueType::Boolean);

	UInputMappingContext* Context = Input.Context;

	// Move: X = right, Y = forward.
	MapAxisKey(Context, Input.Move, EKeys::W, /*Swizzle*/ true, false, false);
	MapAxisKey(Context, Input.Move, EKeys::S, /*Swizzle*/ true, false, /*NegateY*/ true);
	MapAxisKey(Context, Input.Move, EKeys::D, false, false, false);
	MapAxisKey(Context, Input.Move, EKeys::A, false, /*NegateX*/ true, false);

	// Look: the mouse reports up as positive Y; the controller's pitch input wants the opposite.
	MapAxisKey(Context, Input.Look, EKeys::Mouse2D, false, false, /*NegateY*/ true);

	Context->MapKey(Input.Pour, EKeys::LeftMouseButton);
	Context->MapKey(Input.Sprint, EKeys::LeftShift);
	Context->MapKey(Input.Interact, EKeys::E);
	Context->MapKey(Input.Shop, EKeys::Tab);
	Context->MapKey(Input.Pause, EKeys::Escape);
	return Input;
}
