// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameMapsSettings.h"
#include "LotBalance.h"
#include "LotGrid.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAGameMode.h"
#include "Tests/DIATestWorld.h"
#include "Game/DIAWorldSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/DIAPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "World/DIAGroundActor.h"
#include "World/DIAPlantActor.h"

namespace
{
	/** Texel of the grid that contains the world coordinate. */
	FIntPoint TexelAt(FVector2D WorldCm)
	{
		return FIntPoint(FMath::FloorToInt((WorldCm.X + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm),
			FMath::FloorToInt((WorldCm.Y + AsphaltLot::HalfExtentCm) / FLotGrid::TexelCm));
	}

	ADIAGroundActor* FindGround(UWorld* World)
	{
		TActorIterator<ADIAGroundActor> It(World);
		return It ? *It : nullptr;
	}

	/** Height of a level PlayerStart above the ground in the spawn test. */
	constexpr double TestLevelStartZCm = 100.0;
	constexpr double TestYawToleranceDeg = 1.0;
}

DIA_TEST(FWorldBuildsTest, "DoItAsphalt.Game.World.Builds")

bool FWorldBuildsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("project default game mode"), UGameMapsSettings::GetGlobalDefaultGameMode(), FString(TEXT("/Script/DoItAsphalt.DIAGameMode")));

	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	TestNotNull(TEXT("game mode is ADIAGameMode"), Cast<ADIAGameMode>(World->GetAuthGameMode()));

	int32 GroundCount = 0;
	for (TActorIterator<ADIAGroundActor> It(World); It; ++It)
	{
		++GroundCount;
	}
	TestEqual(TEXT("exactly one ground actor"), GroundCount, 1);
	int32 PlantCount = 0;
	for (TActorIterator<ADIAPlantActor> It(World); It; ++It)
	{
		++PlantCount;
		TestTrue(TEXT("the plant stands at the layout position"), FVector2D(It->GetActorLocation()).Equals(LotLayout::PlantPos, 1.0));
		TestEqual(TEXT("the plant faces the car park"), FMath::Abs(FMath::FindDeltaAngleDegrees(It->GetActorRotation().Yaw, LotLayout::PlantYawDeg)), 0.0, TestYawToleranceDeg);
	}
	TestEqual(TEXT("exactly one plant"), PlantCount, 1);

	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	TestEqual(TEXT("the plant starts with its stock"), Subsystem->GetGame().GetPlantStock(), 80.0);

	ADIAGroundActor* Ground = FindGround(World);
	if (TestNotNull(TEXT("ground"), Ground))
	{
		TestNotNull(TEXT("ground material built from M_Ground"), Ground->GetGroundMaterial());
		UTexture2D* Coverage = Ground->GetCoverageTexture();
		if (TestNotNull(TEXT("coverage texture"), Coverage))
		{
			TestEqual(TEXT("coverage width"), Coverage->GetSizeX(), FLotGrid::Size);
			TestEqual(TEXT("coverage height"), Coverage->GetSizeY(), FLotGrid::Size);
			TestFalse(TEXT("coverage is linear"), Coverage->SRGB);
			TestTrue(TEXT("coverage clamps in X"), Coverage->AddressX == TA_Clamp);
			TestTrue(TEXT("coverage clamps in Y"), Coverage->AddressY == TA_Clamp);
			TestTrue(TEXT("coverage has two channels"), Coverage->GetPixelFormat() == PF_R8G8);
			if (UMaterialInstanceDynamic* Material = Ground->GetGroundMaterial())
			{
				UTexture* Bound = nullptr;
				TestTrue(TEXT("CoverageTex is bound"), Material->GetTextureParameterValue(TEXT("CoverageTex"), Bound) && Bound == Coverage);
			}
		}
	}
	return true;
}

DIA_TEST(FGroundMirrorsGridTest, "DoItAsphalt.Game.World.GroundMirrorsGrid")

bool FGroundMirrorsGridTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	ADIAGroundActor* Ground = FindGround(World);
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestNotNull(TEXT("ground"), Ground))
	{
		return false;
	}
	FLotGame& Game = Subsystem->GetGame();
	const int32 FullUploads = Ground->GetNumFullUploads();

	uint8 R = 7;
	uint8 G = 7;
	TestTrue(TEXT("a texel inside cell 0 is readable"), Ground->GetCoverageTexel(10, 10, R, G));
	TestTrue(TEXT("a new game has an empty mirror"), R == 0 && G == 0);

	// A completed cell is finished asphalt: R and G are both set over the whole cell, and only there.
	Subsystem->Dispatch(Game.DebugCompleteCell(0));
	Ground->SyncCoverage();
	for (const FIntPoint Texel : {FIntPoint(0, 0), FIntPoint(10, 10), FIntPoint(19, 19), FIntPoint(0, 19)})
	{
		TestTrue(TEXT("completed cell texel readable"), Ground->GetCoverageTexel(Texel.X, Texel.Y, R, G));
		TestTrue(*FString::Printf(TEXT("texel (%d,%d) of cell 0 is done"), Texel.X, Texel.Y), R == 255 && G == 255);
	}
	Ground->GetCoverageTexel(20, 0, R, G);
	TestTrue(TEXT("the next cell to the right is still free"), R == 0 && G == 0);
	Ground->GetCoverageTexel(0, 20, R, G);
	TestTrue(TEXT("the next cell upwards is still free"), R == 0 && G == 0);

	// A raw strip is R only. It is stamped through the grid, as a test may, and synced by hand.
	const FVector2D Centre = AsphaltLot::CellCentre(AsphaltLot::CellIndex(50, 50));
	const FLotPour Pour = Game.GetGrid().Stamp(Centre - FVector2D(40.0, 0.0), Centre + FVector2D(40.0, 0.0), 40.0, 1000.0, ELotTexel::Raw);
	TestTrue(TEXT("the raw strip changed texels"), Pour.TexelsChanged > 0);
	Ground->SyncCoverage();
	const FIntPoint Mid = TexelAt(Centre);
	TestTrue(TEXT("raw texel readable"), Ground->GetCoverageTexel(Mid.X, Mid.Y, R, G));
	TestTrue(TEXT("a raw texel is R only"), R == 255 && G == 0);
	Ground->GetCoverageTexel(Mid.X, Mid.Y + 40, R, G);
	TestTrue(TEXT("beside the strip nothing changed"), R == 0 && G == 0);

	// Finishing the raw strip turns the same texels to done.
	Game.GetGrid().FinishRaw(Centre - FVector2D(40.0, 0.0), Centre + FVector2D(40.0, 0.0), 40.0);
	Ground->SyncCoverage();
	Ground->GetCoverageTexel(Mid.X, Mid.Y, R, G);
	TestTrue(TEXT("a finished raw texel is R and G"), R == 255 && G == 255);

	// A loaded or restarted game uploads everything again; plain changes never do.
	TestEqual(TEXT("the incremental syncs did not upload the whole texture"), Ground->GetNumFullUploads(), FullUploads);
	// NewGame deletes the slot, so it must not be the real one.
	UDIAWorldSubsystem::SetSlotNameOverride(TEXT("DIA_Test_World"));
	Subsystem->NewGame();
	UDIAWorldSubsystem::SetSlotNameOverride(FString());
	TestEqual(TEXT("a new game uploads the whole texture once"), Ground->GetNumFullUploads(), FullUploads + 1);
	Ground->GetCoverageTexel(10, 10, R, G);
	TestTrue(TEXT("the new game starts empty again"), R == 0 && G == 0);
	return true;
}

DIA_TEST(FSpawnFacingTest, "DoItAsphalt.Game.World.SpawnFacing")

bool FSpawnFacingTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}

	// A level PlayerStart at the layout's start looking along +X must not turn the player away from the trough.
	FActorSpawnParameters StartParams;
	StartParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* LevelStart = World->SpawnActor<APlayerStart>(FVector(LotLayout::PlayerStart.X, LotLayout::PlayerStart.Y, TestLevelStartZCm), FRotator::ZeroRotator, StartParams);
	if (!TestNotNull(TEXT("level player start"), LevelStart))
	{
		return false;
	}
	APlayerController* Controller = World->SpawnActor<ADIAPlayerController>();
	AActor* Start = Mode->ChoosePlayerStart(Controller);
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("start"), Start))
	{
		return false;
	}
	TestTrue(TEXT("the level's start is the one used"), Start == LevelStart);
	TestEqual(TEXT("the level's start looks along +X"), FMath::Abs(FMath::FindDeltaAngleDegrees(Start->GetActorRotation().Yaw, 0.0)), 0.0, TestYawToleranceDeg);
	Mode->RestartPlayer(Controller);
	const APawn* Pawn = Controller->GetPawn();
	if (!TestNotNull(TEXT("the player has a pawn"), Pawn))
	{
		return false;
	}
	TestTrue(TEXT("on the level's start"), FVector2D(Pawn->GetActorLocation()).Equals(LotLayout::PlayerStart, 1.0));
	TestEqual(TEXT("the body faces -Y"), FMath::Abs(FMath::FindDeltaAngleDegrees(Pawn->GetActorRotation().Yaw, LotLayout::PlayerStartYawDeg)), 0.0, TestYawToleranceDeg);
	TestEqual(TEXT("the camera looks -Y"), FMath::Abs(FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Yaw, LotLayout::PlayerStartYawDeg)), 0.0, TestYawToleranceDeg);
	const FVector2D Ahead(Pawn->GetActorForwardVector());
	TestTrue(TEXT("the trough is ahead of the player"), FVector2D::DotProduct(Ahead, (LotLayout::TroughPos - LotLayout::PlayerStart).GetSafeNormal()) > 0.999);
	return true;
}

DIA_TEST(FSpawnFacingNoLevelStartTest, "DoItAsphalt.Game.World.SpawnWithoutLevelStart")

bool FSpawnFacingNoLevelStartTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	// A PlayerStart elsewhere (yaw 0) is ignored: the layout's start wins.
	FActorSpawnParameters StartParams;
	StartParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<APlayerStart>(FVector(-3000.0, 2000.0, TestLevelStartZCm), FRotator::ZeroRotator, StartParams);
	APlayerController* Controller = World->SpawnActor<ADIAPlayerController>();
	if (!TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}
	Mode->RestartPlayer(Controller);
	const APawn* Pawn = Controller->GetPawn();
	if (!TestNotNull(TEXT("the player has a pawn"), Pawn))
	{
		return false;
	}
	TestTrue(TEXT("at the layout's start"), FVector2D(Pawn->GetActorLocation()).Equals(LotLayout::PlayerStart, 1.0));
	TestEqual(TEXT("the body faces -Y"), FMath::Abs(FMath::FindDeltaAngleDegrees(Pawn->GetActorRotation().Yaw, LotLayout::PlayerStartYawDeg)), 0.0, TestYawToleranceDeg);
	return true;
}

DIA_TEST(FSubsystemDispatchTest, "DoItAsphalt.Game.SubsystemDispatch")

bool FSubsystemDispatchTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = Wrapper.GetTestWorld()->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}

	int32 CellBroadcasts = 0;
	int32 CellsReported = 0;
	TArray<int32> Entries;
	TArray<ELotUnlock> Unlocks;
	int32 StateChanges = 0;
	TArray<FString> Order;
	Subsystem->OnCellsCompleted.AddLambda([&](const TArray<int32>& Cells)
	{
		++CellBroadcasts;
		CellsReported += Cells.Num();
		Order.Add(TEXT("cells"));
	});
	Subsystem->OnEntryCompleted.AddLambda([&](int32 Entry) { Entries.Add(Entry); Order.Add(TEXT("entry")); });
	Subsystem->OnUnlocked.AddLambda([&](ELotUnlock Unlock) { Unlocks.Add(Unlock); Order.Add(TEXT("unlock")); });
	Subsystem->OnStateChanged.AddLambda([&]() { ++StateChanges; Order.Add(TEXT("state")); });

	// Entry 0 is the 12 cells of AsphaltLot::EntryCell(0, i); its unlock needs UnlockCells[0] completed cells too.
	FLotGame& Game = Subsystem->GetGame();
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		const int32 Cell = AsphaltLot::EntryCell(0, I);
		TestFalse(TEXT("entry cell starts incomplete"), Game.GetGrid().IsCellComplete(Cell));
		Subsystem->Dispatch(Game.DebugCompleteCell(Cell));
	}

	TestEqual(TEXT("OnCellsCompleted once per dispatch"), CellBroadcasts, AsphaltLot::CellsPerEntry);
	TestEqual(TEXT("every cell reported"), CellsReported, AsphaltLot::CellsPerEntry);
	if (TestEqual(TEXT("OnEntryCompleted fires once"), Entries.Num(), 1))
	{
		TestEqual(TEXT("the completed entry is entry 0"), Entries[0], 0);
	}
	TestTrue(TEXT("the first unlock was announced"), Unlocks.Contains(ELotUnlock::QuickSwap));
	TestTrue(TEXT("OnStateChanged fires for every dispatch"), StateChanges >= AsphaltLot::CellsPerEntry);
	TestTrue(TEXT("OnStateChanged comes last in a dispatch"), Order.Num() > 0 && Order.Last() == TEXT("state"));
	TestFalse(TEXT("the exit stays closed"), Game.IsExitOpen());

	// Buying through the subsystem announces the item and the state.
	Game.DebugGive(1000000.0);
	int32 Bought = 0;
	Subsystem->OnItemBought.AddLambda([&](ELotItem) { ++Bought; });
	const int32 StatesBefore = StateChanges;
	ELotItem Affordable = ELotItem::Bucket;
	bool bFound = false;
	for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
	{
		const ELotItem Item = static_cast<ELotItem>(I);
		if (Game.CanBuyItem(Item))
		{
			Affordable = Item;
			bFound = true;
			break;
		}
	}
	if (TestTrue(TEXT("some item can be bought after completing an entry"), bFound))
	{
		TestTrue(TEXT("the item can be bought"), Subsystem->BuyItem(Affordable));
		TestEqual(TEXT("OnItemBought fired"), Bought, 1);
		TestTrue(TEXT("a purchase changes the state"), StateChanges > StatesBefore);
		TestFalse(TEXT("buying it twice fails"), Subsystem->BuyItem(Affordable));
		TestEqual(TEXT("a failed purchase announces nothing"), Bought, 1);
	}
	return true;
}

DIA_TEST(FAssetFallbackTest, "DoItAsphalt.Game.AssetFallback")

bool FAssetFallbackTest::RunTest(const FString& Parameters)
{
	FVector Scale = FVector::ZeroVector;
	UStaticMesh* Cube = DIAAssets::LoadMeshOrFallback(TEXT("SM_DoesNotExist"), DIAAssets::EFallback::Cube, FVector(200.0, 50.0, 100.0), Scale);
	if (TestNotNull(TEXT("fallback cube"), Cube))
	{
		TestTrue(TEXT("it is the engine cube"), Cube->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube")));
	}
	TestTrue(TEXT("scale yields the requested size"), Scale.Equals(FVector(2.0, 0.5, 1.0)));

	UStaticMesh* Cylinder = DIAAssets::LoadMeshOrFallback(TEXT("SM_DoesNotExist"), DIAAssets::EFallback::Cylinder, FVector(50.0), Scale);
	if (TestNotNull(TEXT("fallback cylinder"), Cylinder))
	{
		TestTrue(TEXT("it is the engine cylinder"), Cylinder->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cylinder")));
	}

	TestNotNull(TEXT("palette material exists"), DIAAssets::Palette(TEXT("DIA_Dirt")));
	TestNull(TEXT("unknown palette material"), DIAAssets::Palette(TEXT("DoesNotExist")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
