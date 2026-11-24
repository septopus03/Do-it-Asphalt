// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "LotLayout.h"
#include "Player/DIACharacter.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "World/DIAAutoPaver.h"
#include "World/DIAPaverCar.h"
#include "World/DIAPlantActor.h"
#include "World/DIAShopBuilding.h"
#include "World/DIAVehicleStation.h"

namespace
{
	constexpr float TickSeconds = 0.1f;
	/** Pawn-height centre of the sweeps of a car through the garage (the half height of the biggest truck is 170 cm). */
	constexpr double CarSweepZCm = 175.0;

	ELotShop ShopOf(int32 I) { return static_cast<ELotShop>(I); }

	/** The test slot for the length of a test: the real slot is never touched, and the override is gone afterwards. */
	struct FShopTestSlot
	{
		static const FString& Name()
		{
			static const FString SlotName(TEXT("DIA_Test_ShopBuilding"));
			return SlotName;
		}
		FShopTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(Name());
			Delete();
		}
		~FShopTestSlot()
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

	ADIAShopBuilding* BuildingOf(UWorld* World, ELotShop Kind)
	{
		for (TActorIterator<ADIAShopBuilding> It(World); It; ++It)
		{
			if (It->Kind == Kind)
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool Grant(UDIAWorldSubsystem* Subsystem, ELotItem Item)
	{
		Subsystem->GetGame().DebugGrantItem(Item);
		Subsystem->OnItemBought.Broadcast(Item);
		Subsystem->OnStateChanged.Broadcast();
		return Subsystem->GetGame().Owns(Item);
	}

	void TickOnce(FTestWorldWrapper& Wrapper)
	{
		Wrapper.TickTestWorld(TickSeconds);
	}

	/** Whether the shape, placed there, overlaps the collision of this one building (the pawn channel, as the player and the car move). */
	bool OverlapsBuilding(UWorld* World, const AActor* Building, const FVector& Centre, const FQuat& Rotation, const FCollisionShape& Shape)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShopOverlap), false);
		World->OverlapMultiByChannel(Overlaps, Centre, Rotation, ECC_Pawn, Shape, Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor() == Building)
			{
				return true;
			}
		}
		return false;
	}

	template <typename T>
	T* TheOnly(UWorld* World)
	{
		TActorIterator<T> It(World);
		T* Found = It ? *It : nullptr;
		if (!Found)
		{
			return nullptr;
		}
		++It;
		return It ? nullptr : Found;
	}
}

#define DIA_SHOP_FIXTURE() \
	FShopTestSlot Slot; \
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
	Subsystem->SetAutosaveEnabled(false);

DIA_TEST(FShopBuildingStandTest, "DoItAsphalt.Game.Shops.Stand")

bool FShopBuildingStandTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	int32 Count = 0;
	for (TActorIterator<ADIAShopBuilding> It(World); It; ++It)
	{
		++Count;
	}
	TestEqual(TEXT("exactly three buildings"), Count, 3);
	for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
	{
		const ELotShop Kind = ShopOf(I);
		const ADIAShopBuilding* Building = BuildingOf(World, Kind);
		if (!TestNotNull(TEXT("the building"), Building))
		{
			continue;
		}
		const FTransform Wanted = DIAShopBuilding::MakeTransform(Kind);
		TestTrue(TEXT("at its place"), Building->GetActorLocation().Equals(Wanted.GetLocation(), 0.5));
		TestEqual(TEXT("on the ground"), Building->GetActorLocation().Z, 0.0, 0.01);
		TestEqual(TEXT("with its yaw"), FRotator::NormalizeAxis(Building->GetActorRotation().Yaw), LotLayout::ShopYawDeg(Kind), 0.01);
		const UStaticMesh* Mesh = Building->GetBodyMesh()->GetStaticMesh();
		if (TestNotNull(TEXT("a mesh"), Mesh))
		{
			TestEqual(TEXT("the real model, not the placeholder cube"), Mesh->GetName(), FString(DIAShopBuilding::ModelName(Kind)));
		}
		TestEqual(TEXT("the building blocks"), Building->GetBodyMesh()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	}
	return true;
}

DIA_TEST(FShopBuildingNoOverlapTest, "DoItAsphalt.Game.Shops.NoOverlap")

bool FShopBuildingNoOverlapTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	TArray<FBox2D> Boxes;
	TArray<FString> Names;
	for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
	{
		const ADIAShopBuilding* Building = BuildingOf(World, ShopOf(I));
		if (!TestNotNull(TEXT("the building"), Building))
		{
			return false;
		}
		const FBox Box = Building->GetComponentsBoundingBox();
		TestTrue(TEXT("inside the strip: south of the fence line"), Box.Min.Y > -11200.0);
		TestTrue(TEXT("inside the strip: north limit"), Box.Max.Y < -10000.0);
		Boxes.Add(FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y)));
		Names.Add(DIAShopBuilding::ModelName(ShopOf(I)));
	}
	if (const ADIAPlantActor* Plant = TheOnly<ADIAPlantActor>(World))
	{
		const FBox Box = Plant->GetComponentsBoundingBox();
		Boxes.Add(FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y)));
		Names.Add(TEXT("plant"));
	}
	if (const ADIAVehicleStation* Station = TheOnly<ADIAVehicleStation>(World))
	{
		const FBox Box = Station->GetComponentsBoundingBox();
		Boxes.Add(FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y)));
		Names.Add(TEXT("vehicle station"));
	}
	TestEqual(TEXT("five envelopes were measured"), Boxes.Num(), 5);
	for (int32 A = 0; A < Boxes.Num(); ++A)
	{
		for (int32 B = A + 1; B < Boxes.Num(); ++B)
		{
			TestFalse(*FString::Printf(TEXT("%s and %s do not overlap"), *Names[A], *Names[B]), Boxes[A].Intersect(Boxes[B]));
		}
	}
	return true;
}

DIA_TEST(FShopBuildingGarageDoorOpenTest, "DoItAsphalt.Game.Shops.GarageDoorOpen")

bool FShopBuildingGarageDoorOpenTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	const ADIAShopBuilding* Garage = BuildingOf(World, ELotShop::Garage);
	if (!TestNotNull(TEXT("the garage"), Garage))
	{
		return false;
	}
	// The biggest truck (T4: 820 x 310 cm body, 340 cm high) as a box, swept against the garage alone.
	const FCollisionShape Truck = FCollisionShape::MakeBox(FVector(410.0, 250.0, 170.0));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GarageSweep), false);
	auto Sweep = [&](FVector2D FromLocal, FVector2D ToLocal, FHitResult& Hit)
	{
		const FVector2D From = LotLayout::ShopLocalToWorld(ELotShop::Garage, FromLocal);
		const FVector2D To = LotLayout::ShopLocalToWorld(ELotShop::Garage, ToLocal);
		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, FVector(From.X, From.Y, CarSweepZCm), FVector(To.X, To.Y, CarSweepZCm), FQuat::Identity, ECC_Pawn, Truck, Params);
		for (const FHitResult& H : Hits)
		{
			if (H.GetActor() == Garage && H.bBlockingHit)
			{
				Hit = H;
				return true;
			}
		}
		return false;
	};
	FHitResult Hit;
	const bool bDoorBlocked = Sweep(FVector2D(900.0, 0.0), FVector2D(0.0, 0.0), Hit);
	TestFalse(*FString::Printf(TEXT("the truck drives in through the door (hit %s at %s)"), Hit.Component.IsValid() ? *Hit.Component->GetName() : TEXT("-"), *Hit.ImpactPoint.ToString()), bDoorBlocked);
	FHitResult SideHit;
	TestTrue(TEXT("the same truck does not drive through the side wall"), Sweep(FVector2D(0.0, 600.0), FVector2D(0.0, 0.0), SideHit));
	return true;
}

DIA_TEST(FShopBuildingPointsFreeTest, "DoItAsphalt.Game.Shops.PointsFree")

bool FShopBuildingPointsFreeTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm);
	// Control: the same capsule in the side wall of the garage does overlap, so a free spot below means something.
	const ADIAShopBuilding* Garage = BuildingOf(World, ELotShop::Garage);
	if (!TestNotNull(TEXT("the garage"), Garage))
	{
		return false;
	}
	const FVector2D InWall = LotLayout::ShopLocalToWorld(ELotShop::Garage, FVector2D(0.0, 375.0));
	TestTrue(TEXT("a capsule in the garage wall overlaps it"),
		OverlapsBuilding(World, Garage, FVector(InWall.X, InWall.Y, DIACharacter::CapsuleHalfHeightCm), FQuat::Identity, Capsule));
	for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
	{
		const FVector2D Point = LotLayout::ShopPoint(ShopOf(I));
		const FVector Centre(Point.X, Point.Y, DIACharacter::CapsuleHalfHeightCm);
		for (TActorIterator<ADIAShopBuilding> It(World); It; ++It)
		{
			TestFalse(*FString::Printf(TEXT("the player's spot of shop %d is free of %s"), I, DIAShopBuilding::ModelName(It->Kind)),
				OverlapsBuilding(World, *It, Centre, FQuat::Identity, Capsule));
		}
	}
	return true;
}

DIA_TEST(FShopBuildingSignTextTest, "DoItAsphalt.Game.Shops.SignText")

bool FShopBuildingSignTextTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
	{
		const ADIAShopBuilding* Building = BuildingOf(World, ShopOf(I));
		if (!TestNotNull(TEXT("the building"), Building))
		{
			continue;
		}
		const FString Name = DIAFormat::ShopName(ShopOf(I)).ToString();
		TestEqual(TEXT("the sign names the shop in capitals"), Building->GetTextComponent()->Text.ToString(), Name.ToUpper());
		TestTrue(TEXT("the name is not empty"), !Name.IsEmpty());
		TestEqual(TEXT("1 cm in front of the board"),
			Building->GetTextComponent()->GetRelativeLocation().X, DIAShopBuilding::SignCentreCm(ShopOf(I)).X + 1.0, 0.01);
	}
	return true;
}

DIA_TEST(FShopBuildingCarSpawnsInGarageTest, "DoItAsphalt.Game.Shops.CarSpawnsInGarage")

bool FShopBuildingCarSpawnsInGarageTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheOnly<ADIAPaverCar>(World);
	const ADIAShopBuilding* Garage = BuildingOf(World, ELotShop::Garage);
	if (!TestNotNull(TEXT("the car"), Car) || !TestNotNull(TEXT("the garage"), Garage))
	{
		return false;
	}
	const FVector2D Spot = LotLayout::ShopPos(ELotShop::Garage);
	TestTrue(TEXT("it stands in the garage"), FVector2D(Car->GetActorLocation()).Equals(Spot, 0.5));
	TestEqual(TEXT("facing out of the door"), FRotator::NormalizeAxis(Car->GetActorRotation().Yaw), LotLayout::CarSpawnYawDeg, 0.01);

	// The biggest tier still fits in without touching a wall or the furniture.
	for (int32 Tier = 1; Tier < 4; ++Tier)
	{
		TestTrue(TEXT("grant the tier"), Grant(Subsystem, static_cast<ELotItem>(static_cast<int32>(ELotItem::Car) + Tier)));
	}
	TickOnce(Wrapper);
	TestEqual(TEXT("the biggest tier is shown"), Car->GetShownLevel(), 3);
	const FVector Body = DIAModelSizes::PaverCarBodyCm[3];
	const FVector Centre(Spot.X, Spot.Y, DIAPaverCar::OriginHeightCm(3));
	TestFalse(TEXT("the biggest truck does not overlap the garage"),
		OverlapsBuilding(World, Garage, Centre, FQuat::Identity, FCollisionShape::MakeBox(Body * 0.5)));
	TestTrue(TEXT("and it has not moved"), FVector2D(Car->GetActorLocation()).Equals(Spot, 0.5));
	return true;
}

DIA_TEST(FShopBuildingCarFallbackInGarageTest, "DoItAsphalt.Game.Shops.CarFallbackInGarage")

bool FShopBuildingCarFallbackInGarageTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	TestTrue(TEXT("grant the car"), Grant(Subsystem, ELotItem::Car));
	ADIAPaverCar* Car = TheOnly<ADIAPaverCar>(World);
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	// The first tier stands in the wall of the garage, sideways: the second tier does not fit there and goes back to its spawn.
	const FVector2D InWall = LotLayout::ShopLocalToWorld(ELotShop::Garage, FVector2D(0.0, 300.0));
	Car->SetActorLocationAndRotation(FVector(InWall.X, InWall.Y, DIAPaverCar::OriginHeightCm(0)), FRotator(0.0, 90.0, 0.0));
	TickOnce(Wrapper);
	TestTrue(TEXT("grant the second tier"), Grant(Subsystem, ELotItem::CarT2));
	TickOnce(Wrapper);
	const FVector2D Spawn = LotLayout::ShopPos(ELotShop::Garage);
	TestTrue(TEXT("the car is back at the garage's spawn"), FVector2D(Car->GetActorLocation()).Equals(Spawn, 0.5));
	TestEqual(TEXT("turned out of the door"), FRotator::NormalizeAxis(Car->GetActorRotation().Yaw), LotLayout::CarSpawnYawDeg, 0.01);
	return true;
}

DIA_TEST(FShopBuildingCarDrivesOutToLotTest, "DoItAsphalt.Game.Shops.CarDrivesOutToLot")

bool FShopBuildingCarDrivesOutToLotTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	const ADIAVehicleStation* Station = TheOnly<ADIAVehicleStation>(World);
	if (!TestNotNull(TEXT("the vehicle station"), Station))
	{
		return false;
	}
	// The biggest truck body (T4) as a box, swept against the shop buildings and the station only.
	const FVector Body = DIAModelSizes::PaverCarBodyCm[3];
	const FCollisionShape Truck = FCollisionShape::MakeBox(Body * 0.5);
	const double Z = DIAPaverCar::OriginHeightCm(3);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CarOutSweep), false);
	auto Blocked = [&](FVector2D From, FVector2D To, FString& What)
	{
		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, FVector(From.X, From.Y, Z), FVector(To.X, To.Y, Z), FQuat::Identity, ECC_Pawn, Truck, Params);
		for (const FHitResult& H : Hits)
		{
			const AActor* Actor = H.GetActor();
			if (H.bBlockingHit && Actor && (Actor->IsA<ADIAShopBuilding>() || Actor->IsA<ADIAVehicleStation>()))
			{
				What = Actor->GetName();
				return true;
			}
		}
		return false;
	};

	// Past the station, before the plant; then north onto the car park.
	const FVector2D Bay = LotLayout::ShopPos(ELotShop::Garage);
	const FVector2D PastStation(-600.0, Bay.Y);
	const FVector2D OnLot(-600.0, -9000.0);
	FString What;
	TestFalse(*FString::Printf(TEXT("the car drives from the garage through the station (hit %s)"), *What), Blocked(Bay, PastStation, What));
	What.Reset();
	TestFalse(*FString::Printf(TEXT("and north onto the car park (hit %s)"), *What), Blocked(PastStation, OnLot, What));

	// Control: shifted 60 cm south the same box must hit the station, so the free paths above mean something.
	What.Reset();
	TestTrue(TEXT("a car 60 cm off the lane axis clips the station"), Blocked(Bay + FVector2D(0.0, -60.0), PastStation + FVector2D(0.0, -60.0), What));
	TestEqual(TEXT("and it is the station it clips"), What, Station->GetName());
	return true;
}

DIA_TEST(FShopBuildingPaverSpawnsInDockTest, "DoItAsphalt.Game.Shops.PaverSpawnsInDock")

bool FShopBuildingPaverSpawnsInDockTest::RunTest(const FString& Parameters)
{
	DIA_SHOP_FIXTURE()
	TestTrue(TEXT("grant the robot"), Grant(Subsystem, ELotItem::AutoPaver));
	const ADIAAutoPaver* Paver = TheOnly<ADIAAutoPaver>(World);
	if (!TestNotNull(TEXT("the robot"), Paver))
	{
		return false;
	}
	const FVector2D Dock = LotLayout::PaverDock(0);
	TestTrue(TEXT("it stands in the first dock"), FVector2D(Paver->GetActorLocation()).Equals(Dock, 0.5));
	TestEqual(TEXT("facing out of the depot"), FRotator::NormalizeAxis(Paver->GetActorRotation().Yaw), LotLayout::PaverDockYawDeg, 0.01);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
