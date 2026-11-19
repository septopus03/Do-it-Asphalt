// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "InputActionValue.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "World/DIAPaverCar.h"

namespace
{
	constexpr int32 CarRoad = 0;
	constexpr float CarTickSeconds = 0.1f;
	constexpr int32 CarTicksPerSecond = 10;
	constexpr double CarToleranceCm = 1.0;
	/** The plant stock is filled by one minute of a level 12 plant: 1000 l. */
	constexpr int32 CarPlantLevel = 12;
	constexpr double CarPlantFillSeconds = 60.0;
	/** The truck's centre this far from the refill point is inside the refill radius plus 200 cm, and clear of the plant. */
	constexpr double CarAtPlantCm = 350.0;
	constexpr double CarOutsideRefillCm = 700.0;
	constexpr double CarExitMaxDistanceCm = 250.0;
	constexpr double CarNextToCm = 200.0;
	/** The plant's collision box reaches this far towards the lot (its centre is at X = -2000 and it is 600 cm deep). */
	constexpr double CarPlantFaceX = -1700.0;

	TArray<ADIAPaverCar*> FindCars(UWorld* World)
	{
		TArray<ADIAPaverCar*> Result;
		for (TActorIterator<ADIAPaverCar> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		return Result;
	}

	/** What the cheat dia.Special does: grants the special for free and announces it like a purchase. */
	ADIAPaverCar* GrantCar(UWorld* World, UDIAWorldSubsystem* Subsystem)
	{
		Subsystem->GetGame().DebugGrantSpecial(EAsphaltSpecial::PaverCar);
		Subsystem->OnSpecialBought.Broadcast(EAsphaltSpecial::PaverCar);
		const TArray<ADIAPaverCar*> Cars = FindCars(World);
		return Cars.Num() == 1 ? Cars[0] : nullptr;
	}

	/** Puts the truck somewhere without a sweep, standing still and facing YawDeg. */
	void PlaceCar(ADIAPaverCar* Car, FVector2D Where, double YawDeg)
	{
		Car->SetActorLocationAndRotation(FVector(Where.X, Where.Y, DIAPaverCar::OriginHeightCm()), FRotator(0.0, YawDeg, 0.0));
	}

	void TickSeconds(FTestWorldWrapper& Wrapper, double Seconds)
	{
		const int32 Ticks = FMath::RoundToInt(Seconds * CarTicksPerSecond);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Wrapper.TickTestWorld(CarTickSeconds);
		}
	}

	FVector2D CarWhere(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}
}

DIA_TEST(FCarSpawnTest, "DoItAsphalt.Game.CarSpawn")

bool FCarSpawnTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	FAsphaltGame& Game = Subsystem->GetGame();

	TestEqual(TEXT("no truck before the special is owned"), FindCars(World).Num(), 0);
	Mode->EnsureCar();
	TestEqual(TEXT("EnsureCar spawns nothing without the special"), FindCars(World).Num(), 0);

	ADIAPaverCar* Car = GrantCar(World, Subsystem);
	if (!TestNotNull(TEXT("one truck after the purchase"), Car))
	{
		return false;
	}
	TestTrue(TEXT("it stands at the spawn point of its road"), CarWhere(Car).Equals(AsphaltLayout::SpecialSpawn(CarRoad), CarToleranceCm));
	TestTrue(TEXT("the same as the core's position"), CarWhere(Car).Equals(Game.World().CarPos, CarToleranceCm));
	TestEqual(TEXT("its box rests on the ground"), Car->GetActorLocation().Z - Car->GetCollision()->GetUnscaledBoxExtent().Z, DIAPaverCar::GroundLiftCm);
	TestTrue(TEXT("the collision box is 460 x 220 x 240"), Car->GetCollision()->GetUnscaledBoxExtent().Equals(FVector(230.0, 110.0, 120.0)));
	TestTrue(TEXT("it blocks"), Car->GetCollision()->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	TestTrue(TEXT("it has a mesh"), Car->GetBodyMesh()->GetStaticMesh() != nullptr);
	TestTrue(TEXT("the mesh is visible"), Car->GetBodyMesh()->IsVisible());
	TestFalse(TEXT("nobody drives it"), Car->HasDriver());

	// Its prompt is the way in, and a second announcement never makes a second truck.
	TestEqual(TEXT("prompt"), Car->GetPrompt(nullptr).ToString(), DIAFormat::CarPrompt(false).ToString());
	TestEqual(TEXT("prompt text"), Car->GetPrompt(nullptr).ToString(), FString(TEXT("E – Get in")));
	Subsystem->OnSpecialBought.Broadcast(EAsphaltSpecial::PaverCar);
	Mode->EnsureCar();
	TestEqual(TEXT("still one truck"), FindCars(World).Num(), 1);

	// The bar is 230 cm behind the centre, wherever the truck faces.
	PlaceCar(Car, FVector2D(100.0, 200.0), 0.0);
	TestTrue(TEXT("bar behind a truck facing +X"), Car->GetBarPoint().Equals(FVector2D(100.0 - AsphaltBalance::CarBarBehindCm, 200.0), CarToleranceCm));
	PlaceCar(Car, FVector2D(100.0, 200.0), 90.0);
	TestTrue(TEXT("bar behind a truck facing +Y"), Car->GetBarPoint().Equals(FVector2D(100.0, 200.0 - AsphaltBalance::CarBarBehindCm), CarToleranceCm));

	// A truck that is gone (stale pointer) is spawned again where the core says, once.
	const FVector2D Where = Game.World().CarPos;
	Car->Destroy();
	Mode->EnsureCar();
	Mode->EnsureCar();
	const TArray<ADIAPaverCar*> Again = FindCars(World);
	if (TestEqual(TEXT("respawned once"), Again.Num(), 1))
	{
		TestTrue(TEXT("at the core's position"), CarWhere(Again[0]).Equals(Where, CarToleranceCm));
	}
	return true;
}

DIA_TEST(FCarDrivesTest, "DoItAsphalt.Game.CarDrives")

bool FCarDrivesTest::RunTest(const FString& Parameters)
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
	ADIAPaverCar* Car = GrantCar(World, Subsystem);
	if (!TestNotNull(TEXT("truck"), Car))
	{
		return false;
	}

	// Forward: it reaches 600 cm/s and writes where it is into the core every frame.
	PlaceCar(Car, FVector2D(0.0, 0.0), 0.0);
	Car->SetThrottle(1.f);
	TickSeconds(Wrapper, 3.0);
	TestEqual(TEXT("full speed forward"), Car->GetSpeed(), AsphaltBalance::CarSpeedCmS, CarToleranceCm);
	TestTrue(TEXT("it drove along +X"), Car->GetActorLocation().X > AsphaltBalance::CarSpeedCmS);
	TestEqual(TEXT("and stayed on its line"), Car->GetActorLocation().Y, 0.0, CarToleranceCm);
	TestTrue(TEXT("the core knows where it is"), Game.World().CarPos.Equals(CarWhere(Car), CarToleranceCm));
	TestEqual(TEXT("and where it faces"), Game.World().CarYawDeg, Car->GetActorRotation().Yaw, CarToleranceCm);

	// Reversing is half as fast.
	Car->SetThrottle(-1.f);
	TickSeconds(Wrapper, 3.0);
	TestEqual(TEXT("reverse is half the speed"), Car->GetSpeed(), -AsphaltBalance::CarSpeedCmS * DIAPaverCar::ReverseSpeedFactor, CarToleranceCm);

	// Without throttle it stops; a standing truck does not turn even with the wheel over.
	Car->SetThrottle(0.f);
	Car->SetSteer(1.f);
	TickSeconds(Wrapper, 2.0);
	TestEqual(TEXT("it stopped"), Car->GetSpeed(), 0.0, CarToleranceCm);
	const double YawStanding = Car->GetActorRotation().Yaw;
	TickSeconds(Wrapper, 1.0);
	TestEqual(TEXT("standing it does not turn"), Car->GetActorRotation().Yaw, YawStanding, 0.01);

	// Steering right at full speed turns it at 90 degrees per second (it is at full speed after 3 s).
	PlaceCar(Car, FVector2D(0.0, 0.0), 0.0);
	Car->SetThrottle(1.f);
	Car->SetSteer(0.f);
	TickSeconds(Wrapper, 1.0);
	const double YawBefore = Car->GetActorRotation().Yaw;
	Car->SetSteer(1.f);
	TickSeconds(Wrapper, 0.1);
	TestTrue(TEXT("steering right turns it clockwise"), FMath::FindDeltaAngleDegrees(YawBefore, Car->GetActorRotation().Yaw) > 0.0);
	TestTrue(TEXT("by at most 90 degrees per second"), FMath::FindDeltaAngleDegrees(YawBefore, Car->GetActorRotation().Yaw) <= DIAPaverCar::TurnRateDegPerSec * CarTickSeconds + 0.01);

	// The plant stops it: driving at it from the lot it stays in front of its collision (centre of the truck 230 cm from the face).
	PlaceCar(Car, FVector2D(-1000.0, 0.0), 180.0);
	Car->SetSteer(0.f);
	Car->SetThrottle(1.f);
	TickSeconds(Wrapper, 4.0);
	TestTrue(TEXT("it moved towards the plant"), Car->GetActorLocation().X < -1000.0 - CarToleranceCm);
	TestTrue(TEXT("but not into it"), Car->GetActorLocation().X >= CarPlantFaceX + Car->GetCollision()->GetUnscaledBoxExtent().X - CarToleranceCm);
	return true;
}

DIA_TEST(FCarPoursWideTest, "DoItAsphalt.Game.CarPoursWide")

bool FCarPoursWideTest::RunTest(const FString& Parameters)
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

	// A plant that holds more than the tank, and a minute of its production.
	Game.DebugSetLevel(EAsphaltUpgrade::Plant, CarPlantLevel);
	Game.Tick(CarPlantFillSeconds);
	TestTrue(TEXT("the plant holds at least the tank"), Game.GetPlantStock() >= AsphaltBalance::CarTankL);

	ADIAPaverCar* Car = GrantCar(World, Subsystem);
	if (!TestNotNull(TEXT("truck"), Car))
	{
		return false;
	}
	TestEqual(TEXT("the tank is empty"), Game.GetCarLitres(), 0.0);

	// Too far from the refill point: nothing is taken from the plant.
	PlaceCar(Car, AsphaltLayout::RefillCenter + FVector2D(CarOutsideRefillCm, 0.0), 0.0);
	TickSeconds(Wrapper, 3.0);
	TestEqual(TEXT("outside the zone the tank stays empty"), Game.GetCarLitres(), 0.0);

	// 350 cm from the refill point is within 200 + 200: 3 s fill the 500 l tank (the backpack is not the character's here, nobody is in it).
	PlaceCar(Car, AsphaltLayout::RefillCenter + FVector2D(CarAtPlantCm, 0.0), 0.0);
	TickSeconds(Wrapper, 3.0);
	TestEqual(TEXT("the tank is full"), Game.GetCarLitres(), AsphaltBalance::CarTankL, CarToleranceCm);

	// Pouring while driving across the middle of the lot: 300 cm wide, from the tank.
	PlaceCar(Car, FVector2D::ZeroVector, 0.0);
	const double PouredBefore = Game.GetTotalLitresPoured();
	Car->SetPouring(true);
	Car->SetThrottle(1.f);
	TickSeconds(Wrapper, 2.0);
	TestTrue(TEXT("it poured more than 100 l"), Game.GetTotalLitresPoured() - PouredBefore > 100.0);
	TestTrue(TEXT("from the car tank"), Game.GetCarLitres() < AsphaltBalance::CarTankL);
	TestTrue(TEXT("the core's position is the actor's"), Game.World().CarPos.Equals(CarWhere(Car), CarToleranceCm));

	// Standing still it lays nothing (the stamp needs a direction), and with the button up nothing either.
	Car->SetThrottle(0.f);
	TickSeconds(Wrapper, 1.0);
	const double LitresStanding = Game.GetCarLitres();
	TickSeconds(Wrapper, 1.0);
	TestEqual(TEXT("a standing truck pours nothing"), Game.GetCarLitres(), LitresStanding);
	Car->SetPouring(false);
	Car->SetThrottle(1.f);
	TickSeconds(Wrapper, 1.0);
	TestEqual(TEXT("without the button nothing is poured"), Game.GetCarLitres(), LitresStanding);
	return true;
}

DIA_TEST(FCarEnterExitTest, "DoItAsphalt.Game.CarEnterExit")

bool FCarEnterExitTest::RunTest(const FString& Parameters)
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
	ADIAPaverCar* Car = GrantCar(World, Subsystem);
	if (!TestNotNull(TEXT("truck"), Car))
	{
		return false;
	}
	PlaceCar(Car, FVector2D::ZeroVector, 0.0);

	// The character stands beside the truck on its door side (-Y), with a controller.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(0.0, -CarNextToCm));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}
	Controller->Possess(Character);
	TestTrue(TEXT("the character is the pawn"), Controller->GetPawn() == Character);
	TestTrue(TEXT("the controller finds the truck"), Controller->FindInteractable() == Cast<IDIAInteractable>(Car));
	TestEqual(TEXT("it offers the way in"), Car->GetPrompt(Character).ToString(), FString(TEXT("E – Get in")));

	// Without a controller there is nobody to put into the truck.
	Car->Interact(nullptr);
	TestFalse(TEXT("no character, nobody gets in"), Game.World().bPlayerInCar);

	// E: the controller possesses the truck, the character is hidden, out of collision and carried along.
	Controller->OnInteract();
	TestTrue(TEXT("the truck is the pawn"), Controller->GetPawn() == Car);
	TestTrue(TEXT("the core knows the player is in the truck"), Game.World().bPlayerInCar);
	TestTrue(TEXT("the truck has a driver"), Car->HasDriver());
	TestTrue(TEXT("the character is hidden"), Character->IsHidden());
	TestFalse(TEXT("the character does not collide"), Character->GetActorEnableCollision());
	TestTrue(TEXT("the character rides along"), Character->GetAttachParentActor() == Car);
	TestEqual(TEXT("it offers the way out"), Car->GetPrompt(nullptr).ToString(), FString(TEXT("E – Get out")));

	// A second E from outside does not put anybody else in.
	Car->Interact(Character);
	TestTrue(TEXT("still the same driver"), Controller->GetPawn() == Car);

	// Driving through the controller's input: forward and pour; releasing the keys stops it.
	Controller->OnMove(FInputActionValue(FVector2D(0.0, 1.0)));
	Controller->OnPourStarted();
	TestTrue(TEXT("the button pours the truck"), Car->IsPouring());
	TickSeconds(Wrapper, 1.0);
	TestTrue(TEXT("W drives it forward"), Car->GetActorLocation().X > 0.0);
	TestTrue(TEXT("the character moved with it"), CarWhere(Character).Equals(CarWhere(Car), CarNextToCm));
	Controller->OnMoveStopped();
	Controller->OnPourCompleted();
	TickSeconds(Wrapper, 2.0);
	TestEqual(TEXT("without the keys it stands"), Car->GetSpeed(), 0.0, CarToleranceCm);
	TestFalse(TEXT("and does not pour"), Car->IsPouring());

	// E again: the truck is stopped, the character stands beside it on the left (-Y), clear of the truck, and is the pawn again.
	Controller->OnMove(FInputActionValue(FVector2D(0.0, 1.0)));
	Controller->OnInteract();
	TestTrue(TEXT("the character is the pawn again"), Controller->GetPawn() == Character);
	TestFalse(TEXT("the core knows the player is out"), Game.World().bPlayerInCar);
	TestFalse(TEXT("the truck has no driver"), Car->HasDriver());
	TestEqual(TEXT("the truck stopped"), Car->GetSpeed(), 0.0);
	TestFalse(TEXT("the character is visible"), Character->IsHidden());
	TestTrue(TEXT("the character collides again"), Character->GetActorEnableCollision());
	TestTrue(TEXT("and is on its own"), Character->GetAttachParentActor() == nullptr);

	const FVector Offset = Character->GetActorLocation() - Car->GetActorLocation();
	TestTrue(TEXT("it stands within 250 cm of the truck"), FVector2D(Offset.X, Offset.Y).Size() <= CarExitMaxDistanceCm);
	const FVector2D Local = FVector2D(Offset.X, Offset.Y).GetRotated(-Car->GetActorRotation().Yaw);
	TestTrue(TEXT("on the door side"), Local.Y < 0.0);
	TestFalse(TEXT("without overlapping the truck"), Car->GetCollision()->OverlapComponent(Character->GetActorLocation(), FQuat::Identity,
		FCollisionShape::MakeCapsule(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm)));

	// The truck keeps its place (the throttle it had when the player got out is gone) and the next E at it gets the player in again.
	const FVector2D Left = CarWhere(Car);
	TickSeconds(Wrapper, 1.0);
	TestTrue(TEXT("the truck stayed where it was left"), CarWhere(Car).Equals(Left, CarToleranceCm));
	TestTrue(TEXT("and the core agrees"), Game.World().CarPos.Equals(Left, CarToleranceCm));
	TestTrue(TEXT("it can be entered again"), Controller->FindInteractable() == Cast<IDIAInteractable>(Car));
	return true;
}
