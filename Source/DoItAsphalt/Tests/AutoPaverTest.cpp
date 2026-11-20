// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "LotBalance.h"
#include "LotMap.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
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

		/** Gives the player the auto paver without the unlock the shop would ask for, and announces it like a purchase. */
		void GrantPaver() const
		{
			Game().DebugGrantItem(ELotItem::AutoPaver);
			Subsystem->OnItemBought.Broadcast(ELotItem::AutoPaver);
			Subsystem->OnStateChanged.Broadcast();
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
	TestEqual(TEXT("the body is as long as the model is, 1.5 times larger"), Bounds.BoxExtent.X * 2.0, static_cast<double>(DIAModelSizes::AutoPaverCm.X * DIAAutoPaver::ModelScale), 3.0);
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
	TestTrue(TEXT("spawned on the core's position"), FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos()) < 0.5);
	TestEqual(TEXT("on the ground"), Paver->GetActorLocation().Z, 0.0, 0.01);

	// The sector is finished already, so the paver drives to it, pours nothing and stops on its last cell.
	const int32 Sector = 2;
	Scene.CompleteSector(Sector);
	TestTrue(TEXT("the sector is assigned"), Scene.Game().AssignAutoPaverSector(Sector));
	const FVector2D Start = Scene.Game().GetAutoPaverPos();
	const FVector2D FirstCell = AsphaltLot::CellCentre(AsphaltLot::SectorCell(Sector, 0));
	const double ExpectedYaw = FMath::RadiansToDegrees(FMath::Atan2(FirstCell.Y - Start.Y, FirstCell.X - Start.X));
	bool bTurned = false;
	double WorstDriveLagCm = 0.0;
	for (int32 I = 0; I < 1200 && Scene.Game().GetAutoPaverSector() != INDEX_NONE; ++I)
	{
		Scene.Wrapper.TickTestWorld(TickSeconds);
		if (Scene.Game().IsAutoPaverTravelling())
		{
			WorstDriveLagCm = FMath::Max(WorstDriveLagCm, FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos()));
		}
		// While it drives to its sector it faces the way it goes.
		if (Scene.Game().IsAutoPaverTravelling() && FVector2D::Distance(Scene.Game().GetAutoPaverPos(), FirstCell) > 500.0 && I > 5)
		{
			bTurned = FMath::Abs(FMath::FindDeltaAngleDegrees(Paver->GetActorRotation().Yaw, ExpectedYaw)) < 5.0;
		}
	}
	TestTrue(TEXT("it faced the direction of travel while driving"), bTurned);
	TestEqual(TEXT("the sector is done"), Scene.Game().GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
	// It glides: while driving it lags about one frame of driving and the smoothing behind the core, not more than 1.5 m.
	TestTrue(TEXT("the shown position stays close to the core's while driving"), WorstDriveLagCm < 150.0);

	// The core stands still now: the shown position settles on it (small tolerance, ticks until it settles).
	Scene.TickFor(2.0);
	TestTrue(TEXT("settled on the core's position"), FVector2D::Distance(Scene.Shown(Paver), Scene.Game().GetAutoPaverPos()) < 0.5);
	const double Yaw = Paver->GetActorRotation().Yaw;
	Scene.TickFor(1.0);
	TestEqual(TEXT("the heading stays while it stands still"), Paver->GetActorRotation().Yaw, Yaw, 0.01);

	// A loaded or restarted game puts it on the core's position at once.
	Scene.Game().AssignAutoPaverSector(Sector);
	Scene.Subsystem->NewGame();
	Scene.GrantPaver();
	TestTrue(TEXT("a fresh paver stands on the core's position"), FVector2D::Distance(Scene.Shown(Scene.FindPaver()), Scene.Game().GetAutoPaverPos()) < 0.5);
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
	TestEqual(TEXT("no sector"), Scene.Game().GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));

	Scene.GrantPaver();
	// In sector 3 (the player's position is the pawn's).
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("sent to sector 3"), Scene.Game().GetAutoPaverSector(), 3);
	TestEqual(TEXT("toast numbers the sector from 1"), Scene.LastToast(), FString(TEXT("Paver → sector 4")));
	TestEqual(TEXT("sector 3 shown on the HUD line"), DIAFormat::PaverLine(Scene.Game()).ToString(), FString(TEXT("Paver: sector 4 · 0 / 400")));

	// Sent again to the sector it is still travelling to: nothing restarts.
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("still sector 3"), Scene.Game().GetAutoPaverSector(), 3);
	TestTrue(TEXT("still travelling"), Scene.Game().IsAutoPaverTravelling());
	TestEqual(TEXT("step untouched"), Scene.Game().GetAutoPaverStep(), 0);
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
		TestEqual(TEXT("the sector does not change off the lot"), Scene.Game().GetAutoPaverSector(), 3);
		TestEqual(TEXT("toast off the lot"), Scene.LastToast(), FString(TEXT("Not on the lot")));
	}

	// Another sector: sent; the one it works in already: kept with its progress.
	Scene.StandAt(AsphaltLot::SectorCentre(10));
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("sent to sector 10"), Scene.Game().GetAutoPaverSector(), 10);
	Scene.Game().DebugSetPlantStock(Scene.Game().GetPlantStorage());
	Scene.TickFor(120.0);
	const int32 Step = Scene.Game().GetAutoPaverStep();
	TestTrue(TEXT("it has paved some cells of sector 10"), Step > 0 && Scene.Game().GetAutoPaverSector() == 10);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the same sector keeps its progress"), Scene.Game().GetAutoPaverStep(), Step);
	TestEqual(TEXT("still sector 10"), Scene.Game().GetAutoPaverSector(), 10);
	TestFalse(TEXT("not travelling again"), Scene.Game().IsAutoPaverTravelling());
	TestEqual(TEXT("toast for the same sector"), Scene.LastToast(), FString(TEXT("Paver is already here")));

	// A menu, the shop or the win screen swallows the key.
	Scene.StandAt(AsphaltLot::SectorCentre(20));
	Scene.Controller->SetShopInput(true);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the shop swallows G"), Scene.Game().GetAutoPaverSector(), 10);
	Scene.Controller->SetShopInput(false);
	Scene.Controller->SetWinInput(true);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("the win screen swallows G"), Scene.Game().GetAutoPaverSector(), 10);
	Scene.Controller->SetWinInput(false);
	Scene.Controller->OnSendPaver();
	TestEqual(TEXT("and with nothing open it works"), Scene.Game().GetAutoPaverSector(), 20);
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
	TestEqual(TEXT("the finished sector"), Scene.Game().GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
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
	TestEqual(TEXT("the sector is back"), Scene.Game().GetAutoPaverSector(), Sector);
	TestTrue(TEXT("the outline is the sector's bounds again after the load"), Scene.RectIs(Centre.X - HalfSectorCm, Centre.Y - HalfSectorCm, Centre.X + HalfSectorCm, Centre.Y + HalfSectorCm));

	// A saved game without a sector clears an outline that was shown before.
	Scene.Subsystem->NewGame();
	Scene.GrantPaver();
	const TArray<uint8> WithoutSector = Scene.Game().SaveToBytes();
	Scene.Controller->OnSendPaver();
	TestFalse(TEXT("outline shown again"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	TestTrue(TEXT("the blob is written"), UDIAWorldSubsystem::WriteSlot(UDIAWorldSubsystem::SlotName(), WithoutSector));
	TestTrue(TEXT("the game loads"), Scene.Subsystem->LoadGame());
	TestEqual(TEXT("no sector in the saved game"), Scene.Game().GetAutoPaverSector(), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("the outline is gone after the load"), Scene.RectIs(0.0, 0.0, 0.0, 0.0));
	return true;
}

DIA_TEST(FAutoPaverHudTest, "DoItAsphalt.Game.AutoPaver.HudLine")

bool FAutoPaverHudTest::RunTest(const FString& Parameters)
{
	FLotGame Game;
	FDIAHudModel Model;
	Model.Update(Game);
	TestTrue(TEXT("no line without the paver"), Model.GetView().Paver.IsEmpty());
	TestTrue(TEXT("no text without the paver"), DIAFormat::PaverLine(Game).IsEmpty());

	Game.DebugGrantItem(ELotItem::AutoPaver);
	TestTrue(TEXT("the purchase changes the view"), Model.Update(Game));
	TestEqual(TEXT("waiting"), Model.GetView().Paver.ToString(), FString(TEXT("Paver: waiting – press G")));

	TestTrue(TEXT("assigned"), Game.AssignAutoPaverSector(11));
	TestTrue(TEXT("a sector changes the view"), Model.Update(Game));
	TestEqual(TEXT("travelling"), Model.GetView().Paver.ToString(), FString(TEXT("Paver: sector 12 · 0 / 400")));

	// Only a shown value rebuilds: nothing moves in the core without a tick.
	const int32 Rebuilds = Model.GetRebuildCount();
	TestFalse(TEXT("nothing changed"), Model.Update(Game));
	TestEqual(TEXT("no rebuild"), Model.GetRebuildCount(), Rebuilds);

	// Working: the step is shown, and a step moves the line.
	// The biggest plant with a full tank: the plant never limits the paver here.
	Game.DebugSetPlantLevel(5);
	for (int32 I = 0; I < 3000 && Game.GetAutoPaverStep() < 143; ++I)
	{
		Game.DebugSetPlantStock(Game.GetPlantStorage());
		Game.Tick(0.1);
	}
	TestTrue(TEXT("it worked its way to a step"), Game.GetAutoPaverStep() >= 143);
	Model.Update(Game);
	TestEqual(TEXT("working"), Model.GetView().Paver.ToString(), FString::Printf(TEXT("Paver: sector 12 · %d / 400"), Game.GetAutoPaverStep()));

	// Finished: waiting again.
	for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
	{
		Game.DebugCompleteCell(AsphaltLot::SectorCell(11, Step));
	}
	for (int32 I = 0; I < 10 && Game.GetAutoPaverSector() != INDEX_NONE; ++I)
	{
		Game.Tick(0.1);
	}
	TestTrue(TEXT("the view follows the finished sector"), Model.Update(Game));
	TestEqual(TEXT("waiting again"), Model.GetView().Paver.ToString(), FString(TEXT("Paver: waiting – press G")));
	return true;
}

DIA_TEST(FAutoPaverFormatTest, "DoItAsphalt.Game.AutoPaver.Format")

bool FAutoPaverFormatTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("sector 0 is shown as 1"), DIAFormat::SectorNumber(0).ToString(), FString(TEXT("1")));
	TestEqual(TEXT("sector 74 is shown as 75"), DIAFormat::SectorNumber(74).ToString(), FString(TEXT("75")));
	TestEqual(TEXT("toast"), DIAFormat::PaverSentToast(11).ToString(), FString(TEXT("Paver → sector 12")));
	TestEqual(TEXT("toast of the first sector"), DIAFormat::PaverSentToast(0).ToString(), FString(TEXT("Paver → sector 1")));
	TestEqual(TEXT("no paver"), DIAFormat::PaverNoneToast().ToString(), FString(TEXT("No paver")));
	TestEqual(TEXT("off the lot"), DIAFormat::PaverOffLotToast().ToString(), FString(TEXT("Not on the lot")));
	TestEqual(TEXT("already here"), DIAFormat::PaverAlreadyHereToast().ToString(), FString(TEXT("Paver is already here")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
