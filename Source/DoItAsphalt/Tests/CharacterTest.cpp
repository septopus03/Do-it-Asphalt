// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIARefill.h"
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

	/** Centre of an ordinary lot cell away from the edges of the car park. */
	FVector2D MiddleOfLot()
	{
		return AsphaltLot::CellCentre(AsphaltLot::CellIndex(40, 40));
	}

	/** Puts the character on the ground at the spot and turns the view (the character has no controller in tests, so the camera holds the view). */
	void Place(ADIACharacter* Character, FVector2D Where, double PitchDeg, double YawDeg)
	{
		Character->SetActorLocation(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm));
		Character->GetCamera()->SetWorldRotation(FRotator(PitchDeg, YawDeg, 0.0));
	}

	void TickFor(FTestWorldWrapper& Wrapper, double Seconds)
	{
		const int32 Steps = FMath::RoundToInt32(Seconds / TickSeconds);
		for (int32 I = 0; I < Steps; ++I)
		{
			Wrapper.TickTestWorld(TickSeconds);
		}
	}

	FString MeshName(const UStaticMeshComponent* Component)
	{
		return Component && Component->GetStaticMesh() ? Component->GetStaticMesh()->GetName() : FString();
	}

	/** State of the texel containing the world point. */
	ELotTexel TexelAt(const FLotGame& Game, FVector2D World)
	{
		const int32 X = FMath::FloorToInt32((World.X + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm);
		const int32 Y = FMath::FloorToInt32((World.Y + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm);
		return Game.GetGrid().GetTexel(X, Y);
	}

	int32 CountTexels(const FLotGame& Game, ELotTexel State)
	{
		int32 Count = 0;
		for (const uint8 Texel : Game.GetGrid().GetTexels())
		{
			Count += Texel == static_cast<uint8>(State) ? 1 : 0;
		}
		return Count;
	}
}

#define DIA_CHARACTER_FIXTURE(NeedsCharacter) \
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
	FLotGame& Game = Subsystem->GetGame(); \
	ADIACharacter* Character = DIATest::SpawnCharacter(World, MiddleOfLot()); \
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("first-person camera"), Character->GetCamera()) || \
		!TestNotNull(TEXT("hand pivot"), Character->GetHandPivot())) \
	{ \
		return false; \
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
	TestFalse(TEXT("the body does not turn towards the walking direction"), Movement->bOrientRotationToMovement);
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

DIA_TEST(FCharacterFirstPersonTest, "DoItAsphalt.Game.Character.FirstPerson")

bool FCharacterFirstPersonTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	const UCameraComponent* Camera = Character->GetCamera();
	TestTrue(TEXT("the camera follows the controller's rotation"), Camera->bUsePawnControlRotation);
	TestTrue(TEXT("the camera is active, the external one is not"), Camera->IsActive() && !Character->IsExternalView());
	TestEqual(TEXT("eyes at EyeHeightCm above the ground"), Camera->GetComponentLocation().Z - (Character->GetActorLocation().Z - DIACharacter::CapsuleHalfHeightCm), static_cast<double>(DIACharacter::EyeHeightCm), 0.5);
	TestEqual(TEXT("normal field of view"), Camera->FieldOfView, DIACharacter::NormalFovDeg, 0.01f);
	TestTrue(TEXT("the body turns with the camera"), Character->bUseControllerRotationYaw);
	TestTrue(TEXT("the owner does not see the body"), Character->GetMesh()->bOwnerNoSee);
	TestTrue(TEXT("... but the body casts its shadow"), Character->GetMesh()->bCastHiddenShadow);
	TestTrue(TEXT("the hand pivot hangs on the camera"), Character->GetHandPivot()->GetAttachParent() == Camera);
	TestTrue(TEXT("the pivot is right of the axis"), Character->GetHandPivot()->GetRelativeLocation().Y > 0.0);
	TestTrue(TEXT("... and below it"), Character->GetHandPivot()->GetRelativeLocation().Z < 0.0);

	Character->SetExternalView(true);
	TestTrue(TEXT("external view switches the first-person camera off"), Character->IsExternalView() && !Camera->IsActive());
	TestFalse(TEXT("the owner sees the body in the external view"), Character->GetMesh()->bOwnerNoSee);
	Character->SetExternalView(false);
	TestTrue(TEXT("first person again"), !Character->IsExternalView() && Camera->IsActive());
	TestTrue(TEXT("the body is hidden again"), Character->GetMesh()->bOwnerNoSee);
	return true;
}

DIA_TEST(FCharacterAimPointReachTest, "DoItAsphalt.Game.Character.AimPointReach")

bool FCharacterAimPointReachTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	const FVector2D Start = MiddleOfLot();

	Place(Character, Start, 0.0, 30.0);
	const FVector2D Level = Character->GetAimPoint();
	TestEqual(TEXT("level view: ReachCm away"), FVector2D::Distance(Level, Start), LotBalance::ReachCm, 0.5);
	TestEqual(TEXT("... in the direction of the view"), FMath::RadiansToDegrees(FMath::Atan2(Level.Y - Start.Y, Level.X - Start.X)), 30.0, 0.5);

	Place(Character, Start, -90.0, 0.0);
	TestTrue(TEXT("looking straight down: under the player"), Character->GetAimPoint().Equals(Start, 0.5));

	// 45 degrees down from the eyes: the view ray meets the ground as far in front as the eyes are high, inside the reach.
	Place(Character, Start, -45.0, 90.0);
	const FVector2D Near = Character->GetAimPoint();
	TestEqual(TEXT("45 degrees down: as far as the eyes are high"), FVector2D::Distance(Near, Start), static_cast<double>(DIACharacter::EyeHeightCm), 0.5);

	Place(Character, Start, 40.0, 0.0);
	TestEqual(TEXT("looking up: still ReachCm away"), FVector2D::Distance(Character->GetAimPoint(), Start), LotBalance::ReachCm, 0.5);
	return true;
}

DIA_TEST(FCharacterHandMeshTest, "DoItAsphalt.Game.Character.HandMeshFollowsCore")

bool FCharacterHandMeshTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("the bowl at the start"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Bowl")));
	TestEqual(TEXT("... with the spoon"), MeshName(Character->GetSpoonMesh()), FString(TEXT("SM_Spoon")));
	TestTrue(TEXT("... the spoon shows"), Character->GetSpoonMesh()->IsVisible());

	TestTrue(TEXT("take the trowel"), Character->SelectHand(ELotHand::Trowel));
	TestEqual(TEXT("trowel mesh"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Trowel")));
	TestFalse(TEXT("no spoon with the trowel"), Character->GetSpoonMesh()->IsVisible());
	TestFalse(TEXT("no asphalt level on the trowel"), Character->GetLiquidMesh()->IsVisible());

	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TestEqual(TEXT("bucket mesh"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Bucket")));
	TestFalse(TEXT("no spoon with the bucket"), Character->GetSpoonMesh()->IsVisible());

	// The core changes the hand without the character: the mesh catches up with the next state change.
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	Game.DebugGrantItem(ELotItem::Scoop);
	TestTrue(TEXT("the core takes the scoop"), Game.SelectHand(ELotHand::Scoop));
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("scoop mesh"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Scoop")));

	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	TestTrue(TEXT("back to the bowl"), Game.SelectHand(ELotHand::Bowl));
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("bowl mesh again"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Bowl")));
	TestTrue(TEXT("the spoon is back"), Character->GetSpoonMesh()->IsVisible());
	return true;
}

DIA_TEST(FCharacterLiquidLevelTest, "DoItAsphalt.Game.Character.LiquidLevel")

bool FCharacterLiquidLevelTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Wrapper.TickTestWorld(TickSeconds);
	const UStaticMeshComponent* Liquid = Character->GetLiquidMesh();
	TestFalse(TEXT("an empty bowl shows no asphalt"), Liquid->IsVisible());
	Game.ScoopFromTrough(LotBalance::ScoopCycleSeconds * 3.5);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("asphalt in the bowl shows"), Liquid->IsVisible());
	const double Low = Liquid->GetRelativeLocation().Z;
	const double LowRadius = Liquid->GetRelativeScale3D().X;
	Game.DebugFillHand();
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the level rises with the content"), Liquid->GetRelativeLocation().Z > Low);
	TestTrue(TEXT("the disc widens with the content"), Liquid->GetRelativeScale3D().X > LowRadius);
	return true;
}

DIA_TEST(FCharacterScoopAtTroughTest, "DoItAsphalt.Game.Character.ScoopAtTrough")

bool FCharacterScoopAtTroughTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	// 200 cm beside the trough looking at it: the aim point (ReachCm along the view) is well inside TroughReachCm of it.
	Place(Character, LotLayout::TroughPos + FVector2D(0.0, 200.0), 0.0, -90.0);
	TestTrue(TEXT("aiming at the trough"), FVector2D::Distance(Character->GetAimPoint(), LotLayout::TroughPos) < LotBalance::ReachCm);
	TestEqual(TEXT("the bowl is empty"), Game.GetHandLitres(ELotHand::Bowl), 0.0);

	Character->SetUsing(true);
	TickFor(Wrapper, LotBalance::ScoopCycleSeconds * 1.2);
	TestNearlyEqual(TEXT("one stroke of the spoon in the bowl"), Game.GetHandLitres(ELotHand::Bowl), LotBalance::SpoonLitres, 1e-6);
	TickFor(Wrapper, LotBalance::ScoopCycleSeconds);
	TestNearlyEqual(TEXT("the next stroke"), Game.GetHandLitres(ELotHand::Bowl), 2.0 * LotBalance::SpoonLitres, 1e-6);

	Character->SetUsing(false);
	TickFor(Wrapper, 1.0);
	TestNearlyEqual(TEXT("nothing scooped while Use is up"), Game.GetHandLitres(ELotHand::Bowl), 2.0 * LotBalance::SpoonLitres, 1e-6);
	TestEqual(TEXT("scooping pours nothing on the lot"), Game.GetTotalLitres(), 0.0);
	return true;
}

DIA_TEST(FCharacterPourOnLotTest, "DoItAsphalt.Game.Character.PourOnLot")

bool FCharacterPourOnLotTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Game.DebugFillHand();
	const double Before = Game.GetHandLitres(ELotHand::Bowl);
	const int32 DoneBefore = CountTexels(Game, ELotTexel::Done);
	TestEqual(TEXT("nothing on the lot yet"), DoneBefore, 0);

	// Looking down at the feet; the player walks on 15 cm per tick so that the stream always meets free texels.
	Place(Character, MiddleOfLot(), -90.0, 0.0);
	Character->SetUsing(true);
	for (int32 I = 0; I < 10; ++I)
	{
		Move(Character, FVector2D(15.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	Character->SetUsing(false);

	TestNearlyEqual(TEXT("the bowl is a litre lighter"), Before - Game.GetHandLitres(ELotHand::Bowl), LotBalance::HandPourRateLPerS * 1.0, 0.05);
	const int32 Gained = CountTexels(Game, ELotTexel::Done) - DoneBefore;
	TestTrue(TEXT("finished texels appeared on the ground"), Gained > 0);
	TestNearlyEqual(TEXT("... as many as the litres pay for"), Gained * FLotGrid::LitresPerTexel, Game.GetTotalLitres(), 1e-6);
	TestNearlyEqual(TEXT("the litres came out of the bowl"), Game.GetTotalLitres(), Before - Game.GetHandLitres(ELotHand::Bowl), 1e-6);

	const double Rest = Game.GetHandLitres(ELotHand::Bowl);
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("nothing pours once Use is up"), Game.GetHandLitres(ELotHand::Bowl), Rest);
	return true;
}

DIA_TEST(FCharacterDumpOncePerPressTest, "DoItAsphalt.Game.Character.DumpOncePerPress")

bool FCharacterDumpOncePerPressTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	Game.DebugFillHand();
	const double Credits = Game.GetCredits();
	TestEqual(TEXT("a full bucket"), Game.GetHandLitres(ELotHand::Bucket), LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)]);

	// Held for a second while the player walks over fresh cells: still one cell only.
	Place(Character, MiddleOfLot(), -90.0, 0.0);
	Character->SetUsing(true);
	for (int32 I = 0; I < 10; ++I)
	{
		Wrapper.TickTestWorld(TickSeconds);
		Move(Character, FVector2D(100.0, 0.0));
	}
	TestEqual(TEXT("one finished cell"), Game.GetCompletedCells(), 1);
	TestNearlyEqual(TEXT("it paid its credits"), Game.GetCredits() - Credits, static_cast<double>(LotBalance::CreditsPerCell), 1e-6);
	const double Rest = Game.GetHandLitres(ELotHand::Bucket);
	TestNearlyEqual(TEXT("only the cell's litres left the bucket"), Game.GetTotalLitres(), LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)] - Rest, 1e-6);
	TestTrue(TEXT("the rest stayed in the bucket"), Rest > 0.0);

	// A new press empties the rest.
	Character->SetUsing(false);
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the next press dumps again"), Game.GetHandLitres(ELotHand::Bucket) < Rest);
	return true;
}

DIA_TEST(FCharacterBucketAtOutletTest, "DoItAsphalt.Game.Character.BucketAtOutlet")

bool FCharacterBucketAtOutletTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);

	// Aiming at the outlet the bucket in hand fills at the dispense rate and is never dumped on the ground.
	Place(Character, LotLayout::TroughPos + FVector2D(0.0, 200.0), 0.0, -90.0);
	TestTrue(TEXT("aiming at the outlet"), DIARefill::IsAtOutlet(Game, Character->GetAimPoint()));
	TestEqual(TEXT("the bucket is empty"), Game.GetHandLitres(ELotHand::Bucket), 0.0);
	Character->SetUsing(true);
	TickFor(Wrapper, 1.0);
	TestNearlyEqual(TEXT("one second of filling"), Game.GetHandLitres(ELotHand::Bucket), LotBalance::DispenseRateLPerS(Game.GetPlantLevel()), 1e-6);
	TestEqual(TEXT("nothing dumped at the outlet"), Game.GetTotalLitres(), 0.0);
	TestEqual(TEXT("no cell finished"), Game.GetCompletedCells(), 0);
	TickFor(Wrapper, 5.0);
	TestNearlyEqual(TEXT("filled to the brim"), Game.GetHandLitres(ELotHand::Bucket), LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)], 1e-6);
	Character->SetUsing(false);

	// Away from the outlet one press empties it into the cell under the aim.
	Place(Character, MiddleOfLot(), -90.0, 0.0);
	Character->SetUsing(true);
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("one dump, one cell"), Game.GetCompletedCells(), 1);
	return true;
}

DIA_TEST(FCharacterSprintLockedTest, "DoItAsphalt.Game.Character.SprintLocked")

bool FCharacterSprintLockedTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Character->SetSprinting(true);
	TestEqual(TEXT("sprint changes nothing while locked"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS);
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		Game.DebugCompleteCell(AsphaltLot::EntryCell(1, I));
	}
	Game.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::Sprint)]);
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("sprint speed once unlocked"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS * LotBalance::SprintMultiplier, 0.01);
	return true;
}

DIA_TEST(FCharacterWorkViewBlendTest, "DoItAsphalt.Game.Character.WorkViewBlend")

bool FCharacterWorkViewBlendTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	const UCameraComponent* Camera = Character->GetCamera();
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("normal field of view"), Camera->FieldOfView, 90.f, 1.f);

	Character->SetWorkView(true);
	TickFor(Wrapper, 0.1);
	TestTrue(TEXT("the blend is smooth, not a jump"), Camera->FieldOfView < 90.f && Camera->FieldOfView > 60.f);
	TickFor(Wrapper, 0.2);
	TestEqual(TEXT("work view field of view after 0.3 s"), Camera->FieldOfView, 60.f, 1.f);

	Character->SetWorkView(false);
	TickFor(Wrapper, 0.3);
	TestEqual(TEXT("normal field of view after 0.3 s"), Camera->FieldOfView, 90.f, 1.f);
	return true;
}

DIA_TEST(FCharacterNextHandTest, "DoItAsphalt.Game.Character.NextHandCycles")

bool FCharacterNextHandTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Character->NextHand();
	TestEqual(TEXT("bowl -> trowel while nothing else is owned"), Game.GetHand(), ELotHand::Trowel);
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	Character->NextHand();
	TestEqual(TEXT("trowel -> bowl"), Game.GetHand(), ELotHand::Bowl);
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);

	Game.DebugGrantItem(ELotItem::Scoop);
	Game.DebugGrantItem(ELotItem::Bucket);
	const ELotHand Order[] = {ELotHand::Scoop, ELotHand::Bucket, ELotHand::Trowel, ELotHand::Bowl};
	for (const ELotHand Expected : Order)
	{
		Character->NextHand();
		TestEqual(TEXT("next item in order"), Game.GetHand(), Expected);
		TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	}
	return true;
}

DIA_TEST(FCharacterTrowelSpeedLimitedTest, "DoItAsphalt.Game.Character.TrowelSpeedLimited")

bool FCharacterTrowelSpeedLimitedTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	TestTrue(TEXT("take the trowel"), Character->SelectHand(ELotHand::Trowel));
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);

	// A raw strip ahead of the player, longer than the trowel can travel in a second.
	const FVector2D Start = MiddleOfLot();
	const double StripLength = LotBalance::ReachCm;
	Game.GetGrid().Stamp(Start - FVector2D(10.0, 0.0), Start + FVector2D(StripLength, 0.0), LotBalance::TrowelWidthCm, 100.0, ELotTexel::Raw);
	TestTrue(TEXT("raw at the far end"), TexelAt(Game, Start + FVector2D(StripLength - 2.5, 0.0)) == ELotTexel::Raw);

	// Press while looking down at the feet: the contact point starts there. Then the aim jumps to the far end of the reach.
	Place(Character, Start, -90.0, 0.0);
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);
	Place(Character, Start, 0.0, 0.0);
	TestEqual(TEXT("the aim jumped a whole reach"), Character->GetAimPoint().X - Start.X, LotBalance::ReachCm, 0.5);
	TickFor(Wrapper, 1.0);
	Character->SetUsing(false);

	double FinishedLength = 0.0;
	for (double X = 2.5; X < StripLength + 20.0; X += FLotGrid::TexelCm)
	{
		if (TexelAt(Game, Start + FVector2D(X, 0.0)) == ELotTexel::Done)
		{
			FinishedLength = X;
		}
	}
	TestEqual(TEXT("in one second only TrowelSpeedCmS of the strip is finished"), FinishedLength, LotBalance::TrowelSpeedCmS, 2.0 * LotBalance::StampPadCm + FLotGrid::TexelCm);
	TestTrue(TEXT("the far end is still raw"), TexelAt(Game, Start + FVector2D(StripLength - 2.5, 0.0)) == ELotTexel::Raw);
	return true;
}

DIA_TEST(FCharacterRefillZonesTest, "DoItAsphalt.Game.Character.RefillZones")

bool FCharacterRefillZonesTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	const FVector2D Trough = LotLayout::TroughPos;
	TestTrue(TEXT("outlet: at the trough"), DIARefill::IsAtOutlet(Game, Trough));
	TestTrue(TEXT("outlet: just inside the reach"), DIARefill::IsAtOutlet(Game, Trough + FVector2D(LotLayout::TroughReachCm - 1.0, 0.0)));
	TestFalse(TEXT("outlet: just outside the reach"), DIARefill::IsAtOutlet(Game, Trough + FVector2D(LotLayout::TroughReachCm + 1.0, 0.0)));
	TestTrue(TEXT("zone: just inside the refill radius"), DIARefill::IsInZone(Game, Trough + FVector2D(0.0, LotLayout::RefillRadiusCm - 1.0)));
	TestFalse(TEXT("zone: just outside it"), DIARefill::IsInZone(Game, Trough + FVector2D(0.0, LotLayout::RefillRadiusCm + 1.0)));

	// The station counts only once it is bought and set down.
	Game.DebugGrantItem(ELotItem::Station);
	const FVector2D Station = Game.World().Movables[static_cast<int32>(ELotMovable::Station)].Pos;
	TestFalse(TEXT("test setup: the station stands away from the trough"), DIARefill::IsInZone(Game, Station + FVector2D(0.0, 0.0)) && FVector2D::Distance(Station, Trough) < LotLayout::RefillRadiusCm);
	TestTrue(TEXT("outlet: at the set down station"), DIARefill::IsAtOutlet(Game, Station));
	TestTrue(TEXT("zone: at the set down station"), DIARefill::IsInZone(Game, Station));
	TestTrue(TEXT("grab the station"), Game.Grab(ELotMovable::Station));
	TestFalse(TEXT("outlet: a held station is no outlet"), DIARefill::IsAtOutlet(Game, Station));
	TestFalse(TEXT("zone: nor a zone"), DIARefill::IsInZone(Game, Station));
	return true;
}

#undef DIA_CHARACTER_FIXTURE

#endif // WITH_DEV_AUTOMATION_TESTS
