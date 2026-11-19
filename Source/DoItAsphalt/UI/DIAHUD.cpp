// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAHUD.h"
#include "AsphaltBalance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/DIAWorldSubsystem.h"
#include "Misc/App.h"
#include "UI/DIAHudStyle.h"
#include "UI/SDIAHudOverlay.h"

ADIAHUD::ADIAHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ADIAHUD::SetPrompt(const FText& Text)
{
	Prompt = Text;
}

void ADIAHUD::ShowToast(const FText& Text)
{
	Toasts.Add(Text);
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
}

void ADIAHUD::DestroyOverlay()
{
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
