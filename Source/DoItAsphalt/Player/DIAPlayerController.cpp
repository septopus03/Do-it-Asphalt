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
#include "Game/DIAWorldSubsystem.h"
#include "Player/DIACharacter.h"
#include "UI/DIAHUD.h"
#include "World/DIAInteractable.h"

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
	Enhanced->BindAction(Input.Sprint, ETriggerEvent::Started, this, &ADIAPlayerController::OnSprintStarted);
	Enhanced->BindAction(Input.Sprint, ETriggerEvent::Completed, this, &ADIAPlayerController::OnSprintCompleted);
	Enhanced->BindAction(Input.Sprint, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnSprintCompleted);
	Enhanced->BindAction(Input.Use, ETriggerEvent::Started, this, &ADIAPlayerController::OnUseStarted);
	Enhanced->BindAction(Input.Use, ETriggerEvent::Completed, this, &ADIAPlayerController::OnUseCompleted);
	Enhanced->BindAction(Input.Use, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnUseCompleted);
	Enhanced->BindAction(Input.Flap, ETriggerEvent::Started, this, &ADIAPlayerController::OnFlapStarted);
	Enhanced->BindAction(Input.Flap, ETriggerEvent::Completed, this, &ADIAPlayerController::OnFlapCompleted);
	Enhanced->BindAction(Input.Flap, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnFlapCompleted);
	Enhanced->BindAction(Input.WorkView, ETriggerEvent::Started, this, &ADIAPlayerController::OnWorkViewStarted);
	Enhanced->BindAction(Input.WorkView, ETriggerEvent::Completed, this, &ADIAPlayerController::OnWorkViewCompleted);
	Enhanced->BindAction(Input.WorkView, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnWorkViewCompleted);
	Enhanced->BindAction(Input.NextHand, ETriggerEvent::Started, this, &ADIAPlayerController::OnNextHand);
	Enhanced->BindAction(Input.Hand1, ETriggerEvent::Started, this, &ADIAPlayerController::OnSelectHand, ELotHand::Bowl);
	Enhanced->BindAction(Input.Hand2, ETriggerEvent::Started, this, &ADIAPlayerController::OnSelectHand, ELotHand::Scoop);
	Enhanced->BindAction(Input.Hand3, ETriggerEvent::Started, this, &ADIAPlayerController::OnSelectHand, ELotHand::Bucket);
	Enhanced->BindAction(Input.Hand4, ETriggerEvent::Started, this, &ADIAPlayerController::OnSelectHand, ELotHand::Trowel);
	Enhanced->BindAction(Input.Interact, ETriggerEvent::Started, this, &ADIAPlayerController::OnInteract);
	Enhanced->BindAction(Input.Shop, ETriggerEvent::Started, this, &ADIAPlayerController::OnShop);
	Enhanced->BindAction(Input.CheckView, ETriggerEvent::Started, this, &ADIAPlayerController::OnCheckViewStarted);
	Enhanced->BindAction(Input.CheckView, ETriggerEvent::Completed, this, &ADIAPlayerController::OnCheckViewCompleted);
	Enhanced->BindAction(Input.CheckView, ETriggerEvent::Canceled, this, &ADIAPlayerController::OnCheckViewCompleted);
	Enhanced->BindAction(Input.Pause, ETriggerEvent::Started, this, &ADIAPlayerController::OnPause);
}

ADIACharacter* ADIAPlayerController::GetDIACharacter() const
{
	return Cast<ADIACharacter>(GetPawn());
}

void ADIAPlayerController::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->DoMove(Axis.X, Axis.Y);
	}
}

void ADIAPlayerController::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->DoLook(Axis.X, Axis.Y);
	}
}

void ADIAPlayerController::OnUseStarted()
{
	if (ADIACharacter* Character = GetDIACharacter(); Character && !IsUiInput())
	{
		Character->SetUsing(true);
	}
}

void ADIAPlayerController::OnUseCompleted()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetUsing(false);
	}
}

void ADIAPlayerController::OnFlapStarted()
{
	if (ADIACharacter* Character = GetDIACharacter(); Character && !IsUiInput())
	{
		Character->SetFlap(true);
	}
}

void ADIAPlayerController::OnFlapCompleted()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetFlap(false);
	}
}

void ADIAPlayerController::OnWorkViewStarted()
{
	if (ADIACharacter* Character = GetDIACharacter(); Character && !IsUiInput())
	{
		Character->SetWorkView(true);
	}
}

void ADIAPlayerController::OnWorkViewCompleted()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		Character->SetWorkView(false);
	}
}

void ADIAPlayerController::OnSelectHand(ELotHand Hand)
{
	if (ADIACharacter* Character = GetDIACharacter(); Character && !IsUiInput())
	{
		Character->SelectHand(Hand);
	}
}

void ADIAPlayerController::OnNextHand()
{
	if (ADIACharacter* Character = GetDIACharacter(); Character && !IsUiInput())
	{
		Character->NextHand();
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

IDIAInteractable* ADIAPlayerController::FindInteractable() const
{
	// A menu covers the screen and the mouse belongs to it: nothing can be used behind it.
	if (IsUiInput())
	{
		return nullptr;
	}
	const ADIACharacter* Character = GetDIACharacter();
	UWorld* World = GetWorld();
	UDIAWorldSubsystem* Subsystem = World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	return Character && Subsystem ? Subsystem->FindInteractable(Character) : nullptr;
}

void ADIAPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdatePrompt();
}

void ADIAPlayerController::UpdatePrompt()
{
	ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD());
	if (!Hud)
	{
		return;
	}
	const ADIACharacter* Character = GetDIACharacter();
	const IDIAInteractable* Interactable = FindInteractable();
	Hud->SetPrompt(Interactable ? Interactable->GetPrompt(Character) : FText::GetEmpty());
}

void ADIAPlayerController::OnInteract()
{
	// The search is repeated here instead of trusting the last frame's, so E always acts on what is in front of the player now.
	if (IDIAInteractable* Interactable = FindInteractable())
	{
		Interactable->Interact(GetDIACharacter());
	}
}

void ADIAPlayerController::OnShop()
{
	if (ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD()))
	{
		Hud->ToggleShop();
	}
}

void ADIAPlayerController::OnCheckViewStarted()
{
	if (ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD()); Hud && !IsUiInput())
	{
		Hud->SetCheckView(true);
	}
}

void ADIAPlayerController::OnCheckViewCompleted()
{
	if (ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD()))
	{
		Hud->SetCheckView(false);
	}
}

void ADIAPlayerController::OnPause()
{
	ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD());
	// The win screen has its own buttons; nothing else answers to Esc there.
	if (!Hud || bWinInput)
	{
		return;
	}
	// Esc closes the shop first; with nothing open it opens or closes the pause menu.
	if (bShopInput)
	{
		Hud->SetShopOpen(false);
		return;
	}
	Hud->TogglePause();
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
		ReleaseControls();
	}
	ApplyInputMode();
}

void ADIAPlayerController::SetMenuInput(bool bOpen)
{
	if (bMenuInput == bOpen)
	{
		return;
	}
	bMenuInput = bOpen;

	SetIgnoreLookInput(bOpen);
	if (bOpen)
	{
		ReleaseControls();
	}
	// The game waits behind the menu.
	SetPause(bOpen);
	ApplyInputMode();
}

void ADIAPlayerController::ApplyInputMode()
{
	// Without a window (tests, servers) there is no cursor or input mode to change.
	if (!IsLocalPlayerController() || !FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const bool bUi = IsUiInput();
	SetShowMouseCursor(bUi);
	if (bWinInput)
	{
		// UI only: not a key reaches the game any more.
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else if (bUi)
	{
		// The shop and the menu are also left with a key (Tab, Esc), which the game has to hear.
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

void ADIAPlayerController::ReleaseControls()
{
	if (ADIACharacter* Character = GetDIACharacter())
	{
		// Releasing a key while a UI is up does not reach the game, so nothing held may stay on.
		Character->SetSprinting(false);
		Character->SetUsing(false);
		Character->SetFlap(false);
		Character->SetWorkView(false);
	}
	if (ADIAHUD* Hud = Cast<ADIAHUD>(GetHUD()))
	{
		Hud->SetCheckView(false);
	}
}

void ADIAPlayerController::SetWinInput(bool bOn)
{
	if (bWinInput == bOn)
	{
		return;
	}
	bWinInput = bOn;
	SetIgnoreLookInput(bOn);
	SetIgnoreMoveInput(bOn);
	if (bOn)
	{
		ReleaseControls();
	}
	ApplyInputMode();
}
