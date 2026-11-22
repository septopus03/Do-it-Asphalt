// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAHazardStripes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAHudWidgets.h"

TArray<float> AsphaltHud::HazardStripeStarts(float Width, float Height, float Band)
{
	TArray<float> Starts;
	if (Width <= 0.f || Height <= 0.f || Band <= 0.f)
	{
		return Starts;
	}
	constexpr int32 MaxStripes = 4096;
	for (int32 Index = 0; Index < MaxStripes; ++Index)
	{
		const float X = -Height + 2.f * Band * static_cast<float>(Index);
		if (X >= Width)
		{
			break;
		}
		Starts.Add(X);
	}
	return Starts;
}

void SDIAHazardStripes::Construct(const FArguments& InArgs)
{
	Band = InArgs._Band;
	Height = InArgs._Height;
	StripeColor = InArgs._StripeColor;
	GroundColor = InArgs._GroundColor;
	Opacity = InArgs._Opacity;
	SetClipping(EWidgetClipping::ClipToBounds);
}

FVector2D SDIAHazardStripes::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(0.0, Height);
}

int32 SDIAHazardStripes::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const float Alpha = Opacity.Get(1.f);
	const FLinearColor Ground = AsphaltHud::Faded(GroundColor.Get(AsphaltHud::SignBlack), Alpha);
	const FLinearColor Stripe = AsphaltHud::Faded(StripeColor.Get(AsphaltHud::SignYellow), Alpha);

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Ground);

	// Each stripe is a thick 45-degree line along the parallelogram's centre line. The line is extended past both ends of the band so
	// that its square end caps fall outside the widget, where the clip to bounds removes them: the parallelogram's horizontal
	// edges come from the clip.
	const TArray<float> Starts = AsphaltHud::HazardStripeStarts(Size.X, Size.Y, Band);
	const float Thickness = Band * UE_INV_SQRT_2;
	const float Overshoot = Band;
	const float Reach = Overshoot * UE_INV_SQRT_2;
	for (const float X : Starts)
	{
		const float CentreBottom = X + 0.5f * Band;
		TArray<FVector2f> Points;
		Points.Add(FVector2f(CentreBottom - Reach, Size.Y + Reach));
		Points.Add(FVector2f(CentreBottom + Size.Y + Reach, -Reach));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Stripe, true, Thickness);
	}
	return LayerId + 1;
}
