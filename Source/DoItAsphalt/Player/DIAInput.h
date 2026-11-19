// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DIAInput.generated.h"

class UInputAction;
class UInputMappingContext;
class UObject;

/**
 * Enhanced Input assets of the game. They are created in code (no assets on disk) with NewObject when the player
 * controller starts, and the owner keeps the struct as a UPROPERTY so that the garbage collector sees them.
 */
USTRUCT()
struct DOITASPHALT_API FDIAInput
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Context = nullptr;

	/** 2D axis: X = right, Y = forward. */
	UPROPERTY()
	TObjectPtr<UInputAction> Move = nullptr;
	/** 2D axis: X = yaw, Y = pitch (positive = look up). */
	UPROPERTY()
	TObjectPtr<UInputAction> Look = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Pour = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Sprint = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Interact = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Shop = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Pause = nullptr;

	/** WASD, mouse XY, left mouse button, left shift, E, Tab, Escape. All objects are owned by Outer. */
	static FDIAInput Create(UObject* Outer);
};
