// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "HAL/FileManager.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/DIACharacter.h"
#include "Player/DIAInput.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAHUD.h"
#include "UI/DIAMenuModel.h"
#include "World/DIAAutoPaver.h"
#include "World/DIAGroundActor.h"
#include "World/DIAMovable.h"
#include "World/DIAPaverCar.h"
#include "World/DIAPlantActor.h"

namespace
{
	/** The slot every save test uses: never the real one. */
	const FString SaveTestSlotName = TEXT("DIA_Test");

	constexpr int32 SaveTestCells = 40;
	constexpr int32 SaveTestManyCells = 100;
	constexpr double SaveTestCredits = 123456.0;
	constexpr double SaveTestToleranceCm = 1.0;
	constexpr int32 SaveTestCorruptBytes = 1000;
	constexpr uint8 SaveTestCorruptFill = 0xAB;
	constexpr float SaveTestTickSeconds = 1.0f;
	/** The autosave comes after 30 s of game time: this much is just before it, this much safely after it. */
	constexpr int32 SaveTestTicksBeforeAutosave = 29;
	constexpr int32 SaveTestTicksAfterAutosave = 2;
	constexpr double SaveTestRichCredits = 1.0e9;
	constexpr float SaveTestShortTick = 0.1f;

	/** The backup of the test slot: the name the game gives it, so that a change of the suffix shows up here. */
	FString SaveTestBackupName()
	{
		return SaveTestSlotName + DIASave::BackupSuffix;
	}

	/** Takes the test slot for the length of a test: it is empty at the start and at the end, and the name override is gone afterwards. */
	struct FSaveTestSlot
	{
		FSaveTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(SaveTestSlotName);
			Delete();
		}
		~FSaveTestSlot()
		{
			Delete();
			UDIAWorldSubsystem::SetSlotNameOverride(FString());
		}
		/** Both the slot and its backup (DIA_Test and DIA_Test_bak). */
		static void Delete()
		{
			UGameplayStatics::DeleteGameInSlot(SaveTestSlotName, DIASave::UserIndex);
			DeleteBackup();
		}
		static void DeleteBackup()
		{
			UGameplayStatics::DeleteGameInSlot(SaveTestBackupName(), DIASave::UserIndex);
		}
		static bool Exists()
		{
			return UGameplayStatics::DoesSaveGameExist(SaveTestSlotName, DIASave::UserIndex);
		}
		static bool BackupExists()
		{
			return UGameplayStatics::DoesSaveGameExist(SaveTestBackupName(), DIASave::UserIndex);
		}
	};

	/** Completes the first NumCells incomplete cells like the cheat does. */
	void SaveTestComplete(UDIAWorldSubsystem* Subsystem, int32 NumCells)
	{
		Subsystem->Dispatch(Subsystem->GetGame().DebugCompleteCells(NumCells));
	}

	/** Moves an actor sideways without a sweep, keeping its height. */
	void SaveTestPlace(AActor* Actor, FVector2D Where)
	{
		Actor->SetActorLocation(FVector(Where.X, Where.Y, Actor->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	}

	FVector2D SaveTestWhere(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}

	/** The slot file on disk, for the test that puts rubbish into it. */
	FString SaveTestFile(const FString& Slot)
	{
		return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"));
	}

	FString SaveTestSlotFile()
	{
		return SaveTestFile(SaveTestSlotName);
	}
}

DIA_TEST(FSaveSlotRoundTripTest, "DoItAsphalt.Game.SaveSlot.RoundTrip")

bool FSaveSlotRoundTripTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	TestFalse(TEXT("the slot starts empty"), Subsystem->HasSave());
	TestFalse(TEXT("without a save there is nothing to load"), Subsystem->LoadGame());

	// A played game: credits, 40 cells, two purchases, the character at (300, 200).
	SaveTestComplete(Subsystem, SaveTestCells);
	Game.DebugGive(SaveTestCredits);
	TestTrue(TEXT("the first purchase"), Subsystem->BuyItem(ELotItem::Scoop));
	TestTrue(TEXT("the second purchase"), Subsystem->BuyPlant());
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D::ZeroVector);
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	const FVector2D Saved(300.0, 200.0);
	SaveTestPlace(Character, Saved);

	const double Credits = Game.GetCredits();
	const int32 Cells = Game.GetCompletedCells();
	const int32 PlantLevel = Game.GetPlantLevel();
	TestTrue(TEXT("something was played"), Cells == SaveTestCells && Game.Owns(ELotItem::Scoop) && PlantLevel == 1);

	Subsystem->SaveGame();
	TestTrue(TEXT("the slot exists now"), FSaveTestSlot::Exists());
	TestTrue(TEXT("and it holds a game"), Subsystem->HasSave());

	// The game moves on, then the save is loaded.
	Game.DebugGive(999.0);
	SaveTestPlace(Character, FVector2D::ZeroVector);
	int32 Resets = 0;
	Subsystem->OnGameReset.AddLambda([&Resets]() { ++Resets; });

	TestTrue(TEXT("LoadGame succeeds"), Subsystem->LoadGame());
	TestEqual(TEXT("credits as saved"), Game.GetCredits(), Credits);
	TestEqual(TEXT("cells as saved"), Game.GetCompletedCells(), Cells);
	TestTrue(TEXT("the item as saved"), Game.Owns(ELotItem::Scoop));
	TestEqual(TEXT("plant level as saved"), Game.GetPlantLevel(), PlantLevel);
	TestTrue(TEXT("the character stands where it was saved"), FVector2D::Distance(SaveTestWhere(Character), Saved) <= SaveTestToleranceCm);
	TestEqual(TEXT("OnGameReset fired once"), Resets, 1);
	return true;
}

DIA_TEST(FCorruptSlotStartsNewGameTest, "DoItAsphalt.Game.SaveSlot.CorruptSlotStartsNewGame")

bool FCorruptSlotStartsNewGameTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	FLotGame& Game = Subsystem->GetGame();
	const double StartCredits = Game.GetCredits();

	// A slot that holds a save object with 1000 bytes of rubbish.
	TArray<uint8> Rubbish;
	Rubbish.Init(SaveTestCorruptFill, SaveTestCorruptBytes);
	TestTrue(TEXT("the rubbish was written"), UDIAWorldSubsystem::WriteSlot(SaveTestSlotName, Rubbish));
	TestFalse(TEXT("corrupt blob: no save"), Subsystem->HasSave());
	TestFalse(TEXT("corrupt blob: LoadGame fails"), Subsystem->LoadGame());
	TestEqual(TEXT("the game is a new game"), Game.GetCredits(), StartCredits);
	TestEqual(TEXT("with no cells"), Game.GetCompletedCells(), 0);

	// A failed load leaves a game in progress alone.
	Game.DebugGive(1234.0);
	const double Credits = Game.GetCredits();
	int32 Resets = 0;
	Subsystem->OnGameReset.AddLambda([&Resets]() { ++Resets; });
	TestFalse(TEXT("LoadGame fails again"), Subsystem->LoadGame());
	TestEqual(TEXT("the game in progress is untouched"), Game.GetCredits(), Credits);
	TestEqual(TEXT("and nothing was announced"), Resets, 0);

	// A truncated real save (with no backup to fall back to).
	Subsystem->SaveGame();
	FSaveTestSlot::DeleteBackup();
	TArray<uint8> Blob = Game.SaveToBytes();
	Blob.SetNum(Blob.Num() / 2);
	UDIAWorldSubsystem::WriteSlot(SaveTestSlotName, Blob);
	TestFalse(TEXT("truncated blob: no save"), Subsystem->HasSave());
	TestFalse(TEXT("truncated blob: LoadGame fails"), Subsystem->LoadGame());

	// A slot file that is not a save at all.
	TArray<uint8> Garbage;
	Garbage.Init(SaveTestCorruptFill, SaveTestCorruptBytes);
	FFileHelper::SaveArrayToFile(Garbage, *SaveTestSlotFile());
	TestFalse(TEXT("garbage file: no save"), Subsystem->HasSave());
	TestFalse(TEXT("garbage file: LoadGame fails"), Subsystem->LoadGame());

	// A file with the right first bytes and rubbish after them.
	Garbage[0] = 'G';
	Garbage[1] = 'V';
	Garbage[2] = 'A';
	Garbage[3] = 'S';
	FFileHelper::SaveArrayToFile(Garbage, *SaveTestSlotFile());
	TestFalse(TEXT("garbage with the magic: no save"), Subsystem->HasSave());
	TestFalse(TEXT("garbage with the magic: LoadGame fails"), Subsystem->LoadGame());

	// The slot and its backup both rubbish: no save, and the main menu offers a new game only.
	TestTrue(TEXT("rubbish into the slot"), FFileHelper::SaveArrayToFile(Garbage, *SaveTestSlotFile()));
	TestTrue(TEXT("rubbish into the backup"), FFileHelper::SaveArrayToFile(Garbage, *SaveTestFile(SaveTestBackupName())));
	TestFalse(TEXT("both damaged: no save"), Subsystem->HasSave());
	TestFalse(TEXT("both damaged: LoadGame fails"), Subsystem->LoadGame());
	FDIAMenuModel Menu;
	Menu.Open(EDIAMenuMode::Main, Subsystem->HasSave());
	TestTrue(TEXT("the menu shows its buttons"), Menu.ShowsButtons());
	TestFalse(TEXT("but no Continue"), Menu.ShowsContinue());
	TestFalse(TEXT("and New Game asks nothing: there is nothing to throw away"), Menu.NeedsNewGameConfirmation());

	// No file.
	FSaveTestSlot::Delete();
	TestFalse(TEXT("no file: no save"), Subsystem->HasSave());
	TestFalse(TEXT("no file: LoadGame fails"), Subsystem->LoadGame());
	TestEqual(TEXT("the game is still the one in progress"), Game.GetCredits(), Credits);
	return true;
}

DIA_TEST(FNewGameResetsWorldTest, "DoItAsphalt.Game.SaveSlot.NewGameResetsWorld")

bool FNewGameResetsWorldTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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

	// 100 cells, a grown plant, the character far from the start, a save on disk.
	SaveTestComplete(Subsystem, SaveTestManyCells);
	Game.DebugGive(SaveTestCredits);
	Subsystem->BuyItem(ELotItem::Scoop);
	Game.DebugSetPlantLevel(LotBalance::PlantAddonBLevel);
	ADIAPlantActor* Plant = nullptr;
	for (TActorIterator<ADIAPlantActor> It(World); It; ++It)
	{
		Plant = *It;
	}
	ADIACharacter* Character = DIATest::SpawnCharacter(World, FVector2D(-800.0, 500.0));
	if (!TestNotNull(TEXT("character"), Character) || !TestNotNull(TEXT("plant"), Plant))
	{
		return false;
	}
	Wrapper.TickTestWorld(SaveTestShortTick);
	TestTrue(TEXT("the grown plant shows its addons"), Plant->GetAddonA()->IsVisible() && Plant->GetAddonB()->IsVisible());
	Subsystem->SaveGame();
	TestTrue(TEXT("there is a save"), Subsystem->HasSave());
	int32 Resets = 0;
	Subsystem->OnGameReset.AddLambda([&Resets]() { ++Resets; });

	Subsystem->NewGame();
	TestEqual(TEXT("no cells"), Game.GetCompletedCells(), 0);
	TestFalse(TEXT("the scoop is not owned"), Game.Owns(ELotItem::Scoop));
	TestEqual(TEXT("the plant is back at level 0"), Game.GetPlantLevel(), 0);
	Wrapper.TickTestWorld(SaveTestShortTick);
	TestFalse(TEXT("the addons are gone from the world"), Plant->GetAddonA()->IsVisible() || Plant->GetAddonB()->IsVisible());
	TestTrue(TEXT("the character is at the player start"), FVector2D::Distance(SaveTestWhere(Character), LotLayout::PlayerStart) <= SaveTestToleranceCm);
	TestFalse(TEXT("the slot is gone"), FSaveTestSlot::Exists());
	TestFalse(TEXT("so there is no save"), Subsystem->HasSave());
	TestEqual(TEXT("OnGameReset fired once"), Resets, 1);
	return true;
}

DIA_TEST(FAutosaveTest, "DoItAsphalt.Game.SaveSlot.Autosave")

bool FAutosaveTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
		Game.DebugGive(SaveTestRichCredits);

		// Off by default: nothing is written, however much happens.
		TestFalse(TEXT("autosave is off at first"), Subsystem->IsAutosaveEnabled());
		TestTrue(TEXT("a purchase"), Subsystem->BuyItem(ELotItem::Scoop));
		for (int32 I = 0; I < SaveTestTicksBeforeAutosave + SaveTestTicksAfterAutosave; ++I)
		{
			Subsystem->Tick(SaveTestTickSeconds);
		}
		TestFalse(TEXT("an autosave that is off writes nothing"), FSaveTestSlot::Exists());

		Subsystem->SetAutosaveEnabled(true);

		// A purchase saves.
		TestTrue(TEXT("a purchase with autosave"), Subsystem->BuyPlant());
		TestTrue(TEXT("the upgrade was saved"), FSaveTestSlot::Exists());

		// A purchase that fails saves nothing: the finisher is still locked (its unlock step is closed).
		FSaveTestSlot::Delete();
		TestFalse(TEXT("a locked item cannot be bought"), Subsystem->BuyItem(ELotItem::Finisher));
		TestFalse(TEXT("and saves nothing"), FSaveTestSlot::Exists());

		// An item saves too.
		TestTrue(TEXT("an item"), Subsystem->BuyItem(ELotItem::Bucket));
		TestTrue(TEXT("the item was saved"), FSaveTestSlot::Exists());

		// The timer: 29 s of game time is nothing, 31 s save.
		FSaveTestSlot::Delete();
		for (int32 I = 0; I < SaveTestTicksBeforeAutosave; ++I)
		{
			Subsystem->Tick(SaveTestTickSeconds);
		}
		TestFalse(TEXT("nothing after 29 s"), FSaveTestSlot::Exists());
		for (int32 I = 0; I < SaveTestTicksAfterAutosave; ++I)
		{
			Subsystem->Tick(SaveTestTickSeconds);
		}
		TestTrue(TEXT("saved after 31 s"), FSaveTestSlot::Exists());

		// Winning saves.
		SaveTestComplete(Subsystem, AsphaltLot::NumCells);
		FSaveTestSlot::Delete();
		Subsystem->Win();
		TestTrue(TEXT("the win is saved"), Game.HasWon());
		TestTrue(TEXT("the win was saved"), FSaveTestSlot::Exists());

		// The end of the world saves: the slot is deleted now and has to be there again after the world is gone.
		FSaveTestSlot::Delete();
		TestFalse(TEXT("the slot is empty before the end"), FSaveTestSlot::Exists());
	}
	TestTrue(TEXT("EndPlay saved"), FSaveTestSlot::Exists());
	return true;
}

DIA_TEST(FSaveSlotNameTest, "DoItAsphalt.Game.SaveSlot.Name")

bool FSaveSlotNameTest::RunTest(const FString& Parameters)
{
	UDIAWorldSubsystem::SetSlotNameOverride(FString());
	TestEqual(TEXT("the real slot by default"), UDIAWorldSubsystem::SlotName(), FString(TEXT("DoItAsphalt_Slot0")));
	UDIAWorldSubsystem::SetSlotNameOverride(SaveTestSlotName);
	TestEqual(TEXT("the override"), UDIAWorldSubsystem::SlotName(), SaveTestSlotName);
	UDIAWorldSubsystem::SetSlotNameOverride(FString());
	TestEqual(TEXT("back to the real slot"), UDIAWorldSubsystem::SlotName(), FString(TEXT("DoItAsphalt_Slot0")));
	return true;
}

namespace
{
	/** A player controller that possesses the character, as in the real game. */
	ADIAPlayerController* SaveTestPossess(UWorld* World, ADIACharacter* Character)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
		if (Controller)
		{
			Controller->Possess(Character);
		}
		return Controller;
	}

	/** The HUD of the controller, spawned like the engine does it (owned by the controller). */
	ADIAHUD* SaveTestHud(UWorld* World, ADIAPlayerController* Controller)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = Controller;
		ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
		// The engine sets this when it creates the HUD for the player.
		Controller->MyHUD = Hud;
		return Hud;
	}

	/** Writes a blob into the slot as the game would. */
	void SaveTestWriteBlob(const TArray<uint8>& Blob)
	{
		UDIAWorldSubsystem::WriteSlot(SaveTestSlotName, Blob);
	}
}

DIA_TEST(FLoadMovesActorsTest, "DoItAsphalt.Game.LoadMovesActors")

bool FLoadMovesActorsTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	const FVector2D CharacterSaved(-200.0, 100.0);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, CharacterSaved);
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	Wrapper.TickTestWorld(SaveTestShortTick);
	Subsystem->SaveGame();

	// The character moves on, and a tick lets it write its new place into the core, as the game does every frame.
	SaveTestPlace(Character, FVector2D(50.0, 50.0));
	Wrapper.TickTestWorld(SaveTestShortTick);
	TestTrue(TEXT("the core knows the new place"), FVector2D::Distance(Game.World().PlayerPos, FVector2D(50.0, 50.0)) <= SaveTestToleranceCm);

	// Loading puts it back at once, before any tick could write the old place over the loaded one.
	TestTrue(TEXT("LoadGame"), Subsystem->LoadGame());
	TestTrue(TEXT("the character is at the saved place"), FVector2D::Distance(SaveTestWhere(Character), CharacterSaved) <= SaveTestToleranceCm);
	TestTrue(TEXT("the core still says so"), FVector2D::Distance(Game.World().PlayerPos, CharacterSaved) <= SaveTestToleranceCm);

	Wrapper.TickTestWorld(SaveTestShortTick);
	TestTrue(TEXT("a tick later the character is still there"), FVector2D::Distance(SaveTestWhere(Character), CharacterSaved) <= SaveTestToleranceCm);
	TestTrue(TEXT("and the core agrees"), FVector2D::Distance(Game.World().PlayerPos, CharacterSaved) <= SaveTestToleranceCm);
	return true;
}

DIA_TEST(FResetRedrawsWorldTest, "DoItAsphalt.Game.ResetRedrawsWorld")

bool FResetRedrawsWorldTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	ADIAGroundActor* Ground = nullptr;
	for (TActorIterator<ADIAGroundActor> It(World); It; ++It)
	{
		Ground = *It;
	}
	if (!TestNotNull(TEXT("ground"), Ground) || !TestNotNull(TEXT("coverage texture"), Ground->GetCoverageTexture()))
	{
		return false;
	}

	// A finished game: the whole car park is done.
	SaveTestComplete(Subsystem, AsphaltLot::NumCells);
	uint8 R = 0;
	uint8 G = 0;
	Ground->GetCoverageTexel(10, 10, R, G);
	TestTrue(TEXT("the ground shows the finished game"), R == 255 && G == 255);
	const TArray<uint8> Finished = Subsystem->GetGame().SaveToBytes();

	// Starting over uploads the whole texture and the ground is empty again.
	const int32 Uploads = Ground->GetNumFullUploads();
	Subsystem->NewGame();
	TestEqual(TEXT("the whole texture was uploaded"), Ground->GetNumFullUploads(), Uploads + 1);
	Ground->GetCoverageTexel(10, 10, R, G);
	TestTrue(TEXT("the ground is empty again"), R == 0 && G == 0);

	// Loading the finished game does it again.
	SaveTestWriteBlob(Finished);
	TestTrue(TEXT("LoadGame"), Subsystem->LoadGame());
	TestEqual(TEXT("the whole texture was uploaded again"), Ground->GetNumFullUploads(), Uploads + 2);
	Ground->GetCoverageTexel(10, 10, R, G);
	TestTrue(TEXT("the ground shows the finished game again"), R == 255 && G == 255);
	return true;
}

DIA_TEST(FMenuModelTest, "DoItAsphalt.Game.MenuModel")

bool FMenuModelTest::RunTest(const FString& Parameters)
{
	FDIAMenuModel Model;
	TestFalse(TEXT("closed at first"), Model.IsOpen());
	TestFalse(TEXT("closed: no buttons"), Model.ShowsButtons());

	// The main menu without a save: New Game and Quit only, and New Game throws nothing away.
	Model.Open(EDIAMenuMode::Main, false);
	TestTrue(TEXT("open"), Model.IsOpen());
	TestTrue(TEXT("buttons"), Model.ShowsButtons());
	TestFalse(TEXT("no Continue without a save"), Model.ShowsContinue());
	TestFalse(TEXT("no Resume in the main menu"), Model.ShowsResume());
	TestFalse(TEXT("New Game asks nothing without a save"), Model.NeedsNewGameConfirmation());

	// With a save: Continue, and New Game asks.
	Model.Open(EDIAMenuMode::Main, true);
	TestTrue(TEXT("Continue with a save"), Model.ShowsContinue());
	TestTrue(TEXT("New Game asks over a save"), Model.NeedsNewGameConfirmation());

	// The pause menu: Resume, never Continue, and New Game asks.
	Model.Open(EDIAMenuMode::Pause, true);
	TestTrue(TEXT("Resume in the pause menu"), Model.ShowsResume());
	TestFalse(TEXT("no Continue in the pause menu"), Model.ShowsContinue());
	TestTrue(TEXT("New Game asks in the pause menu"), Model.NeedsNewGameConfirmation());
	TestEqual(TEXT("title"), Model.GetTitle().ToString(), FString(TEXT("Paused")));

	// The question replaces the buttons.
	Model.bConfirming = true;
	TestTrue(TEXT("the question is up"), Model.ShowsQuestion());
	TestFalse(TEXT("the buttons are gone"), Model.ShowsButtons());
	TestFalse(TEXT("so is Resume"), Model.ShowsResume());
	TestEqual(TEXT("the question"), FDIAMenuModel::GetQuestion().ToString(), FString(TEXT("Start over? Progress will be lost.")));
	Model.Open(EDIAMenuMode::Pause, false);
	TestFalse(TEXT("opening again drops the question"), Model.ShowsQuestion());
	Model.Close();
	TestFalse(TEXT("closed again"), Model.IsOpen());
	TestFalse(TEXT("closed: no question"), Model.ShowsQuestion());
	return true;
}

DIA_TEST(FMenuPauseFlowTest, "DoItAsphalt.Game.MenuPauseFlow")

bool FMenuPauseFlowTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::PlayerStart);
	ADIAPlayerController* Controller = Character ? SaveTestPossess(World, Character) : nullptr;
	ADIAHUD* Hud = Controller ? SaveTestHud(World, Controller) : nullptr;
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}

	// Without a window the game does not start in the menu (the tests depend on it).
	TestFalse(TEXT("no menu at the start of a world without a window"), Hud->IsMenuOpen());
	TestFalse(TEXT("not paused"), World->IsPaused());
	// The controller only builds its input in a world with a local player, so it is built here as it builds it.
	const FDIAInput Input = FDIAInput::Create(World);
	TestTrue(TEXT("Esc is heard while paused"), Input.Pause && Input.Pause->bTriggerWhenPaused);

	// Esc opens the pause menu, pauses and takes the input; Esc again gives everything back.
	Controller->OnPause();
	TestEqual(TEXT("the pause menu is open"), Hud->GetMenuMode(), EDIAMenuMode::Pause);
	TestTrue(TEXT("the controller takes the menu input"), Controller->IsMenuInput());
	TestTrue(TEXT("the game is paused"), World->IsPaused());
	TestTrue(TEXT("nothing can be used"), Controller->FindInteractable() == nullptr);
	Controller->OnPause();
	TestFalse(TEXT("Esc closes the pause menu"), Hud->IsMenuOpen());
	TestFalse(TEXT("the controller gave the input back"), Controller->IsMenuInput());
	TestFalse(TEXT("the game runs again"), World->IsPaused());
	TestFalse(TEXT("opening and closing the pause menu does not start the autosave"), Subsystem->IsAutosaveEnabled());

	// The main menu: Esc does nothing, closing it starts the game (and its autosave).
	Hud->OpenMenu(EDIAMenuMode::Main);
	TestEqual(TEXT("the main menu is open"), Hud->GetMenuMode(), EDIAMenuMode::Main);
	TestTrue(TEXT("paused"), World->IsPaused());
	Controller->OnPause();
	TestEqual(TEXT("Esc does not close the main menu"), Hud->GetMenuMode(), EDIAMenuMode::Main);
	Hud->OpenMenu(EDIAMenuMode::Pause);
	TestEqual(TEXT("a menu does not open over a menu"), Hud->GetMenuMode(), EDIAMenuMode::Main);
	TestFalse(TEXT("the autosave waits"), Subsystem->IsAutosaveEnabled());
	Hud->SkipMenu();
	TestFalse(TEXT("the menu is gone"), Hud->IsMenuOpen());
	TestFalse(TEXT("the game runs"), World->IsPaused());
	TestTrue(TEXT("the game saves itself from now on"), Subsystem->IsAutosaveEnabled());

	// Quitting from the main menu never turns the autosave on (it would overwrite the save with an empty game).
	Subsystem->SetAutosaveEnabled(false);
	Hud->OpenMenu(EDIAMenuMode::Main);
	Hud->Destroy();
	TestFalse(TEXT("the HUD going gives the pause back"), World->IsPaused());
	TestFalse(TEXT("and the input"), Controller->IsMenuInput());
	TestFalse(TEXT("without starting the autosave"), Subsystem->IsAutosaveEnabled());
	return true;
}

DIA_TEST(FHudEndReleasesInputTest, "DoItAsphalt.Game.HudEndReleasesInput")

bool FHudEndReleasesInputTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::PlayerStart);
	ADIAPlayerController* Controller = Character ? SaveTestPossess(World, Character) : nullptr;
	ADIAHUD* Hud = Controller ? SaveTestHud(World, Controller) : nullptr;
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}

	// The menu taking the input, then the HUD going away: the controller gets its input back.
	Controller->OnPause();
	TestTrue(TEXT("the controller takes the menu input"), Controller->IsMenuInput());
	Hud->Destroy();
	TestFalse(TEXT("the HUD's EndPlay gives the menu input back"), Controller->IsMenuInput());
	TestFalse(TEXT("and the pause"), World->IsPaused());
	return true;
}

DIA_TEST(FWinScreenNewGameTest, "DoItAsphalt.Game.WinScreenNewGame")

bool FWinScreenNewGameTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::PlayerStart);
	ADIAPlayerController* Controller = Character ? SaveTestPossess(World, Character) : nullptr;
	ADIAHUD* Hud = Controller ? SaveTestHud(World, Controller) : nullptr;
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}

	SaveTestComplete(Subsystem, AsphaltLot::NumCells);
	Subsystem->Win();
	TestTrue(TEXT("the win screen is up"), Hud->IsWinScreenShown());
	TestTrue(TEXT("the controller takes the win input"), Controller->IsWinInput());
	Controller->OnPause();
	TestFalse(TEXT("Esc does not open a menu over the win screen"), Hud->IsMenuOpen());
	const TArray<uint8> Won = Subsystem->GetGame().SaveToBytes();

	// New Game starts over in the same world: the screen goes and the game is fresh.
	Hud->NewGame();
	TestFalse(TEXT("the win screen is gone"), Hud->IsWinScreenShown());
	TestFalse(TEXT("the controller gave the input back"), Controller->IsWinInput());
	TestEqual(TEXT("a fresh game"), Subsystem->GetGame().GetCompletedCells(), 0);
	TestFalse(TEXT("not won"), Subsystem->GetGame().HasWon());

	// Continuing a won game shows its win screen again.
	SaveTestWriteBlob(Won);
	Subsystem->LoadGame();
	TestTrue(TEXT("the win screen is up again"), Hud->IsWinScreenShown());
	TestTrue(TEXT("with the input"), Controller->IsWinInput());
	return true;
}

namespace
{
	/** A game in a world, saved once: the starting point of the tests of the backup. */
	constexpr double SaveTestCreditsA = 5000.0;
	constexpr double SaveTestCreditsB = 9000.0;
	/** Lengths at which a write can be cut short: inside the magic, inside the header, and deep in the blob. */
	constexpr int32 SaveTestCutLengths[] = {0, 1, 3, 4, 5, 8, 16, 32, 64, 128, 1000};

	void SaveTestWriteFile(const FString& Slot, const TArray<uint8>& Bytes)
	{
		FFileHelper::SaveArrayToFile(Bytes, *SaveTestFile(Slot));
	}

	TArray<uint8> SaveTestReadFile(const FString& Slot)
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *SaveTestFile(Slot));
		return Bytes;
	}

	TArray<uint8> SaveTestRubbish()
	{
		TArray<uint8> Rubbish;
		Rubbish.Init(SaveTestCorruptFill, SaveTestCorruptBytes);
		return Rubbish;
	}
}

DIA_TEST(FSaveBackupFallbackTest, "DoItAsphalt.Game.SaveSlot.BackupFallback")

bool FSaveBackupFallbackTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	FLotGame& Game = Subsystem->GetGame();
	TestEqual(TEXT("the backup is the slot with _bak"), UDIAWorldSubsystem::BackupSlotName(), SaveTestBackupName());

	// A save writes the slot and the backup.
	Game.DebugGive(SaveTestCreditsA);
	const double CreditsA = Game.GetCredits();
	Subsystem->SaveGame();
	TestTrue(TEXT("the slot was written"), FSaveTestSlot::Exists());
	TestTrue(TEXT("and the backup"), FSaveTestSlot::BackupExists());

	// A good slot is read first: the slot holds a later game than the backup does.
	Game.DebugGive(SaveTestCreditsB);
	const double CreditsB = Game.GetCredits();
	SaveTestWriteBlob(Game.SaveToBytes());
	TestTrue(TEXT("a good slot and a good backup"), Subsystem->LoadGame());
	TestEqual(TEXT("the slot wins"), Subsystem->GetGame().GetCredits(), CreditsB);

	// A slot that a cut-short write left as rubbish: the backup's game comes back.
	SaveTestWriteFile(SaveTestSlotName, SaveTestRubbish());
	Subsystem->GetGame().DebugGive(SaveTestCreditsB);
	TestTrue(TEXT("a damaged slot still counts as a save"), Subsystem->HasSave());
	TestTrue(TEXT("it loads from the backup"), Subsystem->LoadGame());
	TestEqual(TEXT("the backup's game"), Subsystem->GetGame().GetCredits(), CreditsA);

	// The same with a slot that is a real save cut in half, and with a slot that is gone.
	Subsystem->SaveGame();
	TArray<uint8> Half = SaveTestReadFile(SaveTestSlotName);
	Half.SetNum(Half.Num() / 2);
	SaveTestWriteFile(SaveTestSlotName, Half);
	Subsystem->GetGame().DebugGive(SaveTestCreditsB);
	TestTrue(TEXT("a half slot loads from the backup"), Subsystem->LoadGame());
	TestEqual(TEXT("the backup's game again"), Subsystem->GetGame().GetCredits(), CreditsA);
	UGameplayStatics::DeleteGameInSlot(SaveTestSlotName, DIASave::UserIndex);
	Subsystem->GetGame().DebugGive(SaveTestCreditsB);
	TestTrue(TEXT("a missing slot loads from the backup"), Subsystem->LoadGame());
	TestEqual(TEXT("the backup's game from a missing slot"), Subsystem->GetGame().GetCredits(), CreditsA);
	return true;
}

DIA_TEST(FSaveBothDamagedTest, "DoItAsphalt.Game.SaveSlot.BothDamaged")

bool FSaveBothDamagedTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	FLotGame& Game = Subsystem->GetGame();
	Game.DebugGive(SaveTestCreditsA);
	const double Credits = Game.GetCredits();
	int32 Resets = 0;
	Subsystem->OnGameReset.AddLambda([&Resets]() { ++Resets; });

	SaveTestWriteFile(SaveTestSlotName, SaveTestRubbish());
	SaveTestWriteFile(SaveTestBackupName(), SaveTestRubbish());
	TestFalse(TEXT("two files of rubbish: no save"), Subsystem->HasSave());
	TestFalse(TEXT("two files of rubbish: LoadGame fails"), Subsystem->LoadGame());

	// Two real saves, both cut short.
	Subsystem->SaveGame();
	TArray<uint8> Cut = SaveTestReadFile(SaveTestSlotName);
	Cut.SetNum(Cut.Num() / 2);
	SaveTestWriteFile(SaveTestSlotName, Cut);
	SaveTestWriteFile(SaveTestBackupName(), Cut);
	TestFalse(TEXT("two cut saves: no save"), Subsystem->HasSave());
	TestFalse(TEXT("two cut saves: LoadGame fails"), Subsystem->LoadGame());

	// A well-formed save file whose blob is rubbish, in front of a cut backup.
	SaveTestWriteBlob(SaveTestRubbish());
	TestFalse(TEXT("a rubbish blob in the slot, a cut backup: no save"), Subsystem->HasSave());

	TestEqual(TEXT("the game was never touched"), Game.GetCredits(), Credits);
	TestEqual(TEXT("and nothing was announced"), Resets, 0);
	return true;
}

DIA_TEST(FNewGameRemovesBackupTest, "DoItAsphalt.Game.SaveSlot.NewGameRemovesBackup")

bool FNewGameRemovesBackupTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	Subsystem->GetGame().DebugGive(SaveTestCreditsA);
	Subsystem->SaveGame();
	TestTrue(TEXT("the slot is there"), FSaveTestSlot::Exists());
	TestTrue(TEXT("and the backup"), FSaveTestSlot::BackupExists());

	Subsystem->NewGame();
	TestFalse(TEXT("New Game removed the slot"), FSaveTestSlot::Exists());
	TestFalse(TEXT("and the backup"), FSaveTestSlot::BackupExists());
	TestFalse(TEXT("there is no save left to continue"), Subsystem->HasSave());
	TestFalse(TEXT("and nothing to load"), Subsystem->LoadGame());
	return true;
}

DIA_TEST(FSaveCutShortNeverCrashesTest, "DoItAsphalt.Game.SaveSlot.CutShortNeverCrashes")

bool FSaveCutShortNeverCrashesTest::RunTest(const FString& Parameters)
{
	FSaveTestSlot Slot;
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
	FLotGame& Game = Subsystem->GetGame();
	SaveTestComplete(Subsystem, SaveTestCells);
	Game.DebugGive(SaveTestCreditsA);
	const double Credits = Game.GetCredits();
	Subsystem->SaveGame();
	const TArray<uint8> Whole = SaveTestReadFile(SaveTestSlotName);
	if (!TestTrue(TEXT("a real save file to cut"), Whole.Num() > SaveTestCutLengths[UE_ARRAY_COUNT(SaveTestCutLengths) - 1]))
	{
		return false;
	}
	FSaveTestSlot::DeleteBackup();
	TestTrue(TEXT("the whole file is a save"), Subsystem->HasSave());

	TArray<int32> Lengths(SaveTestCutLengths, UE_ARRAY_COUNT(SaveTestCutLengths));
	Lengths.Add(Whole.Num() / 4);
	Lengths.Add(Whole.Num() / 2);
	Lengths.Add(Whole.Num() - 1);
	for (const int32 Length : Lengths)
	{
		TArray<uint8> Cut(Whole.GetData(), Length);
		SaveTestWriteFile(SaveTestSlotName, Cut);
		TestFalse(*FString::Printf(TEXT("cut at %d bytes: no save"), Length), Subsystem->HasSave());
		TestFalse(*FString::Printf(TEXT("cut at %d bytes: LoadGame fails"), Length), Subsystem->LoadGame());
	}
	// One flipped byte anywhere in an otherwise whole file: the CRC catches it.
	for (const int32 At : {0, Whole.Num() / 2, Whole.Num() - 1})
	{
		TArray<uint8> Flipped = Whole;
		Flipped[At] ^= 0xFF;
		SaveTestWriteFile(SaveTestSlotName, Flipped);
		TestFalse(*FString::Printf(TEXT("byte %d flipped: no save"), At), Subsystem->HasSave());
		TestFalse(*FString::Printf(TEXT("byte %d flipped: LoadGame fails"), At), Subsystem->LoadGame());
	}
	TestEqual(TEXT("the game was never touched"), Game.GetCredits(), Credits);
	// The whole file once more is a save again.
	SaveTestWriteFile(SaveTestSlotName, Whole);
	TestTrue(TEXT("the whole file is a save"), Subsystem->LoadGame());
	return true;
}

namespace SaveWorldTestSupport
{
	constexpr float TickSeconds = 0.1f;
	constexpr double MovedCm = 0.5;
	constexpr int32 PaverSector = 5;

	/** What the world tests share: a started world, the subsystem, a player with a controller and a HUD. */
	struct FSaveWorldScene
	{
		FTestWorldWrapper Wrapper;
		UWorld* World = nullptr;
		UDIAWorldSubsystem* Subsystem = nullptr;
		ADIACharacter* Character = nullptr;
		ADIAPlayerController* Controller = nullptr;

		bool Start(FAutomationTestBase& Test, FVector2D PlayerAt)
		{
			if (!DIATest::StartGameWorld(Wrapper, Test))
			{
				return false;
			}
			World = Wrapper.GetTestWorld();
			Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
			Character = DIATest::SpawnCharacter(World, PlayerAt);
			if (Character)
			{
				Controller = SaveTestPossess(World, Character);
			}
			ADIAHUD* Hud = Controller ? SaveTestHud(World, Controller) : nullptr;
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

		/** What the cheat dia.Item does: gives the item for free and announces it like a purchase (the actor appears). */
		void Grant(ELotItem Item) const
		{
			Game().DebugGrantItem(Item);
			Subsystem->OnItemBought.Broadcast(Item);
			Subsystem->OnStateChanged.Broadcast();
		}

		template <typename T>
		TArray<T*> Find() const
		{
			TArray<T*> Result;
			for (TActorIterator<T> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					Result.Add(*It);
				}
			}
			return Result;
		}

		/** The one actor of the type; null if there are none or several. */
		template <typename T>
		T* Only() const
		{
			const TArray<T*> All = Find<T>();
			return All.Num() == 1 ? All[0] : nullptr;
		}

		ADIAMovable* Movable(ELotMovable Kind) const
		{
			for (ADIAMovable* Actor : Find<ADIAMovable>())
			{
				if (Actor->GetKind() == Kind)
				{
					return Actor;
				}
			}
			return nullptr;
		}

		/** Where the core says the movable stands, announced to the actors. */
		void SetMovable(ELotMovable Kind, FVector2D Where, double YawDeg) const
		{
			FLotMovable& State = Game().World().Movables[static_cast<int32>(Kind)];
			State.Pos = Where;
			State.YawDeg = YawDeg;
			Subsystem->Dispatch(FLotEvents());
		}

		void StandAt(FVector2D Where) const
		{
			SaveTestPlace(Character, Where);
		}
	};

	void PlaceCarAt(ADIAPaverCar* Car, FVector2D At, double YawDeg)
	{
		Car->SetActorLocationAndRotation(FVector(At.X, At.Y, DIAPaverCar::OriginHeightCm()), FRotator(0.0, YawDeg, 0.0));
	}

	/** A blob of the v1 game (the hex lot): the v1 header ("DIA1", version 1) and then zeros where its state would be. */
	TArray<uint8> V1Blob()
	{
		constexpr uint32 V1Magic = 0x31414944;
		constexpr int32 V1Version = 1;
		constexpr int32 V1Cells = 61;
		constexpr int32 V1GridSize = 256;
		constexpr int32 V1BodyBytes = 300;
		TArray<uint8> Blob;
		Blob.Append(reinterpret_cast<const uint8*>(&V1Magic), sizeof(V1Magic));
		Blob.Append(reinterpret_cast<const uint8*>(&V1Version), sizeof(V1Version));
		Blob.Append(reinterpret_cast<const uint8*>(&V1Cells), sizeof(V1Cells));
		Blob.Append(reinterpret_cast<const uint8*>(&V1GridSize), sizeof(V1GridSize));
		Blob.AddZeroed(V1BodyBytes);
		return Blob;
	}
}

DIA_TEST(FSaveLoadMovesEverythingTest, "DoItAsphalt.Game.SaveSlot.LoadMovesEverything")

bool FSaveLoadMovesEverythingTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
	FSaveWorldScene Scene;
	const FVector2D PlayerSaved(100.0, -9000.0);
	const FVector2D CartSaved(2000.0, -5000.0);
	const FVector2D CarSaved(-3000.0, 0.0);
	constexpr double CartYaw = 30.0;
	constexpr double CarYaw = 90.0;
	if (!Scene.Start(*this, PlayerSaved))
	{
		return false;
	}
	FLotGame& Game = Scene.Game();
	SaveTestComplete(Scene.Subsystem, SaveTestCells);
	Game.DebugGive(SaveTestCredits);
	Scene.Grant(ELotItem::Cart);
	Scene.Grant(ELotItem::AutoPaver);
	Scene.Grant(ELotItem::Car);
	ADIAPaverCar* Car = Scene.Only<ADIAPaverCar>();
	ADIAAutoPaver* Paver = Scene.Only<ADIAAutoPaver>();
	ADIAGroundActor* Ground = Scene.Only<ADIAGroundActor>();
	if (!TestNotNull(TEXT("the cart"), Scene.Movable(ELotMovable::Cart)) || !TestNotNull(TEXT("the auto paver"), Paver)
		|| !TestNotNull(TEXT("the car"), Car) || !TestNotNull(TEXT("the ground"), Ground))
	{
		return false;
	}

	// The game as it is saved: the cart, the car and the player in their places, the auto paver on its way to sector 5.
	Scene.SetMovable(ELotMovable::Cart, CartSaved, CartYaw);
	PlaceCarAt(Car, CarSaved, CarYaw);
	Scene.StandAt(PlayerSaved);
	TestTrue(TEXT("the paver is sent to sector 5"), Game.AssignAutoPaverSector(PaverSector));
	Scene.TickFor(3.0);
	const FVector2D PaverSaved = Game.GetAutoPaverPos();
	TestTrue(TEXT("the paver has left its start"), FVector2D::Distance(PaverSaved, LotLayout::ItemSpawn(ELotItem::AutoPaver)) > 10.0);
	const double Credits = Game.GetCredits();
	const int32 Cells = Game.GetCompletedCells();
	Scene.Subsystem->SaveGame();
	const TArray<uint8> Blob = Game.SaveToBytes();

	// Everything moves on.
	Scene.SetMovable(ELotMovable::Cart, FVector2D(-500.0, -500.0), 0.0);
	PlaceCarAt(Car, FVector2D(3000.0, -3000.0), 0.0);
	Scene.StandAt(FVector2D(0.0, 0.0));
	Game.DebugGive(777.0);
	Scene.TickFor(3.0);
	TestTrue(TEXT("the shown paver lags behind the core while it drives"), FVector2D(Paver->GetActorLocation()).Equals(Game.GetAutoPaverPos(), MovedCm) == false);

	// Every actor stands where the core says, as the call returns and still a few ticks later.
	auto CheckAll = [&](const TCHAR* When, bool bFrozen)
	{
		const FString Suffix = FString::Printf(TEXT(" (%s)"), When);
		if (bFrozen)
		{
			// The auto paver keeps working once the clock runs, so credits and cells only match at the moment of the load.
			TestEqual(*(TEXT("credits") + Suffix), Game.GetCredits(), Credits);
			TestEqual(*(TEXT("cells") + Suffix), Game.GetCompletedCells(), Cells);
		}
		TestEqual(*(TEXT("the paver's sector") + Suffix), Game.GetAutoPaverSector(), PaverSector);
		TestEqual(*(TEXT("one movable") + Suffix), Scene.Find<ADIAMovable>().Num(), 1);
		TestEqual(*(TEXT("one auto paver") + Suffix), Scene.Find<ADIAAutoPaver>().Num(), 1);
		TestEqual(*(TEXT("one car") + Suffix), Scene.Find<ADIAPaverCar>().Num(), 1);
		const ADIAMovable* LoadedCart = Scene.Movable(ELotMovable::Cart);
		const ADIAPaverCar* LoadedCar = Scene.Only<ADIAPaverCar>();
		if (LoadedCart && LoadedCar)
		{
			TestTrue(*(TEXT("the cart is where it was saved") + Suffix), SaveTestWhere(LoadedCart).Equals(CartSaved, MovedCm));
			TestTrue(*(TEXT("turned as it was saved") + Suffix), FMath::Abs(FMath::FindDeltaAngleDegrees(LoadedCart->GetActorRotation().Yaw, CartYaw)) < MovedCm);
			TestTrue(*(TEXT("the car is where it was saved") + Suffix), SaveTestWhere(LoadedCar).Equals(CarSaved, MovedCm));
		}
		TestTrue(*(TEXT("the player is where it was saved") + Suffix), SaveTestWhere(Scene.Character).Equals(PlayerSaved, MovedCm));
		TestTrue(*(TEXT("the core agrees about the player") + Suffix), Game.World().PlayerPos.Equals(PlayerSaved, MovedCm));
		TestTrue(*(TEXT("nothing is held") + Suffix), Game.GetHeld() == ELotMovable::Count);
		TestFalse(*(TEXT("the player is on foot") + Suffix), Game.World().bPlayerInCar);
	};

	const int32 UploadsBefore = Ground->GetNumFullUploads();
	TestTrue(TEXT("LoadGame"), Scene.Subsystem->LoadGame());
	CheckAll(TEXT("as the load returns"), true);
	TestEqual(TEXT("the paver's position is the saved one"), Game.GetAutoPaverPos().X, PaverSaved.X, MovedCm);
	const ADIAAutoPaver* LoadedPaver = Scene.Only<ADIAAutoPaver>();
	if (TestNotNull(TEXT("the paver actor"), LoadedPaver))
	{
		TestTrue(TEXT("the paver stands on the saved position at once (no smoothing from the other game)"), SaveTestWhere(LoadedPaver).Equals(PaverSaved, MovedCm));
	}
	TestEqual(TEXT("the ground was uploaded in full once"), Ground->GetNumFullUploads(), UploadsBefore + 1);
	Scene.TickFor(0.2);
	CheckAll(TEXT("two ticks after the load"), false);
	TestEqual(TEXT("and not again on the ticks"), Ground->GetNumFullUploads(), UploadsBefore + 1);

	// A new game takes every machine away, and the saved game brings them back to the same places.
	Scene.Subsystem->NewGame();
	Scene.Wrapper.TickTestWorld(TickSeconds);
	TestEqual(TEXT("a new game: no movable"), Scene.Find<ADIAMovable>().Num(), 0);
	TestEqual(TEXT("the ground was uploaded in full once more"), Ground->GetNumFullUploads(), UploadsBefore + 2);
	TestTrue(TEXT("the saved game is written back"), UDIAWorldSubsystem::WriteSlot(SaveTestSlotName, Blob));
	TestTrue(TEXT("LoadGame after a new game"), Scene.Subsystem->LoadGame());
	CheckAll(TEXT("loaded into a new game"), true);
	TestEqual(TEXT("and once more for that load"), Ground->GetNumFullUploads(), UploadsBefore + 3);
	Scene.TickFor(0.2);
	CheckAll(TEXT("two ticks after loading into a new game"), false);
	return true;
}

DIA_TEST(FSaveHeldItemIsSavedPutDownTest, "DoItAsphalt.Game.SaveSlot.HeldItemIsSavedPutDown")

bool FSaveHeldItemIsSavedPutDownTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
	FSaveWorldScene Scene;
	if (!Scene.Start(*this, FVector2D(500.0, -3000.0)))
	{
		return false;
	}
	FLotGame& Game = Scene.Game();
	Scene.Grant(ELotItem::Cart);
	ADIAMovable* Cart = Scene.Movable(ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	Cart->Interact(Scene.Character);
	Scene.TickFor(0.3);
	TestTrue(TEXT("the cart is held"), Game.GetHeld() == ELotMovable::Cart);
	const FVector2D HeldAt = SaveTestWhere(Cart);
	TestTrue(TEXT("held in front of the player"), FVector2D::Distance(HeldAt, SaveTestWhere(Scene.Character)) > 50.0);
	Scene.Subsystem->SaveGame();

	// The player walks off and lets the world move on, then loads.
	Scene.StandAt(FVector2D(-2000.0, 4000.0));
	Scene.TickFor(0.3);
	TestTrue(TEXT("LoadGame"), Scene.Subsystem->LoadGame());
	TestTrue(TEXT("nothing is held after the load"), Game.GetHeld() == ELotMovable::Count);
	TestEqual(TEXT("one cart"), Scene.Find<ADIAMovable>().Num(), 1);
	ADIAMovable* Loaded = Scene.Movable(ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart actor"), Loaded))
	{
		return false;
	}
	TestFalse(TEXT("the actor is not held"), Loaded->IsHeld());
	TestTrue(TEXT("it stands where it was held when the game was saved"), SaveTestWhere(Loaded).Equals(HeldAt, MovedCm));
	TestEqual(TEXT("with its collision"), Loaded->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	TestTrue(TEXT("the core has it there"), Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos.Equals(HeldAt, MovedCm));
	Scene.TickFor(0.3);
	TestTrue(TEXT("a tick later it is still there"), SaveTestWhere(Loaded).Equals(HeldAt, MovedCm));
	TestTrue(TEXT("and still not held"), Game.GetHeld() == ELotMovable::Count);

	// It can be picked up again.
	Scene.StandAt(HeldAt + FVector2D(-120.0, 0.0));
	Loaded->Interact(Scene.Character);
	TestTrue(TEXT("grabbed again"), Game.GetHeld() == ELotMovable::Cart);
	return true;
}

DIA_TEST(FSaveWhileHoldingKeepsHoldingTest, "DoItAsphalt.Game.SaveSlot.SavingWhileHoldingKeepsHolding")

bool FSaveWhileHoldingKeepsHoldingTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
	FSaveWorldScene Scene;
	if (!Scene.Start(*this, FVector2D(500.0, -3000.0)))
	{
		return false;
	}
	FLotGame& Game = Scene.Game();
	Scene.Grant(ELotItem::Cart);
	ADIAMovable* Cart = Scene.Movable(ELotMovable::Cart);
	if (!TestNotNull(TEXT("the cart"), Cart))
	{
		return false;
	}
	Cart->Interact(Scene.Character);
	Scene.TickFor(0.3);
	const FLotMovable Before = Game.World().Movables[static_cast<int32>(ELotMovable::Cart)];

	Scene.Subsystem->SaveGame();
	TestTrue(TEXT("the game wrote the save"), Scene.Subsystem->HasSave());
	TestTrue(TEXT("the player still holds the cart"), Game.GetHeld() == ELotMovable::Cart);
	TestTrue(TEXT("the actor is still held"), Cart->IsHeld());
	TestEqual(TEXT("without collision"), Cart->GetCollision()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("the core's own record of the cart is untouched"), Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].bHeld && Game.World().Movables[static_cast<int32>(ELotMovable::Cart)].Pos.Equals(Before.Pos, MovedCm));

	// It goes on following the player, and E lets it go.
	Scene.StandAt(FVector2D(1500.0, -3000.0));
	Scene.TickFor(0.3);
	TestTrue(TEXT("it follows the player after the save"), FVector2D::Distance(SaveTestWhere(Cart), SaveTestWhere(Scene.Character)) < 200.0);
	Cart->Interact(Scene.Character);
	TestTrue(TEXT("and can be let go"), Game.GetHeld() == ELotMovable::Count);
	return true;
}

DIA_TEST(FSaveNewGameRemovesMachinesTest, "DoItAsphalt.Game.SaveSlot.NewGameRemovesMachines")

bool FSaveNewGameRemovesMachinesTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
	FSaveWorldScene Scene;
	if (!Scene.Start(*this, FVector2D::ZeroVector))
	{
		return false;
	}
	for (const ELotItem Item : {ELotItem::Suitcase, ELotItem::Cart, ELotItem::Station, ELotItem::Finisher, ELotItem::AutoPaver, ELotItem::Car})
	{
		Scene.Grant(Item);
	}
	Scene.TickFor(0.2);
	TestEqual(TEXT("four movables"), Scene.Find<ADIAMovable>().Num(), static_cast<int32>(ELotMovable::Count));
	TestEqual(TEXT("an auto paver"), Scene.Find<ADIAAutoPaver>().Num(), 1);
	TestEqual(TEXT("a car"), Scene.Find<ADIAPaverCar>().Num(), 1);

	Scene.Subsystem->NewGame();
	TestEqual(TEXT("a new game: no movable as the call returns"), Scene.Find<ADIAMovable>().Num(), 0);
	TestEqual(TEXT("no auto paver"), Scene.Find<ADIAAutoPaver>().Num(), 0);
	TestEqual(TEXT("no car"), Scene.Find<ADIAPaverCar>().Num(), 0);
	Scene.TickFor(0.2);
	TestEqual(TEXT("and none a tick later"), Scene.Find<ADIAMovable>().Num() + Scene.Find<ADIAAutoPaver>().Num() + Scene.Find<ADIAPaverCar>().Num(), 0);
	TestFalse(TEXT("the game owns none of them"), Scene.Game().Owns(ELotItem::Car) || Scene.Game().Owns(ELotItem::AutoPaver) || Scene.Game().OwnsMovable(ELotMovable::Cart));
	return true;
}

DIA_TEST(FSaveV1SlotIsRejectedTest, "DoItAsphalt.Game.SaveSlot.V1SlotIsRejected")

bool FSaveV1SlotIsRejectedTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
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
	const double Credits = Subsystem->GetGame().GetCredits();

	// The slot (and its backup) hold a well-formed save file whose blob is a v1 game.
	TestTrue(TEXT("the v1 blob was written to the slot"), UDIAWorldSubsystem::WriteSlot(SaveTestSlotName, V1Blob()));
	TestTrue(TEXT("and to the backup"), UDIAWorldSubsystem::WriteSlot(SaveTestBackupName(), V1Blob()));
	TestTrue(TEXT("both files exist"), FSaveTestSlot::Exists() && FSaveTestSlot::BackupExists());
	TestFalse(TEXT("a v1 slot is no save"), Subsystem->HasSave());
	TestFalse(TEXT("LoadGame refuses it"), Subsystem->LoadGame());
	TestEqual(TEXT("the game is untouched"), Subsystem->GetGame().GetCredits(), Credits);

	FDIAMenuModel Menu;
	Menu.Open(EDIAMenuMode::Main, Subsystem->HasSave());
	TestTrue(TEXT("the main menu has its buttons"), Menu.ShowsButtons());
	TestFalse(TEXT("but no Continue"), Menu.ShowsContinue());
	TestFalse(TEXT("and New Game asks nothing"), Menu.NeedsNewGameConfirmation());
	return true;
}

DIA_TEST(FSaveAutosaveOnPurchaseTest, "DoItAsphalt.Game.SaveSlot.AutosaveOnPurchase")

bool FSaveAutosaveOnPurchaseTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
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
	TestEqual(TEXT("the test's own slot is in use"), UDIAWorldSubsystem::SlotName(), SaveTestSlotName);
	FLotGame& Game = Subsystem->GetGame();
	Game.DebugGive(SaveTestRichCredits);
	Subsystem->SetAutosaveEnabled(true);

	// An item: the slot holds it, and the credits it cost.
	TestTrue(TEXT("buy the scoop"), Subsystem->BuyItem(ELotItem::Scoop));
	const double CreditsAfterItem = Game.GetCredits();
	Game.DebugGive(5.0);
	TestTrue(TEXT("load what the purchase saved"), Subsystem->LoadGame());
	TestTrue(TEXT("the slot has the item"), Game.Owns(ELotItem::Scoop));
	TestEqual(TEXT("and the credits after the purchase"), Game.GetCredits(), CreditsAfterItem);

	// A plant level.
	TestTrue(TEXT("buy a plant level"), Subsystem->BuyPlant());
	const double CreditsAfterPlant = Game.GetCredits();
	Game.DebugGive(5.0);
	TestTrue(TEXT("load what the purchase saved"), Subsystem->LoadGame());
	TestEqual(TEXT("the slot has the plant level"), Game.GetPlantLevel(), 1);
	TestEqual(TEXT("and the credits after the purchase"), Game.GetCredits(), CreditsAfterPlant);
	TestTrue(TEXT("and still the item"), Game.Owns(ELotItem::Scoop));
	return true;
}

DIA_TEST(FSaveSavedWhileDrivingTest, "DoItAsphalt.Game.SaveSlot.SavedWhileDriving")

bool FSaveSavedWhileDrivingTest::RunTest(const FString& Parameters)
{
	using namespace SaveWorldTestSupport;
	FSaveTestSlot Slot;
	FSaveWorldScene Scene;
	const FVector2D CarAt(-3000.0, 0.0);
	if (!Scene.Start(*this, CarAt + FVector2D(0.0, -180.0)))
	{
		return false;
	}
	FLotGame& Game = Scene.Game();
	Scene.Grant(ELotItem::Car);
	ADIAPaverCar* Car = Scene.Only<ADIAPaverCar>();
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	PlaceCarAt(Car, CarAt, 0.0);
	Scene.TickFor(0.2);
	Car->Interact(Scene.Character);
	TestTrue(TEXT("the player sits in the car"), Game.World().bPlayerInCar && Car->HasDriver());

	// A drive, then a save at the wheel.
	Car->SetThrottle(1.f);
	Scene.TickFor(1.0);
	Car->SetThrottle(0.f);
	Scene.TickFor(1.5);
	const FVector2D DrivenTo = SaveTestWhere(Car);
	TestTrue(TEXT("the car has moved"), FVector2D::Distance(DrivenTo, CarAt) > 50.0);
	Scene.Subsystem->SaveGame();
	TestTrue(TEXT("saving does not take the player out of the car"), Game.World().bPlayerInCar && Car->HasDriver());

	// The core's save keeps the in-car flag: a game loaded from it has the player at the wheel.
	TestTrue(TEXT("get out"), Car->ExitCar());
	TestFalse(TEXT("on foot"), Game.World().bPlayerInCar);
	TestTrue(TEXT("the car stands where it was driven to"), SaveTestWhere(Car).Equals(DrivenTo, MovedCm));
	Scene.StandAt(FVector2D(0.0, 0.0));
	TestTrue(TEXT("LoadGame"), Scene.Subsystem->LoadGame());
	TestTrue(TEXT("the core says the player drives"), Game.World().bPlayerInCar);
	TestTrue(TEXT("the car has the driver"), Car->HasDriver());
	TestTrue(TEXT("the controller drives the car"), Scene.Controller->GetPawn() == Car);
	TestTrue(TEXT("the character is out of sight"), Scene.Character->IsHidden());
	TestFalse(TEXT("and out of collision"), Scene.Character->GetActorEnableCollision());
	TestTrue(TEXT("the car is where it was saved"), SaveTestWhere(Car).Equals(DrivenTo, MovedCm));
	TestTrue(TEXT("the core has the player where the car is"), Game.World().PlayerPos.Equals(DrivenTo, MovedCm));
	TestTrue(TEXT("nothing is held"), Game.GetHeld() == ELotMovable::Count);
	Scene.TickFor(0.3);
	TestTrue(TEXT("a few ticks later the car has not moved"), SaveTestWhere(Car).Equals(DrivenTo, MovedCm));

	// A game saved on foot, loaded while driving: the player is on foot beside the car.
	TestTrue(TEXT("get out again"), Car->ExitCar());
	const FVector2D OnFoot = SaveTestWhere(Scene.Character);
	Scene.Subsystem->SaveGame();
	Car->Interact(Scene.Character);
	TestTrue(TEXT("in again"), Car->HasDriver());
	TestTrue(TEXT("LoadGame while driving"), Scene.Subsystem->LoadGame());
	TestFalse(TEXT("the core says on foot"), Game.World().bPlayerInCar);
	TestFalse(TEXT("nobody drives"), Car->HasDriver());
	TestTrue(TEXT("the controller has the character"), Scene.Controller->GetPawn() == Scene.Character);
	TestFalse(TEXT("visible"), Scene.Character->IsHidden());
	TestTrue(TEXT("standing where it was saved"), SaveTestWhere(Scene.Character).Equals(OnFoot, MovedCm));

	// A new game while driving puts the player on foot and takes the car away.
	Car->Interact(Scene.Character);
	TestTrue(TEXT("driving again"), Car->HasDriver());
	Scene.Subsystem->NewGame();
	TestEqual(TEXT("no car"), Scene.Find<ADIAPaverCar>().Num(), 0);
	TestTrue(TEXT("the controller has the character"), Scene.Controller->GetPawn() == Scene.Character);
	TestFalse(TEXT("visible"), Scene.Character->IsHidden());
	TestTrue(TEXT("collides"), Scene.Character->GetActorEnableCollision());
	TestTrue(TEXT("at the start"), SaveTestWhere(Scene.Character).Equals(LotLayout::PlayerStart, MovedCm));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
