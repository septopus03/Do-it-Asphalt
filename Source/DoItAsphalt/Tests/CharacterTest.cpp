// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

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
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
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

DIA_TEST(FCharacterWalksTest, "DoItAsphalt.Game.CharacterWalks")

bool FCharacterWalksTest::RunTest(const FString& Parameters)
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
	FLotGame& Game = Subsystem->GetGame();

	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-1200.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	for (int32 I = 0; I < 5; ++I)
	{
		Move(Character, FVector2D(5.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	const FVector2D Where(Character->GetActorLocation().X, Character->GetActorLocation().Y);
	TestTrue(TEXT("the character's tick keeps the game's player position"), Game.World().PlayerPos.Equals(Where, 0.01));
	TestEqual(TEXT("walking spends no credits and earns none"), Game.GetCredits(), 0.0);
	TestEqual(TEXT("walking completes no cell"), Game.GetCompletedCells(), 0);

	// A loaded or new game puts the character where the core says.
	Game.World().PlayerPos = FVector2D(-3000.0, 2500.0);
	Game.World().PlayerYawDeg = 135.0;
	Subsystem->OnGameReset.Broadcast();
	TestTrue(TEXT("a reset moves the character to the saved position"), FVector2D(Character->GetActorLocation()).Equals(FVector2D(-3000.0, 2500.0), 1.0));
	TestEqual(TEXT("... and turns it"), FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, 135.0)), 0.0, 1.0);
	return true;
}

DIA_TEST(FCharacterBodyTest, "DoItAsphalt.Game.CharacterBody")

bool FCharacterBodyTest::RunTest(const FString& Parameters)
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
	FLotGame& Game = Subsystem->GetGame();
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-1200.0, 0.0));
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}

	// The Mannequin: mesh and animation blueprint. There is no equipment on the body any more.
	USkeletalMeshComponent* Body = Character->GetMesh();
	TestNotNull(TEXT("Mannequin mesh loaded"), Body->GetSkeletalMeshAsset());
	TestNotNull(TEXT("anim blueprint class"), Body->GetAnimClass());
	TestEqual(TEXT("only the body mesh hangs on the character"), Body->GetAttachChildren().Num(), 0);

	// Speed: walking, and sprinting once the Sprint step is unlocked (its entry and enough cells).
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	TestEqual(TEXT("walk speed"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS);
	TestTrue(TEXT("orient-to-movement"), Movement->bOrientRotationToMovement);
	Character->SetSprinting(true);
	TestEqual(TEXT("no sprint while it is locked"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS);

	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		Game.DebugCompleteCell(AsphaltLot::EntryCell(1, I));
	}
	Game.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::Sprint)]);
	TestTrue(TEXT("the sprint step is open"), Game.IsUnlocked(ELotUnlock::Sprint));
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("sprint speed"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS * LotBalance::SprintMultiplier, 0.01);
	Character->SetSprinting(false);
	TestEqual(TEXT("walk speed again"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS);
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
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}

	TestTrue(TEXT("the player is an ADIACharacter"), World->GetAuthGameMode()->DefaultPawnClass == ADIACharacter::StaticClass());
	TestTrue(TEXT("the controller is an ADIAPlayerController"), World->GetAuthGameMode()->PlayerControllerClass == ADIAPlayerController::StaticClass());
	if (const AActor* Start = World->GetAuthGameMode()->ChoosePlayerStart(nullptr))
	{
		TestTrue(TEXT("the player starts at the layout's start"), FVector2D(Start->GetActorLocation()).Equals(LotLayout::PlayerStart, 1.0));
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
	TestTrue(TEXT("plant stands at PlantPos on the ground"), Plant->GetActorLocation().Equals(FVector(LotLayout::PlantPos.X, LotLayout::PlantPos.Y, 0.0), 0.01));
	TestEqual(TEXT("plant is turned by the layout's yaw"), FMath::Abs(FMath::FindDeltaAngleDegrees(Plant->GetActorRotation().Yaw, LotLayout::PlantYawDeg)), 0.0, 0.01);

	// The plant and the trough are solid: a pawn-channel trace hits them.
	FHitResult Hit;
	const FVector WestOfPlant(LotLayout::PlantPos.X - 450.0, LotLayout::PlantPos.Y, 100.0);
	const FVector Inside(LotLayout::PlantPos.X, LotLayout::PlantPos.Y, 100.0);
	TestTrue(TEXT("the plant blocks pawns"), World->LineTraceSingleByChannel(Hit, WestOfPlant, Inside, ECC_Pawn));
	TestTrue(TEXT("the trace hits the plant"), Hit.GetActor() == Plant);
	// ... but it must not block the camera: the spring arm probes with the camera channel.
	TestFalse(TEXT("the plant does not block the camera"), World->LineTraceSingleByChannel(Hit, WestOfPlant, Inside, ECC_Camera));
	TestEqual(TEXT("the plant ignores the camera channel"), static_cast<int32>(Plant->GetCollision()->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));
	TestEqual(TEXT("the plant still blocks static-world traces"), static_cast<int32>(Plant->GetCollision()->GetCollisionResponseToChannel(ECC_WorldStatic)), static_cast<int32>(ECR_Block));

	// Body of the plant: 600 x 400 x 500 cm (model X x Y x Z) standing on the ground, turned so that the outlet looks along +Y. The real model
	// also has the outlet, which sticks out up to 40 cm beyond the body on the +X side of the model; the placeholder box has none.
	const FBox Body = Plant->GetPlantMesh()->CalcBounds(Plant->GetPlantMesh()->GetComponentTransform()).GetBox();
	TestTrue(TEXT("plant body envelope across the outlet"), FMath::IsNearlyEqual(Body.Min.X, LotLayout::PlantPos.X - 200.0, 0.5) && FMath::IsNearlyEqual(Body.Max.X, LotLayout::PlantPos.X + 200.0, 0.5));
	TestTrue(TEXT("plant body starts behind the plant"), FMath::IsNearlyEqual(Body.Min.Y, LotLayout::PlantPos.Y - 300.0, 0.5) && FMath::IsNearlyEqual(Body.Min.Z, 0.0, 0.5));
	TestTrue(TEXT("plant body is 500 cm high"), FMath::IsNearlyEqual(Body.Max.Z, 500.0, 0.5));
	TestTrue(TEXT("plant body ends at the outlet, or at most 40 cm beyond it"), Body.Max.Y >= LotLayout::PlantPos.Y + 299.5 && Body.Max.Y <= LotLayout::PlantPos.Y + 340.5);

	// The trough: solid for pawns, invisible to the camera, at TroughPos with its long side along X.
	const UStaticMeshComponent* Trough = Plant->GetTroughMesh();
	if (TestNotNull(TEXT("trough"), Trough))
	{
		const FBox Box = Trough->CalcBounds(Trough->GetComponentTransform()).GetBox();
		TestTrue(TEXT("trough is centred on TroughPos"), FVector2D((Box.Min.X + Box.Max.X) * 0.5, (Box.Min.Y + Box.Max.Y) * 0.5).Equals(LotLayout::TroughPos, 1.0));
		TestTrue(TEXT("the long side of the trough lies along X"), Box.Max.X - Box.Min.X > Box.Max.Y - Box.Min.Y);
		TestTrue(TEXT("the trough stands on the ground"), FMath::IsNearlyEqual(Box.Min.Z, 0.0, 0.5));
		const FVector Before(LotLayout::TroughPos.X, LotLayout::TroughPos.Y + 150.0, 25.0);
		const FVector After(LotLayout::TroughPos.X, LotLayout::TroughPos.Y, 25.0);
		TestTrue(TEXT("the trough blocks pawns"), World->LineTraceSingleByChannel(Hit, Before, After, ECC_Pawn) && Hit.GetComponent() == Trough);
		TestFalse(TEXT("the trough does not block the camera"), World->LineTraceSingleByChannel(Hit, Before, After, ECC_Camera));
	}

	// The addons appear with the plant level, and only then they are solid.
	FLotGame& Game = Subsystem->GetGame();
	Wrapper.TickTestWorld(TickSeconds);
	TestFalse(TEXT("addon A is hidden at level 0"), Plant->GetAddonA()->IsVisible());
	TestFalse(TEXT("addon B is hidden at level 0"), Plant->GetAddonB()->IsVisible());
	Game.DebugSetPlantLevel(LotBalance::PlantAddonALevel);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("addon A shows at its level"), Plant->GetAddonA()->IsVisible());
	TestEqual(TEXT("addon A is solid"), static_cast<int32>(Plant->GetAddonA()->GetCollisionEnabled()), static_cast<int32>(ECollisionEnabled::QueryAndPhysics));
	TestFalse(TEXT("addon B is still hidden"), Plant->GetAddonB()->IsVisible());
	Game.DebugSetPlantLevel(LotBalance::PlantAddonBLevel);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("addon B shows at its level"), Plant->GetAddonB()->IsVisible());
	Game.DebugSetPlantLevel(0);
	Wrapper.TickTestWorld(TickSeconds);
	TestFalse(TEXT("the addons go away with the level"), Plant->GetAddonA()->IsVisible() || Plant->GetAddonB()->IsVisible());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
