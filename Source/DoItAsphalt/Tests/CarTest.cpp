// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
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
#include "World/DIAMovable.h"
#include "World/DIAPaverCar.h"

namespace CarTestSupport
{
	constexpr float TickSeconds = 0.1f;
	/** The character stands this far from the car's axis, beside its door side (the box is 220 cm wide: the capsule is 70 cm from it). */
	constexpr double NextToCm = 180.0;
	constexpr double ExitMaxDistanceCm = 250.0;

	/** The test slot for the length of a test: the real slot is never touched, and the override is gone afterwards. */
	struct FCarTestSlot
	{
		static const FString& Name()
		{
			static const FString SlotName(TEXT("DIA_Test_Car"));
			return SlotName;
		}
		FCarTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(Name());
			Delete();
		}
		~FCarTestSlot()
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

	/** Centre of an ordinary lot cell away from the edges of the car park. */
	FVector2D MiddleOfLot()
	{
		return AsphaltLot::CellCentre(AsphaltLot::CellIndex(40, 40));
	}

	FVector2D Where(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}

	TArray<ADIAPaverCar*> FindCars(UWorld* World)
	{
		TArray<ADIAPaverCar*> Result;
		for (TActorIterator<ADIAPaverCar> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		return Result;
	}

	ADIAPaverCar* TheCar(UWorld* World)
	{
		const TArray<ADIAPaverCar*> Cars = FindCars(World);
		return Cars.Num() == 1 ? Cars[0] : nullptr;
	}

	/** What the cheat dia.Item does: grants the item for free and announces it like a purchase (the actor appears). */
	bool Grant(UDIAWorldSubsystem* Subsystem, ELotItem Item)
	{
		Subsystem->GetGame().DebugGrantItem(Item);
		Subsystem->OnItemBought.Broadcast(Item);
		Subsystem->OnStateChanged.Broadcast();
		return Subsystem->GetGame().Owns(Item);
	}

	/** Puts the car somewhere without a sweep, standing still and facing YawDeg. */
	void PlaceCar(ADIAPaverCar* Car, FVector2D At, double YawDeg)
	{
		Car->SetActorLocationAndRotation(FVector(At.X, At.Y, DIAPaverCar::OriginHeightCm(Car->GetShownLevel())), FRotator(0.0, YawDeg, 0.0));
	}

	/** What the cheat dia.Item does for a tier: it and the ones below it are granted, one after the other. */
	bool GrantTier(UDIAWorldSubsystem* Subsystem, int32 Level)
	{
		bool bAll = true;
		for (int32 Tier = 0; Tier <= Level; ++Tier)
		{
			bAll = Grant(Subsystem, static_cast<ELotItem>(static_cast<int32>(ELotItem::Car) + Tier)) && bAll;
		}
		return bAll;
	}

	void TickFor(FTestWorldWrapper& Wrapper, double Seconds)
	{
		const int32 Ticks = FMath::RoundToInt32(Seconds / TickSeconds);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Wrapper.TickTestWorld(TickSeconds);
		}
	}

	/** A player controller with a HUD that possesses the character, as in the real game (the toasts need it). */
	ADIAPlayerController* Possess(UWorld* World, ADIACharacter* Character)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
		if (!Controller)
		{
			return nullptr;
		}
		Controller->Possess(Character);
		Params.Owner = Controller;
		Controller->MyHUD = World->SpawnActor<ADIAHUD>(Params);
		return Controller;
	}

	/** A solid box of the given half-size standing on the ground at the point: something the car's exit spot cannot be inside. */
	AActor* SpawnBlocker(UWorld* World, FVector2D At, double HalfCm)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(FVector(At.X, At.Y, HalfCm), FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Blocker"));
		Box->SetBoxExtent(FVector(HalfCm, HalfCm, HalfCm * 2.0));
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->SetRootComponent(Box);
		Box->RegisterComponent();
		Box->SetWorldLocation(FVector(At.X, At.Y, HalfCm * 2.0));
		return Actor;
	}

	/**
	 * The third-person view: the whole body is seen by everyone (nothing is hidden from the owner), the camera hangs on a boom, and the
	 * collision stays a blocking box. The view target is the one expected: the car while driven, the character otherwise.
	 */
	void CheckCabinView(FAutomationTestBase& Test, ADIAPaverCar* Car, const ADIAPlayerController* Controller, const AActor* ExpectedViewTarget, const TCHAR* When)
	{
		const FString Tag(When);
		Test.TestTrue(Tag + TEXT(": the view target is the one expected"), Controller->GetViewTarget() == ExpectedViewTarget);
		UStaticMeshComponent* Body = Car->GetBodyMesh();
		Test.TestFalse(Tag + TEXT(": the body is not hidden from the owner's view"), Body->bOwnerNoSee);
		Test.TestFalse(Tag + TEXT(": the body is for everyone"), Body->bOnlyOwnerSee);
		Test.TestTrue(Tag + TEXT(": the body is visible"), Body->IsVisible() && !Car->IsHidden());
			Test.TestEqual(Tag + TEXT(": the collision is still a blocking box"), Car->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
		Test.TestEqual(Tag + TEXT(": the body has no collision of its own"), Body->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}

	int32 CountRawOrDone(const FLotGame& Game)
	{
		int32 Count = 0;
		for (const uint8 Texel : Game.GetGrid().GetTexels())
		{
			Count += Texel != static_cast<uint8>(ELotTexel::Free) ? 1 : 0;
		}
		return Count;
	}
}

#define DIA_CAR_FIXTURE() \
	using namespace CarTestSupport; \
	FCarTestSlot Slot; \
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
	FLotGame& Game = Subsystem->GetGame();

DIA_TEST(FCarSpawnsWhenBoughtTest, "DoItAsphalt.Game.Car.SpawnsWhenBought")

bool FCarSpawnsWhenBoughtTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestEqual(TEXT("not owned, no car stands"), FindCars(World).Num(), 0);

	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	TestEqual(TEXT("owned, exactly one car stands"), FindCars(World).Num(), 1);
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	TestTrue(TEXT("it stands where the core has it"), Where(Car).Equals(Game.World().CarPos, 1.0));

	TestTrue(TEXT("the second tier comes on top"), Grant(Subsystem, ELotItem::CarT2));
	TestEqual(TEXT("still exactly one car"), FindCars(World).Num(), 1);

	// A new game takes it away again, and a game that owns it has one after the reset.
	Subsystem->NewGame();
	TestEqual(TEXT("a new game owns no car, none stands"), FindCars(World).Num(), 0);
	TestTrue(TEXT("grant it again"), Grant(Subsystem, ELotItem::Car));
	TestEqual(TEXT("one car again"), FindCars(World).Num(), 1);
	return true;
}

DIA_TEST(FCarEnterExitTest, "DoItAsphalt.Game.Car.EnterExit")

bool FCarEnterExitTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	TestTrue(TEXT("grant the cart"), Grant(Subsystem, ELotItem::Cart));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	PlaceCar(Car, Spot, 0.0);

	// The character stands beside the car on its door side (-Y), with a controller.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, Spot + FVector2D(0.0, -NextToCm));
	ADIAPlayerController* Controller = Character ? Possess(World, Character) : nullptr;
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the controller finds the car"), Controller->FindInteractable() == Cast<IDIAInteractable>(Car));
	TestEqual(TEXT("it offers the way in"), Car->GetPrompt(Character).ToString(), FString(TEXT("E – Get in")));

	// With the cart in the hands E lets the cart go and never gets into the car; a second E (the hands are free) does.
	ADIAMovable* Cart = nullptr;
	for (TActorIterator<ADIAMovable> It(World); It; ++It)
	{
		Cart = *It;
	}
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	Cart->Interact(Character);
	TestTrue(TEXT("the cart is held"), Game.GetHeld() == ELotMovable::Cart);
	TestTrue(TEXT("a held thing takes the offer"), Controller->FindInteractable() == Cast<IDIAInteractable>(Cart));
	TestTrue(TEXT("the car offers nothing while the hands are full"), Car->GetPrompt(Character).IsEmpty());
	Controller->OnInteract();
	TestTrue(TEXT("E let the cart go"), Game.GetHeld() == ELotMovable::Count);
	TestFalse(TEXT("and did not get into the car"), Game.World().bPlayerInCar);
	TestTrue(TEXT("the character is still the pawn"), Controller->GetPawn() == Character);

	// Walk round the car, away from where the cart stands.
	Character->SetActorLocation(FVector(Spot.X - 100.0, Spot.Y + NextToCm, DIACharacter::CapsuleHalfHeightCm));
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the car is offered again"), Controller->FindInteractable() == Cast<IDIAInteractable>(Car));

	// Without a character nobody gets in.
	Car->Interact(nullptr);
	TestFalse(TEXT("no character, nobody gets in"), Game.World().bPlayerInCar);

	// E: the controller possesses the car, the character is hidden, out of collision and carried along.
	Controller->OnInteract();
	TestTrue(TEXT("the car is the pawn"), Controller->GetPawn() == Car);
	TestTrue(TEXT("the core knows the player is in the car"), Game.World().bPlayerInCar);
	TestTrue(TEXT("the car has a driver"), Car->HasDriver());
	TestTrue(TEXT("the character is hidden"), Character->IsHidden());
	TestFalse(TEXT("the character does not collide"), Character->GetActorEnableCollision());
	TestTrue(TEXT("the character rides along"), Character->GetAttachParentActor() == Car);
	TestEqual(TEXT("it offers the way out"), Car->GetPrompt(nullptr).ToString(), FString(TEXT("E – Get out")));
	TestTrue(TEXT("the controller offers the way out too"), Controller->FindInteractable() == Cast<IDIAInteractable>(Car));

	// Nothing can be grabbed from the car.
	TestFalse(TEXT("the core refuses to grab while driving"), Game.Grab(ELotMovable::Cart));

	// Driving through the controller's input: forward and pour; releasing the keys stops it.
	Controller->OnMove(FInputActionValue(FVector2D(0.0, 1.0)));
	Controller->OnUseStarted();
	TestTrue(TEXT("the button pours the car"), Car->IsPouring());
	TickFor(Wrapper, 1.0);
	TestTrue(TEXT("W drives it forward"), Car->GetActorLocation().X > Spot.X + 10.0);
	TestTrue(TEXT("the core sees the car where the player is"), Game.World().PlayerPos.Equals(Where(Car), 1.0));
	TestTrue(TEXT("and the car where it is"), Game.World().CarPos.Equals(Where(Car), 1.0));
	Controller->OnMoveStopped();
	Controller->OnUseCompleted();
	TestFalse(TEXT("releasing the button stops the pouring"), Car->IsPouring());
	TickFor(Wrapper, 1.0);
	TestTrue(TEXT("without throttle the car stops"), FMath::Abs(Car->GetSpeed()) < 1.0);

	// The hand panel does not show a stale vessel while driving.
	FDIAHudModel Hud;
	Hud.Update(Game);
	TestEqual(TEXT("the hand panel says Car"), Hud.GetView().HandLabel.ToString(), FString(TEXT("Car")));

	// E again: the player is at a free spot beside the car, the controller has the character back.
	Controller->OnInteract();
	TestTrue(TEXT("the character is the pawn again"), Controller->GetPawn() == Character);
	TestFalse(TEXT("the core knows the player is out"), Game.World().bPlayerInCar);
	TestFalse(TEXT("nobody drives"), Car->HasDriver());
	TestFalse(TEXT("the character is visible"), Character->IsHidden());
	TestTrue(TEXT("the character collides again"), Character->GetActorEnableCollision());
	TestTrue(TEXT("the character is beside the car"), FVector2D::Distance(Where(Character), Where(Car)) < ExitMaxDistanceCm);
	TestTrue(TEXT("and the core has the player there"), Game.World().PlayerPos.Equals(Where(Character), 1.0));
	return true;
}

DIA_TEST(FCarExitNeedsRoomTest, "DoItAsphalt.Game.Car.ExitNeedsRoom")

bool FCarExitNeedsRoomTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	PlaceCar(Car, Spot, 0.0);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, Spot + FVector2D(0.0, -NextToCm));
	ADIAPlayerController* Controller = Character ? Possess(World, Character) : nullptr;
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	Controller->OnInteract();
	TestTrue(TEXT("in the car"), Controller->GetPawn() == Car);

	// Walls on both sides: there is no spot to get out at.
	AActor* Left = SpawnBlocker(World, Spot + FVector2D(0.0, -DIAPaverCar::ExitSideCm()), 60.0);
	AActor* Right = SpawnBlocker(World, Spot + FVector2D(0.0, DIAPaverCar::ExitSideCm()), 60.0);
	if (!TestNotNull(TEXT("left blocker"), Left) || !TestNotNull(TEXT("right blocker"), Right))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	Controller->OnInteract();
	TestTrue(TEXT("no room: still the driver"), Controller->GetPawn() == Car);
	TestTrue(TEXT("the core still says in the car"), Game.World().bPlayerInCar);
	TestTrue(TEXT("the car still has its driver"), Car->HasDriver());

	// One side opens up: the player gets out there.
	Left->Destroy();
	Wrapper.TickTestWorld(TickSeconds);
	Controller->OnInteract();
	TestTrue(TEXT("room on one side: out"), Controller->GetPawn() == Character);
	TestTrue(TEXT("on the free (left) side"), Character->GetActorLocation().Y < Car->GetActorLocation().Y - 100.0);
	return true;
}

DIA_TEST(FCarCabinCameraTest, "DoItAsphalt.Game.Car.ThirdPersonCamera")

bool FCarCabinCameraTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car) || !TestNotNull(TEXT("the camera"), Car->GetCamera()) || !TestNotNull(TEXT("the boom"), Car->GetBoom()))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	PlaceCar(Car, Spot, 90.0);
	TestEqual(TEXT("the boom is the planned length"), Car->GetBoom()->TargetArmLength, static_cast<float>(DIAPaverCar::ArmLengthCm), 0.5f);
	TestFalse(TEXT("the camera does not follow the control rotation"), Car->GetCamera()->bUsePawnControlRotation);
	TestFalse(TEXT("nor does the boom"), Car->GetBoom()->bUsePawnControlRotation);
	// Facing +Y: the camera is behind the car (smaller Y), well above the ground.
	const FVector Eye = Car->GetCamera()->GetComponentLocation();
	TestTrue(TEXT("the camera is behind the car"), Eye.Y < Spot.Y - 100.0);
	TestTrue(TEXT("and above the ground"), Eye.Z > 100.0);
	return true;
}

DIA_TEST(FCarPaveSpeedCapTest, "DoItAsphalt.Game.Car.PaveSpeedCap")

bool FCarPaveSpeedCapTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	Game.DebugSetPlantLevel(5);
	Game.DebugSetPlantStock(Game.GetPlantStorage());
	Game.DebugFillTanks(); // the car pours from its tank

	// The numbers of the brief are the numbers of the balance, or this test has nothing to say.
	if (!TestEqual(TEXT("tier 1 pours at 144.4 cm/s at most"), LotBalance::CarPaveSpeedCmS(0), 144.4, 0.1)
		|| !TestEqual(TEXT("tier 2 pours at 138.9 cm/s at most"), LotBalance::CarPaveSpeedCmS(1), 138.9, 0.1)
		|| !TestEqual(TEXT("tier 3 pours at 150.8 cm/s at most"), LotBalance::CarPaveSpeedCmS(2), 150.8, 0.1)
		|| !TestEqual(TEXT("tier 4 pours at 160 cm/s at most"), LotBalance::CarPaveSpeedCmS(3), 160.0, 0.1))
	{
		return false;
	}

	for (const int32 Level : {0, 1, 2, 3})
	{
		TestTrue(TEXT("grant the tier"), GrantTier(Subsystem, Level));
		const double Cap = LotBalance::CarPaveSpeedCmS(Level);
		const FString Name = FString::Printf(TEXT("tier %d"), Level + 1);
		PlaceCar(Car, MiddleOfLot(), 0.0);
		Car->SetThrottle(1.f);
		Car->SetPouring(true);
		double Fastest = 0.0;
		for (int32 I = 0; I < 30; ++I)
		{
			Wrapper.TickTestWorld(TickSeconds);
			Fastest = FMath::Max(Fastest, FMath::Abs(Car->GetSpeed()));
		}
		TestTrue(*(Name + TEXT(": the speed never exceeds the pouring speed")), Fastest <= Cap + 1e-6);
		TestTrue(*(Name + TEXT(": it does reach it")), Fastest > Cap - 1.0);

		// Without pouring the car goes to its driving speed.
		Car->SetPouring(false);
		PlaceCar(Car, MiddleOfLot(), 0.0);
		TickFor(Wrapper, 1.5);
		TestEqual(*(Name + TEXT(": driving speed without pouring")), Car->GetSpeed(), LotBalance::CarSpeedCmS, 0.5);
		Car->SetThrottle(0.f);
		TickFor(Wrapper, 1.0);
	}
	return true;
}

DIA_TEST(FCarPavesThreeRowsTest, "DoItAsphalt.Game.Car.PavesThreeRows")

bool FCarPavesThreeRowsTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	Game.DebugSetPlantLevel(5);
	Game.DebugSetPlantStock(Game.GetPlantStorage());
	Game.DebugFillTanks(); // the car pours from its tank
	TestEqual(TEXT("nothing is paved yet"), Game.GetCompletedCells(), 0);

	// The bar starts on a cell edge along a row centre; the 3 m bar covers three rows.
	const FVector2D RowCentre = MiddleOfLot();
	const double StartX = RowCentre.X - AsphaltLot::CellCm * 0.5;
	PlaceCar(Car, FVector2D(StartX + LotBalance::CarBarBehindCm[0], RowCentre.Y), 0.0);
	Car->SetThrottle(1.f);
	Car->SetPouring(true);
	for (int32 I = 0; I < 400 && Car->GetBarPoint().X < StartX + 1000.0; ++I)
	{
		Wrapper.TickTestWorld(TickSeconds);
	}
	Car->SetPouring(false);
	Car->SetThrottle(0.f);
	TestTrue(TEXT("the bar went 10 m"), Car->GetBarPoint().X >= StartX + 1000.0);
	const int32 Cells = Game.GetCompletedCells();
	TestTrue(*FString::Printf(TEXT("about 30 cells are paved (got %d)"), Cells), FMath::Abs(Cells - 30) <= 3);
	return true;
}

DIA_TEST(FCarNoPaveAcrossAJumpTest, "DoItAsphalt.Game.Car.NoPaveAcrossAJump")

bool FCarNoPaveAcrossAJumpTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	Game.DebugSetPlantLevel(5);
	Game.DebugSetPlantStock(Game.GetPlantStorage());
	Game.DebugFillTanks(); // the car pours from its tank
	const FVector2D RowCentre = MiddleOfLot();
	PlaceCar(Car, RowCentre + FVector2D(LotBalance::CarBarBehindCm[0] - AsphaltLot::CellCm * 0.5, 0.0), 0.0);
	Car->SetPouring(true);
	TickFor(Wrapper, 0.5);
	TestEqual(TEXT("a car that stands still paves nothing"), CountRawOrDone(Game), 0);

	// Teleported 20 m along the row with the button down (dia.Teleport does not let go of it).
	PlaceCar(Car, Where(Car) + FVector2D(2000.0, 0.0), 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("the jump paves nothing between the two points"), CountRawOrDone(Game), 0);
	TestEqual(TEXT("and completes no cell"), Game.GetCompletedCells(), 0);

	// The stroke starts again where the car is now.
	Car->SetThrottle(1.f);
	TickFor(Wrapper, 3.0);
	TestTrue(TEXT("driving on paves again"), Game.GetCompletedCells() > 0);
	return true;
}

DIA_TEST(FCarTierMeshTest, "DoItAsphalt.Game.Car.TierMesh")

bool FCarTierMeshTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	const TCHAR* const MeshNames[4] = {TEXT("SM_PaverCar"), TEXT("SM_PaverCar_T2"), TEXT("SM_PaverCar_T3"), TEXT("SM_PaverCar_T4")};
	const FVector2D Spot = MiddleOfLot();
	for (int32 Level = 0; Level < 4; ++Level)
	{
		const FString Tag = FString::Printf(TEXT("tier %d"), Level + 1);
		// What the cheat dia.Item CarT2..CarT4 does, with the car standing in the open.
		TestTrue(*(Tag + TEXT(": grant")), GrantTier(Subsystem, Level));
		PlaceCar(Car, Spot, 30.0);
		Wrapper.TickTestWorld(TickSeconds);
		TestEqual(*(Tag + TEXT(": the core's level")), Game.GetCarLevel(), Level);
		TestEqual(*(Tag + TEXT(": the actor shows it")), Car->GetShownLevel(), Level);
		TestEqual(*(Tag + TEXT(": still exactly one car")), FindCars(World).Num(), 1);

		UStaticMeshComponent* Body = Car->GetBodyMesh();
		TestNotNull(*(Tag + TEXT(": a model")), Body->GetStaticMesh().Get());
		TestEqual(*(Tag + TEXT(": the model of the tier")), Body->GetStaticMesh() ? Body->GetStaticMesh()->GetName() : FString(), FString(MeshNames[Level]));
		TestTrue(*(Tag + TEXT(": it is the real model, not the placeholder")), Body->GetStaticMesh() && !Body->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/")));
		if (Body->GetStaticMesh())
		{
			const FVector Size = Body->GetStaticMesh()->GetBounds().BoxExtent * 2.0;
			TestTrue(*(Tag + TEXT(": the model is the size of the table")), Size.Equals(DIAModelSizes::PaverCarCm[Level], 6.0));
		}
		TestTrue(*(Tag + TEXT(": the mesh has no scale of a placeholder")), Body->GetRelativeScale3D().Equals(FVector::OneVector, 0.001));

		// The collision box is the body of the tier, and the box stands on the ground, so that the wheels do.
		const FVector Extent = Car->GetCollision()->GetUnscaledBoxExtent();
		TestTrue(*(Tag + TEXT(": the box is the body of the tier")), (Extent * 2.0).Equals(DIAModelSizes::PaverCarBodyCm[Level], 0.01));
		TestEqual(*(Tag + TEXT(": the actor's height is half the box and the lift")), Car->GetActorLocation().Z, DIAPaverCar::OriginHeightCm(Level), 0.01);
		TestEqual(*(Tag + TEXT(": the model stands on the ground")), Body->GetComponentLocation().Z, 0.0, 0.01);
		TestEqual(*(Tag + TEXT(": the bottom of the box is just above the ground")), Car->GetActorLocation().Z - Extent.Z, DIAPaverCar::GroundLiftCm, 0.01);
		TestTrue(*(Tag + TEXT(": it kept its place")), Where(Car).Equals(Spot, 0.5) && Game.World().CarPos.Equals(Spot, 0.5));

		// The bar is behind the centre by the tier's distance, which is half the model's length: it is the rear face.
		const FVector Forward = Car->GetActorForwardVector();
		const FVector2D Bar = Car->GetBarPoint();
		TestEqual(*(Tag + TEXT(": the bar is the tier's distance behind")), FVector2D::Distance(Bar, Spot), LotBalance::CarBarBehindCm[Level], 0.01);
		TestTrue(*(Tag + TEXT(": behind, not in front")), FVector2D::DotProduct(Bar - Spot, FVector2D(Forward.X, Forward.Y)) < 0.0);
		TestEqual(*(Tag + TEXT(": the bar sits at the rear face of the model")), LotBalance::CarBarBehindCm[Level], DIAModelSizes::PaverCarCm[Level].X * 0.5, 0.01);

		// The camera boom keeps the framing: longer and higher with the truck.
		TestEqual(*(Tag + TEXT(": the boom")), Car->GetBoom()->TargetArmLength, static_cast<float>(DIAPaverCar::ArmLengthFor(Level)), 0.5f);
		TestEqual(*(Tag + TEXT(": the pivot is above the ground by the tier's height")), Car->GetBoom()->GetComponentLocation().Z, DIAPaverCar::PivotHeightFor(Level), 0.5);
		TestEqual(*(Tag + TEXT(": exit side is the body's half width and the margin")), DIAPaverCar::ExitSideCm(Level), DIAModelSizes::PaverCarBodyCm[Level].Y * 0.5 + DIAPaverCar::ExitMarginCm, 0.01);
	}
	TestEqual(TEXT("the first tier keeps its boom"), DIAPaverCar::ArmLengthFor(0), DIAPaverCar::ArmLengthCm, 0.001);
	TestEqual(TEXT("and its exit side"), DIAPaverCar::ExitSideCm(0), 200.0, 0.001);
	return true;
}

DIA_TEST(FCarTierBarPointTest, "DoItAsphalt.Game.Car.TierBarPoint")

bool FCarTierBarPointTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	const FVector2D Spot = MiddleOfLot();
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	for (int32 Level = 0; Level < 4; ++Level)
	{
		TestTrue(TEXT("grant the tier"), GrantTier(Subsystem, Level));
		for (const double Yaw : {0.0, 90.0, -135.0})
		{
			PlaceCar(Car, Spot, Yaw);
			const FVector2D Back = -FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
			TestTrue(*FString::Printf(TEXT("tier %d, yaw %.0f: the bar point"), Level + 1, Yaw), Car->GetBarPoint().Equals(Spot + Back * LotBalance::CarBarBehindCm[Level], 0.01));
		}
	}
	return true;
}

DIA_TEST(FCarTierPavesItsWidthTest, "DoItAsphalt.Game.Car.TierPavesItsWidth")

bool FCarTierPavesItsWidthTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	Game.DebugSetPlantLevel(5);
	Game.DebugSetPlantStock(Game.GetPlantStorage());
	for (const int32 Level : {1, 3})
	{
		TestTrue(TEXT("grant the tier"), GrantTier(Subsystem, Level));
		Game.DebugFillTanks();
		const FVector2D RowCentre = FVector2D(MiddleOfLot().X, MiddleOfLot().Y + 3000.0 * (Level - 1));
		// The bar starts on a cell edge along a row centre and goes 10 m: the bar of the tier is as wide as its table says.
		const double StartX = RowCentre.X - AsphaltLot::CellCm * 0.5;
		const int32 Before = Game.GetCompletedCells();
		PlaceCar(Car, FVector2D(StartX + LotBalance::CarBarBehindCm[Level], RowCentre.Y), 0.0);
		Car->SetThrottle(1.f);
		Car->SetPouring(true);
		for (int32 I = 0; I < 400 && Car->GetBarPoint().X < StartX + 1000.0; ++I)
		{
			Wrapper.TickTestWorld(TickSeconds);
		}
		Car->SetPouring(false);
		Car->SetThrottle(0.f);
		const int32 Rows = FMath::FloorToInt(LotBalance::CarWidthCm[Level] / AsphaltLot::CellCm);
		const int32 Cells = Game.GetCompletedCells() - Before;
		TestTrue(*FString::Printf(TEXT("tier %d: about %d cells (got %d)"), Level + 1, Rows * 10, Cells), FMath::Abs(Cells - Rows * 10) <= 3 * Rows);
	}
	return true;
}

DIA_TEST(FCarUpgradeMovesWhenBlockedTest, "DoItAsphalt.Game.Car.UpgradeMovesWhenBlocked")

bool FCarUpgradeMovesWhenBlockedTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	PlaceCar(Car, Spot, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	// A post just beyond the nose of the plain car (half length 230): the second tier (half length 280) would be inside it.
	AActor* Post = SpawnBlocker(World, Spot + FVector2D(270.0, 0.0), 30.0);
	if (!TestNotNull(TEXT("the post"), Post))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the plain car stands free next to it"), Where(Car).Equals(Spot, 0.5));

	TestTrue(TEXT("grant the second tier"), Grant(Subsystem, ELotItem::CarT2));
	Wrapper.TickTestWorld(TickSeconds);
	const FVector2D Spawn = LotLayout::ItemSpawn(ELotItem::Car);
	TestTrue(TEXT("the car is not stuck in the post: it is at its spawn"), Where(Car).Equals(Spawn, 0.5));
	TestTrue(TEXT("and the core's position is in step"), Game.World().CarPos.Equals(Spawn, 0.5));
	TestEqual(TEXT("on the ground of its tier"), Car->GetActorLocation().Z, DIAPaverCar::OriginHeightCm(1), 0.01);
	TestEqual(TEXT("level 2 shown"), Car->GetShownLevel(), 1);

	// With room it stays: the same upgrade next in a free place does not teleport.
	Post->Destroy();
	PlaceCar(Car, Spot, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("grant the third tier"), Grant(Subsystem, ELotItem::CarT3));
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("a free car stays where it stands"), Where(Car).Equals(Spot, 0.5));
	return true;
}

DIA_TEST(FCarSpawnHasRoomForBiggestTest, "DoItAsphalt.Game.Car.SpawnHasRoomForBiggest")

bool FCarSpawnHasRoomForBiggestTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	// Everything the world holds is standing; the biggest truck fits at the car's spawn, and that spot is the one an upgrade falls back to.
	const FVector2D Spawn = LotLayout::ItemSpawn(ELotItem::Car);
	const FVector Body = DIAModelSizes::PaverCarBodyCm[3];
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CarSpawnRoom), false);
	const FCollisionShape Box = FCollisionShape::MakeBox(Body * 0.5);
	TestFalse(TEXT("the 820 x 310 cm box overlaps nothing at the spawn"),
		World->OverlapBlockingTestByChannel(FVector(Spawn.X, Spawn.Y, DIAPaverCar::OriginHeightCm(3)), FQuat::Identity, ECC_Pawn, Box, Params));
	return true;
}

DIA_TEST(FCarTierSurvivesSaveTest, "DoItAsphalt.Game.Car.TierSurvivesSave")

bool FCarTierSurvivesSaveTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant tier 3"), GrantTier(Subsystem, 2));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	TestEqual(TEXT("tier 3 shown"), Car->GetShownLevel(), 2);
	const TArray<uint8> Saved = Game.SaveToBytes();

	// A new game takes the car away (and the slot); buying the plain car again shows tier 1, and loading the saved game brings tier 3 back.
	Subsystem->NewGame();
	TestTrue(TEXT("write the slot"), UDIAWorldSubsystem::WriteSlot(FCarTestSlot::Name(), Saved));
	TestEqual(TEXT("no car after a new game"), FindCars(World).Num(), 0);
	TestTrue(TEXT("buy the plain car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Fresh = TheCar(World);
	if (!TestNotNull(TEXT("the new car"), Fresh))
	{
		return false;
	}
	TestEqual(TEXT("it is tier 1"), Fresh->GetShownLevel(), 0);
	TestEqual(TEXT("its box is the plain body"), (Fresh->GetCollision()->GetUnscaledBoxExtent() * 2.0).X, DIAModelSizes::PaverCarBodyCm[0].X, 0.01);

	if (!TestTrue(TEXT("load"), Subsystem->LoadGame()))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	ADIAPaverCar* Loaded = TheCar(World);
	if (!TestNotNull(TEXT("the loaded car"), Loaded))
	{
		return false;
	}
	TestEqual(TEXT("loaded level"), Game.GetCarLevel(), 2);
	TestEqual(TEXT("tier 3 shown after the load"), Loaded->GetShownLevel(), 2);
	TestEqual(TEXT("its mesh"), Loaded->GetBodyMesh()->GetStaticMesh() ? Loaded->GetBodyMesh()->GetStaticMesh()->GetName() : FString(), FString(TEXT("SM_PaverCar_T3")));
	TestEqual(TEXT("its box"), (Loaded->GetCollision()->GetUnscaledBoxExtent() * 2.0).X, DIAModelSizes::PaverCarBodyCm[2].X, 0.01);
	TestEqual(TEXT("it stands on the ground"), Loaded->GetActorLocation().Z, DIAPaverCar::OriginHeightCm(2), 0.01);
	return true;
}

DIA_TEST(FCarCabinViewTest, "DoItAsphalt.Game.Car.CabinView")

bool FCarCabinViewTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	PlaceCar(Car, Spot, 0.0);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, Spot + FVector2D(0.0, -NextToCm));
	ADIAPlayerController* Controller = Character ? Possess(World, Character) : nullptr;
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);

	// On foot the car is seen from outside, with its glass.
	CheckCabinView(*this, Car, Controller, Character, TEXT("on foot"));

	Controller->OnInteract();
	if (!TestTrue(TEXT("the player drives"), Car->HasDriver()))
	{
		return false;
	}
	CheckCabinView(*this, Car, Controller, Car, TEXT("driving"));

	// Out again: the exterior is what it was.
	Controller->OnInteract();
	TestFalse(TEXT("nobody drives"), Car->HasDriver());
	CheckCabinView(*this, Car, Controller, Character, TEXT("after getting out"));

	// A new game while driving: the car is taken away and the player is on foot with a plain view.
	Controller->OnInteract();
	TestTrue(TEXT("the player drives again"), Car->HasDriver());
	Subsystem->NewGame();
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("a new game takes the car away"), FindCars(World).Num() == 0);
	TestTrue(TEXT("the view is the character's again"), Controller->GetViewTarget() == Character);

	// A load with the in-car flag: the car is driven again, with the cabin view.
	TestTrue(TEXT("grant the car again"), Grant(Subsystem, ELotItem::Car));
	Car = TheCar(World);
	if (!TestNotNull(TEXT("the car again"), Car))
	{
		return false;
	}
	PlaceCar(Car, Spot, 0.0);
	Character->SetActorLocation(FVector(Spot.X, Spot.Y - NextToCm, DIACharacter::CapsuleHalfHeightCm));
	Wrapper.TickTestWorld(TickSeconds);
	Controller->OnInteract();
	TestTrue(TEXT("driving before the save"), Car->HasDriver());
	Subsystem->SaveGame();
	TestTrue(TEXT("the game loads"), Subsystem->LoadGame());
	Wrapper.TickTestWorld(TickSeconds);
	Car = TheCar(World);
	if (!TestNotNull(TEXT("the car after the load"), Car))
	{
		return false;
	}
	TestTrue(TEXT("the load keeps the player at the wheel"), Car->HasDriver());
	CheckCabinView(*this, Car, Controller, Car, TEXT("after a load while driving"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
