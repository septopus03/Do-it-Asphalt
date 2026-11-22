// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One line of the contextual prompt: the key on the square plate and what it does on the black plate. */
struct FDIAPromptLine
{
	FText Key;
	FText Action;
};

/** Pure text helpers of the HUD: how the model's texts are cut for the sign layout. */
namespace DIAHudText
{
	/** The prompt has room for this many lines; more are joined to the last one. */
	inline constexpr int32 MaxPromptLines = 2;

	/**
	 * Cuts the prompt at every newline into lines and each line at its first " – " (en dash with spaces) into key and action.
	 * A line without the separator has an empty Key; empty lines are dropped; lines past MaxPromptLines are appended to the
	 * Action of the last one with " · ".
	 */
	DOITASPHALT_API TArray<FDIAPromptLine> SplitPrompt(const FText& Prompt);

	/** The text after the first ": " of Line; the whole Line when there is no ": " or nothing would be left. */
	DOITASPHALT_API FText StripLabel(const FText& Line);
}
