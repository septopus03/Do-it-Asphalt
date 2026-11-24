// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "UI/SDIAPaverMap.h"
#include "World/DIAAutoPaver.h"
#include "World/DIAGroundActor.h"

namespace AutoPaverTestSupport
{
	constexpr float TickSeconds = 0.1f;
	/** Half of a sector's side (cm). */
	constexpr double HalfSectorCm = AsphaltLot::SectorSide * AsphaltLot::CellCm * 0.5;

	/** The test slot for the length of a test: the real slot is never touched, and the override is gone afterwards. */
	struct FAutoPaverTestSlot
	{
		static const FString& Name()
		{
			static const FString SlotName(TEXT("DIA_Test_AutoPaver"));
			return SlotName;
		}
		FAutoPaverTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(Name());
			Delete();
		}
		~FAutoPaverTestSlot()
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

	/** What the tests share: a started world, the subsystem, a player with a controller and a HUD. */
	struct FAutoPaverScene
	{
		FTestWorldWrapper Wrapper;
		UWorld* World = nullptr;
		UDIAWorldSubsystem* Subsystem = nullptr;
		ADIACharacter* Character = nullptr;
		ADIAPlayerController* Controller = nullptr;
		ADIAHUD* Hud = nullptr;

		bool Start(FAutomationTestBase& Test, FVector2D PlayerAt = FVector2D::ZeroVector)
		{
			if (!DIATest::StartGameWorld(Wrapper, Test))
			{
				return false;
			}
			World = Wrapper.GetTestWorld();
			Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
			Character = DIATest::SpawnCharacter(World, PlayerAt);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<ADIAPlayerController>(Params);
			if (Controller && Character)
			{
				Controller->Possess(Character);
				Params.Owner = Controller;
				Hud = World->SpawnActor<ADIAHUD>(Params);
				Controller->MyHUD = Hud;
			}
			return Test.TestNotNull(TEXT("subsystem"), Subsystem) && Test.TestNotNull(TEXT("character"), Character)
				&& Test.TestNotNull(TEXT("controller"), Controller) && Test.TestNotNull(TEXT("HUD"), Hud);
		}

		FLotGame& Game() const { return Subsystem->GetGame(); }

		void TickFor(double Seconds)
		{
			const int32 Steps = FMath::RoundToInt32(Seconds / TickSeconds);
			for (int32 I = 0; I < Steps; ++I)
			{
				Wrapper.TickTestWorld(TickSeconds);
			}
		}

		/** Gives the player the auto paver without the unlock the shop would ask for, and announces it like a purchase. The tank is full. */
		void GrantPaver() const
		{
			Game().DebugGrantItem(ELotItem::AutoPaver);
			Game().SetAutoPaverAuto(false);
			Game().DebugFillTanks();
			Subsystem->OnItemBought.Broadcast(ELotItem::AutoPaver);
			Subsystem->OnStateChanged.Broadcast();
		}

		/** Gives the player the first Count robots (they are bought in order) and announces each like a purchase. The tanks are full. */
		void GrantPavers(int32 Count) const
		{
			Game().SetAutoPaverAuto(false);
			for (int32 I = 0; I < Count; ++I)
			{
				const ELotItem Item = AutoPaverItem[I];
				Game().DebugGrantItem(Item);
				Subsystem->OnItemBought.Broadcast(Item);
			}
			Game().DebugFillTanks();
			Subsystem->OnStateChanged.Broadcast();
		}

		/** The actor that shows robot Index; null if there is none. */
		ADIAAutoPaver* FindPaverOf(int32 Index) const
		{
			for (TActorIterator<ADIAAutoPaver> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed() && It->Index == Index)
				{
					return *It;
				}
			}
			return nullptr;
		}

		ADIAGroundActor* Ground() const
		{
			TActorIterator<ADIAGroundActor> It(World);
			return It ? *It : nullptr;
		}

		void StandAt(FVector2D Where) const
		{
			Character->SetActorLocation(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm));
		}

		/** The text of the newest toast; empty if there is none. */
		FString LastToast() const
		{
			const int32 Num = Hud->GetToasts().Num();
			return Num > 0 ? Hud->GetToasts().GetText(Num - 1).ToString() : FString();
		}

		/** Finishes every cell of the sector through the core (what the paver would do with enough time and asphalt). */
		void CompleteSector(int32 Sector) const
		{
			for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
			{
				Game().DebugCompleteCell(AsphaltLot::SectorCell(Sector, Step));
			}
		}

		/** True if SectorRect is exactly the rectangle (min X, min Y, max X, max Y). */
		bool RectIs(double MinX, double MinY, double MaxX, double MaxY) const
		{
			const FVector4 Rect = SectorRect();
			return Rect.X == static_cast<float>(MinX) && Rect.Y == static_cast<float>(MinY) && Rect.Z == static_cast<float>(MaxX) && Rect.W == static_cast<float>(MaxY);
		}

		int32 CountPavers() const
		{
			int32 Count = 0;
			for (TActorIterator<ADIAAutoPaver> It(World); It; ++It)
			{
				Count += It->IsActorBeingDestroyed() ? 0 : 1;
			}
			return Count;
		}

		ADIAAutoPaver* FindPaver() const
		{
			for (TActorIterator<ADIAAutoPaver> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					return *It;
				}
			}
			return nullptr;
		}

		FVector2D Shown(const AActor* Actor) const { return FVector2D(Actor->GetActorLocation()); }

		/** SectorRect of the ground's material as min X, min Y, max X, max Y; all -1 if there is no such parameter. */
		FVector4 SectorRect() const
		{
			TActorIterator<ADIAGroundActor> It(World);
			FLinearColor Value(-1.f, -1.f, -1.f, -1.f);
			if (It && It->GetGroundMaterial())
			{
				It->GetGroundMaterial()->GetVectorParameterValue(TEXT("SectorRect"), Value);
			}
			return FVector4(Value.R, Value.G, Value.B, Value.A);
		}
	};
}

DIA_TEST(FAutoPaverSpawnsTest, "DoItAsphalt.Game.AutoPaver.SpawnsWhenBought")

bool FAutoPaverSpawnsTest::RunTest(const FString& Parameters)
{
	AutoPaverTestSupport::FAutoPaverTestSlot Slot;
	AutoPaverTestSupport::FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	TestEqual(TEXT("no paver without the item"), Scene.CountPavers(), 0);

	// A purchase through the shop's path.
	Scene.Game().DebugGive(100000.0);
	Scene.Game().DebugCompleteCells(AsphaltLot::NumCells);
	TestTrue(TEXT("the shop sells the auto paver"), Scene.Subsystem->BuyItem(ELotItem::AutoPaver));
	Scene.Wrapper.TickTestWorld(AutoPaverTestSupport::TickSeconds);
	TestEqual(TEXT("one paver after the purchase"), Scene.CountPavers(), 1);

	ADIAAutoPaver* Paver = Scene.FindPaver();
	if (!TestNotNull(TEXT("the paver"), Paver))
	{
		return false;
	}
	// The body: no collision, the model 1.5 times larger, standing on the ground.
	UStaticMeshComponent* Body = Paver->GetBodyMesh();
	TestEqual(TEXT("no collision"), Body->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	const FBoxSphereBounds Bounds = Body->Bounds;
	// It stands in its dock turned by PaverDockYawDeg (90): the length of the body lies along Y of the world.
	TestEqual(TEXT("the body is as long as the model is, 1.5 times larger"), Bounds.BoxExtent.Y * 2.0, static_cast<double>(DIAModelSizes::AutoPaverCm.X * DIAAutoPaver::ModelScale), 3.0);
	TestEqual(TEXT("the body stands on the ground"), Bounds.Origin.Z - Bounds.BoxExtent.Z, 0.0, 1.0);

	// A second announcement of the same purchase never makes a second actor.
	Scene.Subsystem->OnItemBought.Broadcast(ELotItem::AutoPaver);
	TestEqual(TEXT("still one paver"), Scene.CountPavers(), 1);

	// A saved game with the paver brings exactly one back; a new game removes it.
	const TArray<uint8> Blob = Scene.Game().SaveToBytes();
	Scene.Subsystem->NewGame();
	TestEqual(TEXT("none after a new game"), Scene.CountPavers(), 0);
	TestTrue(TEXT("the blob is written"), UDIAWorldSubsystem::WriteSlot(UDIAWorldSubsystem::SlotName(), Blob));
	TestTrue(TEXT("the game loads"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("one after a load"), Scene.CountPavers(), 1);
	TestTrue(TEXT("a load of the game that has it keeps one"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("one after a second load"), Scene.CountPavers(), 1);
	Scene.Subsystem->NewGame();
	TestEqual(TEXT("none after a second new game"), Scene.CountPavers(), 0);
	return true;
}

DIA_TEST(FAutoPaverFollowsTest, "DoItAsphalt.Game.AutoPaver.FollowsCore")

bool FAutoPaverFollowsTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPaver();
	ADIAAutoPaver* Paver = Scene.FindPaver();
	if (!TestNotNull(TEXT("the paver"), Paver))
	{
		return false;
	}
	// A new paver stands exactly where the core has it, with the heading it was spawned with.
	TestTrue(TEXT("spawned on the core's position"), FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos(0)) < 0.5);
	TestEqual(TEXT("on the ground"), Paver->GetActorLocation().Z, 0.0, 0.01);

	// The sector is finished already, so the paver drives to it, pours nothing and stops on its last cell.
	const int32 Sector = 2;
	Scene.CompleteSector(Sector);
	TestTrue(TEXT("the sector is assigned"), Scene.Game().AssignAutoPaverSector(0, Sector));
	const FVector2D Start = Scene.Game().GetAutoPaverPos(0);
	const FVector2D FirstCell = AsphaltLot::CellCentre(AsphaltLot::SectorCell(Sector, 0));
	const double ExpectedYaw = FMath::RadiansToDegrees(FMath::Atan2(FirstCell.Y - Start.Y, FirstCell.X - Start.X));
	bool bTurned = false;
	double WorstDriveLagCm = 0.0;
	for (int32 I = 0; I < 1200 && Scene.Game().GetAutoPaverSector(0) != INDEX_NONE; ++I)
	{
		Scene.Wrapper.TickTestWorld(TickSeconds);
		if (Scene.Game().IsAutoPaverTravelling(0))
		{
			WorstDriveLagCm = FMath::Max(WorstDriveLagCm, FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos(0)));
		}
		// While it drives to its sector it faces the way it goes.
		if (Scene.Game().IsAutoPaverTravelling(0) && FVector2D::Distance(Scene.Game().GetAutoPaverPos(0), FirstCell) > 500.0 && I > 5)
		{
			bTurned = FMath::Abs(FMath::FindDeltaAngleDegrees(Paver->GetActorRotation().Yaw, ExpectedYaw)) < 5.0;
		}
	}
	TestTrue(TEXT("it faced the direction of travel while driving"), bTurned);
	TestEqual(TEXT("the sector is done"), Scene.Game().GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	// It glides: while driving it lags about one frame of driving and the smoothing behind the core, not more than 1.5 m.
	TestTrue(TEXT("the shown position stays close to the core's while driving"), WorstDriveLagCm < 150.0);

	// The core stands still now: the shown position settles on it (small tolerance, ticks until it settles).
	Scene.TickFor(2.0);
	TestTrue(TEXT("settled on the core's position"), FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos(0)) < 0.5);
	const double Yaw = Paver->GetActorRotation().Yaw;
	Scene.TickFor(1.0);
	TestEqual(TEXT("the heading stays while it stands still"), Paver->GetActorRotation().Yaw, Yaw, 0.01);

	// A loaded or restarted game puts it on the core's position at once.
	Scene.Game().AssignAutoPaverSector(0, Sector);
	Scene.Subsystem->NewGame();
	Scene.GrantPaver();
	TestTrue(TEXT("a fresh paver stands on the core's position"), FVector2D::Distance(Scene.Shown(Scene.FindPaver()), Scene.Game().GetAutoPaverPos(0)) < 0.5);
	return true;
}

DIA_TEST(FAutoPaverSendTest, "DoItAsphalt.Game.AutoPaver.SendFromPlayerPosition")

bool FAutoPaverSendTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	// Without the paver: a toast, nothing else.
	Scene.StandAt(AsphaltLot::SectorCentre(3));
	Scene.Wrapper.TickTestWorld(TickSeconds);
	const int32 Toasts = Scene.Hud->GetToasts().Num();
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("one toast without the paver"), Scene.Hud->GetToasts().Num(), Toasts + 1);
	TestEqual(TEXT("toast text without the paver"), Scene.LastToast(), FString(TEXT("No paver")));
	TestEqual(TEXT("no sector"), Scene.Game().GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));

	Scene.GrantPaver();
	// In sector 3 (the player's position is the pawn's).
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("sent to sector 3"), Scene.Game().GetAutoPaverSector(0), 3);
	TestEqual(TEXT("toast numbers the sector from 1"), Scene.LastToast(), FString(TEXT("Paver → sector 4")));
	TestTrue(TEXT("sector 3 shown on the HUD line"), DIAFormat::PaverLine(Scene.Game(), 0).ToString().StartsWith(TEXT("Paver → sector 4 · ")));

	// Sent again to the sector it is still travelling to: nothing restarts.
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("still sector 3"), Scene.Game().GetAutoPaverSector(0), 3);
	TestTrue(TEXT("still travelling"), Scene.Game().IsAutoPaverTravelling(0));
	TestEqual(TEXT("step untouched"), Scene.Game().GetAutoPaverStep(0), 0);
	TestEqual(TEXT("toast for the sector it travels to"), Scene.LastToast(), FString(TEXT("Paver is already here")));

	// Outside the sectors (the strip behind the kerb, the mall quadrant): the sector stays.
	const FVector2D Outside[] = {
		FVector2D(0.0, -AsphaltLot::HalfExtentCm - 200.0),
		FVector2D(5000.0, 5000.0),
	};
	for (const FVector2D& Where : Outside)
	{
		Scene.StandAt(Where);
		Scene.Wrapper.TickTestWorld(TickSeconds);
		Scene.Controller->OnSendPaver();
		TestEqual(TEXT("the sector does not change off the lot"), Scene.Game().GetAutoPaverSector(0), 3);
		TestEqual(TEXT("toast off the lot"), Scene.LastToast(), FString(TEXT("Not on the lot")));
	}

	// Another sector: sent; the one it works in already: kept with its progress.
	Scene.StandAt(AsphaltLot::SectorCentre(10));
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("sent to sector 10"), Scene.Game().GetAutoPaverSector(0), 10);
	Scene.Game().DebugSetPlantStock(Scene.Game().GetPlantStorage());
	Scene.TickFor(120.0);
	const int32 Step = Scene.Game().GetAutoPaverStep(0);
	TestTrue(TEXT("it has paved some cells of sector 10"), Step > 0 && Scene.Game().GetAutoPaverSector(0) == 10);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the same sector keeps its progress"), Scene.Game().GetAutoPaverStep(0), Step);
	TestEqual(TEXT("still sector 10"), Scene.Game().GetAutoPaverSector(0), 10);
	TestFalse(TEXT("not travelling again"), Scene.Game().IsAutoPaverTravelling(0));
	TestEqual(TEXT("toast for the same sector"), Scene.LastToast(), FString(TEXT("Paver is already here")));

	// A menu, the shop or the win screen swallows the key.
	Scene.StandAt(AsphaltLot::SectorCentre(20));
	Scene.Controller->SetShopInput(true);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the shop swallows H"), Scene.Game().GetAutoPaverSector(0), 10);
	Scene.Controller->SetShopInput(false);
	Scene.Controller->SetWinInput(true);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the win screen swallows H"), Scene.Game().GetAutoPaverSector(0), 10);
	Scene.Controller->SetWinInput(false);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("and with nothing open it works"), Scene.Game().GetAutoPaverSector(0), 20);
	return true;
}

DIA_TEST(FAutoPaverOutlineTest, "DoItAsphalt.Game.AutoPaver.SectorOutlineParameter")

bool FAutoPaverOutlineTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	TestTrue(TEXT("no outline at the start"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	Scene.GrantPaver();
	Scene.Wrapper.TickTestWorld(TickSeconds);
	TestTrue(TEXT("no outline without a sector"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));

	const int32 Sector = 7;
	const FVector2D Centre = AsphaltLot::SectorCentre(Sector);
	Scene.CompleteSector(Sector);
	Scene.StandAt(Centre);
	Scene.Controller->OnSendPaver();
	TestTrue(TEXT("the outline is the sector's bounds"), Scene.RectIs(Centre.X - HalfSectorCm, Centre.Y - HalfSectorCm, Centre.X + HalfSectorCm, Centre.Y + HalfSectorCm));

	// The rectangle follows a re-assignment to another sector at once, and is zero when the sector is finished.
	const int32 Other = 12;
	const FVector2D OtherCentre = AsphaltLot::SectorCentre(Other);
	Scene.CompleteSector(Other);
	Scene.StandAt(OtherCentre);
	Scene.Controller->OnSendPaver();
	TestTrue(TEXT("the outline moves to the new sector"), Scene.RectIs(OtherCentre.X - HalfSectorCm, OtherCentre.Y - HalfSectorCm, OtherCentre.X + HalfSectorCm, OtherCentre.Y + HalfSectorCm));
	Scene.TickFor(90.0);
	TestEqual(TEXT("the finished sector"), Scene.Game().GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("no outline once it is finished"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));

	// A new game: no paver, no outline.
	Scene.Controller->OnSendPaver();
	Scene.Subsystem->NewGame();
	TestTrue(TEXT("no outline after a new game"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	return true;
}

DIA_TEST(FAutoPaverOutlineLoadTest, "DoItAsphalt.Game.AutoPaver.SectorOutlineAfterLoad")

bool FAutoPaverOutlineLoadTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPaver();
	const int32 Sector = 9;
	const FVector2D Centre = AsphaltLot::SectorCentre(Sector);
	Scene.CompleteSector(Sector);
	Scene.StandAt(Centre);
	Scene.Controller->OnSendPaver();
	TestTrue(TEXT("outline shown"), Scene.RectIs(Centre.X - HalfSectorCm, Centre.Y - HalfSectorCm, Centre.X + HalfSectorCm, Centre.Y + HalfSectorCm));
	// A new game deletes the slot, so the saved game is written back after it.
	const TArray<uint8> WithSector = Scene.Game().SaveToBytes();
	Scene.Subsystem->NewGame();
	TestTrue(TEXT("a new game clears the outline"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	TestTrue(TEXT("the blob is written"), UDIAWorldSubsystem::WriteSlot(UDIAWorldSubsystem::SlotName(), WithSector));
	TestTrue(TEXT("the game loads"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("the sector is back"), Scene.Game().GetAutoPaverSector(0), Sector);
	TestTrue(TEXT("the outline is the sector's bounds again after the load"), Scene.RectIs(Centre.X - HalfSectorCm, Centre.Y - HalfSectorCm, Centre.X + HalfSectorCm, Centre.Y + HalfSectorCm));

	// A saved game without a sector clears an outline that was shown before.
	Scene.Subsystem->NewGame();
	Scene.GrantPaver();
	const TArray<uint8> WithoutSector = Scene.Game().SaveToBytes();
	Scene.Controller->OnSendPaver();
	TestFalse(TEXT("outline shown again"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	TestTrue(TEXT("the blob is written"), UDIAWorldSubsystem::WriteSlot(UDIAWorldSubsystem::SlotName(), WithoutSector));
	TestTrue(TEXT("the game loads"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("no sector in the saved game"), Scene.Game().GetAutoPaverSector(0), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("the outline is gone after the load"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	return true;
}

DIA_TEST(FAutoPaverHudTest, "DoItAsphalt.Game.AutoPaver.HudLine")

bool FAutoPaverHudTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAHudModel Model;
	Model.Update(Game);
	TestTrue(TEXT("no line without the paver"), Model.GetView().Pavers[0].IsEmpty());
	TestTrue(TEXT("no text without the paver"), DIAFormat::PaverLine(Game, 0).IsEmpty());

	Game.DebugGrantItem(ELotItem::AutoPaver);
	TestTrue(TEXT("the purchase changes the view"), Model.Update(Game));
	TestEqual(TEXT("automatic with nothing to pave"), Model.GetView().Pavers[0].ToString(), FString(TEXT("Paver: AUTO – no unpaved sector to go to")));
	Game.SetAutoPaverAuto(false);
	TestTrue(TEXT("switching to manual changes the view"), Model.Update(Game));
	TestEqual(TEXT("waiting"), Model.GetView().Pavers[0].ToString(), FString(TEXT("Paver: waiting – press M for the map")));

	TestTrue(TEXT("assigned"), Game.AssignAutoPaverSector(0, 11));
	TestTrue(TEXT("a sector changes the view"), Model.Update(Game));
	TestEqual(TEXT("an empty tank sends it to the station first"), Model.GetView().Pavers[0].ToString(), FString(TEXT("Paver → station (tank empty) · sector 12")));

	// A full tank: it drives to the sector.
	Game.DebugFillTanks();
	Game.Tick(0.1);
	TestTrue(TEXT("the tank changes the view"), Model.Update(Game));
	TestTrue(TEXT("travelling"), Model.GetView().Pavers[0].ToString().StartsWith(TEXT("Paver → sector 12 · ")) || Model.GetView().Pavers[0].ToString().StartsWith(TEXT("Paver → station")));

	// Only a shown value rebuilds: nothing moves in the core without a tick.
	const int32 Rebuilds = Model.GetRebuildCount();
	TestFalse(TEXT("nothing changed"), Model.Update(Game));
	TestEqual(TEXT("no rebuild"), Model.GetRebuildCount(), Rebuilds);

	// Working: the step is shown, and a step moves the line.
	// The biggest plant with a full tank: the plant never limits the paver here.
	Game.DebugSetPlantLevel(5);
	for (int32 I = 0; I < 3000 && Game.GetAutoPaverStep(0) < 143; ++I)
	{
		Game.DebugSetPlantStock(Game.GetPlantStorage());
		Game.Tick(0.1);
	}
	TestTrue(TEXT("it worked its way to a step"), Game.GetAutoPaverStep(0) >= 143);
	Model.Update(Game);
	TestTrue(TEXT("working"), Model.GetView().Pavers[0].ToString().StartsWith(FString::Printf(TEXT("Paver: sector 12 · %d / 400 · tank "), Game.GetAutoPaverStep(0))));

	// Finished: waiting again.
	for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
	{
		Game.DebugCompleteCell(AsphaltLot::SectorCell(11, Step));
	}
	for (int32 I = 0; I < 10 && Game.GetAutoPaverSector(0) != INDEX_NONE; ++I)
	{
		Game.Tick(0.1);
	}
	TestTrue(TEXT("the view follows the finished sector"), Model.Update(Game));
	TestEqual(TEXT("waiting again"), Model.GetView().Pavers[0].ToString(), FString(TEXT("Paver: waiting – press M for the map")));
	return true;
}

DIA_TEST(FAutoPaverFormatTest, "DoItAsphalt.Game.AutoPaver.Format")

bool FAutoPaverFormatTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("sector 0 is shown as 1"), DIAFormat::SectorNumber(0).ToString(), FString(TEXT("1")));
	TestEqual(TEXT("sector 74 is shown as 75"), DIAFormat::SectorNumber(74).ToString(), FString(TEXT("75")));
	TestEqual(TEXT("toast"), DIAFormat::PaverSentToast(0, 1, 11).ToString(), FString(TEXT("Paver → sector 12")));
	TestEqual(TEXT("toast of the first sector"), DIAFormat::PaverSentToast(0, 1, 0).ToString(), FString(TEXT("Paver → sector 1")));
	TestEqual(TEXT("no paver"), DIAFormat::PaverNoneToast().ToString(), FString(TEXT("No paver")));
	TestEqual(TEXT("off the lot"), DIAFormat::PaverOffLotToast().ToString(), FString(TEXT("Not on the lot")));
	TestEqual(TEXT("already here"), DIAFormat::PaverAlreadyHereToast(0, 1).ToString(), FString(TEXT("Paver is already here")));
	return true;
}

DIA_TEST(FAutoPaverThreeActorsTest, "DoItAsphalt.Game.AutoPaver.ThreeActors")

bool FAutoPaverThreeActorsTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	for (int32 Owned = 1; Owned <= MaxAutoPavers; ++Owned)
	{
		Scene.Game().DebugGrantItem(AutoPaverItem[Owned - 1]);
		Scene.Subsystem->OnItemBought.Broadcast(AutoPaverItem[Owned - 1]);
		TestEqual(*FString::Printf(TEXT("%d actors after %d robots"), Owned, Owned), Scene.CountPavers(), Owned);
	}
	for (int32 Index = 0; Index < MaxAutoPavers; ++Index)
	{
		const ADIAAutoPaver* Paver = Scene.FindPaverOf(Index);
		if (!TestNotNull(*FString::Printf(TEXT("actor of robot %d"), Index), Paver))
		{
			return false;
		}
		TestTrue(*FString::Printf(TEXT("robot %d stands in its dock"), Index), Scene.Shown(Paver).Equals(LotLayout::PaverDock(Index), 0.5));
		TestEqual(*FString::Printf(TEXT("robot %d faces out of the depot"), Index), FRotator::NormalizeAxis(Paver->GetActorRotation().Yaw), LotLayout::PaverDockYawDeg, 0.01);
	}

	// Asking again, by an announcement or by the game mode itself, never makes a duplicate.
	Scene.Subsystem->OnItemBought.Broadcast(ELotItem::AutoPaver2);
	if (ADIAGameMode* Mode = Scene.World->GetAuthGameMode<ADIAGameMode>())
	{
		Mode->EnsureAutoPavers();
		Mode->EnsureAutoPavers();
	}
	TestEqual(TEXT("still three"), Scene.CountPavers(), 3);
	return true;
}

DIA_TEST(FAutoPaverEachFollowsTest, "DoItAsphalt.Game.AutoPaver.EachFollowsItsOwn")

bool FAutoPaverEachFollowsTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPavers(3);
	const int32 Sectors[3] = {2, 40, 70};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(TEXT("the sector is assigned"), Scene.Game().AssignAutoPaverSector(Index, Sectors[Index]));
	}
	Scene.TickFor(6.0);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const ADIAAutoPaver* Paver = Scene.FindPaverOf(Index);
		if (!TestNotNull(TEXT("the actor"), Paver))
		{
			return false;
		}
		const FVector2D Core = Scene.Game().GetAutoPaverPos(Index);
		TestTrue(*FString::Printf(TEXT("robot %d left its dock"), Index), FVector2D::Distance(Core, LotLayout::PaverDock(Index)) > 200.0);
		TestTrue(*FString::Printf(TEXT("actor %d follows its robot"), Index), FVector2D::Distance(Scene.Shown(Paver), Core) < 150.0);
		for (int32 Other = 0; Other < 3; ++Other)
		{
			if (Other != Index)
			{
				TestTrue(TEXT("and not another one"), FVector2D::Distance(Scene.Shown(Paver), Core) < FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos(Other)));
			}
		}
	}
	return true;
}

DIA_TEST(FAutoPaverOutlinePerPaverTest, "DoItAsphalt.Game.AutoPaver.OutlinePerPaver")

bool FAutoPaverOutlinePerPaverTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPavers(3);
	const ADIAGroundActor* Ground = Scene.Ground();
	if (!TestNotNull(TEXT("the ground"), Ground))
	{
		return false;
	}
	const auto Visible = [Ground](int32 Paver)
	{
		const UStaticMeshComponent* Strip = Ground->GetOutlineStrip(Paver, 0);
		return Strip && Strip->IsVisible();
	};
	TestTrue(TEXT("no outline at first"), Scene.RectIs(0.0, 0.0, 0.0, 0.0) && !Visible(1) && !Visible(2));

	// Two robots, two sectors: two outlines.
	const int32 SectorA = 7;
	const int32 SectorB = 12;
	TestTrue(TEXT("robot 0 sent"), Scene.Game().AssignAutoPaverSector(0, SectorA));
	TestTrue(TEXT("robot 1 sent"), Scene.Game().AssignAutoPaverSector(1, SectorB));
	Scene.Wrapper.TickTestWorld(TickSeconds);
	const FVector2D CentreA = AsphaltLot::SectorCentre(SectorA);
	const FVector2D CentreB = AsphaltLot::SectorCentre(SectorB);
	TestTrue(TEXT("robot 0's outline is its sector"), Scene.RectIs(CentreA.X - HalfSectorCm, CentreA.Y - HalfSectorCm, CentreA.X + HalfSectorCm, CentreA.Y + HalfSectorCm));
	const FVector4 RectB = Ground->GetPaverOutline(1);
	TestTrue(TEXT("robot 1's outline is its sector"), RectB.X == static_cast<float>(CentreB.X - HalfSectorCm) && RectB.Y == static_cast<float>(CentreB.Y - HalfSectorCm)
		&& RectB.Z == static_cast<float>(CentreB.X + HalfSectorCm) && RectB.W == static_cast<float>(CentreB.Y + HalfSectorCm));
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const UStaticMeshComponent* Strip = Ground->GetOutlineStrip(1, Side);
		if (!TestNotNull(TEXT("strip"), Strip))
		{
			return false;
		}
		TestTrue(TEXT("a strip of robot 1 is shown"), Strip->IsVisible());
		// Every strip lies inside the sector's rectangle.
		const FBoxSphereBounds Bounds = Strip->Bounds;
		TestTrue(TEXT("inside the sector"), Bounds.Origin.X - Bounds.BoxExtent.X >= CentreB.X - HalfSectorCm - 0.5 && Bounds.Origin.X + Bounds.BoxExtent.X <= CentreB.X + HalfSectorCm + 0.5
			&& Bounds.Origin.Y - Bounds.BoxExtent.Y >= CentreB.Y - HalfSectorCm - 0.5 && Bounds.Origin.Y + Bounds.BoxExtent.Y <= CentreB.Y + HalfSectorCm + 0.5);
	}
	TestFalse(TEXT("robot 2 has none"), Visible(2));

	// A robot without a sector shows no outline.
	Scene.CompleteSector(SectorB);
	Scene.TickFor(20.0);
	TestEqual(TEXT("robot 1 finished"), Scene.Game().GetAutoPaverSector(1), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("robot 1's outline is gone"), Visible(1));
	TestTrue(TEXT("robot 0's stays"), !Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	return true;
}

DIA_TEST(FAutoPaverRemovedOnNewGameTest, "DoItAsphalt.Game.AutoPaver.RemovedOnNewGame")

bool FAutoPaverRemovedOnNewGameTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPavers(1);
	const TArray<uint8> OneRobot = Scene.Game().SaveToBytes();
	Scene.GrantPavers(3);
	TestEqual(TEXT("three actors"), Scene.CountPavers(), 3);

	// A loaded game with fewer robots than there are actors keeps only the first.
	TestTrue(TEXT("the blob is written"), UDIAWorldSubsystem::WriteSlot(UDIAWorldSubsystem::SlotName(), OneRobot));
	TestTrue(TEXT("the game loads"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("one actor after the load"), Scene.CountPavers(), 1);
	TestNotNull(TEXT("it is robot 0's"), Scene.FindPaverOf(0));

	Scene.GrantPavers(3);
	TestEqual(TEXT("three again"), Scene.CountPavers(), 3);
	Scene.Subsystem->NewGame();
	TestEqual(TEXT("none after a new game"), Scene.CountPavers(), 0);
	return true;
}

DIA_TEST(FAutoPaverSendSelectedTest, "DoItAsphalt.Game.AutoPaver.SendSelected")

bool FAutoPaverSendSelectedTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPavers(3);
	TestTrue(TEXT("robot 0 holds a sector"), Scene.Game().AssignAutoPaverSector(0, 20));

	// H sends the robot selected on the map: robot 2 (index 1) and nobody else.
	Scene.Hud->SelectPaver(1);
	Scene.StandAt(AsphaltLot::SectorCentre(5));
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the selected robot is sent"), Scene.Game().GetAutoPaverSector(1), 5);
	TestEqual(TEXT("robot 0 keeps its sector"), Scene.Game().GetAutoPaverSector(0), 20);
	TestEqual(TEXT("robot 2 is untouched"), Scene.Game().GetAutoPaverSector(2), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the toast names the robot"), Scene.LastToast(), FString(TEXT("Paver 2 → sector 6")));

	// The question "already here" is asked of the selected robot.
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("already here for robot 2"), Scene.LastToast(), FString(TEXT("Paver 2 is already here")));
	Scene.Hud->SelectPaver(2);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("robot 3 goes to the sector robot 2 holds"), Scene.Game().GetAutoPaverSector(2), 5);
	TestEqual(TEXT("robot 2 lost it"), Scene.Game().GetAutoPaverSector(1), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("normal sent toast"), Scene.LastToast(), FString(TEXT("Paver 3 → sector 6")));
	return true;
}

DIA_TEST(FPaverMapSelectAndAssignTest, "DoItAsphalt.Game.PaverMap.SelectAndAssign")

bool FPaverMapSelectAndAssignTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPavers(2);
	TestTrue(TEXT("robot 0 holds a sector"), Scene.Game().AssignAutoPaverSector(0, 20));
	const TSharedRef<SDIAPaverMap> Map = SNew(SDIAPaverMap).Hud(Scene.Hud).Subsystem(Scene.Subsystem);

	TestEqual(TEXT("robot 1 is selected at first"), Scene.Hud->GetSelectedPaver(), 0);
	// A real click on the button of robot 2 (a synthetic pointer event over its rectangle).
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(1280.0, 720.0), FSlateLayoutTransform());
	const auto Click = [&](FVector2D Local)
	{
		const FPointerEvent Event(0, FVector2D(Local), FVector2D(Local), TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		return Map->OnMouseButtonDown(Geometry, Event);
	};
	const FVector2D Button2 = Map->PaverButtonRect(Geometry, 1).GetCenter();
	TestTrue(TEXT("the click on the switch is handled"), Click(Button2).IsEventHandled());
	TestEqual(TEXT("robot 2 is selected"), Scene.Hud->GetSelectedPaver(), 1);
	TestEqual(TEXT("the switch click reached no sector"), Scene.Game().GetAutoPaverSector(1), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and moved no robot"), Scene.Game().GetAutoPaverSector(0), 20);
	Map->ChooseSector(33);
	TestEqual(TEXT("the click gave the sector to robot 2"), Scene.Game().GetAutoPaverSector(1), 33);
	TestEqual(TEXT("robot 1 stayed as it was"), Scene.Game().GetAutoPaverSector(0), 20);
	TestEqual(TEXT("toast"), Scene.LastToast(), FString(TEXT("Paver 2 → sector 34")));

	// The selection stays while the map is closed and opened again.
	Scene.Hud->SetPaverMapOpen(true);
	Scene.Hud->SetPaverMapOpen(false);
	TestEqual(TEXT("the selection stays"), Scene.Hud->GetSelectedPaver(), 1);

	// The button of a robot that is not owned does nothing.
	TestFalse(TEXT("robot 3 is not owned"), Map->ChoosePaver(2));
	Click(Map->PaverButtonRect(Geometry, 2).GetCenter());
	TestEqual(TEXT("still robot 2"), Scene.Hud->GetSelectedPaver(), 1);

	// Clicking the sector another robot holds takes it from that robot.
	Click(Map->PaverButtonRect(Geometry, 0).GetCenter());
	TestEqual(TEXT("robot 1 selected by its button"), Scene.Hud->GetSelectedPaver(), 0);
	Map->ChooseSector(33);
	TestEqual(TEXT("robot 1 took the sector"), Scene.Game().GetAutoPaverSector(0), 33);
	TestEqual(TEXT("robot 2 lost it"), Scene.Game().GetAutoPaverSector(1), static_cast<int32>(INDEX_NONE));
	return true;
}

DIA_TEST(FPaverMapSelectionClampsTest, "DoItAsphalt.Game.PaverMap.SelectionClamps")

bool FPaverMapSelectionClampsTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	TestEqual(TEXT("no robot: 0"), Scene.Hud->GetSelectedPaver(), 0);
	Scene.Hud->SelectPaver(1);
	TestEqual(TEXT("selecting a robot that is not owned does nothing"), Scene.Hud->GetSelectedPaver(), 0);

	Scene.GrantPavers(1);
	Scene.Hud->SelectPaver(2);
	TestEqual(TEXT("robot 3 with one robot owned: ignored"), Scene.Hud->GetSelectedPaver(), 0);
	Scene.Hud->SelectPaver(-1);
	TestEqual(TEXT("a negative robot: ignored"), Scene.Hud->GetSelectedPaver(), 0);
	// H with that selection sends robot 1 (the only one) and nobody else; no robot appears.
	Scene.StandAt(AsphaltLot::SectorCentre(8));
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the only robot is sent"), Scene.Game().GetAutoPaverSector(0), 8);
	TestEqual(TEXT("still one robot"), Scene.Game().GetNumAutoPavers(), 1);
	TestEqual(TEXT("one robot: today's toast"), Scene.LastToast(), FString(TEXT("Paver → sector 9")));
	TestEqual(TEXT("the other robots hold nothing"), Scene.Game().GetAutoPaverSector(1), static_cast<int32>(INDEX_NONE));

	// Three robots, the third selected: the selection drops to 0 when a game with one robot is loaded, and on a new game.
	const TArray<uint8> OneRobot = Scene.Game().SaveToBytes();
	Scene.GrantPavers(3);
	Scene.Hud->SelectPaver(2);
	TestEqual(TEXT("robot 3 is selected"), Scene.Hud->GetSelectedPaver(), 2);
	TestTrue(TEXT("the blob is written"), UDIAWorldSubsystem::WriteSlot(UDIAWorldSubsystem::SlotName(), OneRobot));
	TestTrue(TEXT("the game loads"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("after the load of a game with one robot"), Scene.Hud->GetSelectedPaver(), 0);

	Scene.GrantPavers(3);
	Scene.Hud->SelectPaver(1);
	Scene.Subsystem->NewGame();
	TestEqual(TEXT("after a new game"), Scene.Hud->GetSelectedPaver(), 0);
	return true;
}

DIA_TEST(FAutoPaverUpgradeBuyTest, "DoItAsphalt.Game.AutoPaver.UpgradeBuy")

bool FAutoPaverUpgradeBuyTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	Scene.GrantPavers(2);
	Scene.Game().DebugGive(100000.0);

	int32 Bought = 0;
	int32 LastPaver = -1;
	ELotPaverUpgrade LastKind = ELotPaverUpgrade::Count;
	int32 Changed = 0;
	const FDelegateHandle BoughtHandle = Scene.Subsystem->OnPaverUpgradeBought.AddLambda([&](int32 Paver, ELotPaverUpgrade Kind) { ++Bought; LastPaver = Paver; LastKind = Kind; });
	const FDelegateHandle ChangedHandle = Scene.Subsystem->OnStateChanged.AddLambda([&Changed]() { ++Changed; });

	const double Before = Scene.Game().GetCredits();
	TestTrue(TEXT("robot 2 capacity is bought"), Scene.Subsystem->BuyPaverUpgrade(1, ELotPaverUpgrade::Capacity));
	TestEqual(TEXT("one announcement"), Bought, 1);
	TestEqual(TEXT("for robot 2"), LastPaver, 1);
	TestTrue(TEXT("for the capacity"), LastKind == ELotPaverUpgrade::Capacity);
	TestEqual(TEXT("the state changed once"), Changed, 1);
	TestEqual(TEXT("it was paid"), Before - Scene.Game().GetCredits(), static_cast<double>(LotBalance::PaverUpgradePrice[1][0]));
	TestEqual(TEXT("robot 2 has capacity level 1"), Scene.Game().GetPaverUpgradeLevel(1, ELotPaverUpgrade::Capacity), 1);
	TestEqual(TEXT("robot 1 has not"), Scene.Game().GetPaverUpgradeLevel(0, ELotPaverUpgrade::Capacity), 0);
	TestEqual(TEXT("the toast of robot 2"), DIAFormat::PaverUpgradeToast(Scene.Game(), 1, ELotPaverUpgrade::Capacity).ToString(), FString(TEXT("Paver 2: tank 3,000 l")));

	TestTrue(TEXT("robot 1 speed is bought"), Scene.Subsystem->BuyPaverUpgrade(0, ELotPaverUpgrade::Speed));
	TestEqual(TEXT("the toast of robot 1"), DIAFormat::PaverUpgradeToast(Scene.Game(), 0, ELotPaverUpgrade::Speed).ToString(), FString(TEXT("Paver 1: speed 150 cells/min")));

	// A refusal announces nothing: robot 3 is not owned, a level above the maximum does not exist, an invalid kind is nothing.
	const int32 BoughtBefore = Bought;
	const int32 ChangedBefore = Changed;
	const double CreditsBefore = Scene.Game().GetCredits();
	TestFalse(TEXT("robot 3 is not owned"), Scene.Subsystem->BuyPaverUpgrade(2, ELotPaverUpgrade::Speed));
	TestTrue(TEXT("robot 2 capacity, second level"), Scene.Subsystem->BuyPaverUpgrade(1, ELotPaverUpgrade::Capacity));
	TestFalse(TEXT("robot 2 capacity, third level does not exist"), Scene.Subsystem->BuyPaverUpgrade(1, ELotPaverUpgrade::Capacity));
	TestFalse(TEXT("an invalid kind"), Scene.Subsystem->BuyPaverUpgrade(0, ELotPaverUpgrade::Count));
	TestEqual(TEXT("only the one purchase was announced"), Bought, BoughtBefore + 1);
	TestEqual(TEXT("and changed the state once"), Changed, ChangedBefore + 1);
	TestEqual(TEXT("and was paid once"), CreditsBefore - Scene.Game().GetCredits(), static_cast<double>(LotBalance::PaverUpgradePrice[1][1]));
	TestEqual(TEXT("the toast at the top level"), DIAFormat::PaverUpgradeToast(Scene.Game(), 1, ELotPaverUpgrade::Capacity).ToString(), FString(TEXT("Paver 2: tank 4,500 l")));

	Scene.Subsystem->OnPaverUpgradeBought.Remove(BoughtHandle);
	Scene.Subsystem->OnStateChanged.Remove(ChangedHandle);
	return true;
}

DIA_TEST(FAutoPaverUpgradeCheatTest, "DoItAsphalt.Game.AutoPaver.UpgradeCheat")

bool FAutoPaverUpgradeCheatTest::RunTest(const FString& Parameters)
{
	using namespace AutoPaverTestSupport;
	FAutoPaverTestSlot Slot;
	FAutoPaverScene Scene;
	if (!Scene.Start(*this))
	{
		return false;
	}
	const auto Run = [&Scene](const TCHAR* Command) { IConsoleManager::Get().ProcessUserConsoleInput(Command, *GLog, Scene.World); };

	// Robot 3 and the robots before it are given, and the level is set; the actors appear.
	Run(TEXT("dia.PaverUpgrade 3 Capacity 2"));
	TestEqual(TEXT("three robots"), Scene.Game().GetNumAutoPavers(), 3);
	TestEqual(TEXT("robot 3 capacity"), Scene.Game().GetPaverUpgradeLevel(2, ELotPaverUpgrade::Capacity), 2);
	TestEqual(TEXT("robot 3 speed"), Scene.Game().GetPaverUpgradeLevel(2, ELotPaverUpgrade::Speed), 0);
	TestEqual(TEXT("robot 1 capacity is untouched"), Scene.Game().GetPaverUpgradeLevel(0, ELotPaverUpgrade::Capacity), 0);
	TestEqual(TEXT("an actor for each"), Scene.CountPavers(), 3);

	Run(TEXT("dia.PaverUpgrade 1 speed 2"));
	TestEqual(TEXT("robot 1 speed (the kind is not case sensitive)"), Scene.Game().GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 2);
	Run(TEXT("dia.PaverUpgrade 1 Speed 0"));
	TestEqual(TEXT("and back to 0"), Scene.Game().GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);

	// A wrong call changes nothing.
	AddExpectedError(TEXT("dia.PaverUpgrade"), EAutomationExpectedErrorFlags::Contains, 0);
	Run(TEXT("dia.PaverUpgrade 4 Speed 1"));
	Run(TEXT("dia.PaverUpgrade 1 Speed 3"));
	Run(TEXT("dia.PaverUpgrade 1 Wings 1"));
	Run(TEXT("dia.PaverUpgrade 1 Speed"));
	TestEqual(TEXT("robot 1 speed stays"), Scene.Game().GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
