// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ADIAHUD;
class UDIAWorldSubsystem;

/** Layout and colours of the paver map; the cell numbers are Slate units. */
namespace DIAPaverMap
{
	/** The map is a square of this size (10 x 10 sectors, the shopping-centre quadrant left empty). */
	inline constexpr float MapSize = 640.f;
	inline constexpr int32 SectorsPerSide = 10;
	inline constexpr float Gap = 2.f;

	/** Column and row of a sector on the map (row 0 at the top = the far side of the lot, +Y). */
	DOITASPHALT_API FIntPoint SectorSlot(int32 Sector);
	/** The sector at a slot, or INDEX_NONE for an empty one (the mall quadrant) or a slot outside the map. */
	DOITASPHALT_API int32 SectorAtSlot(FIntPoint Slot);
	/** Where a world point (cm) lies on the map, in map units (0..MapSize). */
	DOITASPHALT_API FVector2D WorldToMap(FVector2D WorldCm);
}

/**
 * The paver map (G): a full-screen sheet with the lot as a grid of its 75 sectors. A sector shows how much of it is paved (grey →
 * yellow, green when done); a click sends the auto paver there (FLotGame::AssignAutoPaverSector). The paver, the player, the car and the
 * stations are marked on it. The mouse goes to the map while it is open (like the shop), G or Esc close it.
 */
class SDIAPaverMap : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDIAPaverMap) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ADIAHUD>, Hud)
		SLATE_ARGUMENT(TWeakObjectPtr<UDIAWorldSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Counts the paved cells of every sector again (the HUD calls it when the map opens, and it refreshes itself twice a second while shown). */
	void Refresh();

	/** The sector under a point of the widget (local units), or INDEX_NONE. */
	int32 SectorAtLocal(const FGeometry& Geometry, const FVector2D& LocalPos) const;
	/** Click on a sector: sends the paver. Public for tests. */
	void ChooseSector(int32 Sector);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

	/** Paved cells of a sector (0..400) as of the last refresh. */
	int32 GetSectorDone(int32 Sector) const { return Done.IsValidIndex(Sector) ? Done[Sector] : 0; }

private:
	/** Top left of the map square inside the widget. */
	FVector2D MapOrigin(const FGeometry& Geometry) const;

	TWeakObjectPtr<ADIAHUD> Hud;
	TWeakObjectPtr<UDIAWorldSubsystem> Subsystem;
	TArray<int32> Done;
	int32 Hovered = INDEX_NONE;
	float SinceRefresh = 0.f;
};
