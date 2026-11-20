// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
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
		Car->SetActorLocationAndRotation(FVector(At.X, At.Y, DIAPaverCar::OriginHeightCm()), FRotator(0.0, YawDeg, 0.0));
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

	TestTrue(TEXT("the wide bar comes on top"), Grant(Subsystem, ELotItem::CarWideBar));
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
	AActor* Left = SpawnBlocker(World, Spot + FVector2D(0.0, -DIAPaverCar::ExitSideCm), 60.0);
	AActor* Right = SpawnBlocker(World, Spot + FVector2D(0.0, DIAPaverCar::ExitSideCm), 60.0);
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

DIA_TEST(FCarCabinCameraTest, "DoItAsphalt.Game.Car.CabinCamera")

bool FCarCabinCameraTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car) || !TestNotNull(TEXT("the camera"), Car->GetCabinCamera()))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	PlaceCar(Car, Spot, 90.0);
	const FVector Eye = Car->GetCabinCamera()->GetComponentLocation();
	TestEqual(TEXT("the eyes are 190 cm high"), Eye.Z, 190.0, 0.5);
	// Facing +Y: 120 cm ahead of the centre is +Y.
	TestEqual(TEXT("120 cm ahead of the centre (X)"), Eye.X, Spot.X, 0.5);
	TestEqual(TEXT("120 cm ahead of the centre (Y)"), Eye.Y, Spot.Y + 120.0, 0.5);
	TestFalse(TEXT("first person: the camera does not follow the control rotation"), Car->GetCabinCamera()->bUsePawnControlRotation);
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

	// The numbers of the brief are the numbers of the balance, or this test has nothing to say.
	if (!TestEqual(TEXT("plain bar pours at 144.5 cm/s at most"), LotBalance::CarPaveSpeedCmS(0), 144.5, 0.1)
		|| !TestEqual(TEXT("wide bar pours at 142.9 cm/s at most"), LotBalance::CarPaveSpeedCmS(1), 142.9, 0.1))
	{
		return false;
	}

	for (const int32 Level : {0, 1})
	{
		if (Level == 1)
		{
			TestTrue(TEXT("grant the wide bar"), Grant(Subsystem, ELotItem::CarWideBar));
		}
		const double Cap = LotBalance::CarPaveSpeedCmS(Level);
		const FString Name = Level == 0 ? TEXT("plain bar") : TEXT("wide bar");
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
	TestEqual(TEXT("nothing is paved yet"), Game.GetCompletedCells(), 0);

	// The bar starts on a cell edge along a row centre; the 3 m bar covers three rows.
	const FVector2D RowCentre = MiddleOfLot();
	const double StartX = RowCentre.X - AsphaltLot::CellCm * 0.5;
	PlaceCar(Car, FVector2D(StartX + LotBalance::CarBarBehindCm, RowCentre.Y), 0.0);
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
	const FVector2D RowCentre = MiddleOfLot();
	PlaceCar(Car, RowCentre + FVector2D(LotBalance::CarBarBehindCm - AsphaltLot::CellCm * 0.5, 0.0), 0.0);
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

DIA_TEST(FCarWideBarMeshTest, "DoItAsphalt.Game.Car.WideBarMesh")

bool FCarWideBarMeshTest::RunTest(const FString& Parameters)
{
	DIA_CAR_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheCar(World);
	if (!TestNotNull(TEXT("the car"), Car) || !TestNotNull(TEXT("the wide bar component"), Car->GetWideBarMesh()))
	{
		return false;
	}
	TestFalse(TEXT("the plain car shows no wide bar"), Car->GetWideBarMesh()->IsVisible());

	// What the cheat dia.Item CarWideBar does.
	TestTrue(TEXT("grant the wide bar"), Grant(Subsystem, ELotItem::CarWideBar));
	Wrapper.TickTestWorld(TickSeconds);
	UStaticMeshComponent* Bar = Car->GetWideBarMesh();
	TestTrue(TEXT("the wide bar shows"), Bar->IsVisible());
	TestNotNull(TEXT("with a model"), Bar->GetStaticMesh().Get());
	TestEqual(TEXT("the model is the wide bar"), Bar->GetStaticMesh() ? Bar->GetStaticMesh()->GetName() : FString(), FString(TEXT("SM_PaverCar_WideBar")));
	TestTrue(TEXT("attached to the car"), Bar->GetAttachParent() != nullptr && Bar->GetOwner() == Car);
	TestTrue(TEXT("with no offset of its own"), Bar->GetRelativeLocation().IsNearlyZero(0.01) && Bar->GetRelativeRotation().IsNearlyZero(0.01));
	TestEqual(TEXT("it stands where the body stands"), Bar->GetComponentLocation().Z, Car->GetBodyMesh()->GetComponentLocation().Z, 0.01);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
