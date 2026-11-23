// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/SLeafWidget.h"

namespace AsphaltHud
{
	/**
	 * The X of the bottom left corner of every stripe in a band of the given size. A stripe is a parallelogram leaning 45 degrees
	 * (its top edge is shifted by +Height), Band wide, with a gap of Band between stripes. The first starts at -Height and the
	 * list ends before Width; at most 4096 stripes. Empty when Width, Height or Band is not positive.
	 */
	DOITASPHALT_API TArray<float> HazardStripeStarts(float Width, float Height, float Band);

	/** The screen-pixel thickness of a stripe line whose horizontal width is Band local units at the given layout scale; 0 for a non-positive scale. */
	DOITASPHALT_API float HazardStripeThickness(float Band, float Scale);
}

/** A band of diagonal stripes on a ground colour, clipped to its own rectangle: the hazard-sign edge of the plates. */
class DOITASPHALT_API SDIAHazardStripes : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDIAHazardStripes)
		: _Band(12.f)
		, _Height(12.f)
		, _StripeColor(AsphaltHud::SignYellow)
		, _GroundColor(AsphaltHud::SignBlack)
		, _Opacity(1.f)
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}
		/** The horizontal width of one stripe (and of the gap between two). */
		SLATE_ARGUMENT(float, Band)
		/** The height of the widget. */
		SLATE_ARGUMENT(float, Height)
		SLATE_ATTRIBUTE(FLinearColor, StripeColor)
		SLATE_ATTRIBUTE(FLinearColor, GroundColor)
		SLATE_ATTRIBUTE(float, Opacity)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	float Band = 12.f;
	float Height = 12.f;
	TAttribute<FLinearColor> StripeColor;
	TAttribute<FLinearColor> GroundColor;
	TAttribute<float> Opacity;
};
