// Copyright Do it Asphalt. All Rights Reserved.

#include "Player/DIAPlayerController.h"
#include "DoItAsphalt.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputSettings.h"
#include "InputActionValue.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Player/DIACharacter.h"
#include "UI/DIAHUD.h"

void ADIAPlayerController::InitInputSystem()
{
	// The engine defaults to the legacy input classes unless the project config (DefaultInput.ini) says otherwise;
	// the game needs the Enhanced ones, so it selects them itself instead of relying on a config file.
	UInputSettings::SetDefaultPlayerInputClass(UEnhancedPlayerInput::StaticClass());
	UInputSettings::SetDefaultInputComponentClass(UEnhancedInputComponent::StaticClass());
	Super::InitInputSystem();
}

void ADIAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	Input = FDIAInput::Create(this);

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(Input.Context, 0);
		}
	}

	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Enhanced)
	{
		UE_LOG(LogDoItAsphalt, Error, TEXT("The player controller has no Enhanced Input component; the character cannot be controlled"));
		return;
	}

	Enhanced->BindAction(Input.Move, ETriggerEvent::Triggered, this, &ADIAPlayerController::OnMove);
	Enhanced->BindAction(Input.Look, ETriggerEvent::Triggered, this, &ADIAPlayerController::OnLook);
	Enhanced->BindAction(Input.Pour, ETriggerEvent::Started, this, &ADIAPlayerController::OnPourStarted);
	Enhanced->BindAction(Input.Pour, ETriggerEvent::Completed, this, &ADIAPlayerController::OnPourCompleted);
	Enhanced->BindAction(Input.Pour, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnPourCompleted);
	Enhanced->BindAction(Input.Sprint, ETriggerEvent::Started, this, &ADIAPlayerController::OnSprintStarted);
	Enhanced->BindAction(Input.Sprint, ETriggerEvent::Completed, this, &ADIAPlayerController::OnSprintCompleted);
	Enhanced->BindAction(Input.Sprint, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnSprintCompleted);
	Enhanced->BindAction(Input.Interact, ETriggerEvent::Started, this, &ADIAPlayerController::OnInteract);
	Enhanced->BindAction(Input.Shop, ETriggerEvent::Started, this, &ADIAPlayerController::OnShop);
	Enhanced->BindAction(Input.Pause, ETriggerEvent::Started, this, &ADIAPlayerController::OnPause);
}

ADIACharacter* ADIAPlayerController::GetDIACharacter() const
{
	return Cast<ADIACharacter>(GetPawn());
}

void ADIAPlayerController::OnMove(const FInputActionValue& Value)
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		const FVector2D Axis = Value.Get<FVector2D>();
		Character->DoMove(Axis.X, Axis.Y);
	}
}

void ADIAPlayerController::OnLook(const FInputActionValue& Value)
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		const FVector2D Axis = Value.Get<FVector2D>();
		Character->DoLook(Axis.X, Axis.Y);
	}
}

void ADIAPlayerController::OnPourStarted()
{
	// A click in the shop is for the shop.
	if (bShopInput)
	{
		return;
	}
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetPouring(true);
	}
}

void ADIAPlayerController::OnPourCompleted()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetPouring(false);
	}
}

void ADIAPlayerController::OnSprintStarted()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetSprinting(true);
	}
}

void ADIAPlayerController::OnSprintCompleted()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetSprinting(false);
	}
}

void ADIAPlayerController::OnInteract()
{
	// Later tasks: kiosks, station, car.
}

void ADIAPlayerController::OnShop()
{
	if (ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD()))
	{
		Hud->ToggleShop();
	}
}

void ADIAPlayerController::OnPause()
{
	// Esc closes the shop; the pause menu comes in a later task.
	if (bShopInput)
	{
		if (ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD()))
		{
			Hud->SetShopOpen(false);
		}
	}
}

void ADIAPlayerController::SetShopInput(bool bOpen)
{
	if (bShopInput == bOpen)
	{
		return;
	}
	bShopInput = bOpen;

	// SetIgnoreLookInput counts: every open is matched by one close.
	SetIgnoreLookInput(bOpen);
	if (bOpen)
	{
		if (ADIACharacter* Character = GetDIACharacter())
		{
			Character->SetPouring(false);
		}
	}

	// Without a window (tests, servers) there is no cursor or input mode to change.
	if (!IsLocalPlayerController() || !FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	SetShowMouseCursor(bOpen);
	if (bOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}
