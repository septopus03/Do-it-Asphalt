// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/DIAIcons.h"
#include "Brushes/SlateNoResource.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace
{
	constexpr int32 NumHands = static_cast<int32>(ELotHand::Count);

	struct FIconSlot
	{
		bool bTried = false;
		FSlateBrush Brush;
	};

	FIconSlot& SlotOf(ELotHand Hand)
	{
		static FIconSlot Slots[NumHands];
		return Slots[FMath::Clamp(static_cast<int32>(Hand), 0, NumHands - 1)];
	}
}

const TCHAR* DIAIcons::HandIconName(ELotHand Hand)
{
	switch (Hand)
	{
	case ELotHand::Bowl:
		return TEXT("ICON_Bowl");
	case ELotHand::Bucket:
		return TEXT("ICON_Bucket");
	case ELotHand::Canister:
		return TEXT("ICON_Canister");
	case ELotHand::Tub:
		return TEXT("ICON_Tub");
	default:
		return TEXT("");
	}
}

const FSlateBrush* DIAIcons::HandBrush(ELotHand Hand)
{
	FIconSlot& Slot = SlotOf(Hand);
	if (!Slot.bTried)
	{
		Slot.bTried = true;
		const FString Path = FString::Printf(TEXT("/Game/Icons/%s.%s"), HandIconName(Hand), HandIconName(Hand));
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn))
		{
			// Kept for the life of the process: the brush points at it and is never rebuilt.
			Texture->AddToRoot();
			Slot.Brush = FSlateBrush();
			Slot.Brush.SetResourceObject(Texture);
			Slot.Brush.ImageSize = FVector2f(HandIconSize, HandIconSize);
			Slot.Brush.DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			Slot.Brush = FSlateBrush();
			Slot.Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
			Slot.Brush.ImageSize = FVector2f(HandIconSize, HandIconSize);
		}
	}
	return &Slot.Brush;
}
