// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAKioskActor.h"
#include "AsphaltBalance.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "UI/DIAFormat.h"

ADIAKioskActor::ADIAKioskActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// The origin is the centre of the footprint on the ground (like SM_Kiosk); +X is the side with the screen.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Root);
	Collision->SetBoxExtent(DIAModelSizes::KioskCm * 0.5);
	Collision->SetRelativeLocation(FVector(0.0, 0.0, DIAModelSizes::KioskCm.Z * 0.5));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The camera boom is never pushed in by a kiosk.
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LockMesh"));
	LockMesh->SetupAttachment(Root);
	LockMesh->SetRelativeLocation(DIAKiosk::LockCm);
	LockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BoardText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoardText"));
	BoardText->SetupAttachment(Root);
	BoardText->SetRelativeLocation(DIAKiosk::BoardTextCm);
	BoardText->SetHorizontalAlignment(EHTA_Center);
	BoardText->SetVerticalAlignment(EVRTA_TextCenter);
	BoardText->SetTextRenderColor(FColor(230, 90, 0));
	BoardText->SetCastShadow(false);
}

UDIAWorldSubsystem* ADIAKioskActor::GetSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
}

int32 ADIAKioskActor::GetRoad() const
{
	return AsphaltBalance::SpecialRoad[FMath::Clamp(static_cast<int32>(Special), 0, 4)];
}

bool ADIAKioskActor::IsLocked() const
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	return Subsystem && !Subsystem->GetGame().IsRoadComplete(GetRoad());
}

void ADIAKioskActor::SetSpecial(EAsphaltSpecial NewSpecial)
{
	Special = NewSpecial;
	if (bBegun)
	{
		ApplySpecialName();
		// Force the next Refresh to rebuild the prompt.
		ShownProgress = INDEX_NONE;
		Refresh();
	}
}

void ADIAKioskActor::ApplySpecialName()
{
	const FText Name = DIAFormat::SpecialName(Special);
	// The text component has a fixed height; the longer the name, the smaller it is written, so that it fits the board.
	const double Chars = FMath::Max(1, Name.ToString().Len());
	const double Size = FMath::Clamp(DIAKiosk::BoardTextWidthCm / (Chars * 0.6), 6.0, DIAKiosk::BoardTextMaxHeightCm);
	BoardText->SetWorldSize(static_cast<float>(Size));
	BoardText->SetText(Name);
}

void ADIAKioskActor::BeginPlay()
{
	Super::BeginPlay();

	// The real model stands on the ground at the origin; the placeholder box is lifted by half its height.
	DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_Kiosk"), DIAAssets::EFallback::Cube, DIAModelSizes::KioskCm,
		FVector(0.0, 0.0, DIAModelSizes::KioskCm.Z * 0.5), TEXT("DIA_Yellow"));
	// The lock's origin is its centre, already at the placement; the placeholder only needs its size.
	DIAAssets::SetModelOrFallback(LockMesh, TEXT("SM_Lock"), DIAAssets::EFallback::Cube, DIAModelSizes::LockCm, FVector::ZeroVector, TEXT("DIA_Red"));

	bBegun = true;
	ApplySpecialName();
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->RegisterInteractable(this, this);
		// Fires every tick; Refresh returns at once unless something the kiosk shows changed.
		StateHandle = Subsystem->OnStateChanged.AddUObject(this, &ADIAKioskActor::Refresh);
	}
	Refresh();
}

void ADIAKioskActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		if (StateHandle.IsValid())
		{
			Subsystem->OnStateChanged.Remove(StateHandle);
		}
		Subsystem->UnregisterInteractable(this);
	}
	StateHandle.Reset();
	bBegun = false;
	Super::EndPlay(Reason);
}

void ADIAKioskActor::Refresh() const
{
	const UDIAWorldSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const FAsphaltGame& Game = Subsystem->GetGame();
	const int32 Progress = Game.GetRoadProgress(GetRoad());
	const bool bOwned = Game.HasSpecial(Special);
	const bool bAffordable = Game.CanBuySpecial(Special);
	if (Progress == ShownProgress && bOwned == bShownOwned && bAffordable == bShownAffordable)
	{
		return;
	}
	ShownProgress = Progress;
	bShownOwned = bOwned;
	bShownAffordable = bAffordable;

	CachedPrompt = DIAFormat::KioskPrompt(Game, Special);
	LockMesh->SetVisibility(!Game.IsRoadComplete(GetRoad()));
}

FText ADIAKioskActor::GetPrompt(const ADIACharacter* By) const
{
	// Cheap when nothing changed, and never stale even if the state changed after the last tick.
	Refresh();
	return CachedPrompt;
}

void ADIAKioskActor::Interact(ADIACharacter* By)
{
	// Buying does the checks itself (road complete, not owned, enough money); anything else is a no-op, and the prompt
	// already says why.
	if (UDIAWorldSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->BuySpecial(Special);
	}
}
