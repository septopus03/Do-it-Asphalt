// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "World/DIAExitGate.h"
#include "World/DIAPaverCar.h"

namespace ExitTestSupport
{
	constexpr float TickSeconds = 0.1f;

	/** The test slot for the length of a test: the real slot is never touched, and the override is gone afterwards. */
	struct FExitTestSlot
	{
		static const FString& Name()
		{
			static const FString SlotName(TEXT("DIA_Test_Exit"));
			return SlotName;
		}
		FExitTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(Name());
			Delete();
		}
		~FExitTestSlot()
		{
			Delete();
			UDIAWorldSubsystem::SetSlotNameOverride(FString());
		}
		static void Delete()
		{
			UGameplayStatics::DeleteGameInSlot(Name(), DIASave::UserIndex);
			UGameplayStatics::DeleteGameInSlot(Name() + DIASave::BackupSuffix, DIASave::UserIndex);
		}
	};

	ADIAExitGate* TheGate(UWorld* World)
	{
		ADIAExitGate* Found = nullptr;
		int32 Count = 0;
		for (TActorIterator<ADIAExitGate> It(World); It; ++It)
		{
			Found = *It;
			++Count;
		}
		return Count == 1 ? Found : nullptr;
	}

	ADIAPaverCar* TheCar(UWorld* World)
	{
		ADIAPaverCar* Found = nullptr;
		int32 Count = 0;
		for (TActorIterator<ADIAPaverCar> It(World); It; ++It)
		{
			Found = *It;
			++Count;
		}
		return Count == 1 ? Found : nullptr;
	}

	void TickFor(FTestWorldWrapper& Wrapper, double Seconds)
	{
		const int32 Ticks = FMath::RoundToInt32(Seconds / TickSeconds);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Wrapper.TickTestWorld(TickSeconds);
		}
	}

	/** Puts the character on the ground at the point, with a sweep-free move that still tells the overlaps. */
	void StandAt(ADIACharacter* Character, FVector2D At)
	{
		Character->SetActorLocation(FVector(At.X, At.Y, DIACharacter::CapsuleHalfHeightCm));
	}

	/** Counts how often a delegate fired. */
	struct FExitCounter
	{
		explicit FExitCounter(UDIAWorldSubsystem* InSubsystem)
			: Subsystem(InSubsystem)
		{
			OpenedHandle = Subsystem->OnExitOpened.AddLambda([this]() { ++Opened; });
			WonHandle = Subsystem->OnWon.AddLambda([this]() { ++Won; });
		}
		~FExitCounter()
		{
			Subsystem->OnExitOpened.Remove(OpenedHandle);
			Subsystem->OnWon.Remove(WonHandle);
		}
		UDIAWorldSubsystem* Subsystem;
		FDelegateHandle OpenedHandle;
		FDelegateHandle WonHandle;
		int32 Opened = 0;
		int32 Won = 0;
	};
}

#define DIA_EXIT_FIXTURE() \
	using namespace ExitTestSupport; \
	FExitTestSlot Slot; \
	FTestWorldWrapper Wrapper; \
	if (!DIATest::StartGameWorld(Wrapper, *this)) \
	{ \
		return false; \
	} \
	UWorld* World = Wrapper.GetTestWorld(); \
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>(); \
	if (!TestNotNull(TEXT("world subsystem"), Subsystem)) \
	{ \
		return false; \
	} \
	Subsystem->SetAutosaveEnabled(false); \
	FLotGame& Game = Subsystem->GetGame(); \
	ADIAExitGate* Gate = TheGate(World); \
	if (!TestNotNull(TEXT("exactly one exit gate"), Gate)) \
	{ \
		return false; \
	}

DIA_TEST(FExitGateStandsTest, "DoItAsphalt.Game.Exit.GateStands")

bool FExitGateStandsTest::RunTest(const FString& Parameters)
{
	DIA_EXIT_FIXTURE()
	TestTrue(TEXT("at the gate position"), FVector2D(Gate->GetActorLocation()).Equals(LotLayout::GatePos(), 1.0));
	const FVector2D Outward = AsphaltLot::EntryOutward(LotLayout::ExitEntry);
	TestTrue(TEXT("turned along the road, outwards"), FVector2D(Gate->GetActorForwardVector()).Equals(Outward, 1e-3));
	TestTrue(TEXT("the trigger lies where the layout says"),
		FVector2D(Gate->GetTrigger()->GetComponentLocation()).Equals(LotLayout::ExitTrigger(), 1.0));
	TestEqual(TEXT("with the layout's radius"), static_cast<double>(Gate->GetTrigger()->GetScaledSphereRadius()), LotLayout::ExitTriggerRadiusCm, 0.01);
	TestFalse(TEXT("closed in a new game"), Gate->IsOpen());
	TestTrue(TEXT("the arm blocks"), Gate->GetArmCollision()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	for (int32 Side = 0; Side < DIAExitGate::SignFaceCount; ++Side)
	{
		TestNotNull(TEXT("a sign face"), Gate->GetSignText(Side));
		TestEqual(TEXT("it says EXIT"), Gate->GetSignText(Side) ? Gate->GetSignText(Side)->Text.ToString() : FString(), FString(TEXT("EXIT")));
	}
	TestNotNull(TEXT("the frame has its model"), Gate->GetFrameMesh()->GetStaticMesh().Get());
	TestNotNull(TEXT("the arm has its model"), Gate->GetArmMesh()->GetStaticMesh().Get());
	return true;
}

DIA_TEST(FExitClosedUntilAllTest, "DoItAsphalt.Game.Exit.ClosedUntilAll")

bool FExitClosedUntilAllTest::RunTest(const FString& Parameters)
{
	DIA_EXIT_FIXTURE()
	FExitCounter Counter(Subsystem);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::GatePos() - FVector2D(2000.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	StandAt(Character, LotLayout::ExitTrigger());
	TickFor(Wrapper, 0.5);
	TestFalse(TEXT("walking into the sphere before the end does not win"), Game.HasWon());
	TestEqual(TEXT("and announces nothing"), Counter.Won, 0);

	// Not even a nearly finished lot opens it.
	Subsystem->Dispatch(Game.DebugCompleteCells(AsphaltLot::NumCells - 1));
	StandAt(Character, LotLayout::ExitTrigger() - FVector2D(2000.0, 0.0));
	Wrapper.TickTestWorld(TickSeconds);
	StandAt(Character, LotLayout::ExitTrigger());
	TickFor(Wrapper, 0.5);
	TestFalse(TEXT("one cell short: still closed"), Game.HasWon());
	TestFalse(TEXT("the arm is down"), Gate->IsOpen());
	TestEqual(TEXT("the exit did not open"), Counter.Opened, 0);
	return true;
}

DIA_TEST(FExitOpensAndWinsTest, "DoItAsphalt.Game.Exit.OpensAndWins")

bool FExitOpensAndWinsTest::RunTest(const FString& Parameters)
{
	DIA_EXIT_FIXTURE()
	FExitCounter Counter(Subsystem);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::GatePos() - FVector2D(2000.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}

	// dia.Complete 30000
	Subsystem->Dispatch(Game.DebugCompleteCells(30000));
	TestEqual(TEXT("the exit opened once"), Counter.Opened, 1);
	TestTrue(TEXT("the core says so"), Game.IsExitOpen());
	TickFor(Wrapper, DIAExitGate::OpenSeconds + 0.5);
	TestTrue(TEXT("the gate is open"), Gate->IsOpen());
	TestTrue(TEXT("the arm does not block any more"), Gate->GetArmCollision()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestEqual(TEXT("it opened once and stays so"), Counter.Opened, 1);
	TestFalse(TEXT("not won yet"), Game.HasWon());

	// Through the gate: the sphere wins the game.
	StandAt(Character, LotLayout::ExitTrigger());
	TickFor(Wrapper, 0.5);
	TestTrue(TEXT("passing the sphere wins"), Game.HasWon());
	TestEqual(TEXT("OnWon fired once"), Counter.Won, 1);
	TickFor(Wrapper, 0.5);
	StandAt(Character, LotLayout::ExitTrigger() - FVector2D(2000.0, 0.0));
	TickFor(Wrapper, 0.2);
	StandAt(Character, LotLayout::ExitTrigger());
	TickFor(Wrapper, 0.5);
	TestEqual(TEXT("and only once, however often it is touched"), Counter.Won, 1);

	FDIAWinModel Model;
	Model.Update(Game);
	TestEqual(TEXT("30,000 cells on the win screen"), Model.GetView().Cells.ToString(), FString(TEXT("30,000")));
	TestEqual(TEXT("300,000 credits earned"), Model.GetView().Credits.ToString(), FString(TEXT("300,000")));
	return true;
}

DIA_TEST(FExitCarWinsTest, "DoItAsphalt.Game.Exit.CarWins")

bool FExitCarWinsTest::RunTest(const FString& Parameters)
{
	DIA_EXIT_FIXTURE()
	FExitCounter Counter(Subsystem);
	Game.DebugGrantItem(ELotItem::Car);
	Subsystem->OnItemBought.Broadcast(ELotItem::Car);
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	const FVector Far(LotLayout::GatePos().X - 3000.0, LotLayout::GatePos().Y, DIAPaverCar::OriginHeightCm());
	Car->SetActorLocation(Far);
	Wrapper.TickTestWorld(TickSeconds);
	Car->SetActorLocation(FVector(LotLayout::ExitTrigger().X, LotLayout::ExitTrigger().Y, DIAPaverCar::OriginHeightCm()));
	TickFor(Wrapper, 0.5);
	TestFalse(TEXT("the car does not win a closed exit"), Game.HasWon());

	Subsystem->Dispatch(Game.DebugCompleteCells(30000));
	Car->SetActorLocation(Far);
	Wrapper.TickTestWorld(TickSeconds);
	Car->SetActorLocation(FVector(LotLayout::ExitTrigger().X, LotLayout::ExitTrigger().Y, DIAPaverCar::OriginHeightCm()));
	TickFor(Wrapper, 0.5);
	TestTrue(TEXT("the car through the open gate wins"), Game.HasWon());
	TestEqual(TEXT("once"), Counter.Won, 1);

	// Nothing keeps pouring after the win.
	Car->SetThrottle(1.f);
	Car->SetPouring(true);
	TestFalse(TEXT("the button does not start the pouring after the win"), Car->IsPouring());
	TickFor(Wrapper, 1.0);
	TestFalse(TEXT("the car does not pour after the win"), Car->IsPouring());
	return true;
}

DIA_TEST(FExitGateFollowsTheCoreTest, "DoItAsphalt.Game.Exit.GateFollowsTheCore")

bool FExitGateFollowsTheCoreTest::RunTest(const FString& Parameters)
{
	DIA_EXIT_FIXTURE()
	// A loaded game that is finished: the gate is open at once, with no rise.
	Game.DebugCompleteCells(AsphaltLot::NumCells);
	Subsystem->OnGameReset.Broadcast();
	TestTrue(TEXT("a loaded finished game has the gate open at once"), Gate->IsOpen());
	TestTrue(TEXT("and not blocking"), Gate->GetArmCollision()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

	// A new game closes it again.
	Subsystem->NewGame();
	TestFalse(TEXT("a new game has the gate closed"), Gate->IsOpen());
	TestTrue(TEXT("the arm blocks again"), Gate->GetArmCollision()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	TestTrue(TEXT("and stands down"), Gate->GetOpenFraction() == 0.0);
	TestTrue(TEXT("still exactly one gate"), TheGate(World) == Gate);
	return true;
}

DIA_TEST(FExitDrivingPlayerWinsTest, "DoItAsphalt.Game.Exit.DrivingPlayerWins")

bool FExitDrivingPlayerWinsTest::RunTest(const FString& Parameters)
{
	DIA_EXIT_FIXTURE()
	Game.DebugGrantItem(ELotItem::Car);
	Subsystem->OnItemBought.Broadcast(ELotItem::Car);
	ADIAPaverCar* Car = TheCar(World);
	const FVector2D Start = LotLayout::GatePos() - FVector2D(2000.0, 0.0);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, Start + FVector2D(0.0, -180.0));
	if (!TestNotNull(TEXT("the car"), Car) || !TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	Car->SetActorLocationAndRotation(FVector(Start.X, Start.Y, DIAPaverCar::OriginHeightCm()), FRotator::ZeroRotator);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
	if (!TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}
	Controller->Possess(Character);
	Params.Owner = Controller;
	Controller->MyHUD = World->SpawnActor<ADIAHUD>(Params);
	Wrapper.TickTestWorld(TickSeconds);
	Controller->OnInteract();
	if (!TestTrue(TEXT("driving"), Controller->GetPawn() == Car))
	{
		return false;
	}

	// Drive out through the open gate with the button down.
	Subsystem->Dispatch(Game.DebugCompleteCells(30000));
	TickFor(Wrapper, DIAExitGate::OpenSeconds + 0.2);
	Controller->OnMove(FInputActionValue(FVector2D(0.0, 1.0)));
	Controller->OnUseStarted();
	for (int32 I = 0; I < 200 && !Game.HasWon(); ++I)
	{
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestTrue(TEXT("driving into the sphere wins"), Game.HasWon());
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the win screen took the controls: the car is not pouring"), !Car->IsPouring());
	TestEqual(TEXT("or accelerating"), Car->GetThrottle(), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
