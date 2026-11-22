// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAHudText.h"

TArray<FDIAPromptLine> DIAHudText::SplitPrompt(const FText& Prompt)
{
	static const FString Separator = TEXT(" – ");

	TArray<FString> Raw;
	Prompt.ToString().ParseIntoArray(Raw, TEXT("\n"), true);

	TArray<FDIAPromptLine> Lines;
	TArray<FString> Keys;
	TArray<FString> Actions;
	for (const FString& Line : Raw)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}
		FString Key;
		FString Action = Trimmed;
		const int32 At = Trimmed.Find(Separator);
		if (At != INDEX_NONE)
		{
			Key = Trimmed.Left(At);
			Action = Trimmed.Mid(At + Separator.Len());
		}
		if (Keys.Num() < MaxPromptLines)
		{
			Keys.Add(Key);
			Actions.Add(Action);
		}
		else
		{
			// Past the room: the whole line (key included) goes behind the last action.
			Actions.Last() += TEXT(" · ") + Trimmed;
		}
	}
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		Lines.Add({FText::FromString(Keys[Index]), FText::FromString(Actions[Index])});
	}
	return Lines;
}

FText DIAHudText::StripLabel(const FText& Line)
{
	const FString Text = Line.ToString();
	const int32 At = Text.Find(TEXT(": "));
	if (At == INDEX_NONE || At + 2 >= Text.Len())
	{
		return Line;
	}
	return FText::FromString(Text.Mid(At + 2));
}
