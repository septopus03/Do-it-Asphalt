// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIARefill.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "World/DIAMovable.h"

namespace MovableTestSupport
{
	constexpr float TickSeconds = 0.1f;

	/** Centre of an ordinary lot cell away from the edges of the car park. */
	FVector2D MiddleOfLot()
	{
		return AsphaltLot::CellCentre(AsphaltLot::CellIndex(40, 40));
	}

	FVector2D Where(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}

	void Move(ADIACharacter* Character, FVector2D Delta)
	{
		Character->SetActorLocation(Character->GetActorLocation() + FVector(Delta.X, Delta.Y, 0.0));
	}

	/** Puts the character on the ground at the spot and turns the view (the character has no controller in tests, so the camera holds the view). */
	void Place(ADIACharacter* Character, FVector2D Spot, double PitchDeg, double YawDeg)
	{
		Character->SetActorLocation(FVector(Spot.X, Spot.Y, DIACharacter::CapsuleHalfHeightCm));
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

	TArray<ADIAMovable*> FindMovables(UWorld* World)
	{
		TArray<ADIAMovable*> Result;
		for (TActorIterator<ADIAMovable> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		return Result;
	}

	ADIAMovable* FindMovable(UWorld* World, ELotMovable Kind)
	{
		for (TActorIterator<ADIAMovable> It(World); It; ++It)
		{
			if (It->GetKind() == Kind)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Puts the movable where the core keeps it: what a loaded game does. */
	void SetCorePos(FLotGame& Game, ELotMovable Kind, FVector2D Pos)
	{
		Game.World().Movables[static_cast<int32>(Kind)].Pos = Pos;
	}

	/** Fills the suitcase or the cart from the plant stock until it holds the litres (the stock is large enough for a test). */
	void FillTo(FLotGame& Game, ELotMovable Kind, double Litres)
	{
		Game.DebugSetPlantStock(Game.GetPlantStorage());
		for (int32 I = 0; I < 200 && Game.GetMovableLitres(Kind) < Litres; ++I)
		{
			Game.FillMovable(Kind, 1.0);
		}
	}

	int32 CountRawTexels(const FLotGame& Game)
	{
		int32 Count = 0;
		for (const uint8 Texel : Game.GetGrid().GetTexels())
		{
			Count += Texel == static_cast<uint8>(ELotTexel::Raw) ? 1 : 0;
		}
		return Count;
	}
}

using namespace MovableTestSupport;

/** A game world with the mode, the subsystem, the core and a character in the middle of the lot; the bought items are announced like a purchase. */
#define DIA_MOVABLE_FIXTURE() \
	FTestWorldWrapper Wrapper; \
	if (!DIATest::StartGameWorld(Wrapper, *this)) \
	{ \
		return false; \
	} \
	UWorld* World = Wrapper.GetTestWorld(); \
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>(); \
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>(); \
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestNotNull(TEXT("game mode"), Mode)) \
	{ \
		return false; \
	} \
	FLotGame& Game = Subsystem->GetGame(); \
	ADIACharacter* Character = DIATest::SpawnCharacter(World, MiddleOfLot()); \
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("first-person camera"), Character->GetCamera())) \
	{ \
		return false; \
	} \
	auto Buy = [&](ELotItem Item) \
	{ \
		Game.DebugGive(100000.0); \
		return Subsystem->BuyItem(Item); \
	};

DIA_TEST(FMovableSpawnsWhenBoughtTest, "DoItAsphalt.Game.Movable.SpawnsWhenBought")

bool FMovableSpawnsWhenBoughtTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestEqual(TEXT("nothing is owned, nothing stands"), FindMovables(World).Num(), 0);

	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	if (!TestEqual(TEXT("one movable after the purchase"), FindMovables(World).Num(), 1))
	{
		return false;
	}
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("it is the cart"), Cart))
	{
		return false;
	}
	TestTrue(TEXT("it stands at the cart's spawn point"), Where(Cart).Equals(LotLayout::ItemSpawn(ELotItem::Cart), 0.5));
	TestEqual(TEXT("on the ground"), Cart->GetActorLocation().Z, 0.0);
	TestTrue(TEXT("with a model"), Cart->GetBodyMesh() && Cart->GetBodyMesh()->GetStaticMesh() != nullptr);
	TestTrue(TEXT("and a collision"), Cart->GetCollision()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);

	// Asking again, or announcing the purchase twice, never makes a second one.
	Mode->EnsureMovable(ELotMovable::Cart);
	Subsystem->OnItemBought.Broadcast(ELotItem::Cart);
	TestEqual(TEXT("still one"), FindMovables(World).Num(), 1);

	TestTrue(TEXT("buy the suitcase"), Buy(ELotItem::Suitcase));
	TestEqual(TEXT("one actor per owned kind"), FindMovables(World).Num(), 2);

	// A loaded game that owns the station gets it, whatever happened before.
	Game.DebugGrantItem(ELotItem::Station);
	Subsystem->OnGameReset.Broadcast();
	TestEqual(TEXT("a game load spawns what the game owns"), FindMovables(World).Num(), 3);
	TestNotNull(TEXT("the station"), FindMovable(World, ELotMovable::Station));

	// A new game owns nothing.
	UDIAWorldSubsystem::SetSlotNameOverride(TEXT("DIA_Test_Movable"));
	Subsystem->NewGame();
	UDIAWorldSubsystem::SetSlotNameOverride(FString());
	TestEqual(TEXT("a new game removes them"), FindMovables(World).Num(), 0);
	return true;
}

DIA_TEST(FMovableGrabFollowReleaseTest, "DoItAsphalt.Game.Movable.GrabFollowReleaseWritesCore")

bool FMovableGrabFollowReleaseTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	const FVector2D Spawn = Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos;
	Wrapper.TickTestWorld(TickSeconds);

	Cart->Interact(Character);
	TestTrue(TEXT("the core says it is held"), Game.GetHeld() == ELotMovable::Cart);
	TestEqual(TEXT("no collision while held"), Cart->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// The player walks 5 m along +Y; the cart stays in front of the player, handle towards the player.
	Character->SetActorRotation(FRotator(0.0, 90.0, 0.0));
	for (int32 I = 0; I < 50; ++I)
	{
		Move(Character, FVector2D(0.0, 10.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	const FVector2D Player = Where(Character);
	TestTrue(TEXT("the cart is in front of the player"), (Where(Cart) - Player).Y > 0.0 && FMath::Abs((Where(Cart) - Player).X) < 1.0);
	TestTrue(TEXT("it follows the player"), FVector2D::Distance(Where(Cart), Player) < 150.0);
	TestEqual(TEXT("it faces the player's way"), FMath::Abs(FMath::FindDeltaAngleDegrees(Cart->GetActorRotation().Yaw, 90.0)), 0.0, 0.5);
	TestTrue(TEXT("the core still has the old position until it is let go"), Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos.Equals(Spawn, 0.01));

	Cart->Interact(Character);
	TestTrue(TEXT("let go"), Game.GetHeld() == ELotMovable::Count);
	const FLotMovable& Core = Game.World().Movables[static_cast<int32>(ELotMovable::Cart)];
	TestFalse(TEXT("the core does not hold it"), Core.bHeld);
	TestTrue(TEXT("the core has it within 150 cm in front of the player"), FVector2D::Distance(Core.Pos, Player) <= 150.0 && (Core.Pos - Player).Y > 0.0);
	TestTrue(TEXT("the actor stands where the core says"), Where(Cart).Equals(Core.Pos, 0.5));
	TestEqual(TEXT("and has its collision"), Cart->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);

	// A standing one stays when the player walks away.
	Move(Character, FVector2D(0.0, 400.0));
	TickFor(Wrapper, 0.5);
	TestTrue(TEXT("it stays"), Where(Cart).Equals(Core.Pos, 0.5));
	return true;
}

DIA_TEST(FMovableNoRoomKeepsHeldTest, "DoItAsphalt.Game.Movable.NoRoomKeepsItHeld")

bool FMovableNoRoomKeepsHeldTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	TestTrue(TEXT("buy the suitcase"), Buy(ELotItem::Suitcase));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	ADIAMovable* Suitcase = FindMovable(World, ELotMovable::Suitcase);
	if (!TestNotNull(TEXT("the cart"), Cart) || !TestNotNull(TEXT("the suitcase"), Suitcase))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("holding the cart"), Game.GetHeld() == ELotMovable::Cart);
	Wrapper.TickTestWorld(TickSeconds);

	// The suitcase stands exactly where the cart is now.
	SetCorePos(Game, ELotMovable::Suitcase, Where(Cart));
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the suitcase is there"), Where(Suitcase).Equals(Where(Cart), 0.5));

	const FVector2D Old = Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos;
	Cart->Interact(Character);
	TestTrue(TEXT("still held: no room"), Game.GetHeld() == ELotMovable::Cart);
	TestTrue(TEXT("the core was not written"), Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos.Equals(Old, 0.01));
	TestEqual(TEXT("still no collision"), Cart->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// One step further there is room.
	Move(Character, FVector2D(-500.0, 0.0));
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("let go with room"), Game.GetHeld() == ELotMovable::Count);
	return true;
}

DIA_TEST(FMovableCartLaysRawStripTest, "DoItAsphalt.Game.Movable.CartLaysRawStrip")

bool FMovableCartLaysRawStripTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	FillTo(Game, ELotMovable::Cart, 50.0);
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("holding the cart"), Game.GetHeld() == ELotMovable::Cart);

	// Without Use the cart lays nothing.
	for (int32 I = 0; I < 5; ++I)
	{
		Move(Character, FVector2D(10.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestEqual(TEXT("moving without Use lays nothing"), CountRawTexels(Game), 0);

	// Three metres with Use down: a strip one strip width wide.
	const double Before = Game.GetMovableLitres(ELotMovable::Cart);
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("the first tick of a press lays nothing (the cart did not move)"), CountRawTexels(Game), 0);
	for (int32 I = 0; I < 30; ++I)
	{
		Move(Character, FVector2D(10.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	Character->SetUsing(false);
	const double TexelsPerSquareMetre = 1.0e4 / FMath::Square(FLotGrid::TexelCm);
	const double Expected = 3.0 * (LotBalance::CartStripWidthCm / 100.0) * TexelsPerSquareMetre;
	TestTrue(TEXT("about 3 m of strip of raw texels"), FMath::Abs(CountRawTexels(Game) - Expected) <= Expected * 0.1);
	TestEqual(TEXT("no cell is finished by raw asphalt"), Game.GetCompletedCells(), 0);
	TestTrue(TEXT("the cart's content was used"), Game.GetMovableLitres(ELotMovable::Cart) < Before);

	// A stopped cart lays nothing more, though Use stays down.
	Character->SetUsing(true);
	const int32 Laid = CountRawTexels(Game);
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("a cart that does not move lays nothing"), CountRawTexels(Game), Laid);
	return true;
}

DIA_TEST(FMovableSuitcaseFlapTest, "DoItAsphalt.Game.Movable.SuitcaseFlap")

bool FMovableSuitcaseFlapTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the suitcase"), Buy(ELotItem::Suitcase));
	ADIAMovable* Suitcase = FindMovable(World, ELotMovable::Suitcase);
	if (!TestNotNull(TEXT("the suitcase"), Suitcase))
	{
		return false;
	}
	FillTo(Game, ELotMovable::Suitcase, LotBalance::SuitcaseCapacityL);
	const FVector2D Stand = MiddleOfLot();
	SetCorePos(Game, ELotMovable::Suitcase, Stand);
	Place(Character, Stand - FVector2D(80.0, 0.0), 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);

	const FVector2D Mouth = Suitcase->GetMouthPoint();
	TestTrue(TEXT("the mouth is at the front of the suitcase"), Mouth.Equals(Stand + FVector2D(DIAModelSizes::SuitcaseMouthCm.X, 0.0), 0.5));
	const int32 Cell = AsphaltLot::CellAt(Mouth);
	const double Before = Game.GetMovableLitres(ELotMovable::Suitcase);

	// Without Flap nothing happens.
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("no dosing without the flap"), Game.GetGrid().GetCellRaw(Cell), 0);

	// Too far from it: the flap does nothing.
	Move(Character, FVector2D(-400.0, 0.0));
	Character->SetFlap(true);
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("out of reach"), Game.GetGrid().GetCellRaw(Cell), 0);
	Move(Character, FVector2D(400.0, 0.0));

	TickFor(Wrapper, 5.0);
	Character->SetFlap(false);
	TestTrue(TEXT("10 l of raw asphalt in the cell under the mouth"), Game.GetGrid().GetCellRaw(Cell) >= FLotGrid::TexelsPerCell - 4);
	TestNearlyEqual(TEXT("the suitcase gave those litres"), Before - Game.GetMovableLitres(ELotMovable::Suitcase), 10.0, 0.2);
	TestEqual(TEXT("raw asphalt finishes no cell"), Game.GetCompletedCells(), 0);

	// Held, it doses nothing.
	Suitcase->Interact(Character);
	TestTrue(TEXT("holding the suitcase"), Game.GetHeld() == ELotMovable::Suitcase);
	const double Held = Game.GetMovableLitres(ELotMovable::Suitcase);
	Character->SetFlap(true);
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("a held suitcase does not dose"), Game.GetMovableLitres(ELotMovable::Suitcase), Held);
	return true;
}

DIA_TEST(FMovableFillsInZoneTest, "DoItAsphalt.Game.Movable.FillsInZone")

bool FMovableFillsInZoneTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	// Set down beside the trough.
	const FVector2D Beside = LotLayout::TroughPos + FVector2D(0.0, 120.0);
	SetCorePos(Game, ELotMovable::Cart, Beside);
	Game.DebugSetPlantStock(Game.GetPlantStorage());
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("it is in the refill zone"), DIARefill::IsInZone(Game, Where(Cart)));
	const double Before = Game.GetMovableLitres(ELotMovable::Cart);
	TickFor(Wrapper, 10.0);
	TestNearlyEqual(TEXT("ten seconds fill it at the dispense rate"), Game.GetMovableLitres(ELotMovable::Cart) - Before, 10.0 * LotBalance::DispenseRateLPerS(Game.GetPlantLevel()), 1.0);

	// Held in the same place, it does not fill.
	Character->SetActorLocation(FVector(Beside.X, Beside.Y - 200.0, DIACharacter::CapsuleHalfHeightCm));
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("holding the cart"), Game.GetHeld() == ELotMovable::Cart);
	const double Full = Game.GetMovableLitres(ELotMovable::Cart);
	Character->SetActorLocation(FVector(LotLayout::TroughPos.X, LotLayout::TroughPos.Y + 100.0, DIACharacter::CapsuleHalfHeightCm));
	TickFor(Wrapper, 5.0);
	TestEqual(TEXT("a held cart does not fill"), Game.GetMovableLitres(ELotMovable::Cart), Full);
	return true;
}

DIA_TEST(FMovableBucketFromCartTest, "DoItAsphalt.Game.Movable.BucketFromCart")

bool FMovableBucketFromCartTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	FillTo(Game, ELotMovable::Cart, 30.0);
	const FVector2D Stand = MiddleOfLot();
	SetCorePos(Game, ELotMovable::Cart, Stand);

	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);

	// 100 cm in front of the cart looking down at its middle: the view ray meets the ground 100 cm ahead.
	const double Pitch = -FMath::RadiansToDegrees(FMath::Atan2(DIACharacter::EyeHeightCm, 100.0));
	Place(Character, Stand - FVector2D(100.0, 0.0), Pitch, 0.0);
	TestTrue(TEXT("the aim is on the cart"), Cart->ContainsPoint(Character->GetAimPoint()));
	const double CartBefore = Game.GetMovableLitres(ELotMovable::Cart);
	Character->SetUsing(true);
	TickFor(Wrapper, 1.0);
	TestNearlyEqual(TEXT("one second of tapping fills 5 l"), Game.GetHandLitres(ELotHand::Bucket), LotBalance::TapRateLPerS, 1e-6);
	TestNearlyEqual(TEXT("from the cart's content"), CartBefore - Game.GetMovableLitres(ELotMovable::Cart), LotBalance::TapRateLPerS, 1e-6);
	TestEqual(TEXT("nothing was dumped on the ground"), Game.GetTotalLitres(), 0.0);
	Character->SetUsing(false);

	// Aiming beside the cart, the same press empties the bucket as before.
	Place(Character, Stand + FVector2D(0.0, 300.0), -90.0, 0.0);
	TestFalse(TEXT("the aim is off the cart"), Cart->ContainsPoint(Character->GetAimPoint()));
	Character->SetUsing(true);
	TickFor(Wrapper, 0.5);
	TestTrue(TEXT("the bucket was emptied into a cell"), Game.GetHandLitres(ELotHand::Bucket) < LotBalance::TapRateLPerS);
	return true;
}

DIA_TEST(FMovableDoesNotBlockPavingTest, "DoItAsphalt.Game.Movable.DoesNotBlockPaving")

bool FMovableDoesNotBlockPavingTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	const FVector2D Stand = MiddleOfLot();
	SetCorePos(Game, ELotMovable::Cart, Stand);
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("the pawn is blocked by it"), Cart->GetCollision()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
	TestEqual(TEXT("the camera is not"), Cart->GetCollision()->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);

	// The cart's own cell is finished while it stands there.
	const int32 CartCell = AsphaltLot::CellAt(Stand);
	Subsystem->Dispatch(Game.DebugCompleteCell(CartCell));
	TestTrue(TEXT("the cell under the cart is complete"), Game.GetGrid().IsCellComplete(CartCell));

	// The bowl pours into the next cell, aiming past the cart.
	const FVector2D Next = Stand + FVector2D(0.0, AsphaltLot::CellCm);
	const int32 NextCell = AsphaltLot::CellAt(Next);
	Place(Character, Next, -90.0, 0.0);
	Game.DebugFillHand();
	TestTrue(TEXT("the aim is off the cart"), !Cart->ContainsPoint(Character->GetAimPoint()));
	Character->SetUsing(true);
	TickFor(Wrapper, 1.0);
	TestTrue(TEXT("the pour reached the next cell"), Game.GetGrid().GetCellDone(NextCell) > 0);
	return true;
}

DIA_TEST(FMovablePromptsTest, "DoItAsphalt.Game.Movable.Prompts")

bool FMovablePromptsTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	TestTrue(TEXT("buy the suitcase"), Buy(ELotItem::Suitcase));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	ADIAMovable* Suitcase = FindMovable(World, ELotMovable::Suitcase);
	if (!TestNotNull(TEXT("the cart"), Cart) || !TestNotNull(TEXT("the suitcase"), Suitcase))
	{
		return false;
	}
	TestEqual(TEXT("a standing cart"), Cart->GetPrompt(Character).ToString(), FString(TEXT("E – Push")));
	TestEqual(TEXT("a standing suitcase also has the flap"), Suitcase->GetPrompt(Character).ToString(), FString(TEXT("E – Pull\nF – Open flap")));

	Suitcase->Interact(Character);
	TestEqual(TEXT("a held one can be let go"), Suitcase->GetPrompt(Character).ToString(), FString(TEXT("E – Let go")));
	TestTrue(TEXT("nothing else can be taken meanwhile"), Cart->GetPrompt(Character).IsEmpty());
	return true;
}

DIA_TEST(FMovableSpeedWhileHeldTest, "DoItAsphalt.Game.Movable.SpeedWhileHeld")

bool FMovableSpeedWhileHeldTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	TestTrue(TEXT("buy the suitcase"), Buy(ELotItem::Suitcase));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	ADIAMovable* Suitcase = FindMovable(World, ELotMovable::Suitcase);
	if (!TestNotNull(TEXT("the cart"), Cart) || !TestNotNull(TEXT("the suitcase"), Suitcase))
	{
		return false;
	}
	// The sprint is open, so that sprinting could be told apart from walking.
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		Game.DebugCompleteCell(AsphaltLot::EntryCell(1, I));
	}
	Game.DebugCompleteCells(LotBalance::UnlockCells[static_cast<int32>(ELotUnlock::Sprint)]);
	TestTrue(TEXT("sprint is unlocked"), Game.IsUnlocked(ELotUnlock::Sprint));

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Character->SetSprinting(true);
	Wrapper.TickTestWorld(TickSeconds);
	TestNearlyEqual(TEXT("sprinting on foot"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS * LotBalance::SprintMultiplier, 0.01);

	Suitcase->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestNearlyEqual(TEXT("pulling the suitcase, sprint does not help"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::SuitcasePullSpeedCmS, 0.01);
	Suitcase->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestNearlyEqual(TEXT("let go: sprinting again"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS * LotBalance::SprintMultiplier, 0.01);

	Character->SetSprinting(false);
	Cart->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestNearlyEqual(TEXT("pushing the cart"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::CartPushSpeedCmS, 0.01);
	Character->SetUsing(true);
	TestNearlyEqual(TEXT("laying a strip is slower"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::CartLaySpeedCmS, 0.01);
	Character->SetSprinting(true);
	TestNearlyEqual(TEXT("sprinting does not change it"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::CartLaySpeedCmS, 0.01);
	Character->SetUsing(false);
	TestNearlyEqual(TEXT("Use up: pushing again"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::CartPushSpeedCmS, 0.01);
	return true;
}

DIA_TEST(FMovableHandHiddenWhileHeldTest, "DoItAsphalt.Game.Movable.HandToolHiddenWhileHeld")

bool FMovableHandHiddenWhileHeldTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the bowl is shown"), Character->GetHandMesh()->IsVisible());
	Cart->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestFalse(TEXT("the hand tool is hidden while the cart is held"), Character->GetHandMesh()->IsVisible());
	TestFalse(TEXT("and its spoon"), Character->GetSpoonMesh()->IsVisible());
	Cart->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the bowl is back"), Character->GetHandMesh()->IsVisible());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
