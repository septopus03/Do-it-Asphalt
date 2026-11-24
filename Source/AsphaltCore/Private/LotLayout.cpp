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

	FVector2D ShopPos(ELotShop Shop)
	{
		switch (Shop)
		{
		case ELotShop::Tools: return FVector2D(900.0, -10500.0);
		case ELotShop::Robots: return FVector2D(1800.0, -10550.0);
		// The garage bay axis is the vehicle station's lane axis, so the car leaves the garage straight through the
		// station (the post faces are 200 cm from that axis; any offset would clip a post).
		case ELotShop::Garage: return FVector2D(-3000.0, VehicleStationPos.Y);
		default: return FVector2D::ZeroVector;
		}
	}

	double ShopYawDeg(ELotShop Shop)
	{
		return Shop == ELotShop::Garage ? 0.0 : 90.0;
	}

	FVector2D ShopLocalToWorld(ELotShop Shop, FVector2D LocalCm)
	{
		const double Yaw = FMath::DegreesToRadians(ShopYawDeg(Shop));
		const double C = FMath::Cos(Yaw);
		const double S = FMath::Sin(Yaw);
		return ShopPos(Shop) + FVector2D(LocalCm.X * C - LocalCm.Y * S, LocalCm.X * S + LocalCm.Y * C);
	}

	FVector2D ShopPoint(ELotShop Shop)
	{
		switch (Shop)
		{
		case ELotShop::Tools: return ShopLocalToWorld(Shop, FVector2D(222.0, -130.0));
		case ELotShop::Robots: return ShopLocalToWorld(Shop, FVector2D(185.0, 245.0));
		case ELotShop::Garage: return ShopLocalToWorld(Shop, FVector2D(430.0, 295.0));
		default: return ShopPos(Shop);
		}
	}

	FVector2D PaverDock(int32 Index)
	{
		static constexpr double DockY[NumPaverDocks] = {-220.0, -60.0, 100.0};
		if (Index < 0 || Index >= NumPaverDocks)
		{
			return FVector2D::ZeroVector;
		}
		return ShopLocalToWorld(ELotShop::Robots, FVector2D(-35.0, DockY[Index]));
	}

	FVector2D ItemSpawn(ELotItem Item)
	{
		switch (Item)
		{
		case ELotItem::Trolley: return FVector2D(150.0, -9700.0);
		case ELotItem::Cart: return FVector2D(350.0, -9700.0);
		case ELotItem::Station: return FVector2D(-350.0, -9700.0);
		case ELotItem::AutoPaver: return PaverDock(0);
		case ELotItem::AutoPaver2: return PaverDock(1);
		case ELotItem::AutoPaver3: return PaverDock(2);
		// The car leaves the garage; the garage sits west of the vehicle station.
		case ELotItem::Car: return ShopPos(ELotShop::Garage);
		default: return PlayerStart;
		}
	}
}
