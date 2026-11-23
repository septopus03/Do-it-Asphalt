// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Engine/World.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "LotGame.h"
#include "LotMap.h"
#include "Tests/DIATestWorld.h"

DIA_TEST(FFluidRunsInTheWorldTest, "DoItAsphalt.Game.Fluid.RunsInTheWorld")
DIA_TEST(FFluidOnByDefaultTest, "DoItAsphalt.Game.Fluid.OnByDefault")
DIA_TEST(FFluidSavedSettledTest, "DoItAsphalt.Game.Fluid.SavedSettled")

namespace
{
	int32 GroundTexels(const FLotGame& Game)
	{
		int32 Count = 0;
		for (uint8 Texel : Game.GetGrid().GetTexels())
		{
			Count += Texel != 0 ? 1 : 0;
		}
		return Count;
	}

	/** Pours the bowl on a cell in the middle of the lot; the subsystem books it. */
	void PourBowl(UDIAWorldSubsystem* Subsystem)
	{
		FLotGame& Game = Subsystem->GetGame();
		Game.ScoopFromTrough(20.0);
		const FVector2D Aim = AsphaltLot::CellCentre(50 * AsphaltLot::Side + 50);
		Subsystem->Dispatch(Game.PourHand(Aim, Aim + FVector2D(150.0, 0.0), 1.0));
	}
}

bool FFluidOnByDefaultTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this, true))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(Wrapper.GetTestWorld());
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	TestTrue(TEXT("the running game lets the asphalt run"), Subsystem->GetGame().IsFluidEnabled());
	Subsystem->NewGame();
	TestTrue(TEXT("and still does after a new game"), Subsystem->GetGame().IsFluidEnabled());
	return true;
}

bool FFluidRunsInTheWorldTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this, true))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(Wrapper.GetTestWorld());
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	PourBowl(Subsystem);
	const FLotGame& Game = Subsystem->GetGame();
	TestTrue(TEXT("the asphalt runs"), Game.IsFluidActive());
	TestEqual(TEXT("there is no ground at the moment of the pour"), GroundTexels(Game), 0);
	const double Paid = Game.GetCredits();
	TestTrue(TEXT("a dollar a litre is paid at once"), Paid > 0.0);

	for (int32 Frame = 0; Frame < 60 * 30 && Game.IsFluidActive(); ++Frame)
	{
		Wrapper.TickTestWorld(1.f / 30.f);
	}
	TestFalse(TEXT("it settles"), Game.IsFluidActive());
	TestTrue(TEXT("and has set ground where it ran"), GroundTexels(Game) > 0);
	TestTrue(TEXT("the credits only grew"), Game.GetCredits() >= Paid);
	return true;
}

bool FFluidSavedSettledTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this, true))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(Wrapper.GetTestWorld());
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	PourBowl(Subsystem);
	Wrapper.TickTestWorld(0.1f);
	const FLotGame& Game = Subsystem->GetGame();
	TestTrue(TEXT("the asphalt is running"), Game.IsFluidActive());

	const int32 GroundBefore = GroundTexels(Game);

	// A save holds only the ground: what still runs is let to set on a copy, and the running game goes on.
	const FString Slot = TEXT("DIA_FluidTest");
	UDIAWorldSubsystem::SetSlotNameOverride(Slot);
	Subsystem->SaveGame();
	TestTrue(TEXT("a save does not stop the running game"), Game.IsFluidActive());
	TestEqual(TEXT("or change its ground"), GroundTexels(Game), GroundBefore);

	TestTrue(TEXT("the save loads"), Subsystem->LoadGame());
	TestTrue(TEXT("it holds the ground the asphalt set"), GroundTexels(Game) > GroundBefore);
	TestFalse(TEXT("and nothing that still runs"), Game.IsFluidActive());
	TestTrue(TEXT("the fluid is still on for what is poured next"), Game.IsFluidEnabled());

	UGameplayStatics::DeleteGameInSlot(Slot, DIASave::UserIndex);
	UGameplayStatics::DeleteGameInSlot(Slot + DIASave::BackupSuffix, DIASave::UserIndex);
	UDIAWorldSubsystem::SetSlotNameOverride(FString());
	return true;
}

#endif
