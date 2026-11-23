// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotGame.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHudText.h"
#include "UI/DIAMenuModel.h"

DIA_TEST(FUiPromptSplitTest, "DoItAsphalt.Game.Ui.Prompt.Split")

bool FUiPromptSplitTest::RunTest(const FString& Parameters)
{
	auto Split = [](const TCHAR* Prompt) { return DIAHudText::SplitPrompt(FText::FromString(Prompt)); };
	auto Check = [this](const TCHAR* What, const FDIAPromptLine& Line, const TCHAR* Key, const TCHAR* Action)
	{
		TestEqual(*FString::Printf(TEXT("%s: key"), What), Line.Key.ToString(), FString(Key));
		TestEqual(*FString::Printf(TEXT("%s: action"), What), Line.Action.ToString(), FString(Action));
	};

	TArray<FDIAPromptLine> Lines = Split(TEXT("E – Push"));
	if (TestEqual(TEXT("one line"), Lines.Num(), 1))
	{
		Check(TEXT("one line"), Lines[0], TEXT("E"), TEXT("Push"));
	}

	Lines = Split(TEXT("E – Pull\nF – Open flap"));
	if (TestEqual(TEXT("two lines"), Lines.Num(), 2))
	{
		Check(TEXT("two lines [0]"), Lines[0], TEXT("E"), TEXT("Pull"));
		Check(TEXT("two lines [1]"), Lines[1], TEXT("F"), TEXT("Open flap"));
	}

	Lines = Split(TEXT("No room here"));
	if (TestEqual(TEXT("no separator"), Lines.Num(), 1))
	{
		Check(TEXT("no separator"), Lines[0], TEXT(""), TEXT("No room here"));
	}

	TestEqual(TEXT("empty prompt has no lines"), Split(TEXT("")).Num(), 0);
	TestEqual(TEXT("blank lines are dropped"), Split(TEXT("\n\n")).Num(), 0);

	Lines = Split(TEXT("E – Get in – now"));
	if (TestEqual(TEXT("cut at the first separator only"), Lines.Num(), 1))
	{
		Check(TEXT("first separator"), Lines[0], TEXT("E"), TEXT("Get in – now"));
	}

	Lines = Split(TEXT("A – a\nB – b\nC – c"));
	if (TestEqual(TEXT("lines past the maximum are joined"), Lines.Num(), DIAHudText::MaxPromptLines))
	{
		Check(TEXT("joined [0]"), Lines[0], TEXT("A"), TEXT("a"));
		Check(TEXT("joined [1]"), Lines[1], TEXT("B"), TEXT("b · C – c"));
	}

	// Every prompt the game can show has a key on every line.
	auto CheckKeys = [this](const TCHAR* What, const FText& Prompt)
	{
		const TArray<FDIAPromptLine> Cut = DIAHudText::SplitPrompt(Prompt);
		TestTrue(*FString::Printf(TEXT("%s has lines"), What), Cut.Num() > 0);
		for (const FDIAPromptLine& Line : Cut)
		{
			TestFalse(*FString::Printf(TEXT("%s: key is not empty"), What), Line.Key.IsEmpty());
		}
	};
	for (int32 Index = 0; Index < static_cast<int32>(ELotMovable::Count); ++Index)
	{
		for (const bool bHeld : {false, true})
		{
			const FText Prompt = DIAFormat::MovablePrompt(static_cast<ELotMovable>(Index), bHeld);
			if (!Prompt.IsEmpty())
			{
				CheckKeys(*FString::Printf(TEXT("movable %d held %d"), Index, bHeld), Prompt);
			}
		}
	}
	CheckKeys(TEXT("car in"), DIAFormat::CarPrompt(true));
	CheckKeys(TEXT("car out"), DIAFormat::CarPrompt(false));
	return true;
}

DIA_TEST(FUiNextLineTest, "DoItAsphalt.Game.Ui.NextLine.StripLabel")

bool FUiNextLineTest::RunTest(const FString& Parameters)
{
	auto Strip = [](const TCHAR* Line) { return DIAHudText::StripLabel(FText::FromString(Line)).ToString(); };
	TestEqual(TEXT("next step"), Strip(TEXT("Next: Exit – entry 6 / 12, cells 1 / 2")), FString(TEXT("Exit – entry 6 / 12, cells 1 / 2")));
	TestEqual(TEXT("no label"), Strip(TEXT("Exit")), FString(TEXT("Exit")));
	TestEqual(TEXT("empty"), Strip(TEXT("")), FString(TEXT("")));
	TestEqual(TEXT("nothing after the label"), Strip(TEXT("Next: ")), FString(TEXT("Next: ")));
	TestEqual(TEXT("paver"), Strip(TEXT("Paver: sector 12 · 143 / 400")), FString(TEXT("sector 12 · 143 / 400")));
	TestEqual(TEXT("colon later in a line without a label"), Strip(TEXT("Exit – needs: 3 cells")), FString(TEXT("Exit – needs: 3 cells")));
	TestEqual(TEXT("next step with a dash"), Strip(TEXT("Next: Sprint – entry 0 / 12")), FString(TEXT("Sprint – entry 0 / 12")));
	return true;
}

DIA_TEST(FMenuPrimaryButtonTest, "DoItAsphalt.Game.Menu.PrimaryButton")

bool FMenuPrimaryButtonTest::RunTest(const FString& Parameters)
{
	FDIAMenuModel Model;
	// IsPrimary is false for None, and true only for the button PrimaryButton names.
	auto CheckIsPrimary = [this, &Model](const TCHAR* State)
	{
		const EDIAMenuButton All[] = {EDIAMenuButton::None, EDIAMenuButton::Continue, EDIAMenuButton::Resume, EDIAMenuButton::NewGame, EDIAMenuButton::Yes};
		for (const EDIAMenuButton Which : All)
		{
			const bool bExpected = Which != EDIAMenuButton::None && Which == Model.PrimaryButton();
			TestEqual(FString::Printf(TEXT("%s: IsPrimary %d"), State, static_cast<int32>(Which)), Model.IsPrimary(Which), bExpected);
		}
	};
	TestTrue(TEXT("closed"), Model.PrimaryButton() == EDIAMenuButton::None);
	CheckIsPrimary(TEXT("closed"));
	Model.Open(EDIAMenuMode::Main, true);
	TestTrue(TEXT("main with a save"), Model.PrimaryButton() == EDIAMenuButton::Continue);
	CheckIsPrimary(TEXT("main with a save"));
	Model.Open(EDIAMenuMode::Main, false);
	TestTrue(TEXT("main without a save"), Model.PrimaryButton() == EDIAMenuButton::NewGame);
	CheckIsPrimary(TEXT("main without a save"));
	Model.Open(EDIAMenuMode::Pause, false);
	TestTrue(TEXT("pause"), Model.PrimaryButton() == EDIAMenuButton::Resume);
	CheckIsPrimary(TEXT("pause"));
	Model.Open(EDIAMenuMode::Pause, true);
	Model.bConfirming = true;
	TestTrue(TEXT("question"), Model.PrimaryButton() == EDIAMenuButton::Yes);
	CheckIsPrimary(TEXT("question"));
	Model.Open(EDIAMenuMode::Pause, true);
	Model.OpenSettings();
	TestTrue(TEXT("settings"), Model.PrimaryButton() == EDIAMenuButton::None);
	CheckIsPrimary(TEXT("settings"));
	Model.CloseSettings();
	TestTrue(TEXT("back from settings"), Model.PrimaryButton() == EDIAMenuButton::Resume);
	CheckIsPrimary(TEXT("back from settings"));
	return true;
}

#endif
