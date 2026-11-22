// Copyright Do it Asphalt. All Rights Reserved.

#include "LotLayout.h"
#include "LotMap.h"

namespace LotLayout
{
	int32 FirstCell()
	{
		return AsphaltLot::CellIndex(99, 0);
	}

	FVector2D GatePos()
	{
		return AsphaltLot::EntryCentre(ExitEntry) + AsphaltLot::EntryOutward(ExitEntry) * StripCm;
	}

	FVector2D ExitTrigger()
	{
		return GatePos() + AsphaltLot::EntryOutward(ExitEntry) * ExitTriggerRadiusCm;
	}

	FVector2D BoardPos(int32 Entry)
	{
		const FVector2D Out = AsphaltLot::EntryOutward(Entry);
		const FVector2D Right(-Out.Y, Out.X);
		return AsphaltLot::EntryCentre(Entry) + Out * 150.0 + Right * 450.0;
	}

	FVector2D ItemSpawn(ELotItem Item)
	{
		switch (Item)
		{
		case ELotItem::Suitcase: return FVector2D(150.0, -9700.0);
		case ELotItem::Cart: return FVector2D(350.0, -9700.0);
		case ELotItem::Station: return FVector2D(-350.0, -9700.0);
		case ELotItem::AutoPaver: return FVector2D(-600.0, -9700.0);
		case ELotItem::Car: return FVector2D(-1000.0, -9500.0);
		default: return PlayerStart;
		}
	}
}
