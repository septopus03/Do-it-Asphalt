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

	void TickFor(FTestWorldWrapper& Wrapper, double Seconds, double Step = TickSeconds)
	{
		const int32 Steps = FMath::RoundToInt32(Seconds / Step);
		for (int32 I = 0; I < Steps; ++I)
		{
			Wrapper.TickTestWorld(static_cast<float>(Step));
		}
	}

	/** Where a view ray meets the ground (ground at GroundZ), pulled in to ReachCm from the player: the aim point worked out from a rendered view. */
	FVector2D AimFromView(const FMinimalViewInfo& View, FVector2D Player, double GroundZ)
	{
		const FVector Direction = View.Rotation.Vector();
		FVector2D Hit = Player;
		if (Direction.Z < -UE_KINDA_SMALL_NUMBER)
		{
			const double Distance = (GroundZ - View.Location.Z) / Direction.Z;
			Hit = FVector2D(View.Location.X + Direction.X * Distance, View.Location.Y + Direction.Y * Distance);
		}
		const FVector2D Offset = Hit - Player;
		return Offset.SizeSquared() > FMath::Square(LotBalance::ReachCm) ? Player + Offset.GetSafeNormal() * LotBalance::ReachCm : Hit;
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

	int32 CountTexels(const FLotGame& Game, ELotTexel State = ELotTexel::Done)
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

	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TestEqual(TEXT("bucket mesh"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Bucket")));
	TestFalse(TEXT("no spoon with the bucket"), Character->GetSpoonMesh()->IsVisible());

	// The core changes the hand without the character: the mesh catches up with the next state change.
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	Game.DebugGrantItem(ELotItem::Canister);
	TestTrue(TEXT("the core takes the canister"), Game.SelectHand(ELotHand::Canister));
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("canister mesh"), MeshName(Character->GetHandMesh()), FString(TEXT("SM_Canister")));

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

	Character->SetFilling(true);
	TickFor(Wrapper, LotBalance::ScoopCycleSeconds * 1.2);
	TestNearlyEqual(TEXT("one stroke of the spoon in the bowl"), Game.GetHandLitres(ELotHand::Bowl), LotBalance::SpoonLitres, 1e-6);
	TickFor(Wrapper, LotBalance::ScoopCycleSeconds);
	TestNearlyEqual(TEXT("the next stroke"), Game.GetHandLitres(ELotHand::Bowl), 2.0 * LotBalance::SpoonLitres, 1e-6);

	Character->SetFilling(false);
	TickFor(Wrapper, 1.0);
	TestNearlyEqual(TEXT("nothing scooped while Fill is up"), Game.GetHandLitres(ELotHand::Bowl), 2.0 * LotBalance::SpoonLitres, 1e-6);
	TestEqual(TEXT("scooping pours nothing on the lot"), Game.GetTotalLitres(), 0.0);
	return true;
}

DIA_TEST(FCharacterPourOnLotTest, "DoItAsphalt.Game.Character.PourOnLot")

bool FCharacterPourOnLotTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Game.DebugFillHand();
	const double Before = Game.GetHandLitres(ELotHand::Bowl);
	const int32 DoneBefore = CountTexels(Game);
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

	TestNearlyEqual(TEXT("the bowl is a litre lighter"), Before - Game.GetHandLitres(ELotHand::Bowl), LotBalance::HandPourRateLPerS[static_cast<int32>(ELotHand::Bowl)] * 1.0, 0.05);
	const int32 Gained = CountTexels(Game) - DoneBefore;
	TestTrue(TEXT("finished texels appeared on the ground"), Gained > 0);
	TestNearlyEqual(TEXT("... as many as the litres pay for"), Gained * FLotGrid::LitresPerTexel, Game.GetTotalLitres(), 1e-6);
	TestNearlyEqual(TEXT("the litres came out of the bowl"), Game.GetTotalLitres(), Before - Game.GetHandLitres(ELotHand::Bowl), 1e-6);

	const double Rest = Game.GetHandLitres(ELotHand::Bowl);
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("nothing pours once Use is up"), Game.GetHandLitres(ELotHand::Bowl), Rest);
	return true;
}

DIA_TEST(FCharacterBucketPoursAtAimTest, "DoItAsphalt.Game.Character.BucketPoursAtAim")

bool FCharacterBucketPoursAtAimTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	Game.DebugFillHand();
	const double Full = Game.GetHandLitres(ELotHand::Bucket);
	TestEqual(TEXT("a full bucket"), Full, LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)]);

	// Looking down at the feet: nothing happens before the button is down.
	Place(Character, MiddleOfLot(), -90.0, 0.0);
	TickFor(Wrapper, 0.2);
	TestEqual(TEXT("the bucket does not pour by itself"), Game.GetHandLitres(ELotHand::Bucket), Full);

	// Held, it pours as a stream, not all at once, and the asphalt lies where the player aims.
	const FVector2D Aim = Character->GetAimPoint();
	Character->SetUsing(true);
	TickFor(Wrapper, 0.1);
	const double After = Game.GetHandLitres(ELotHand::Bucket);
	TestTrue(TEXT("a little left the bucket"), After < Full);
	TestTrue(TEXT("but not all of it"), After > Full - 0.5 * LotBalance::HandPourRateLPerS[static_cast<int32>(ELotHand::Bucket)]);
	TestTrue(TEXT("the asphalt lies at the aim point"), TexelAt(Game, Aim) == ELotTexel::Done);
	TestTrue(TEXT("a point beyond the stream's footprint is still free"), TexelAt(Game, Aim + FVector2D(LotBalance::PourRadiusCm[static_cast<int32>(ELotHand::Bucket)] * 2.0, 0.0)) == ELotTexel::Free);

	// The ground under the stream is covered quickly; what the bucket could not lay stays in it.
	TickFor(Wrapper, 2.0);
	Character->SetUsing(false);
	const double Rest = Game.GetHandLitres(ELotHand::Bucket);
	TestTrue(TEXT("the stream stops paying once its footprint is covered"), Rest > 0.0 && Rest < After);
	TestNearlyEqual(TEXT("only what lay on the ground left the bucket"), Game.GetTotalLitres(), Full - Rest, 1e-6);
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
	Character->SetFilling(true);
	TickFor(Wrapper, 1.0);
	TestNearlyEqual(TEXT("one second of filling"), Game.GetHandLitres(ELotHand::Bucket), LotBalance::DispenseRateLPerS(Game.GetPlantLevel()), 1e-6);
	TestEqual(TEXT("nothing dumped at the outlet"), Game.GetTotalLitres(), 0.0);
	TestEqual(TEXT("no cell finished"), Game.GetCompletedCells(), 0);
	TickFor(Wrapper, 5.0);
	TestNearlyEqual(TEXT("filled to the brim"), Game.GetHandLitres(ELotHand::Bucket), LotBalance::HandCapacityL[static_cast<int32>(ELotHand::Bucket)], 1e-6);
	Character->SetFilling(false);

	// Away from the outlet the button pours it as a stream under the aim.
	Place(Character, MiddleOfLot(), -90.0, 0.0);
	Character->SetUsing(true);
	TickFor(Wrapper, 1.0);
	TestTrue(TEXT("it poured on the ground"), Game.GetTotalLitres() > 1.0);
	TestTrue(TEXT("but not the whole cell at once"), Game.GetCompletedCells() == 0);
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

DIA_TEST(FCharacterWorkViewTiltTest, "DoItAsphalt.Game.Character.WorkViewTilt")

bool FCharacterWorkViewTiltTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	UCameraComponent* Camera = Character->GetCamera();
	const double BasePitch = -30.0;
	Place(Character, MiddleOfLot(), BasePitch, 20.0);
	const double GroundZ = Character->GetActorLocation().Z - DIACharacter::CapsuleHalfHeightCm;

	// The pitch of what the camera really renders, and the aim point worked out from that picture, against GetAimPoint().
	auto Check = [&](const TCHAR* When, double ExpectedPitch)
	{
		FMinimalViewInfo View;
		Camera->GetCameraView(0.f, View);
		TestEqual(FString::Printf(TEXT("rendered pitch %s"), When), static_cast<double>(View.Rotation.Pitch), ExpectedPitch, 1.0);
		const FVector2D FromPicture = AimFromView(View, FVector2D(Character->GetActorLocation()), GroundZ);
		TestTrue(FString::Printf(TEXT("the aim point is where the picture looks %s"), When), FromPicture.Equals(Character->GetAimPoint(), 5.0));
	};
	Wrapper.TickTestWorld(0.005f);
	Check(TEXT("at rest"), BasePitch);

	Character->SetWorkView(true);
	TickFor(Wrapper, 0.3, 0.01);
	Check(TEXT("after 0.3 s of holding"), BasePitch - DIACharacter::WorkViewTiltDeg);
	TickFor(Wrapper, 1.0, 0.01);
	Check(TEXT("after 1 s more (no spiral)"), BasePitch - DIACharacter::WorkViewTiltDeg);

	Character->SetWorkView(false);
	TickFor(Wrapper, 0.3, 0.01);
	Check(TEXT("after the release"), BasePitch);
	TickFor(Wrapper, 0.5, 0.01);
	Check(TEXT("and it stays there"), BasePitch);
	return true;
}

namespace
{
	/** What one second of holding Use does with each tool, for a given tick length. */
	struct FUseResult
	{
		double Scooped = 0.0;
		double Poured = 0.0;
		double DumpedLitres = 0.0;
		int32 DumpedCells = 0;
	};

	bool RunUse(FAutomationTestBase& Test, double Step, FUseResult& Out)
	{
		{
			FTestWorldWrapper Wrapper;
			if (!DIATest::StartGameWorld(Wrapper, Test))
			{
				return false;
			}
			UWorld* World = Wrapper.GetTestWorld();
			FLotGame& Game = World->GetSubsystem<UDIAWorldSubsystem>()->GetGame();
			ADIACharacter* Character = DIATest::SpawnCharacter(World, MiddleOfLot());
			// Scoop: 1.2 cycles of the trough, whatever the tick.
			Place(Character, LotLayout::TroughPos + FVector2D(0.0, 200.0), 0.0, -90.0);
			Character->SetFilling(true);
			TickFor(Wrapper, 1.2, Step);
			Out.Scooped = Game.GetHandLitres(ELotHand::Bowl);

			// Pour: walking at 150 cm/s over free ground, looking down.
			Character->SetFilling(false);
			Game.DebugFillHand();
			const double Before = Game.GetHandLitres(ELotHand::Bowl);
			Place(Character, MiddleOfLot(), -90.0, 0.0);
			Character->SetUsing(true);
			for (double T = 0.0; T < 1.0 - 1e-9; T += Step)
			{
				Move(Character, FVector2D(150.0 * Step, 0.0));
				Wrapper.TickTestWorld(static_cast<float>(Step));
			}
			Out.Poured = Before - Game.GetHandLitres(ELotHand::Bowl);
		}
		{
			FTestWorldWrapper Wrapper;
			if (!DIATest::StartGameWorld(Wrapper, Test))
			{
				return false;
			}
			UWorld* World = Wrapper.GetTestWorld();
			FLotGame& Game = World->GetSubsystem<UDIAWorldSubsystem>()->GetGame();
			ADIACharacter* Character = DIATest::SpawnCharacter(World, MiddleOfLot());
			Game.DebugGrantItem(ELotItem::Bucket);
			Character->SelectHand(ELotHand::Bucket);
			TickFor(Wrapper, LotBalance::SwapSeconds + 0.5, Step);
			Game.DebugFillHand();
			Place(Character, MiddleOfLot(), -90.0, 0.0);
			Character->SetUsing(true);
			for (double T = 0.0; T < 1.0 - 1e-9; T += Step)
			{
				Wrapper.TickTestWorld(static_cast<float>(Step));
				Move(Character, FVector2D(1000.0 * Step, 0.0));
			}
			Out.DumpedCells = Game.GetCompletedCells();
			Out.DumpedLitres = Game.GetTotalLitres();
		}
		return true;
	}
}

DIA_TEST(FCharacterTickRateIndependentTest, "DoItAsphalt.Game.Character.TickRateIndependent")

bool FCharacterTickRateIndependentTest::RunTest(const FString& Parameters)
{
	FUseResult Fine, Coarse;
	if (!RunUse(*this, 0.005, Fine) || !RunUse(*this, 0.1, Coarse))
	{
		return false;
	}
	TestNearlyEqual(TEXT("scooped litres"), Fine.Scooped, Coarse.Scooped, 1e-6);
	TestEqual(TEXT("two strokes of the spoon"), Fine.Scooped, 2.0 * LotBalance::SpoonLitres, 1e-6);
	TestNearlyEqual(TEXT("poured litres (within two texels)"), Fine.Poured, Coarse.Poured, 2.0 * FLotGrid::LitresPerTexel);
	TestEqual(TEXT("poured about a litre"), Fine.Poured, LotBalance::HandPourRateLPerS[static_cast<int32>(ELotHand::Bowl)], 0.1);
	TestNearlyEqual(TEXT("the bucket's stream pours the same in both"), Fine.DumpedLitres, Coarse.DumpedLitres, 2.0 * FLotGrid::LitresPerTexel);
	TestEqual(TEXT("a second of the bucket's stream"), Fine.DumpedLitres, LotBalance::HandPourRateLPerS[static_cast<int32>(ELotHand::Bucket)], 0.1);
	return true;
}

DIA_TEST(FCharacterNextHandTest, "DoItAsphalt.Game.Character.NextHandCycles")

bool FCharacterNextHandTest::RunTest(const FString& Parameters)
{
	DIA_CHARACTER_FIXTURE()
	Character->NextHand();
	TestEqual(TEXT("the bowl stays while nothing else is owned"), Game.GetHand(), ELotHand::Bowl);
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);

	Game.DebugGrantItem(ELotItem::Canister);
	Game.DebugGrantItem(ELotItem::Bucket);
	const ELotHand Order[] = {ELotHand::Bucket, ELotHand::Canister, ELotHand::Bowl};
	for (const ELotHand Expected : Order)
	{
		Character->NextHand();
		TestEqual(TEXT("next item in order"), Game.GetHand(), Expected);
		TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	}
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
