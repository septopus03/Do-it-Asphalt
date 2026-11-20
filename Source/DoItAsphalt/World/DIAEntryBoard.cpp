// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAEntryBoard.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Materials/MaterialInterface.h"
#include "UI/DIAFormat.h"

namespace
{
	double YawDeg(FVector2D Direction)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	}

	/** Number of lines of a text, and the length of the longest. */
	void MeasureText(const FString& Text, int32& OutLines, int32& OutLongest)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		OutLines = FMath::Max(1, Lines.Num());
		OutLongest = 1;
		for (const FString& Line : Lines)
		{
			OutLongest = FMath::Max(OutLongest, Line.Len());
		}
	}
}

FTransform DIABoard::BoardTransform(int32 Entry)
{
	// Both kinds of board face the car park, against the direction in which the entry leads out of it.
	const FVector2D Facing = -AsphaltLot::EntryOutward(Entry);
	const FRotator Turn(0.0, YawDeg(Facing), 0.0);
	if (HasKiosk(Entry))
	{
		const FVector2D Where = LotLayout::BoardPos(Entry);
		return FTransform(Turn, FVector(Where.X, Where.Y, 0.0));
	}
	// The sign of the mall is given in the model's axes for the facade towards -X; the facade towards -Y is its mirror about the diagonal.
	const FVector2D Sign = Entry == 2 ? FVector2D(DIAModelSizes::MallSignCm.X, DIAModelSizes::MallSignCm.Y) : FVector2D(DIAModelSizes::MallSignCm.Y, DIAModelSizes::MallSignCm.X);
	const FVector2D Where = LotLayout::MallCentre + Sign + Facing * SignTextStandOffCm;
	return FTransform(Turn, FVector(Where.X, Where.Y, DIAModelSizes::MallSignCm.Z));
}

ADIAEntryBoard::ADIAEntryBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	// The origin is the centre of the kiosk's footprint on the ground; +X is the side with the screen.
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

	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(Root);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	if (UMaterialInterface* TextMaterial = DIAAssets::TextMaterial())
	{
		Text->SetTextMaterial(TextMaterial);
	}
	Text->SetCastShadow(false);
}

FText ADIAEntryBoard::GetBoardText() const
{
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	return Subsystem ? DIAFormat::EntryBoard(Subsystem->GetGame(), Entry) : FText::GetEmpty();
}

void ADIAEntryBoard::BeginPlay()
{
	Super::BeginPlay();

	if (DIABoard::HasKiosk(Entry))
	{
		// The real model stands on the ground at the origin; the placeholder box is lifted by half its height.
		DIAAssets::SetModelOrFallback(BodyMesh, TEXT("SM_Kiosk"), DIAAssets::EFallback::Cube, DIAModelSizes::KioskCm,
			FVector(0.0, 0.0, DIAModelSizes::KioskCm.Z * 0.5), TEXT("DIA_Yellow"));
		Text->SetRelativeLocation(DIABoard::KioskTextCm);
	}
	else
	{
		// The text is all there is: the sign it is written on belongs to the mall.
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Text->SetRelativeLocation(FVector::ZeroVector);
	}

	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		CellsHandle = Subsystem->OnCellsCompleted.AddUObject(this, &ADIAEntryBoard::OnCells);
		EntryHandle = Subsystem->OnEntryCompleted.AddUObject(this, &ADIAEntryBoard::OnEntry);
		UnlockHandle = Subsystem->OnUnlocked.AddUObject(this, &ADIAEntryBoard::OnUnlock);
		ResetHandle = Subsystem->OnGameReset.AddUObject(this, &ADIAEntryBoard::Refresh);
	}
	Refresh();
}

void ADIAEntryBoard::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this))
	{
		Subsystem->OnCellsCompleted.Remove(CellsHandle);
		Subsystem->OnEntryCompleted.Remove(EntryHandle);
		Subsystem->OnUnlocked.Remove(UnlockHandle);
		Subsystem->OnGameReset.Remove(ResetHandle);
	}
	CellsHandle.Reset();
	EntryHandle.Reset();
	UnlockHandle.Reset();
	ResetHandle.Reset();
	Super::EndPlay(Reason);
}

void ADIAEntryBoard::Refresh()
{
	const FText Shown = GetBoardText();
	if (Text->Text.ToString() == Shown.ToString())
	{
		return;
	}
	const bool bKiosk = DIABoard::HasKiosk(Entry);
	const UDIAWorldSubsystem* Subsystem = UDIAWorldSubsystem::Get(this);
	const bool bUnlocked = Subsystem && Subsystem->GetGame().IsUnlocked(static_cast<ELotUnlock>(Entry));

	// The text component has a fixed glyph height: the board gets the size at which the longest line fills its width, or all the lines its height.
	int32 Lines = 1;
	int32 Longest = 1;
	MeasureText(Shown.ToString(), Lines, Longest);
	const FVector2D Area = bKiosk ? DIABoard::KioskTextAreaCm : DIAModelSizes::MallSignSizeCm * 0.9;
	Text->SetWorldSize(static_cast<float>(FMath::Min(Area.X / (Longest * DIABoard::CharAspect), Area.Y / (Lines * DIABoard::LineSpacing))));
	Text->SetTextRenderColor(bUnlocked ? DIABoard::UnlockedTextColor : bKiosk ? DIABoard::KioskTextColor : DIABoard::SignTextColor);
	Text->SetText(Shown);
}
