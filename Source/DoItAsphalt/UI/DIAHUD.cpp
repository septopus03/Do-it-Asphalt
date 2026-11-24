// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAHUD.h"
#include "LotBalance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Game/DIAWorldSubsystem.h"
#include "Misc/App.h"
#include "UI/DIAHudStyle.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "World/DIAGroundActor.h"
#include "UI/SDIAHudOverlay.h"
#include "UI/SDIAMenu.h"
#include "UI/SDIAPaverMap.h"
#include "UI/SDIAShopPanel.h"
#include "UI/SDIAWinScreen.h"

ADIAHUD::ADIAHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ADIAHUD::SetPrompt(const FText& Text)
{
	// Called every frame by the player controller: unchanged text must cost nothing.
	if (Prompt.IdenticalTo(Text) || Prompt.ToString().Equals(Text.ToString()))
	{
		return;
	}
	Prompt = Text;
}

void ADIAHUD::ShowToast(const FText& Text)
{
	Toasts.Add(Text);
}

void ADIAHUD::OpenShop(ELotShop Shop)
{
	// The win screen and the menu own the screen.
	if (Shop >= ELotShop::Count || bWinShown || IsMenuOpen())
	{
		return;
	}
	const bool bChanges = !bShopOpen || LastShop != Shop;
	if (!bShopOpen)
	{
		SetPaverMapOpen(false);
	}
	bShopOpen = true;
	LastShop = Shop;
	if (ShopPanel.IsValid())
	{
		if (bChanges)
		{
			ShopPanel->Refresh();
			ShopPanel->SetShop(Shop);
		}
		ShopPanel->SetVisibility(EVisibility::SelfHitTestInvisible);
	}
	UpdatePanelInput();
}

void ADIAHUD::SetShopOpen(bool bOpen)
{
	if (bOpen)
	{
		OpenShop(LastShop);
		return;
	}
	if (!bShopOpen)
	{
		return;
	}
	bShopOpen = false;
	if (ShopPanel.IsValid())
	{
		ShopPanel->SetVisibility(EVisibility::Collapsed);
	}
	UpdatePanelInput();
}

void ADIAHUD::TogglePaverMap()
{
	SetPaverMapOpen(!bPaverMapOpen);
}

void ADIAHUD::SetPaverMapOpen(bool bOpen)
{
	if (bPaverMapOpen == bOpen || (bOpen && (bWinShown || IsMenuOpen())))
	{
		return;
	}
	if (bOpen)
	{
		SetShopOpen(false);
	}
	bPaverMapOpen = bOpen;
	if (PaverMap.IsValid())
	{
		if (bPaverMapOpen)
		{
			PaverMap->Refresh();
		}
		PaverMap->SetVisibility(bPaverMapOpen ? EVisibility::Visible : EVisibility::Collapsed);
	}
	UpdatePanelInput();
}

int32 ADIAHUD::GetSelectedPaver() const
{
	const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get();
	return Subsystem && SelectedPaver < Subsystem->GetGame().GetNumAutoPavers() ? SelectedPaver : 0;
}

void ADIAHUD::SelectPaver(int32 Paver)
{
	const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get();
	if (Subsystem && Paver >= 0 && Paver < Subsystem->GetGame().GetNumAutoPavers())
	{
		SelectedPaver = Paver;
	}
}

void ADIAHUD::UpdatePanelInput()
{
	// The cursor and the mouse go to the panel (and back to the camera); the controller also lets go of what is held.
	if (ADIAPlayerController* Controller = Cast<ADIAPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetShopInput(bShopOpen || bPaverMapOpen);
	}
}

void ADIAHUD::SetCheckView(bool bOn)
{
	if (bCheckView == bOn)
	{
		return;
	}
	bCheckView = bOn;
	if (!Ground.IsValid())
	{
		for (TActorIterator<ADIAGroundActor> It(GetWorld()); It; ++It)
		{
			Ground = *It;
		}
	}
	if (ADIAGroundActor* GroundActor = Ground.Get())
	{
		GroundActor->SetMissingHighlight(bOn ? 1.f : 0.f);
	}
}

void ADIAHUD::ShowWinScreen()
{
	if (bWinShown)
	{
		return;
	}
	SetShopOpen(false);
	SetPaverMapOpen(false);
	bWinShown = true;
	if (const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		if (WinScreen.IsValid())
		{
			WinScreen->Show(Subsystem->GetGame());
		}
	}
	if (ADIAPlayerController* Controller = Cast<ADIAPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetWinInput();
	}
}

void ADIAHUD::HideWinScreen()
{
	if (!bWinShown)
	{
		return;
	}
	bWinShown = false;
	if (WinScreen.IsValid())
	{
		WinScreen->Hide();
	}
	if (ADIAPlayerController* Controller = Cast<ADIAPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetWinInput(false);
	}
}

void ADIAHUD::NewGame()
{
	// A fresh game in the same world: the subsystem resets the core and every actor follows (this also takes the win screen away).
	if (UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->NewGame();
	}
	CloseMenu();
}

void ADIAHUD::DevStart()
{
	if (UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->DevStart();
	}
	CloseMenu();
}

void ADIAHUD::ContinueGame()
{
	UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}
	if (Subsystem->LoadGame())
	{
		CloseMenu();
	}
	else if (Menu.IsValid())
	{
		// The save turned out to be unusable: the menu now offers only a new game.
		Menu->Open(MenuMode, Subsystem->HasSave());
	}
}

void ADIAHUD::OpenMenu(EDIAMenuMode Mode)
{
	if (Mode == EDIAMenuMode::None || IsMenuOpen() || bWinShown)
	{
		return;
	}
	SetShopOpen(false);
	SetPaverMapOpen(false);
	MenuMode = Mode;
	if (Menu.IsValid())
	{
		const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get();
		Menu->Open(Mode, Mode == EDIAMenuMode::Main && Subsystem && Subsystem->HasSave());
	}
	if (ADIAPlayerController* Controller = Cast<ADIAPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetMenuInput(true);
	}
}

void ADIAHUD::DismissMenu()
{
	if (!IsMenuOpen())
	{
		return;
	}
	MenuMode = EDIAMenuMode::None;
	if (Menu.IsValid())
	{
		Menu->Close();
	}
	if (ADIAPlayerController* Controller = Cast<ADIAPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetMenuInput(false);
	}
}

void ADIAHUD::CloseMenu()
{
	const bool bWasMain = MenuMode == EDIAMenuMode::Main;
	DismissMenu();
	if (bWasMain)
	{
		if (UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
		{
			Subsystem->SetAutosaveEnabled(true);
		}
	}
}

void ADIAHUD::TogglePause()
{
	// In the settings Esc goes back to the buttons first (in either menu).
	if (Menu.IsValid() && Menu->BackFromSettings())
	{
		return;
	}
	if (MenuMode == EDIAMenuMode::Pause)
	{
		CloseMenu();
	}
	else if (MenuMode == EDIAMenuMode::None)
	{
		OpenMenu(EDIAMenuMode::Pause);
	}
}

#if !UE_BUILD_SHIPPING
bool ADIAHUD::DebugShowScreen(const FString& Name)
{
	if (Name.Equals(TEXT("Win"), ESearchCase::IgnoreCase))
	{
		ShowWinScreen();
		return true;
	}
	if (Name.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
	{
		SetPaverMapOpen(true);
		return true;
	}
	const bool bSettings = Name.Equals(TEXT("Settings"), ESearchCase::IgnoreCase);
	const bool bQuestion = Name.Equals(TEXT("Question"), ESearchCase::IgnoreCase);
	if (!bSettings && !bQuestion && !Name.Equals(TEXT("Pause"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	OpenMenu(EDIAMenuMode::Pause);
	if (Menu.IsValid())
	{
		Menu->DebugSetState(bSettings, bQuestion);
	}
	return true;
}
#endif

void ADIAHUD::SkipMenu()
{
	CloseMenu();
}

void ADIAHUD::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

void ADIAHUD::BeginPlay()
{
	Super::BeginPlay();

	UDIAWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	if (Subsystem)
	{
		BoundSubsystem = Subsystem;
		CellsHandle = Subsystem->OnCellsCompleted.AddUObject(this, &ADIAHUD::HandleCellsCompleted);
		EntryHandle = Subsystem->OnEntryCompleted.AddUObject(this, &ADIAHUD::HandleEntryCompleted);
		UnlockHandle = Subsystem->OnUnlocked.AddUObject(this, &ADIAHUD::HandleUnlocked);
		ItemHandle = Subsystem->OnItemBought.AddUObject(this, &ADIAHUD::HandleItemBought);
		PaverUpgradeHandle = Subsystem->OnPaverUpgradeBought.AddUObject(this, &ADIAHUD::HandlePaverUpgradeBought);
		ExitHandle = Subsystem->OnExitOpened.AddUObject(this, &ADIAHUD::HandleExitOpened);
		WonHandle = Subsystem->OnWon.AddUObject(this, &ADIAHUD::HandleWon);
		ResetHandle = Subsystem->OnGameReset.AddUObject(this, &ADIAHUD::HandleGameReset);
		CreateOverlay(Subsystem);
	}
}

void ADIAHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnCellsCompleted.Remove(CellsHandle);
		Subsystem->OnEntryCompleted.Remove(EntryHandle);
		Subsystem->OnUnlocked.Remove(UnlockHandle);
		Subsystem->OnItemBought.Remove(ItemHandle);
		Subsystem->OnPaverUpgradeBought.Remove(PaverUpgradeHandle);
		Subsystem->OnExitOpened.Remove(ExitHandle);
		Subsystem->OnWon.Remove(WonHandle);
		Subsystem->OnGameReset.Remove(ResetHandle);
	}
	// The controller outlives the HUD's panels: give back what they took (the cursor, the input mode, the pause).
	// A win screen that is still up releases its input too.
	SetShopOpen(false);
	SetPaverMapOpen(false);
	SetCheckView(false);
	HideWinScreen();
	DismissMenu();
	DestroyOverlay();
	Super::EndPlay(Reason);
}

void ADIAHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Toasts.Advance(DeltaSeconds);
	if (const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		// Where the player is now: the character's own location. The core's PlayerPos is only written by the character's tick,
		// which may run after this one in the same frame, so it is not read here.
		const UWorld* World = GetWorld();
		TActorIterator<ADIACharacter> Character(World);
		const bool bHeld = bCheckView && Character;
		const FVector2D From = Character ? FVector2D(Character->GetActorLocation()) : FVector2D::ZeroVector;
		CheckModel.Update(Subsystem->GetGame(), bHeld, DeltaSeconds, From);
	}
}

void ADIAHUD::CreateOverlay(UDIAWorldSubsystem* Subsystem)
{
	// Without a renderer (tests, servers) there is no viewport to show anything in.
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	Overlay = SNew(SDIAHudOverlay).Hud(this).Subsystem(Subsystem);
	GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), AsphaltHud::ViewportZOrder);

	ShopPanel = SNew(SDIAShopPanel).Hud(this).Subsystem(Subsystem);
	ShopPanel->SetShop(LastShop);
	ShopPanel->SetVisibility(bShopOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	GEngine->GameViewport->AddViewportWidgetContent(ShopPanel.ToSharedRef(), AsphaltHud::ShopZOrder);

	PaverMap = SNew(SDIAPaverMap).Hud(this).Subsystem(Subsystem);
	PaverMap->SetVisibility(bPaverMapOpen ? EVisibility::Visible : EVisibility::Collapsed);
	GEngine->GameViewport->AddViewportWidgetContent(PaverMap.ToSharedRef(), AsphaltHud::ShopZOrder);

	WinScreen = SNew(SDIAWinScreen)
		.OnNewGame(FSimpleDelegate::CreateUObject(this, &ADIAHUD::NewGame))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &ADIAHUD::QuitGame));
	GEngine->GameViewport->AddViewportWidgetContent(WinScreen.ToSharedRef(), AsphaltHud::WinZOrder);

	Menu = SNew(SDIAMenu)
		.OnContinue(FSimpleDelegate::CreateUObject(this, &ADIAHUD::ContinueGame))
		.OnResume(FSimpleDelegate::CreateUObject(this, &ADIAHUD::CloseMenu))
		.OnNewGame(FSimpleDelegate::CreateUObject(this, &ADIAHUD::NewGame))
		.OnDevStart(FSimpleDelegate::CreateUObject(this, &ADIAHUD::DevStart))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &ADIAHUD::QuitGame));
	GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), AsphaltHud::MenuZOrder);

	// The game starts in the main menu, paused (without a window there is no menu and nothing waits).
	OpenMenu(EDIAMenuMode::Main);
}

void ADIAHUD::DestroyOverlay()
{
	if (Menu.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
		}
		Menu.Reset();
	}
	if (WinScreen.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(WinScreen.ToSharedRef());
		}
		WinScreen.Reset();
	}
	if (PaverMap.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(PaverMap.ToSharedRef());
		}
		PaverMap.Reset();
	}
	if (ShopPanel.IsValid())
	{
		ShopPanel->Unbind();
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(ShopPanel.ToSharedRef());
		}
		ShopPanel.Reset();
	}
	if (Overlay.IsValid())
	{
		Overlay->Unbind();
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		}
		Overlay.Reset();
	}
}

void ADIAHUD::HandleCellsCompleted(const TArray<int32>& Cells)
{
	Toasts.AddCredits(Cells.Num() * LotBalance::CreditsPerCell);
}

void ADIAHUD::HandleEntryCompleted(int32 Entry)
{
	ShowToast(DIAFormat::EntryToast(Entry));
}

void ADIAHUD::HandleUnlocked(ELotUnlock Unlock)
{
	ShowToast(DIAFormat::UnlockToast(Unlock));
}

void ADIAHUD::HandleItemBought(ELotItem Item)
{
	ShowToast(DIAFormat::ItemBoughtToast(Item));
}

void ADIAHUD::HandlePaverUpgradeBought(int32 Paver, ELotPaverUpgrade Upgrade)
{
	if (const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		ShowToast(DIAFormat::PaverUpgradeToast(Subsystem->GetGame(), Paver, Upgrade));
	}
}

void ADIAHUD::HandleExitOpened()
{
	ShowToast(DIAFormat::ExitToast());
}

void ADIAHUD::HandleWon()
{
	ShowWinScreen();
}

void ADIAHUD::HandleGameReset()
{
	// The game was replaced under the panel (the buildings' goods and the player's place are new): it closes.
	SetShopOpen(false);
	SelectedPaver = 0;
	// A won game that is loaded shows its win screen again; any other game takes it away.
	const UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get();
	if (Subsystem && Subsystem->GetGame().HasWon())
	{
		ShowWinScreen();
	}
	else
	{
		HideWinScreen();
	}
}
