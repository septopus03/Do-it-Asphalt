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
	/** Left button, held: scoop, fill, pour, dump or trowel with the item in hand. */
	UPROPERTY()
	TObjectPtr<UInputAction> Use = nullptr;
	/** Right button, held: the work view (narrower field of view, tilted down). */
	UPROPERTY()
	TObjectPtr<UInputAction> WorkView = nullptr;
	/** Q: the next item in the hand. */
	UPROPERTY()
	TObjectPtr<UInputAction> NextHand = nullptr;
	/** 1 - 4: bowl, scoop, bucket, trowel (the order of ELotHand). */
	UPROPERTY()
	TObjectPtr<UInputAction> Hand1 = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Hand2 = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Hand3 = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Hand4 = nullptr;
	/** F, held: the suitcase flap (not bound to behaviour yet). */
	UPROPERTY()
	TObjectPtr<UInputAction> Flap = nullptr;
	/** G: sends the auto paver (not bound to behaviour yet). */
	UPROPERTY()
	TObjectPtr<UInputAction> SendPaver = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Sprint = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Interact = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Shop = nullptr;
	UPROPERTY()
	TObjectPtr<UInputAction> Pause = nullptr;

	/** WASD, mouse XY, left and right mouse button, Q, 1 - 4, F, G, left shift, E, Tab, Escape. All objects are owned by Outer. */
	static FDIAInput Create(UObject* Outer);
};
