// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAHUD.h"
#include "AsphaltBalance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Game/DIAWorldSubsystem.h"
#include "Misc/App.h"
#include "UI/DIAHudStyle.h"
#include "Player/DIAPlayerController.h"
#include "UI/SDIAHudOverlay.h"
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

void ADIAHUD::ToggleShop()
{
	SetShopOpen(!bShopOpen);
}

void ADIAHUD::SetShopOpen(bool bOpen)
{
	// The win screen owns the screen for good.
	if (bShopOpen == bOpen || (bOpen && bWinShown))
	{
		return;
	}
	bShopOpen = bOpen;
	if (ShopPanel.IsValid())
	{
		ShopPanel->SetVisibility(bShopOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	}
	// The cursor and the mouse go to the panel (and back to the camera); the controller also stops pouring.
	if (ADIAPlayerController* Controller = Cast<ADIAPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetShopInput(bShopOpen);
	}
}

void ADIAHUD::ShowWinScreen()
{
	if (bWinShown)
	{
		return;
	}
	SetShopOpen(false);
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

void ADIAHUD::NewGame()
{
	// Task 18 starts a new game in the same world; until then the level is loaded again, which gives a fresh game.
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)));
	}
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
		RoadHandle = Subsystem->OnRoadCompleted.AddUObject(this, &ADIAHUD::HandleRoadCompleted);
		SpecialHandle = Subsystem->OnSpecialBought.AddUObject(this, &ADIAHUD::HandleSpecialBought);
		ExitHandle = Subsystem->OnExitOpened.AddUObject(this, &ADIAHUD::HandleExitOpened);
		WonHandle = Subsystem->OnWon.AddUObject(this, &ADIAHUD::HandleWon);
		CreateOverlay(Subsystem);
	}
}

void ADIAHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnCellsCompleted.Remove(CellsHandle);
		Subsystem->OnRoadCompleted.Remove(RoadHandle);
		Subsystem->OnSpecialBought.Remove(SpecialHandle);
		Subsystem->OnExitOpened.Remove(ExitHandle);
		Subsystem->OnWon.Remove(WonHandle);
	}
	DestroyOverlay();
	Super::EndPlay(Reason);
}

void ADIAHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Toasts.Advance(DeltaSeconds);
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

	ShopPanel = SNew(SDIAShopPanel).Subsystem(Subsystem);
	ShopPanel->SetVisibility(bShopOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	GEngine->GameViewport->AddViewportWidgetContent(ShopPanel.ToSharedRef(), AsphaltHud::ShopZOrder);

	WinScreen = SNew(SDIAWinScreen)
		.OnNewGame(FSimpleDelegate::CreateUObject(this, &ADIAHUD::NewGame))
		.OnQuit(FSimpleDelegate::CreateUObject(this, &ADIAHUD::QuitGame));
	GEngine->GameViewport->AddViewportWidgetContent(WinScreen.ToSharedRef(), AsphaltHud::WinZOrder);
}

void ADIAHUD::DestroyOverlay()
{
	if (WinScreen.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(WinScreen.ToSharedRef());
		}
		WinScreen.Reset();
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
	ShowToast(DIAFormat::ShardsToast(Cells.Num() * AsphaltBalance::ShardsPerCell));
}

void ADIAHUD::HandleRoadCompleted(int32 Road)
{
	ShowToast(DIAFormat::RoadToast(Road));
}

void ADIAHUD::HandleSpecialBought(EAsphaltSpecial Special)
{
	ShowToast(DIAFormat::SpecialToast(Special));
}

void ADIAHUD::HandleExitOpened()
{
	ShowToast(DIAFormat::ExitToast());
}

void ADIAHUD::HandleWon()
{
	ShowWinScreen();
}
