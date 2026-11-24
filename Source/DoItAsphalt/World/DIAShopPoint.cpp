// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAShopPoint.h"
#include "Components/SceneComponent.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "LotLayout.h"
#include "Player/DIACharacter.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"

ADIAShopPoint::ADIAShopPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

FTransform ADIAShopPoint::MakeTransform(ELotShop Shop)
{
	const FVector2D Point = LotLayout::ShopPoint(Shop);
	return FTransform(FVector(Point.X, Point.Y, DIACharacter::CapsuleHalfHeightCm));
}

FText ADIAShopPoint::GetPrompt(const ADIACharacter* By) const
{
	return DIAFormat::ShopPrompt(Kind);
}

void ADIAShopPoint::Interact(ADIACharacter* By)
{
	const APlayerController* Controller = By ? Cast<APlayerController>(By->GetController()) : nullptr;
	if (ADIAHUD* Hud = Controller ? Cast<ADIAHUD>(Controller->GetHUD()) : nullptr)
	{
		Hud->OpenShop(Kind);
	}
}

void ADIAShopPoint::BeginPlay()
{
	Super::BeginPlay();
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->RegisterInteractable(this, this);
	}
}

void ADIAShopPoint::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->UnregisterInteractable(this);
	}
	Super::EndPlay(Reason);
}
