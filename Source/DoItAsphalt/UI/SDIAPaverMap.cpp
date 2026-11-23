// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAPaverMap.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudFont.h"
#include "UI/DIAHudStyle.h"

namespace
{
	constexpr float RefreshSeconds = 0.5f;
	constexpr float HeaderHeight = 70.f;
	constexpr float FooterHeight = 40.f;

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("GenericWhiteBox");
	}

	void Box(FSlateWindowElementList& List, int32 Layer, const FGeometry& Geometry, FVector2D Pos, FVector2D Size, const FLinearColor& Colour)
	{
		FSlateDrawElement::MakeBox(List, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), White(), ESlateDrawEffect::None, Colour);
	}

	void Label(FSlateWindowElementList& List, int32 Layer, const FGeometry& Geometry, FVector2D Pos, FVector2D Size, const FText& Text, int32 FontSize, const FLinearColor& Colour, EDIAFontWeight Weight = EDIAFontWeight::Bold)
	{
		// Centred in the rectangle by measuring the text.
		const FSlateFontInfo Font = AsphaltHud::Font(Weight, FontSize);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D Extent = Measure->Measure(Text, Font);
		const FVector2D At = Pos + (Size - Extent) * 0.5;
		FSlateDrawElement::MakeText(List, Layer, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(At)), Text, Font, ESlateDrawEffect::None, Colour);
	}

	/** Sector colour: grey when untouched, yellow with the progress, green when done. */
	FLinearColor SectorColour(int32 Done)
	{
		const float T = FMath::Clamp(Done / static_cast<float>(AsphaltLot::CellsPerSector), 0.f, 1.f);
		if (Done >= AsphaltLot::CellsPerSector)
		{
			return FLinearColor(FColor(0x3B, 0xA5, 0x5C, 0xFF));
		}
		return FMath::Lerp(FLinearColor(FColor(0x2C, 0x2D, 0x31, 0xFF)), AsphaltHud::SignYellow, T * 0.85f);
	}
}

FIntPoint DIAPaverMap::SectorSlot(int32 Sector)
{
	// Sectors are numbered like the cells: row by row from the bottom-left (Col 0, Row 0), skipping the mall quadrant; the map has the far side on top.
	const FVector2D Centre = AsphaltLot::SectorCentre(Sector);
	const int32 Col = FMath::FloorToInt((Centre.X + AsphaltLot::HalfExtentCm) / (AsphaltLot::SectorSide * AsphaltLot::CellCm));
	const int32 Row = FMath::FloorToInt((Centre.Y + AsphaltLot::HalfExtentCm) / (AsphaltLot::SectorSide * AsphaltLot::CellCm));
	return FIntPoint(Col, SectorsPerSide - 1 - Row);
}

int32 DIAPaverMap::SectorAtSlot(FIntPoint Slot)
{
	if (Slot.X < 0 || Slot.Y < 0 || Slot.X >= SectorsPerSide || Slot.Y >= SectorsPerSide)
	{
		return INDEX_NONE;
	}
	const double X = -AsphaltLot::HalfExtentCm + (Slot.X + 0.5) * AsphaltLot::SectorSide * AsphaltLot::CellCm;
	const double Y = -AsphaltLot::HalfExtentCm + (SectorsPerSide - 1 - Slot.Y + 0.5) * AsphaltLot::SectorSide * AsphaltLot::CellCm;
	return AsphaltLot::SectorAt(FVector2D(X, Y));
}

FVector2D DIAPaverMap::WorldToMap(FVector2D WorldCm)
{
	const double Side = 2.0 * AsphaltLot::HalfExtentCm;
	const double U = (WorldCm.X + AsphaltLot::HalfExtentCm) / Side;
	const double V = (WorldCm.Y + AsphaltLot::HalfExtentCm) / Side;
	return FVector2D(U * MapSize, (1.0 - V) * MapSize);
}

void SDIAPaverMap::Construct(const FArguments& InArgs)
{
	Hud = InArgs._Hud;
	Subsystem = InArgs._Subsystem;
	Done.Init(0, AsphaltLot::NumSectors);
	SetCanTick(true);
	Refresh();
}

void SDIAPaverMap::Refresh()
{
	SinceRefresh = 0.f;
	const UDIAWorldSubsystem* S = Subsystem.Get();
	if (!S)
	{
		return;
	}
	const FLotGrid& Grid = S->GetGame().GetGrid();
	for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
	{
		int32 Count = 0;
		for (int32 Step = 0; Step < AsphaltLot::CellsPerSector; ++Step)
		{
			Count += Grid.IsCellComplete(AsphaltLot::SectorCell(Sector, Step)) ? 1 : 0;
		}
		Done[Sector] = Count;
	}
}

void SDIAPaverMap::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (GetVisibility() != EVisibility::Collapsed)
	{
		SinceRefresh += InDeltaTime;
		if (SinceRefresh >= RefreshSeconds)
		{
			Refresh();
		}
	}
}

FVector2D SDIAPaverMap::MapOrigin(const FGeometry& Geometry) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	return FVector2D((Size.X - DIAPaverMap::MapSize) * 0.5, HeaderHeight + FMath::Max(0.0, (Size.Y - HeaderHeight - FooterHeight - DIAPaverMap::MapSize) * 0.5));
}

int32 SDIAPaverMap::SectorAtLocal(const FGeometry& Geometry, const FVector2D& LocalPos) const
{
	const FVector2D Rel = LocalPos - MapOrigin(Geometry);
	if (Rel.X < 0.0 || Rel.Y < 0.0 || Rel.X >= DIAPaverMap::MapSize || Rel.Y >= DIAPaverMap::MapSize)
	{
		return INDEX_NONE;
	}
	const float Cell = DIAPaverMap::MapSize / DIAPaverMap::SectorsPerSide;
	return DIAPaverMap::SectorAtSlot(FIntPoint(FMath::FloorToInt(Rel.X / Cell), FMath::FloorToInt(Rel.Y / Cell)));
}

void SDIAPaverMap::ChooseSector(int32 Sector)
{
	UDIAWorldSubsystem* S = Subsystem.Get();
	ADIAHUD* H = Hud.Get();
	if (!S || Sector == INDEX_NONE)
	{
		return;
	}
	FLotGame& Game = S->GetGame();
	if (!Game.Owns(ELotItem::AutoPaver))
	{
		return;
	}
	if (Game.GetAutoPaverSector() == Sector)
	{
		if (H)
		{
			H->ShowToast(DIAFormat::PaverAlreadyHereToast());
		}
		return;
	}
	if (Game.AssignAutoPaverSector(Sector))
	{
		if (H)
		{
			H->ShowToast(DIAFormat::PaverSentToast(Sector));
		}
		S->Dispatch(FLotEvents());
	}
}

FReply SDIAPaverMap::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ChooseSector(SectorAtLocal(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDIAPaverMap::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Hovered = SectorAtLocal(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	return FReply::Unhandled();
}

void SDIAPaverMap::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	Hovered = INDEX_NONE;
}

int32 SDIAPaverMap::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const UDIAWorldSubsystem* S = Subsystem.Get();
	if (!S)
	{
		return LayerId;
	}
	const FLotGame& Game = S->GetGame();
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	Box(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, AsphaltHud::ScreenBackdropColor);

	const FVector2D Origin = MapOrigin(AllottedGeometry);
	const float Cell = DIAPaverMap::MapSize / DIAPaverMap::SectorsPerSide;
	Label(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(0.0, 12.0), FVector2D(Size.X, 36.0), NSLOCTEXT("DIAPaverMap", "Title", "AUTO PAVER – CHOOSE A SECTOR"), 30, AsphaltHud::SignYellow, EDIAFontWeight::ExtraBold);

	// Sectors.
	for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
	{
		const FIntPoint Slot = DIAPaverMap::SectorSlot(Sector);
		const FVector2D Pos = Origin + FVector2D(Slot.X, Slot.Y) * Cell;
		const int32 DoneCells = GetSectorDone(Sector);
		FLinearColor Colour = SectorColour(DoneCells);
		if (Sector == Hovered)
		{
			Colour = FMath::Lerp(Colour, AsphaltHud::SignWhite, 0.35f);
		}
		Box(OutDrawElements, LayerId + 1, AllottedGeometry, Pos + FVector2D(DIAPaverMap::Gap * 0.5f), FVector2D(Cell - DIAPaverMap::Gap), Colour);
		const bool bLight = DoneCells > AsphaltLot::CellsPerSector / 2 || Sector == Hovered;
		Label(OutDrawElements, LayerId + 2, AllottedGeometry, Pos, FVector2D(Cell), DIAFormat::SectorNumber(Sector), 20, bLight ? AsphaltHud::SignBlack : AsphaltHud::SignWhite);
	}
	// The sector the paver is assigned to: an orange frame.
	if (Game.Owns(ELotItem::AutoPaver) && Game.GetAutoPaverSector() != INDEX_NONE)
	{
		const FIntPoint Slot = DIAPaverMap::SectorSlot(Game.GetAutoPaverSector());
		const FVector2D Pos = Origin + FVector2D(Slot.X, Slot.Y) * Cell;
		constexpr float Frame = 4.f;
		const FLinearColor Orange = AsphaltHud::SignOrange;
		Box(OutDrawElements, LayerId + 3, AllottedGeometry, Pos, FVector2D(Cell, Frame), Orange);
		Box(OutDrawElements, LayerId + 3, AllottedGeometry, Pos + FVector2D(0.0, Cell - Frame), FVector2D(Cell, Frame), Orange);
		Box(OutDrawElements, LayerId + 3, AllottedGeometry, Pos, FVector2D(Frame, Cell), Orange);
		Box(OutDrawElements, LayerId + 3, AllottedGeometry, Pos + FVector2D(Cell - Frame, 0.0), FVector2D(Frame, Cell), Orange);
	}

	// Markers: stations (white squares), the player (white dot), the paver (orange diamond-ish square), the car (yellow bar).
	const auto Marker = [&](FVector2D WorldCm, float Radius, const FLinearColor& Colour)
	{
		// Whatever is outside the lot (the strip with the fixed station, a paver on its way there) sits on the map's edge.
		const double Limit = AsphaltLot::HalfExtentCm;
		const FVector2D At = Origin + DIAPaverMap::WorldToMap(FVector2D(FMath::Clamp(WorldCm.X, -Limit, Limit), FMath::Clamp(WorldCm.Y, -Limit, Limit)));
		Box(OutDrawElements, LayerId + 4, AllottedGeometry, At - FVector2D(Radius + 2.f), FVector2D(2.f * Radius + 4.f), AsphaltHud::SignBlack);
		Box(OutDrawElements, LayerId + 5, AllottedGeometry, At - FVector2D(Radius), FVector2D(2.f * Radius), Colour);
	};
	for (int32 I = 0; I < Game.GetNumStations(); ++I)
	{
		Marker(Game.GetStationPos(I), 7.f, AsphaltHud::SignWhite);
	}
	if (Game.Owns(ELotItem::AutoPaver))
	{
		Marker(Game.GetAutoPaverPos(), 8.f, AsphaltHud::SignOrange);
	}
	if (Game.Owns(ELotItem::Car))
	{
		Marker(Game.World().CarPos, 7.f, AsphaltHud::SignYellow);
	}
	Marker(Game.World().PlayerPos, 5.f, FLinearColor(FColor(0x4D, 0xB8, 0xFF, 0xFF)));

	// Legend and the keys.
	Label(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(0.0, Size.Y - FooterHeight), FVector2D(Size.X, FooterHeight),
		NSLOCTEXT("DIAPaverMap", "Legend", "Click a sector  ·  white = stations  ·  orange = paver  ·  blue = you  ·  yellow = car  ·  G / Esc close"), 20, AsphaltHud::SignDimText);
	if (Game.Owns(ELotItem::AutoPaver))
	{
		Label(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(0.0, 46.0), FVector2D(Size.X, 28.0), DIAFormat::PaverLine(Game), 20, AsphaltHud::SignWhite);
	}
	return LayerId + 6;
}
