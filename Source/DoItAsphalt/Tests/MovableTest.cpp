// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIARefill.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "World/DIAInteractable.h"
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
	/** The test slot for the length of a test: the real slot is never touched, and the override is gone afterwards. */
	struct FMovableTestSlot
	{
		static const FString& Name()
		{
			static const FString SlotName(TEXT("DIA_Test_Movable"));
			return SlotName;
		}
		FMovableTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(Name());
			Delete();
		}
		~FMovableTestSlot()
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

	/** A player controller with a HUD that possesses the character, as in the real game (the toasts need it). */
	ADIAHUD* Possess(UWorld* World, ADIACharacter* Character)
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
		ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
		Controller->MyHUD = Hud;
		return Hud;
	}

	/** Raw texels laid by walking the cart 3 m along +X with Use down, ticking at the given step. -1 if the world did not start. */
	int32 RawTexelsOfAWalk(FAutomationTestBase& Test, double Step)
	{
		FTestWorldWrapper Wrapper;
		if (!DIATest::StartGameWorld(Wrapper, Test))
		{
			return -1;
		}
		UWorld* World = Wrapper.GetTestWorld();
		UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
		ADIACharacter* Character = DIATest::SpawnCharacter(World, MiddleOfLot());
		if (!Subsystem || !Character)
		{
			return -1;
		}
		FLotGame& Game = Subsystem->GetGame();
		Game.DebugGive(100000.0);
		Subsystem->BuyItem(ELotItem::Cart);
		ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
		if (!Cart)
		{
			return -1;
		}
		FillTo(Game, ELotMovable::Cart, 50.0);
		Wrapper.TickTestWorld(static_cast<float>(Step));
		Cart->Interact(Character);
		Character->SetUsing(true);
		Wrapper.TickTestWorld(static_cast<float>(Step));
		const int32 Steps = FMath::RoundToInt32(300.0 / (100.0 * Step));
		for (int32 I = 0; I < Steps; ++I)
		{
			// 100 cm/s: well below the laying speed.
			Move(Character, FVector2D(100.0 * Step, 0.0));
			Wrapper.TickTestWorld(static_cast<float>(Step));
		}
		return CountRawTexels(Game);
	}

	/**
	 * Lays a raw strip of the cart's width through the core's cart API, from the point LengthCm towards +X, and gives the cart back
	 * where it stood (no cart actor is involved; a cart that is owned already keeps its place and its content).
	 */
	void LayRawStripThroughCore(FLotGame& Game, FVector2D From, double LengthCm)
	{
		if (!Game.OwnsMovable(ELotMovable::Cart))
		{
			Game.DebugGrantItem(ELotItem::Cart);
		}
		FillTo(Game, ELotMovable::Cart, 150.0);
		const FLotMovable Before = Game.World().Movables[static_cast<int32>(ELotMovable::Cart)];
		Game.Grab(ELotMovable::Cart);
		Game.LayCartStrip(From, From + FVector2D(LengthCm, 0.0));
		Game.Release(Before.Pos, Before.YawDeg);
	}

	/** Gives the player the item without the unlock the shop would ask for, and announces it like a purchase (the actor appears). */
	bool Grant(UDIAWorldSubsystem* Subsystem, ELotItem Item)
	{
		Subsystem->GetGame().DebugGrantItem(Item);
		Subsystem->OnItemBought.Broadcast(Item);
		Subsystem->OnStateChanged.Broadcast();
		return Subsystem->GetGame().Owns(Item);
	}

	/** The start of the raw strip the finisher tests use: on a cell edge in the middle of the lot, so that 5 m of it are exactly 5 cells. */
	FVector2D RawStripStart()
	{
		return MiddleOfLot() - FVector2D(AsphaltLot::CellCm * 0.5, 0.0);
	}

	/**
	 * Walks the character DistanceCm along +X at the speed, with tick sizes taken in turn from Steps. Each tick moves the character by
	 * what the speed allows for the PREVIOUS tick: the held actor then samples a pose one frame old, as it would when it ticked before
	 * the character (a long frame followed by a short one is the worst case).
	 */
	void WalkWithLaggedPose(FTestWorldWrapper& Wrapper, ADIACharacter* Character, double SpeedCmS, double DistanceCm, const TArray<double>& Steps)
	{
		double PreviousStep = Steps[0];
		double Walked = 0.0;
		for (int32 I = 0; Walked < DistanceCm; ++I)
		{
			const double Step = Steps[I % Steps.Num()];
			const double Distance = FMath::Min(SpeedCmS * PreviousStep, DistanceCm - Walked);
			Move(Character, FVector2D(Distance, 0.0));
			Walked += Distance;
			Wrapper.TickTestWorld(static_cast<float>(Step));
			PreviousStep = Step;
		}
	}

	/** Counts the cells the game announces as completed (what the HUD and the sounds listen to). */
	struct FCompletedCounter
	{
		explicit FCompletedCounter(UDIAWorldSubsystem* InSubsystem)
			: Subsystem(InSubsystem)
		{
			Handle = Subsystem->OnCellsCompleted.AddLambda([this](const TArray<int32>& Cells) { Count += Cells.Num(); });
		}
		~FCompletedCounter()
		{
			Subsystem->OnCellsCompleted.Remove(Handle);
		}
		UDIAWorldSubsystem* Subsystem;
		FDelegateHandle Handle;
		int32 Count = 0;
	};
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

/** A cart held with Use down: the cart gets somewhere in one jump (a teleport, a spin) and lays nothing on the way. */
DIA_TEST(FMovableNoStripOnATeleportTest, "DoItAsphalt.Game.Movable.NoStripOnATeleport")

bool FMovableNoStripOnATeleportTest::RunTest(const FString& Parameters)
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
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("standing still lays nothing"), CountRawTexels(Game), 0);
	const double Before = Game.GetMovableLitres(ELotMovable::Cart);

	// Eight metres in one tick, with Use down (dia.Teleport does not let go of it).
	Move(Character, FVector2D(800.0, 0.0));
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("a jump lays no strip between the two points"), CountRawTexels(Game), 0);
	TestEqual(TEXT("and spends nothing"), Game.GetMovableLitres(ELotMovable::Cart), Before);

	// The path starts again where the cart is now: the next 1 m of walking is laid.
	for (int32 I = 0; I < 10; ++I)
	{
		Move(Character, FVector2D(10.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	const double TexelsPerSquareMetre = 1.0e4 / FMath::Square(FLotGrid::TexelCm);
	const double Expected = 1.0 * (LotBalance::CartStripWidthCm / 100.0) * TexelsPerSquareMetre;
	TestTrue(TEXT("walking on lays the strip again"), FMath::Abs(CountRawTexels(Game) - Expected) <= Expected * 0.2);
	return true;
}

DIA_TEST(FMovableNoStripOnASpinTest, "DoItAsphalt.Game.Movable.NoStripOnASpin")

bool FMovableNoStripOnASpinTest::RunTest(const FString& Parameters)
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
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);

	// One turn in a second, in place: the mouth is 1.7 m from the player and sweeps about 1 m per tick.
	const double Before = Game.GetMovableLitres(ELotMovable::Cart);
	for (int32 I = 0; I < 10; ++I)
	{
		Character->SetActorRotation(FRotator(0.0, 36.0 * (I + 1), 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	// What a second at the laying speed could lay: a strip of that length, with the stamps' padding.
	const double MostTexels = (LotBalance::CartLaySpeedCmS * 1.0 + 20.0) * LotBalance::CartStripWidthCm / FMath::Square(FLotGrid::TexelCm);
	TestTrue(TEXT("a spin lays no more than the laying speed allows"), CountRawTexels(Game) <= MostTexels);
	TestTrue(TEXT("in fact nothing"), CountRawTexels(Game) == 0 && Game.GetMovableLitres(ELotMovable::Cart) == Before);
	return true;
}

DIA_TEST(FMovableStripDoesNotDependOnTheTickTest, "DoItAsphalt.Game.Movable.StripDoesNotDependOnTheTick")

bool FMovableStripDoesNotDependOnTheTickTest::RunTest(const FString& Parameters)
{
	const int32 Fast = RawTexelsOfAWalk(*this, 0.005);
	const int32 Slow = RawTexelsOfAWalk(*this, 0.1);
	if (!TestTrue(TEXT("both worlds ran"), Fast > 0 && Slow > 0))
	{
		return false;
	}
	TestTrue(*FString::Printf(TEXT("the same strip at 200 Hz (%d) and at 10 Hz (%d)"), Fast, Slow), FMath::Abs(Fast - Slow) <= Slow * 0.1);
	return true;
}

DIA_TEST(FMovableNewGameWhileHoldingTest, "DoItAsphalt.Game.Movable.NewGameWhileHolding")

bool FMovableNewGameWhileHoldingTest::RunTest(const FString& Parameters)
{
	FMovableTestSlot Slot;
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Cart->Interact(Character);
	Character->SetUsing(true);
	TickFor(Wrapper, 0.3);
	TestTrue(TEXT("holding the cart"), Game.GetHeld() == ELotMovable::Cart);
	TestNearlyEqual(TEXT("slow while laying"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::CartLaySpeedCmS, 0.01);
	TestFalse(TEXT("the hand tool is hidden"), Character->GetHandMesh()->IsVisible());

	Subsystem->NewGame();
	TickFor(Wrapper, 0.3);
	TestEqual(TEXT("a new game: no movable stands"), FindMovables(World).Num(), 0);
	TestTrue(TEXT("nothing is held"), Game.GetHeld() == ELotMovable::Count);
	TestNearlyEqual(TEXT("walking speed again"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::WalkSpeedCmS, 0.01);
	TestTrue(TEXT("the hand tool is back"), Character->GetHandMesh()->IsVisible());
	return true;
}

DIA_TEST(FMovableSaveLoadWhileHoldingTest, "DoItAsphalt.Game.Movable.SaveLoadWhileHolding")

bool FMovableSaveLoadWhileHoldingTest::RunTest(const FString& Parameters)
{
	FMovableTestSlot Slot;
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	Cart->Interact(Character);
	TickFor(Wrapper, 0.3);
	TestTrue(TEXT("holding the cart"), Game.GetHeld() == ELotMovable::Cart);
	Subsystem->SaveGame();

	// The game moves on: the cart is put down and the player walks off.
	Cart->Interact(Character);
	TestTrue(TEXT("let go"), Game.GetHeld() == ELotMovable::Count);
	Move(Character, FVector2D(0.0, 600.0));
	TickFor(Wrapper, 0.3);

	if (!TestTrue(TEXT("the game loads"), Subsystem->LoadGame()))
	{
		return false;
	}
	TickFor(Wrapper, 0.3);
	TestTrue(TEXT("the core says the cart is held"), Game.GetHeld() == ELotMovable::Cart);
	TestEqual(TEXT("exactly one actor"), FindMovables(World).Num(), 1);
	ADIAMovable* Loaded = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart actor"), Loaded))
	{
		return false;
	}
	TestTrue(TEXT("the actor is held"), Loaded->IsHeld());
	TestEqual(TEXT("without collision"), Loaded->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// It follows the player, wherever the player is now.
	Move(Character, FVector2D(300.0, 0.0));
	TickFor(Wrapper, 0.3);
	TestTrue(TEXT("it follows the player"), FVector2D::Distance(Where(Loaded), Where(Character)) < 150.0);
	Loaded->Interact(Character);
	TestTrue(TEXT("it can be let go"), Game.GetHeld() == ELotMovable::Count);
	TestEqual(TEXT("and stands with its collision"), Loaded->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	return true;
}

DIA_TEST(FMovableBuyWhileHoldingTest, "DoItAsphalt.Game.Movable.BuyWhileHolding")

bool FMovableBuyWhileHoldingTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	Cart->Interact(Character);
	TickFor(Wrapper, 0.3);
	TestTrue(TEXT("buy the suitcase while holding the cart"), Buy(ELotItem::Suitcase));
	TickFor(Wrapper, 0.3);
	TestEqual(TEXT("one actor each"), FindMovables(World).Num(), 2);
	TestTrue(TEXT("the same cart actor is still there"), FindMovable(World, ELotMovable::Cart) == Cart);
	TestTrue(TEXT("the cart is still held"), Game.GetHeld() == ELotMovable::Cart && Cart->IsHeld());
	ADIAMovable* Suitcase = FindMovable(World, ELotMovable::Suitcase);
	if (!TestNotNull(TEXT("the suitcase"), Suitcase))
	{
		return false;
	}
	TestFalse(TEXT("the suitcase is not held"), Suitcase->IsHeld());
	TestTrue(TEXT("it stands at its spawn point"), Where(Suitcase).Equals(LotLayout::ItemSpawn(ELotItem::Suitcase), 0.5));
	TestEqual(TEXT("with its collision"), Suitcase->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	return true;
}

DIA_TEST(FMovableReleaseRefusedAtTheSiteTest, "DoItAsphalt.Game.Movable.ReleaseRefusedAtTheSite")

bool FMovableReleaseRefusedAtTheSiteTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	ADIAHUD* Hud = Possess(World, Character);
	if (!TestNotNull(TEXT("the cart"), Cart) || !TestNotNull(TEXT("a HUD"), Hud))
	{
		return false;
	}
	Place(Character, MiddleOfLot(), 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("holding the cart"), Game.GetHeld() == ELotMovable::Cart);

	struct FSpot
	{
		const TCHAR* Name;
		FVector2D Where;
		double YawDeg;
	};
	const FSpot Spots[] = {
		{TEXT("the site wall"), FVector2D(-3000.0, -AsphaltLot::HalfExtentCm - LotLayout::StripCm + 100.0), -90.0},
		{TEXT("the trough"), FVector2D(LotLayout::TroughPos.X, LotLayout::TroughPos.Y + 80.0), -90.0},
		{TEXT("the mall wall"), FVector2D(LotLayout::MallCentre.X, -100.0), 90.0},
	};
	for (const FSpot& Spot : Spots)
	{
		Character->SetActorLocation(FVector(Spot.Where.X, Spot.Where.Y, DIACharacter::CapsuleHalfHeightCm));
		Character->SetActorRotation(FRotator(0.0, Spot.YawDeg, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
		const int32 Toasts = Hud->GetToasts().Num();
		Cart->Interact(Character);
		TestTrue(*FString::Printf(TEXT("%s: the cart stays held"), Spot.Name), Game.GetHeld() == ELotMovable::Cart && Cart->IsHeld());
		TestTrue(*FString::Printf(TEXT("%s: the toast says there is no room"), Spot.Name), Hud->GetToasts().Num() == Toasts + 1);

		// A free spot: it can be set down, and picked up again for the next one.
		Character->SetActorLocation(FVector(MiddleOfLot().X, MiddleOfLot().Y, DIACharacter::CapsuleHalfHeightCm));
		Character->SetActorRotation(FRotator::ZeroRotator);
		Wrapper.TickTestWorld(TickSeconds);
		Cart->Interact(Character);
		TestTrue(*FString::Printf(TEXT("%s: let go on free ground"), Spot.Name), Game.GetHeld() == ELotMovable::Count);
		Cart->Interact(Character);
		TestTrue(*FString::Printf(TEXT("%s: held again"), Spot.Name), Game.GetHeld() == ELotMovable::Cart);
	}
	return true;
}

DIA_TEST(FMovableWalkingSpeedTest, "DoItAsphalt.Game.Movable.WalkingSpeedWithTheCart")

bool FMovableWalkingSpeedTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	// Without a controller nothing drives the movement component, unless it is told to run its physics anyway.
	Movement->bRunPhysicsWithNoController = true;
	Movement->SetMovementMode(MOVE_Walking);
	Place(Character, FVector2D(-9000.0, -9000.0), 0.0, 0.0);
	Cart->Interact(Character);

	// Walks along +X for the seconds with the movement component itself and returns the speed over the last second.
	auto Walk = [&](bool bUse) -> double
	{
		Character->SetUsing(bUse);
		Character->SetActorRotation(FRotator::ZeroRotator);
		TickFor(Wrapper, 0.1);
		// A second to get up to speed, then the second that is measured.
		for (int32 I = 0; I < 20; ++I)
		{
			Character->AddMovementInput(FVector::ForwardVector, 1.0f);
			Wrapper.TickTestWorld(0.05f);
		}
		const double MidX = Character->GetActorLocation().X;
		for (int32 I = 0; I < 20; ++I)
		{
			Character->AddMovementInput(FVector::ForwardVector, 1.0f);
			Wrapper.TickTestWorld(0.05f);
		}
		return Character->GetActorLocation().X - MidX;
	};
	const double Pushing = Walk(false);
	const double Laying = Walk(true);
	TestTrue(*FString::Printf(TEXT("pushing the cart: %.0f cm in a second"), Pushing), FMath::Abs(Pushing - LotBalance::CartPushSpeedCmS) <= LotBalance::CartPushSpeedCmS * 0.1);
	TestTrue(*FString::Printf(TEXT("laying a strip: %.0f cm in a second"), Laying), FMath::Abs(Laying - LotBalance::CartLaySpeedCmS) <= LotBalance::CartLaySpeedCmS * 0.1);
	return true;
}

/** The portable station: set down it is an outlet and a refill zone like the trough; held it is neither and walks at the carrying speed. */
DIA_TEST(FMovableStationMakesRefillZoneTest, "DoItAsphalt.Game.Movable.StationMakesRefillZone")

bool FMovableStationMakesRefillZoneTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the station"), Grant(Subsystem, ELotItem::Station));
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Station = FindMovable(World, ELotMovable::Station);
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the station"), Station) || !TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	// Set down far from the trough (the middle of the lot is some 70 m away).
	const FVector2D StationAt = MiddleOfLot();
	TestTrue(TEXT("far from the trough"), FVector2D::Distance(StationAt, LotLayout::TroughPos) > 5000.0);
	SetCorePos(Game, ELotMovable::Station, StationAt);
	Game.DebugSetPlantStock(Game.GetPlantStorage());
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the station is placed"), DIARefill::IsStationPlaced(Game));

	// A player with the bucket aims at it: Use fills the bucket as at the trough.
	Game.DebugGrantItem(ELotItem::Bucket);
	TestTrue(TEXT("take the bucket"), Character->SelectHand(ELotHand::Bucket));
	TickFor(Wrapper, LotBalance::SwapSeconds + 0.5);
	const double Pitch = -FMath::RadiansToDegrees(FMath::Atan2(DIACharacter::EyeHeightCm, 100.0));
	Place(Character, StationAt - FVector2D(100.0, 0.0), Pitch, 0.0);
	TestTrue(TEXT("the aim is on the station"), Station->ContainsPoint(Character->GetAimPoint()));
	TestEqual(TEXT("the bucket is empty"), Game.GetHandLitres(ELotHand::Bucket), 0.0);
	Character->SetUsing(true);
	TickFor(Wrapper, 1.0);
	Character->SetUsing(false);
	TestTrue(TEXT("one second at the station fills the bucket"), Game.GetHandLitres(ELotHand::Bucket) > 1.0);

	// A cart set down within the refill radius fills itself.
	SetCorePos(Game, ELotMovable::Cart, StationAt + FVector2D(0.0, LotLayout::RefillRadiusCm - 50.0));
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("the cart is in the refill zone"), DIARefill::IsInZone(Game, Where(Cart)));
	const double Before = Game.GetMovableLitres(ELotMovable::Cart);
	TickFor(Wrapper, 5.0);
	TestTrue(TEXT("the cart fills itself beside the station"), Game.GetMovableLitres(ELotMovable::Cart) > Before + 1.0);

	// Held, the station is no outlet and no zone: the cart beside it stays as full as it is.
	Place(Character, StationAt - FVector2D(100.0, 0.0), 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	Station->Interact(Character);
	TestTrue(TEXT("holding the station"), Game.GetHeld() == ELotMovable::Station);
	TestFalse(TEXT("a held station is no outlet"), DIARefill::IsAtOutlet(Game, StationAt));
	TestFalse(TEXT("and no refill zone"), DIARefill::IsInZone(Game, StationAt));
	const double Held = Game.GetMovableLitres(ELotMovable::Cart);
	TickFor(Wrapper, 5.0);
	TestEqual(TEXT("a cart no longer fills beside a held station"), Game.GetMovableLitres(ELotMovable::Cart), Held);

	// Carried at the carrying speed; sprint does nothing.
	Character->SetSprinting(true);
	Wrapper.TickTestWorld(TickSeconds);
	TestNearlyEqual(TEXT("carrying the station"), static_cast<double>(Character->GetCharacterMovement()->MaxWalkSpeed), LotBalance::StationCarrySpeedCmS, 0.01);
	return true;
}

/** The finisher finishes only with the bar lowered (held and Use down); its walking speed is capped then, whatever the player does. */
DIA_TEST(FMovableFinisherSpeedCapTest, "DoItAsphalt.Game.Movable.FinisherSpeedCap")

bool FMovableFinisherSpeedCapTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	if (!TestNotNull(TEXT("the finisher"), Finisher))
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
	Finisher->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("holding the finisher"), Game.GetHeld() == ELotMovable::Finisher);
	TestNearlyEqual(TEXT("bar raised: pushed at the pushing speed, sprint does not help"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::FinisherPushSpeedCmS, 0.01);
	Character->SetUsing(true);
	TestNearlyEqual(TEXT("bar lowered: capped at the finishing speed"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::FinisherSpeedCmS(), 0.01);
	TestNearlyEqual(TEXT("about 141.7 cm/s"), static_cast<double>(Movement->MaxWalkSpeed), 141.7, 0.1);
	Character->SetSprinting(false);
	TestNearlyEqual(TEXT("not sprinting: the same"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::FinisherSpeedCmS(), 0.01);
	Character->SetUsing(false);
	TestNearlyEqual(TEXT("bar raised again"), static_cast<double>(Movement->MaxWalkSpeed), LotBalance::FinisherPushSpeedCmS, 0.01);
	return true;
}

/** A raw strip 1 x 5 m, a player guiding the finisher over it with Use down: five finished cells and their credits. */
DIA_TEST(FMovableFinisherCompletesRawStripTest, "DoItAsphalt.Game.Movable.FinisherCompletesRawStrip")

bool FMovableFinisherCompletesRawStripTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	if (!TestNotNull(TEXT("the finisher"), Finisher))
	{
		return false;
	}
	const FVector2D Start = RawStripStart();
	LayRawStripThroughCore(Game, Start, 5.0 * AsphaltLot::CellCm);
	TestTrue(TEXT("the strip is raw"), CountRawTexels(Game) > 0);
	TestEqual(TEXT("raw asphalt completes nothing"), Game.GetCompletedCells(), 0);

	// The player stands behind the start of the strip, the bar ahead of the player, and walks the strip with the bar lowered.
	Place(Character, Start - FVector2D(300.0, 0.0), 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	Finisher->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	FCompletedCounter Counter(Subsystem);
	const double CreditsBefore = Game.GetCredits();
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);
	const double Reach = (Where(Finisher) - Where(Character)).X;
	const double Walk = 300.0 + 5.0 * AsphaltLot::CellCm + 200.0 - Reach;
	for (int32 I = 0; I < FMath::RoundToInt32(Walk / 10.0); ++I)
	{
		Move(Character, FVector2D(10.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	Character->SetUsing(false);
	TestEqual(TEXT("five cells are finished"), Game.GetCompletedCells(), 5);
	TestEqual(TEXT("and announced"), Counter.Count, 5);
	TestNearlyEqual(TEXT("they pay 10 credits each"), Game.GetCredits() - CreditsBefore, 5.0 * LotBalance::CreditsPerCell, 1e-6);
	TestEqual(TEXT("nothing raw is left"), CountRawTexels(Game), 0);
	return true;
}

/** The same walk with the bar raised finishes nothing. */
DIA_TEST(FMovableFinisherBarRaisedFinishesNothingTest, "DoItAsphalt.Game.Movable.FinisherBarRaisedFinishesNothing")

bool FMovableFinisherBarRaisedFinishesNothingTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	if (!TestNotNull(TEXT("the finisher"), Finisher))
	{
		return false;
	}
	const FVector2D Start = RawStripStart();
	LayRawStripThroughCore(Game, Start, 5.0 * AsphaltLot::CellCm);
	const int32 Raw = CountRawTexels(Game);
	Place(Character, Start - FVector2D(300.0, 0.0), 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	Finisher->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	for (int32 I = 0; I < 90; ++I)
	{
		Move(Character, FVector2D(10.0, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
	}
	TestTrue(TEXT("the bar went over the whole strip"), Where(Finisher).X > Start.X + 5.0 * AsphaltLot::CellCm);
	TestEqual(TEXT("raised, it finishes nothing"), CountRawTexels(Game), Raw);
	TestEqual(TEXT("no cell is complete"), Game.GetCompletedCells(), 0);

	// Let go with the button down: a finisher that stands finishes nothing either.
	Character->SetUsing(true);
	Finisher->Interact(Character);
	TestTrue(TEXT("let go"), Game.GetHeld() == ELotMovable::Count);
	Move(Character, FVector2D(-600.0, 0.0));
	TickFor(Wrapper, 1.0);
	TestEqual(TEXT("a standing finisher finishes nothing"), CountRawTexels(Game), Raw);
	return true;
}

/** The bar teleported over a raw strip (a jump, not a walk) finishes nothing on the way: the finisher has the cart's bound. */
DIA_TEST(FMovableFinisherNoPassOnATeleportTest, "DoItAsphalt.Game.Movable.FinisherNoPassOnATeleport")

bool FMovableFinisherNoPassOnATeleportTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	if (!TestNotNull(TEXT("the finisher"), Finisher))
	{
		return false;
	}
	const FVector2D Start = RawStripStart();
	LayRawStripThroughCore(Game, Start, 5.0 * AsphaltLot::CellCm);
	const int32 Raw = CountRawTexels(Game);
	Place(Character, Start - FVector2D(1000.0, 0.0), 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	Finisher->Interact(Character);
	Character->SetUsing(true);
	Wrapper.TickTestWorld(TickSeconds);
	Wrapper.TickTestWorld(TickSeconds);
	// Ten metres in one tick, straight over the strip.
	Move(Character, FVector2D(1200.0, 0.0));
	Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("a jump over the strip finishes nothing"), CountRawTexels(Game), Raw);
	TestEqual(TEXT("no cell is complete"), Game.GetCompletedCells(), 0);
	return true;
}

/** A walk at the capped speed whose ticks alternate long and short, with the pose one frame old: no segment is dropped (cart and finisher). */
DIA_TEST(FMovableToolsSurviveUnevenTicksTest, "DoItAsphalt.Game.Movable.ToolsSurviveUnevenTicks")

bool FMovableToolsSurviveUnevenTicksTest::RunTest(const FString& Parameters)
{
	const TArray<double> Uneven = {0.1, 0.016};
	{
		DIA_MOVABLE_FIXTURE()
		TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
		ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
		if (!TestNotNull(TEXT("the cart"), Cart))
		{
			return false;
		}
		FillTo(Game, ELotMovable::Cart, 100.0);
		Wrapper.TickTestWorld(TickSeconds);
		Cart->Interact(Character);
		Character->SetUsing(true);
		Wrapper.TickTestWorld(static_cast<float>(Uneven[0]));
		WalkWithLaggedPose(Wrapper, Character, LotBalance::CartLaySpeedCmS, 600.0, Uneven);
		const double TexelsPerSquareMetre = 1.0e4 / FMath::Square(FLotGrid::TexelCm);
		const double Expected = 6.0 * (LotBalance::CartStripWidthCm / 100.0) * TexelsPerSquareMetre;
		TestTrue(*FString::Printf(TEXT("the cart's strip has no gaps (%d of about %.0f texels)"), CountRawTexels(Game), Expected), FMath::Abs(CountRawTexels(Game) - Expected) <= Expected * 0.1);
	}
	{
		DIA_MOVABLE_FIXTURE()
		TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
		ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
		if (!TestNotNull(TEXT("the finisher"), Finisher))
		{
			return false;
		}
		const FVector2D Start = RawStripStart();
		LayRawStripThroughCore(Game, Start, 5.0 * AsphaltLot::CellCm);
		Place(Character, Start - FVector2D(300.0, 0.0), 0.0, 0.0);
		Wrapper.TickTestWorld(TickSeconds);
		Finisher->Interact(Character);
		Character->SetUsing(true);
		Wrapper.TickTestWorld(static_cast<float>(Uneven[0]));
		WalkWithLaggedPose(Wrapper, Character, LotBalance::FinisherSpeedCmS(), 1000.0, Uneven);
		TestEqual(TEXT("the finisher finishes the whole strip"), Game.GetCompletedCells(), 5);
		TestEqual(TEXT("with no raw texel left"), CountRawTexels(Game), 0);
	}
	return true;
}

/** The held actor ticks after the character (and its movement), so that the pose it follows is this frame's; letting go removes the order. */
DIA_TEST(FMovableTicksAfterTheCharacterTest, "DoItAsphalt.Game.Movable.TicksAfterTheCharacter")

bool FMovableTicksAfterTheCharacterTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	auto Follows = [&](const FTickFunction& Function)
	{
		for (const FTickPrerequisite& Prerequisite : Cart->PrimaryActorTick.GetPrerequisites())
		{
			if (Prerequisite.Get() == &Function)
			{
				return true;
			}
		}
		return false;
	};
	TestFalse(TEXT("a standing cart waits for nobody"), Follows(Character->PrimaryActorTick));
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("held: after the character"), Follows(Character->PrimaryActorTick));
	TestTrue(TEXT("and after its movement"), Follows(Character->GetCharacterMovement()->PrimaryComponentTick));
	Wrapper.TickTestWorld(TickSeconds);
	Move(Character, FVector2D(-500.0, 0.0));
	Wrapper.TickTestWorld(TickSeconds);
	Cart->Interact(Character);
	TestTrue(TEXT("let go"), Game.GetHeld() == ELotMovable::Count);
	TestFalse(TEXT("let go: no order any more"), Follows(Character->PrimaryActorTick));
	TestFalse(TEXT("nor on the movement"), Follows(Character->GetCharacterMovement()->PrimaryComponentTick));
	return true;
}

/** The held finisher: bar ahead and across the view, its rear edge clear of the player's capsule already while held; let go, it stays where it was held and the player can walk away in every direction. */
DIA_TEST(FMovableFinisherReleaseClearOfPlayerTest, "DoItAsphalt.Game.Movable.FinisherReleaseClearOfPlayer")

bool FMovableFinisherReleaseClearOfPlayerTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	if (!TestNotNull(TEXT("the finisher"), Finisher))
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	for (const double YawDeg : {0.0, 90.0, 215.0})
	{
		Place(Character, MiddleOfLot(), 0.0, YawDeg);
		Character->SetActorRotation(FRotator(0.0, YawDeg, 0.0));
		Wrapper.TickTestWorld(TickSeconds);
		Finisher->Interact(Character);
		Wrapper.TickTestWorld(TickSeconds);
		TestTrue(TEXT("holding the finisher"), Game.GetHeld() == ELotMovable::Finisher);

		// The bar is ahead of the player and across the direction of walking.
		const FVector2D Forward(FMath::Cos(FMath::DegreesToRadians(YawDeg)), FMath::Sin(FMath::DegreesToRadians(YawDeg)));
		TestTrue(TEXT("the bar is ahead of the player"), FVector2D::DotProduct(Where(Finisher) - Where(Character), Forward) > 50.0);
		TestNearlyEqual(TEXT("the machine points the way the player walks"), static_cast<double>(FVector2D::DotProduct(FVector2D(Finisher->GetActorForwardVector()), Forward)), 1.0, 1e-3);
		TestTrue(TEXT("the bar's centre is the machine's origin"), Finisher->GetMouthPoint().Equals(Where(Finisher), 1e-3));

		// The footprint is clear of the capsule while held, so that letting go needs no push forward.
		TestTrue(TEXT("held: the rear edge is clear of the capsule"), Finisher->GetDistanceToFootprint(Where(Character)) >= DIACharacter::CapsuleRadiusCm);
		AActor* Found = nullptr;
		TestTrue(TEXT("E finds the held finisher"), Subsystem->FindInteractable(Character, &Found) != nullptr && Found == Finisher);

		const FVector2D HeldAt = Where(Finisher);
		Finisher->Interact(Character);
		TestTrue(TEXT("let go"), Game.GetHeld() == ELotMovable::Count);
		TestTrue(TEXT("it stands where it was held"), Where(Finisher).Equals(HeldAt, 0.1));
		const FVector Centre = Character->GetActorLocation();
		TestFalse(TEXT("the player's capsule does not overlap the finisher"), Finisher->GetCollision()->OverlapComponent(Centre, FQuat::Identity, Capsule->GetCollisionShape()));
		TestTrue(TEXT("and E reaches it again"), Subsystem->FindInteractable(Character, &Found) != nullptr && Found == Finisher);
		for (int32 Direction = 0; Direction < 8; ++Direction)
		{
			const double Angle = FMath::DegreesToRadians(45.0 * Direction);
			const FVector Step = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 40.0;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MovableWalkAway), false, Character);
			World->SweepSingleByChannel(Hit, Centre, Centre + Step, FQuat::Identity, ECC_Pawn, Capsule->GetCollisionShape(), Params);
			TestFalse(*FString::Printf(TEXT("yaw %.0f, direction %d: not stuck in the finisher"), YawDeg, Direction), Hit.bStartPenetrating && Hit.GetActor() == Finisher);
		}
	}
	return true;
}

namespace MovableTestSupport
{
	/** An interactable of someone else with a prompt of its own, standing in range of the player. */
	struct FOtherThing : public IDIAInteractable
	{
		int32 Uses = 0;
		virtual FText GetPrompt(const ADIACharacter* By) const override { return FText::FromString(TEXT("E - Other")); }
		virtual void Interact(ADIACharacter* By) override { ++Uses; }
	};

	/** A blocking box of half side HalfCm on the ground at Where (a wall, a crate). */
	AActor* SpawnBlock(UWorld* World, FVector2D Where, double HalfCm)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(Where.X, Where.Y, 0.0)), Params);
		if (!Actor)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Box"));
		Box->SetBoxExtent(FVector(HalfCm, HalfCm, 100.0));
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->SetRootComponent(Box);
		Box->RegisterComponent();
		Actor->SetActorLocation(FVector(Where.X, Where.Y, 100.0));
		return Actor;
	}
}

/** While something is held, E addresses the held movable: whatever the player looks at, whatever else is in range. */
DIA_TEST(FMovableHeldIsAlwaysAddressedTest, "DoItAsphalt.Game.Movable.HeldIsAlwaysAddressed")

bool FMovableHeldIsAlwaysAddressedTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the cart"), Buy(ELotItem::Cart));
	TestTrue(TEXT("grant the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Cart = FindMovable(World, ELotMovable::Cart);
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	ADIAHUD* Hud = Possess(World, Character);
	if (!TestNotNull(TEXT("the cart"), Cart) || !TestNotNull(TEXT("the finisher"), Finisher) || !TestNotNull(TEXT("a HUD"), Hud))
	{
		return false;
	}
	ADIAPlayerController* Controller = Cast<ADIAPlayerController>(Character->GetController());
	if (!TestNotNull(TEXT("the controller"), Controller))
	{
		return false;
	}
	// Someone else's interactable stands right beside the player, nearer than anything held.
	const FVector2D Spot = MiddleOfLot();
	AActor* OtherActor = SpawnBlock(World, Spot + FVector2D(0.0, 90.0), 20.0);
	FOtherThing Other;
	if (!TestNotNull(TEXT("the other actor"), OtherActor))
	{
		return false;
	}
	Subsystem->RegisterInteractable(OtherActor, &Other);

	for (ADIAMovable* Movable : {Cart, Finisher})
	{
		const FString Name = Movable == Cart ? TEXT("cart") : TEXT("finisher");
		// Each round on fresh ground (what was let go in the round before stands in the way), the other thing beside the player.
		const FVector2D Here = Spot + FVector2D(0.0, 1000.0 * (Movable == Cart ? 0 : 1));
		Place(Character, Here, 0.0, 0.0);
		OtherActor->SetActorLocation(FVector(Here.X, Here.Y + 90.0, 100.0));
		Wrapper.TickTestWorld(TickSeconds);
		TestTrue(*(Name + TEXT(": the other thing is offered when nothing is held")), Controller->FindInteractable() == &Other);
		Movable->Interact(Character);
		TestTrue(*(Name + TEXT(": held")), Game.GetHeld() == Movable->GetKind());
		for (const double Pitch : {-89.0, 0.0, 89.0})
		{
			Character->GetCamera()->SetWorldRotation(FRotator(Pitch, 0.0, 0.0));
			Wrapper.TickTestWorld(TickSeconds);
			IDIAInteractable* Found = Controller->FindInteractable();
			TestTrue(*FString::Printf(TEXT("%s, pitch %.0f: E addresses the held movable"), *Name, Pitch), Found == static_cast<IDIAInteractable*>(Movable));
			TestEqual(*FString::Printf(TEXT("%s, pitch %.0f: the prompt is its own"), *Name, Pitch), Found ? Found->GetPrompt(Character).ToString() : FString(), FString(TEXT("E – Let go")));
		}
		Character->GetCamera()->SetWorldRotation(FRotator(-89.0, 0.0, 0.0));
		Controller->OnInteract();
		TestTrue(*(Name + TEXT(": E let it go")), Game.GetHeld() == ELotMovable::Count);
		TestEqual(*(Name + TEXT(": the other thing was not used")), Other.Uses, 0);
	}
	// A menu covers the screen: nothing is used behind it, the held movable included.
	Place(Character, Spot, 0.0, 0.0);
	Cart->Interact(Character);
	Controller->SetMenuInput(true);
	TestTrue(TEXT("with the menu open nothing can be used"), Controller->FindInteractable() == nullptr);
	Controller->SetMenuInput(false);
	Subsystem->UnregisterInteractable(OtherActor);
	return true;
}

/** The station is pulled behind the player with its handle towards the player; letting go uses the same free-footprint check. */
DIA_TEST(FMovableStationIsPulledBehindTest, "DoItAsphalt.Game.Movable.StationIsPulledBehind")

bool FMovableStationIsPulledBehindTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("grant the station"), Grant(Subsystem, ELotItem::Station));
	ADIAMovable* Station = FindMovable(World, ELotMovable::Station);
	if (!TestNotNull(TEXT("the station"), Station))
	{
		return false;
	}
	const FVector2D Spot = MiddleOfLot();
	Place(Character, Spot, 0.0, 0.0);
	Wrapper.TickTestWorld(TickSeconds);
	Station->Interact(Character);
	Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("holding the station"), Game.GetHeld() == ELotMovable::Station);
	TestTrue(TEXT("it is behind the player"), (Where(Station) - Where(Character)).X < -50.0);
	TestTrue(TEXT("its handle (model -X) points to the player"), Station->GetActorForwardVector().X < -0.99);
	TestTrue(TEXT("it is clear of the capsule"), Station->GetDistanceToFootprint(Where(Character)) >= DIACharacter::CapsuleRadiusCm);

	// Something stands where it would be set down: it stays held and the player is told.
	AActor* Block = SpawnBlock(World, Where(Station), 30.0);
	if (!TestNotNull(TEXT("a block"), Block))
	{
		return false;
	}
	Station->Interact(Character);
	TestTrue(TEXT("no room behind: still held"), Game.GetHeld() == ELotMovable::Station);
	Block->Destroy();
	Station->Interact(Character);
	TestTrue(TEXT("room: let go"), Game.GetHeld() == ELotMovable::Count);
	TestTrue(TEXT("it stands behind the player"), (Where(Station) - Where(Character)).X < -50.0);
	return true;
}

/** The prompts of the two machines. */
DIA_TEST(FMovableMachinePromptsTest, "DoItAsphalt.Game.Movable.MachinePrompts")

bool FMovableMachinePromptsTest::RunTest(const FString& Parameters)
{
	DIA_MOVABLE_FIXTURE()
	TestTrue(TEXT("buy the station"), Grant(Subsystem, ELotItem::Station));
	TestTrue(TEXT("buy the finisher"), Grant(Subsystem, ELotItem::Finisher));
	ADIAMovable* Station = FindMovable(World, ELotMovable::Station);
	ADIAMovable* Finisher = FindMovable(World, ELotMovable::Finisher);
	if (!TestNotNull(TEXT("the station"), Station) || !TestNotNull(TEXT("the finisher"), Finisher))
	{
		return false;
	}
	TestEqual(TEXT("a standing station"), Station->GetPrompt(Character).ToString(), FString(TEXT("E – Carry")));
	TestEqual(TEXT("a standing finisher"), Finisher->GetPrompt(Character).ToString(), FString(TEXT("E – Guide")));
	Station->Interact(Character);
	TestEqual(TEXT("a held station"), Station->GetPrompt(Character).ToString(), FString(TEXT("E – Put down")));
	TestTrue(TEXT("nothing else can be taken meanwhile"), Finisher->GetPrompt(Character).IsEmpty());
	Station->Interact(Character);
	Finisher->Interact(Character);
	TestEqual(TEXT("a held finisher"), Finisher->GetPrompt(Character).ToString(), FString(TEXT("E – Let go")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
