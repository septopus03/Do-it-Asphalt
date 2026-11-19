// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HexCoord.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "World/DIAPlantActor.h"

namespace
{
	constexpr float TickSeconds = 0.1f;

	void Move(ADIACharacter* Character, FVector2D Delta)
	{
		Character->SetActorLocation(Character->GetActorLocation() + FVector(Delta.X, Delta.Y, 0.0));
	}
}

DIA_TEST(FCharacterPoursTest, "DoItAsphalt.Game.CharacterPours")

bool FCharacterPoursTest::RunTest(const FString& Parameters)
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

	// Plant stock, then a full backpack of 5 l.
	Game.Tick(600.0);
	Game.RefillBackpack(10.0);
	TestEqual(TEXT("backpack holds 5 l"), Game.GetBackpackLitres(), 5.0);

	const FVector2D Cell0 = AsphaltHex::ToWorld(Game.GetMap().GetCell(0).Coord);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, Cell0);
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}

	// Moving without pouring pours nothing: the character really ticks, and walking alone does not spend asphalt.
	for (int32 I = 0; I < 5; ++I)
	{
		Move(Character, FVector2D(5.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestFalse(TEXT("not pouring"), Character->IsPouring());
	TestEqual(TEXT("no money without pouring"), Game.GetMoney(), 0.0);
	TestEqual(TEXT("backpack untouched without pouring"), Game.GetBackpackLitres(), 5.0);
	const FVector2D Where(Character->GetActorLocation().X, Character->GetActorLocation().Y);
	TestTrue(TEXT("the character's tick keeps the game's player position"), Game.World().PlayerPos.Equals(Where, 0.01));

	// The nozzle is NozzleAheadCm in front of the actor.
	TestTrue(TEXT("nozzle point is ahead of the actor"), Character->GetNozzlePoint().Equals(Where + FVector2D(AsphaltBalance::NozzleAheadCm, 0.0), 0.01));

	Character->SetPouring(true);
	TestTrue(TEXT("pouring"), Character->IsPouring());
	for (int32 I = 0; I < 20; ++I)
	{
		Move(Character, FVector2D(5.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestTrue(TEXT("pouring earned money"), Game.GetMoney() > 0.0);
	TestTrue(TEXT("pouring emptied the backpack a little"), Game.GetBackpackLitres() < 5.0);

	// Letting go of the button stops it.
	Character->SetPouring(false);
	const double MoneyAfter = Game.GetMoney();
	for (int32 I = 0; I < 5; ++I)
	{
		Move(Character, FVector2D(5.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestEqual(TEXT("no more money after the button is released"), Game.GetMoney(), MoneyAfter);
	return true;
}

DIA_TEST(FCharacterRefillsInZoneTest, "DoItAsphalt.Game.CharacterRefillsInZone")

bool FCharacterRefillsInZoneTest::RunTest(const FString& Parameters)
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

	// The plant is full (60 l after four minutes at the lowest level), the backpack is empty.
	Game.Tick(600.0);
	TestEqual(TEXT("plant stock is 60 l"), Game.GetPlantStock(), 60.0);
	TestEqual(TEXT("backpack starts empty"), Game.GetBackpackLitres(), 0.0);

	// 300 cm from the centre is outside the 200 cm zone: nothing is refilled in 2 s.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, AsphaltLayout::RefillCenter + FVector2D(300.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	for (int32 I = 0; I < 20; ++I)
	{
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestEqual(TEXT("outside the zone the backpack stays empty"), Game.GetBackpackLitres(), 0.0);

	// In the centre the backpack fills up within the refill time (1.5 s).
	Character->SetActorLocation(FVector(AsphaltLayout::RefillCenter.X, AsphaltLayout::RefillCenter.Y, DIACharacter::CapsuleHalfHeightCm));
	for (int32 I = 0; I < 20; ++I)
	{
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestEqual(TEXT("in the zone the backpack is full (5 l)"), Game.GetBackpackLitres(), 5.0);
	TestTrue(TEXT("the litres came out of the plant stock"), Game.GetPlantStock() < 60.0);
	return true;
}

DIA_TEST(FCharacterEquipmentTest, "DoItAsphalt.Game.CharacterEquipment")

bool FCharacterEquipmentTest::RunTest(const FString& Parameters)
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

	// The Mannequin: mesh, animation blueprint and the two bones that carry the equipment.
	USkeletalMeshComponent* Body = Character->GetMesh();
	if (TestNotNull(TEXT("Mannequin mesh loaded"), Body->GetSkeletalMeshAsset()))
	{
		TestTrue(TEXT("bone spine_03 exists"), Body->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE);
		TestTrue(TEXT("bone head exists"), Body->GetBoneIndex(TEXT("head")) != INDEX_NONE);
	}
	TestNotNull(TEXT("anim blueprint class"), Body->GetAnimClass());
	// The backpack pivot has the character's axes (forward, up), whatever the bone's own axes are. The idle pose twists
	// the spine by some 20 degrees, hence the loose tolerance (a wrong bone rotation would be off by 90 degrees).
	{
		const USceneComponent* Pivot = Character->GetBackpackPivot();
		TestTrue(TEXT("backpack pivot looks forward"), FVector::DotProduct(Pivot->GetForwardVector(), Character->GetActorForwardVector()) > 0.8);
		TestTrue(TEXT("backpack pivot points up"), FVector::DotProduct(Pivot->GetUpVector(), FVector::UpVector) > 0.8);
		TestTrue(TEXT("backpack pivot is on the back of the chest"), Pivot->GetComponentLocation().X < Character->GetActorLocation().X);
	}
	TestEqual(TEXT("backpack hangs on spine_03"), Character->GetBackpackPivot()->GetAttachSocketName(), FName(TEXT("spine_03")));

	if (!TestNotNull(TEXT("spreader"), Character->GetSpreaderMesh()) || !TestNotNull(TEXT("backpack"), Character->GetBackpackMesh()) ||
		!TestNotNull(TEXT("helmet"), Character->GetHelmetMesh()))
	{
		return false;
	}

	// The spreader's pivot lies on the ground, NozzleAheadCm in front of the actor.
	const FVector Pivot = Character->GetSpreaderMesh()->GetAttachParent()->GetComponentLocation();
	TestTrue(TEXT("spreader pivot is the nozzle on the ground"),
		Pivot.Equals(FVector(-1200.0 + AsphaltBalance::NozzleAheadCm, 0.0, 0.0), 0.1));

	// Placeholders (no models yet): box 8 x W x 8 cm, W = tool width.
	const UStaticMeshComponent* Spreader = Character->GetSpreaderMesh();
	const bool bPlaceholder = Spreader->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"));
	if (bPlaceholder)
	{
		TestTrue(TEXT("placeholder spreader is as wide as the tool"), Spreader->GetRelativeScale3D().Equals(FVector(0.08, 0.2, 0.08), 0.001));
	}

	// Nothing is rebuilt while nothing changes.
	const UStaticMeshComponent* Backpack = Character->GetBackpackMesh();
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("spreader is kept while the levels stay"), Character->GetSpreaderMesh() == Spreader);
	TestTrue(TEXT("backpack is kept while the levels stay"), Character->GetBackpackMesh() == Backpack);

	// A backpack level within the same tier changes nothing; the next tier swaps the backpack only.
	Game.DebugSetLevel(EAsphaltUpgrade::Backpack, 2);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("same tier keeps the backpack"), Character->GetBackpackMesh() == Backpack);
	Game.DebugSetLevel(EAsphaltUpgrade::Backpack, 3);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("tier 1 swaps the backpack"), Character->GetBackpackMesh() != nullptr && Character->GetBackpackMesh() != Backpack);
	TestTrue(TEXT("the spreader stays"), Character->GetSpreaderMesh() == Spreader);
	if (bPlaceholder && Character->GetBackpackMesh())
	{
		TestTrue(TEXT("backpack M placeholder size"), Character->GetBackpackMesh()->GetRelativeScale3D().Equals(DIAModelSizes::BackpackCm[1] / 100.0, 0.001));
	}

	// A new tool level or the TwoHands special swaps the spreader.
	Game.DebugSetLevel(EAsphaltUpgrade::Tool, 6);
	Wrapper.TickTestWorld(TickSeconds);
	if (TestNotNull(TEXT("spreader after tool upgrade"), Character->GetSpreaderMesh()) && bPlaceholder)
	{
		TestTrue(TEXT("spreader is 200 cm wide"), Character->GetSpreaderMesh()->GetRelativeScale3D().Equals(FVector(0.08, 2.0, 0.08), 0.001));
	}
	Game.DebugGrantSpecial(EAsphaltSpecial::TwoHands);
	Wrapper.TickTestWorld(TickSeconds);
	// TwoHands: two spreaders of the level's width side by side (Y = -W/2 and +W/2), each 2 m wide here.
	if (TestNotNull(TEXT("spreader after TwoHands"), Character->GetSpreaderMesh()) && TestNotNull(TEXT("second spreader after TwoHands"), Character->GetSecondSpreaderMesh()) && bPlaceholder)
	{
		TestTrue(TEXT("each of the two spreaders is 200 cm wide"), Character->GetSpreaderMesh()->GetRelativeScale3D().Equals(FVector(0.08, 2.0, 0.08), 0.001)
			&& Character->GetSecondSpreaderMesh()->GetRelativeScale3D().Equals(FVector(0.08, 2.0, 0.08), 0.001));
		TestTrue(TEXT("they sit at Y = -100 and +100"), FMath::IsNearlyEqual(Character->GetSpreaderMesh()->GetRelativeLocation().Y, -100.0, 0.01)
			&& FMath::IsNearlyEqual(Character->GetSecondSpreaderMesh()->GetRelativeLocation().Y, 100.0, 0.01));
	}

	// Speed: walking, pouring (slower) and sprinting (only with the special).
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	TestEqual(TEXT("walk speed"), static_cast<double>(Movement->MaxWalkSpeed), AsphaltBalance::WalkSpeedCmS);
	Character->SetPouring(true);
	TestEqual(TEXT("pour speed"), static_cast<double>(Movement->MaxWalkSpeed), AsphaltBalance::PourSpeedCmS);
	TestFalse(TEXT("no orient-to-movement while pouring"), Movement->bOrientRotationToMovement);
	TestTrue(TEXT("faces the camera while pouring"), Character->bUseControllerRotationYaw);
	Character->SetPouring(false);
	TestTrue(TEXT("orient-to-movement again"), Movement->bOrientRotationToMovement);
	Character->SetSprinting(true);
	TestEqual(TEXT("no sprint without the special"), static_cast<double>(Movement->MaxWalkSpeed), AsphaltBalance::WalkSpeedCmS);
	Game.DebugGrantSpecial(EAsphaltSpecial::Sprint);
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("sprint speed"), static_cast<double>(Movement->MaxWalkSpeed), AsphaltBalance::WalkSpeedCmS * AsphaltBalance::SprintMultiplier, 0.01);
	return true;
}

DIA_TEST(FPlantPlacedTest, "DoItAsphalt.Game.PlantPlaced")

bool FPlantPlacedTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	TestTrue(TEXT("the player is an ADIACharacter"), World->GetAuthGameMode()->DefaultPawnClass == ADIACharacter::StaticClass());
	TestTrue(TEXT("the controller is an ADIAPlayerController"), World->GetAuthGameMode()->PlayerControllerClass == ADIAPlayerController::StaticClass());
	if (const AActor* Start = World->GetAuthGameMode()->ChoosePlayerStart(nullptr))
	{
		TestTrue(TEXT("the player starts at the layout's start"), FVector2D(Start->GetActorLocation()).Equals(AsphaltLayout::PlayerStart, 1.0));
	}
	else
	{
		AddError(TEXT("no player start"));
	}

	int32 Count = 0;
	ADIAPlantActor* Plant = nullptr;
	for (TActorIterator<ADIAPlantActor> It(World); It; ++It)
	{
		Plant = *It;
		++Count;
	}
	if (!TestEqual(TEXT("exactly one plant"), Count, 1))
	{
		return false;
	}
	TestTrue(TEXT("plant stands at PlantPos on the ground"), Plant->GetActorLocation().Equals(FVector(AsphaltLayout::PlantPos.X, AsphaltLayout::PlantPos.Y, 0.0), 0.01));

	// The plant is solid: a pawn-channel trace from the west hits it, one across the player's start (the plant body ends at X = -1700) does not.
	FHitResult Hit;
	const FVector WestOfPlant(AsphaltLayout::PlantPos.X - 1000.0, 0.0, 100.0);
	const FVector Inside(AsphaltLayout::PlantPos.X, 0.0, 100.0);
	TestTrue(TEXT("the plant blocks pawns"), World->LineTraceSingleByChannel(Hit, WestOfPlant, Inside, ECC_Pawn));
	TestTrue(TEXT("the trace hits the plant"), Hit.GetActor() == Plant);
	// ... but it must not block the camera: the spring arm probes with the camera channel.
	TestFalse(TEXT("the plant does not block the camera"), World->LineTraceSingleByChannel(Hit, WestOfPlant, Inside, ECC_Camera));
	TestEqual(TEXT("the plant ignores the camera channel"), static_cast<int32>(Plant->GetCollision()->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));
	TestEqual(TEXT("the plant still blocks static-world traces"), static_cast<int32>(Plant->GetCollision()->GetCollisionResponseToChannel(ECC_WorldStatic)), static_cast<int32>(ECR_Block));
	const FVector ZoneA(AsphaltLayout::RefillCenter.X - 50.0, 0.0, 100.0);
	const FVector ZoneB(AsphaltLayout::RefillCenter.X + 150.0, 0.0, 100.0);
	TestFalse(TEXT("the refill zone is free to walk"), World->LineTraceSingleByChannel(Hit, ZoneA, ZoneB, ECC_Pawn));

	// Body of the plant: 600 x 400 x 500 cm standing on the ground. The real model also has the outlet, which sticks out
	// up to 40 cm beyond the body on the +X side; the placeholder box has none.
	const FBox Body = Plant->GetPlantMesh()->CalcBounds(Plant->GetPlantMesh()->GetComponentTransform()).GetBox();
	TestTrue(TEXT("plant body envelope"), Body.Min.Equals(FVector(-2300.0, -200.0, 0.0), 0.5) && FMath::IsNearlyEqual(Body.Max.Y, 200.0, 0.5) && FMath::IsNearlyEqual(Body.Max.Z, 500.0, 0.5));
	TestTrue(TEXT("plant body ends at X = -1700, or at most 40 cm behind it with the outlet"), Body.Max.X >= -1700.5 && Body.Max.X <= -1660.0 + 0.5);

	// The marker: a thin disc of the zone's radius around RefillCenter, without collision.
	const UStaticMeshComponent* Marker = Plant->GetZoneMarker();
	const FBox Disc = Marker->CalcBounds(Marker->GetComponentTransform()).GetBox();
	TestTrue(TEXT("marker is centred on the refill centre"), FVector2D((Disc.Min.X + Disc.Max.X) * 0.5, (Disc.Min.Y + Disc.Max.Y) * 0.5).Equals(AsphaltLayout::RefillCenter, 0.5));
	TestEqual(TEXT("marker radius is the zone radius"), (Disc.Max.X - Disc.Min.X) * 0.5, AsphaltLayout::RefillRadiusCm, 0.5);
	TestTrue(TEXT("marker is thin"), Disc.Max.Z - Disc.Min.Z <= 2.0);
	TestTrue(TEXT("marker lies just above the ground"), Disc.Min.Z >= 0.0);
	TestEqual(TEXT("marker has no collision"), static_cast<int32>(Marker->GetCollisionEnabled()), static_cast<int32>(ECollisionEnabled::NoCollision));
	return true;
}
