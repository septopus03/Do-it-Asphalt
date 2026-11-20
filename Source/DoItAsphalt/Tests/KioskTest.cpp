// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "World/DIAKioskActor.h"
#include "World/DIARoadActor.h"

namespace
{
	ADIAKioskActor* FindKiosk(UWorld* World, EAsphaltSpecial Special)
	{
		for (TActorIterator<ADIAKioskActor> It(World); It; ++It)
		{
			if (It->Special == Special)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Completes the given number of cells of the road, through the subsystem like a real pour would. */
	void CompleteRoadCells(UDIAWorldSubsystem* Subsystem, int32 Road, int32 Count)
	{
		FAsphaltGame& Game = Subsystem->GetGame();
		int32 Done = 0;
		for (const int32 Cell : Game.GetMap().GetRoadCells(Road))
		{
			if (Done >= Count)
			{
				break;
			}
			if (!Game.GetGrid().IsCellComplete(Cell))
			{
				Subsystem->Dispatch(Game.DebugCompleteCell(Cell));
			}
			++Done;
		}
	}
}

DIA_TEST(FKioskFlowTest, "DoItAsphalt.Game.KioskFlow")

bool FKioskFlowTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FAsphaltGame& Game = Subsystem->GetGame();

	int32 KioskCount = 0;
	for (TActorIterator<ADIAKioskActor> It(World); It; ++It)
	{
		++KioskCount;
	}
	TestEqual(TEXT("one kiosk per special"), KioskCount, 5);
	ADIAKioskActor* Kiosk = FindKiosk(World, EAsphaltSpecial::Sprint);
	if (!TestNotNull(TEXT("Sprint kiosk"), Kiosk))
	{
		return false;
	}
	TestEqual(TEXT("Sprint is sold on road 3"), Kiosk->GetRoad(), 3);

	int32 Bought = 0;
	Subsystem->OnSpecialBought.AddLambda([&](EAsphaltSpecial) { ++Bought; });

	// Three cells of road 3: the road is not finished, the prompt counts, the padlock shows and nothing can be bought.
	CompleteRoadCells(Subsystem, 3, 3);
	const FString Prompt = Kiosk->GetPrompt(nullptr).ToString();
	TestTrue(TEXT("prompt shows the progress 3 / 5"), Prompt.Contains(TEXT("3 / 5")));
	TestEqual(TEXT("prompt text"), Prompt, FString(TEXT("Finish the road (3 / 5)")));
	TestTrue(TEXT("the padlock shows"), Kiosk->GetLockMesh()->IsVisible());
	TestTrue(TEXT("the kiosk is locked"), Kiosk->IsLocked());
	Game.DebugGive(1000.0, 0);
	Kiosk->Interact(nullptr);
	TestFalse(TEXT("a locked kiosk sells nothing"), Game.HasSpecial(EAsphaltSpecial::Sprint));
	TestEqual(TEXT("nothing announced"), Bought, 0);
	Game.DebugGive(-1.0e9, 0);

	// All five cells, no money: "need 100" (the cells paid out money too, so the money is zeroed first).
	CompleteRoadCells(Subsystem, 3, 5);
	Game.DebugGive(-1.0e9, 0);
	TestEqual(TEXT("need money"), Kiosk->GetPrompt(nullptr).ToString(), FString(TEXT("Sprint – need 100")));
	TestFalse(TEXT("the padlock is gone"), Kiosk->GetLockMesh()->IsVisible());
	Kiosk->Interact(nullptr);
	TestFalse(TEXT("too poor to buy"), Game.HasSpecial(EAsphaltSpecial::Sprint));

	// With the money it can be bought, exactly once.
	Game.DebugGive(100.0, 0);
	TestEqual(TEXT("buy prompt"), Kiosk->GetPrompt(nullptr).ToString(), FString(TEXT("E – Buy Sprint (100)")));
	Kiosk->Interact(nullptr);
	TestTrue(TEXT("Sprint is owned"), Game.HasSpecial(EAsphaltSpecial::Sprint));
	TestEqual(TEXT("OnSpecialBought fired once"), Bought, 1);
	TestEqual(TEXT("the price was paid"), Game.GetMoney(), 0.0);
	TestEqual(TEXT("owned prompt"), Kiosk->GetPrompt(nullptr).ToString(), FString(TEXT("Sprint – owned")));
	Game.DebugGive(500.0, 0);
	Kiosk->Interact(nullptr);
	TestEqual(TEXT("a second Interact announces nothing"), Bought, 1);
	TestEqual(TEXT("and costs nothing"), Game.GetMoney(), 500.0);
	return true;
}

DIA_TEST(FKioskPromptTest, "DoItAsphalt.Game.KioskPrompt")

bool FKioskPromptTest::RunTest(const FString& Parameters)
{
	// The texts of all five kiosks (names and prices from the design), as pure functions of the game.
	struct FCase
	{
		EAsphaltSpecial Special;
		const TCHAR* Name;
		const TCHAR* Price;
	};
	const FCase Cases[] = {
		{EAsphaltSpecial::Sprint, TEXT("Sprint"), TEXT("100")},
		{EAsphaltSpecial::TwoHands, TEXT("Two Hands"), TEXT("200")},
		{EAsphaltSpecial::PortableStation, TEXT("Portable Station"), TEXT("300")},
		{EAsphaltSpecial::AutoPaver, TEXT("Auto Paver"), TEXT("450")},
		{EAsphaltSpecial::PaverCar, TEXT("Paver Truck"), TEXT("800")},
	};
	for (const FCase& Case : Cases)
	{
		FAsphaltGame Game;
		const int32 Road = AsphaltBalance::SpecialRoad[static_cast<int32>(Case.Special)];
		TestEqual(*FString::Printf(TEXT("%s: name"), Case.Name), DIAFormat::SpecialName(Case.Special).ToString(), FString(Case.Name));
		TestEqual(*FString::Printf(TEXT("%s: locked"), Case.Name), DIAFormat::KioskPrompt(Game, Case.Special).ToString(), FString(TEXT("Finish the road (0 / 5)")));
		for (const int32 Cell : Game.GetMap().GetRoadCells(Road))
		{
			Game.DebugCompleteCell(Cell);
		}
		Game.DebugGive(-1.0e9, 0);
		TestEqual(*FString::Printf(TEXT("%s: poor"), Case.Name), DIAFormat::KioskPrompt(Game, Case.Special).ToString(),
			FString::Printf(TEXT("%s – need %s"), Case.Name, Case.Price));
		Game.DebugGive(1000.0, 0);
		TestEqual(*FString::Printf(TEXT("%s: buy"), Case.Name), DIAFormat::KioskPrompt(Game, Case.Special).ToString(),
			FString::Printf(TEXT("E – Buy %s (%s)"), Case.Name, Case.Price));
		Game.BuySpecial(Case.Special);
		TestEqual(*FString::Printf(TEXT("%s: owned"), Case.Name), DIAFormat::KioskPrompt(Game, Case.Special).ToString(),
			FString::Printf(TEXT("%s – owned"), Case.Name));
	}
	return true;
}

DIA_TEST(FKioskPlacementTest, "DoItAsphalt.Game.KioskPlacement")

bool FKioskPlacementTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	for (int32 Index = 0; Index < static_cast<int32>(EAsphaltSpecial::Count); ++Index)
	{
		const EAsphaltSpecial Special = static_cast<EAsphaltSpecial>(Index);
		ADIAKioskActor* Kiosk = FindKiosk(World, Special);
		if (!TestNotNull(TEXT("kiosk exists"), Kiosk))
		{
			continue;
		}
		const int32 Road = AsphaltBalance::SpecialRoad[Index];
		const FVector2D Location(Kiosk->GetActorLocation());
		TestTrue(TEXT("kiosk stands at KioskPos of its road"), Location.Equals(AsphaltLayout::KioskPos(Road), 0.1));
		TestEqual(TEXT("kiosk stands on the ground"), Kiosk->GetActorLocation().Z, 0.0);
		// The screen (+X of the actor) looks at the road.
		const FVector2D ToRoad = (AsphaltLayout::RoadEnd(Road) - Location).GetSafeNormal();
		TestTrue(TEXT("kiosk faces the road"), FVector2D::DotProduct(FVector2D(Kiosk->GetActorForwardVector()), ToRoad) > 0.999);
		// The board carries the name of the special.
		TestEqual(TEXT("board text is the name"), Kiosk->GetBoardText()->Text.ToString(), DIAFormat::SpecialName(Special).ToString());
		// Solid for the player, transparent for the camera.
		TestEqual(TEXT("kiosk blocks pawns"), static_cast<int32>(Kiosk->GetCollision()->GetCollisionResponseToChannel(ECC_Pawn)), static_cast<int32>(ECR_Block));
		TestEqual(TEXT("kiosk does not block the camera"), static_cast<int32>(Kiosk->GetCollision()->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));
	}
	return true;
}

DIA_TEST(FInteractionSearchTest, "DoItAsphalt.Game.InteractionSearch")

bool FInteractionSearchTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	ADIAKioskActor* Kiosk = FindKiosk(World, EAsphaltSpecial::Sprint);
	const FVector2D KioskPos = AsphaltLayout::KioskPos(3);
	const FVector2D ToRoad = (AsphaltLayout::RoadEnd(3) - KioskPos).GetSafeNormal();

	// Near the screen (about 100 cm in front of it) the kiosk is found; at the end of its road, 220 cm from the kiosk's
	// centre but still 150 cm from its collision, it is found only just; far away it is not.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, KioskPos + ToRoad * 100.0);
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("kiosk"), Kiosk))
	{
		return false;
	}
	AActor* Found = nullptr;
	IDIAInteractable* Interactable = Subsystem->FindInteractable(Character, &Found);
	TestTrue(TEXT("the kiosk is found when close"), Interactable == Cast<IDIAInteractable>(Kiosk) && Found == Kiosk);

	// The range is measured to the collision box (half depth 30 cm), not to the kiosk's origin: 175 cm from the origin is
	// 145 cm from the box, 190 cm is 160 cm.
	Character->SetActorLocation(FVector(KioskPos.X + ToRoad.X * 175.0, KioskPos.Y + ToRoad.Y * 175.0, DIACharacter::CapsuleHalfHeightCm));
	TestTrue(TEXT("still in range 145 cm from the collision"), Subsystem->FindInteractable(Character) != nullptr);
	Character->SetActorLocation(FVector(KioskPos.X + ToRoad.X * 190.0, KioskPos.Y + ToRoad.Y * 190.0, DIACharacter::CapsuleHalfHeightCm));
	TestTrue(TEXT("out of range 160 cm from the collision"), Subsystem->FindInteractable(Character) == nullptr);
	Character->SetActorLocation(FVector(AsphaltLayout::PlayerStart.X, AsphaltLayout::PlayerStart.Y, DIACharacter::CapsuleHalfHeightCm));
	TestTrue(TEXT("nothing at the player's start"), Subsystem->FindInteractable(Character) == nullptr);

	// The player controller uses the same search, but not while the shop is open.
	Character->SetActorLocation(FVector(KioskPos.X + ToRoad.X * 100.0, KioskPos.Y + ToRoad.Y * 100.0, DIACharacter::CapsuleHalfHeightCm));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params))
	{
		Controller->Possess(Character);
		TestTrue(TEXT("the controller finds the kiosk"), Controller->FindInteractable() == Cast<IDIAInteractable>(Kiosk));
		Controller->SetShopInput(true);
		TestTrue(TEXT("with the shop open nothing can be used"), Controller->FindInteractable() == nullptr);
		Controller->SetShopInput(false);
		TestTrue(TEXT("with the shop closed again the kiosk is found"), Controller->FindInteractable() == Cast<IDIAInteractable>(Kiosk));
	}
	else
	{
		AddError(TEXT("no controller"));
	}
	return true;
}

DIA_TEST(FSprintSpeedTest, "DoItAsphalt.Game.SprintSpeed")

bool FSprintSpeedTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-1200.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	Character->SetSprinting(true);
	TestEqual(TEXT("no sprint without the special"), static_cast<double>(Movement->MaxWalkSpeed), 450.0);

	Subsystem->GetGame().DebugGrantSpecial(EAsphaltSpecial::Sprint);
	Character->SetSprinting(true);
	TestEqual(TEXT("sprint with the special"), static_cast<double>(Movement->MaxWalkSpeed), 720.0, 0.01);

	Character->SetPouring(true);
	TestEqual(TEXT("pouring slows the sprint down to the pouring speed"), static_cast<double>(Movement->MaxWalkSpeed), 200.0);
	Character->SetPouring(false);
	TestEqual(TEXT("sprinting resumes when pouring stops (Shift still held)"), static_cast<double>(Movement->MaxWalkSpeed), 720.0, 0.01);

	Character->SetSprinting(false);
	TestEqual(TEXT("releasing Shift walks"), static_cast<double>(Movement->MaxWalkSpeed), 450.0);
	return true;
}

DIA_TEST(FTwoHandsTest, "DoItAsphalt.Game.TwoHands")

bool FTwoHandsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FAsphaltGame& Game = Subsystem->GetGame();
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-1200.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}

	// Tool level 4 is 1.0 m wide.
	Game.DebugSetLevel(EAsphaltUpgrade::Tool, 4);
	Wrapper.TickTestWorld(0.1f);
	TestEqual(TEXT("one spreader of 100 cm"), Game.GetToolWidthCm(), 100.0);
	TestNotNull(TEXT("first spreader"), Character->GetSpreaderMesh());
	TestNull(TEXT("no second spreader without TwoHands"), Character->GetSecondSpreaderMesh());

	Game.DebugGrantSpecial(EAsphaltSpecial::TwoHands);
	Wrapper.TickTestWorld(0.1f);
	TestEqual(TEXT("the pouring width is doubled"), Game.GetToolWidthCm(), 200.0);
	const UStaticMeshComponent* A = Character->GetSpreaderMesh();
	const UStaticMeshComponent* B = Character->GetSecondSpreaderMesh();
	if (TestNotNull(TEXT("first spreader"), A) && TestNotNull(TEXT("second spreader"), B))
	{
		// They sit side by side at Y = -W/2 and +W/2 of the pivot (W = 100 cm, the width of one), together 2 m wide.
		const FVector Pivot = A->GetAttachParent()->GetComponentLocation();
		const FBox BoxA = A->CalcBounds(A->GetComponentTransform()).GetBox();
		const FBox BoxB = B->CalcBounds(B->GetComponentTransform()).GetBox();
		// The real model's envelope is the bar plus 5 cm of wheel at each end (W + 10); the placeholder is exactly W.
		const double Wheels = A->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")) ? 0.0 : 10.0;
		TestEqual(TEXT("the first is one level wide"), BoxA.Max.Y - BoxA.Min.Y, 100.0 + Wheels, 0.5);
		TestEqual(TEXT("the second is one level wide"), BoxB.Max.Y - BoxB.Min.Y, 100.0 + Wheels, 0.5);
		TestEqual(TEXT("the first is centred at -W/2"), (BoxA.Min.Y + BoxA.Max.Y) * 0.5, Pivot.Y - 50.0, 0.5);
		TestEqual(TEXT("the second is centred at +W/2"), (BoxB.Min.Y + BoxB.Max.Y) * 0.5, Pivot.Y + 50.0, 0.5);
		TestEqual(TEXT("together they span 2 W"), FMath::Max(BoxA.Max.Y, BoxB.Max.Y) - FMath::Min(BoxA.Min.Y, BoxB.Min.Y), 200.0 + Wheels, 0.5);
	}
	return true;
}

DIA_TEST(FRoadPropsTest, "DoItAsphalt.Game.RoadProps")

bool FRoadPropsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	int32 Roads = 0;
	int32 Barriers = 0;
	for (TActorIterator<ADIARoadActor> It(World); It; ++It)
	{
		const int32 Road = It->Road;
		++Roads;
		TestTrue(TEXT("the road piece starts at RoadEnd"), FVector2D(It->GetActorLocation()).Equals(AsphaltLayout::RoadEnd(Road), 0.1));
		// The stub runs outwards: its far end (12 m on) is beyond the fence line, the near end at the road end.
		const FVector2D Dir = FHexMap::RoadDirection(Road);
		TestTrue(TEXT("the road piece points away from the lot"), FVector2D(It->GetActorForwardVector()).Equals(Dir, 0.001));
		const FBox Stub = It->GetStubMesh()->CalcBounds(It->GetStubMesh()->GetComponentTransform()).GetBox();
		const FVector2D Far = FVector2D(It->GetActorLocation()) + Dir * DIAModelSizes::RoadStubCm.X;
		TestTrue(TEXT("the stub reaches the fence"), Far.Size() >= AsphaltLayout::SiteRadiusCm - 30.0);
		TestTrue(TEXT("the stub lies on the ground"), Stub.Min.Z >= -0.5 && Stub.Max.Z <= DIAModelSizes::RoadStubCm.Z + 0.5);

		if (Road == AsphaltBalance::ExitRoad)
		{
			TestFalse(TEXT("the exit road has no barrier"), It->HasBarrier());
			continue;
		}
		++Barriers;
		TestTrue(TEXT("the other roads have a barrier"), It->HasBarrier());
		const FVector Gate(AsphaltLayout::GatePos(Road).X, AsphaltLayout::GatePos(Road).Y, 50.0);
		TestTrue(TEXT("the barrier stands at the gate position"), FVector2D(It->GetBarrierCollision()->GetComponentLocation()).Equals(AsphaltLayout::GatePos(Road), 0.1));
		// A pawn walking out along the road is stopped by it, the camera is not.
		FHitResult Hit;
		const FVector Inside = Gate - FVector(Dir.X, Dir.Y, 0.0) * 200.0;
		const FVector Outside = Gate + FVector(Dir.X, Dir.Y, 0.0) * 200.0;
		TestTrue(TEXT("the barrier blocks pawns"), World->LineTraceSingleByChannel(Hit, Inside, Outside, ECC_Pawn));
		TestFalse(TEXT("the barrier does not block the camera"), World->LineTraceSingleByChannel(Hit, Inside, Outside, ECC_Camera));
	}
	TestEqual(TEXT("six road pieces"), Roads, 6);
	TestEqual(TEXT("five barriers"), Barriers, 5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
