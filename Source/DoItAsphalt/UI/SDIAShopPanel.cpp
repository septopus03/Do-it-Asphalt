// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAShopPanel.h"
#include "Game/DIAWorldSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAButtonStyle.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudFont.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "UI/DIAShopLook.h"
#include "UI/SDIAHazardStripes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** The padding of a plate's content for a CSS padding of the mockup: the mockup measures it from inside the border, so the ring line is part of it. */
	FMargin Inset(float Left, float Top, float Right, float Bottom)
	{
		const float Ring = AsphaltHud::HudRingWidth;
		return FMargin(Left - Ring, Top - Ring, Right - Ring, Bottom - Ring);
	}

	/** The look of each state, built once (building one looks its label up). */
	const FDIAShopLook& LookOf(EDIAShopState State)
	{
		static const FDIAShopLook Looks[] = {
			AsphaltHud::ShopLook(EDIAShopState::Buy),
			AsphaltHud::ShopLook(EDIAShopState::NoCredits),
			AsphaltHud::ShopLook(EDIAShopState::Locked),
			AsphaltHud::ShopLook(EDIAShopState::Owned)};
		return Looks[FMath::Clamp(static_cast<int32>(State), 0, 3)];
	}

	/** The tick in front of OWNED, drawn as two lines (the font has no such glyph). */
	class SDIAShopTick : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDIAShopTick) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs) {}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			// The mockup draws the tick in a 24-unit square; the points and the line width follow the box.
			const float Unit = AsphaltHud::ShopTickSize / 24.f;
			TArray<FVector2f> Points;
			Points.Add(FVector2f(3.f, 13.f) * Unit);
			Points.Add(FVector2f(9.f, 19.f) * Unit);
			Points.Add(FVector2f(21.f, 5.f) * Unit);
			// MakeLines takes the thickness in screen pixels, while the points are local units.
			const float Thickness = AsphaltHud::ShopTickLine * Unit * AllottedGeometry.GetAccumulatedLayoutTransform().GetScale();
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, AsphaltHud::SignWhite, true, Thickness);
			return LayerId;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			return FVector2D(AsphaltHud::ShopTickSize, AsphaltHud::ShopTickSize);
		}
	};
}

void SDIAShopPanel::Construct(const FArguments& InArgs)
{
	Hud = InArgs._Hud;
	Subsystem = InArgs._Subsystem;

	ButtonStyle = AsphaltHud::MakeSignButtonStyle();

	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
		StateHandle = S->OnStateChanged.AddSP(this, &SDIAShopPanel::HandleStateChanged);
	}

	// The grid pads every slot by half the gap; the panel's padding takes that off so that the cards sit where the mockup puts them.
	const float HalfGap = AsphaltHud::ShopCardGap / 2.f;
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(HalfGap))
		.MinDesiredSlotHeight(AsphaltHud::ShopCardHeight);
	for (int32 Row = 0; Row < FDIAShopModel::NumRows; ++Row)
	{
		Grid->AddSlot(Row % AsphaltHud::ShopColumns, Row / AsphaltHud::ShopColumns)
		[
			BuildCard(Row)
		];
	}

	TWeakObjectPtr<UDIAWorldSubsystem> WeakSubsystem = Subsystem;
	// The root takes no mouse, so that the game is clickable around the panel; the panel itself is hit-testable.
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(AsphaltHud::ShopPanelWidth)
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.BorderBackgroundColor(AsphaltHud::SignBlack)
				.Padding(0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SDIAHazardStripes)
						.Band(AsphaltHud::ShopStripeBand)
						.Height(AsphaltHud::ShopStripeHeight)
					]
					+ SVerticalBox::Slot().AutoHeight()
					.Padding(AsphaltHud::ShopContentSide - HalfGap, AsphaltHud::ShopContentTop, AsphaltHud::ShopContentSide - HalfGap, AsphaltHud::ShopContentBottom)
					[
						SNew(SVerticalBox)
						// The header: the title with what the shop is for, and the credits to spend.
						+ SVerticalBox::Slot().AutoHeight().Padding(HalfGap, 0.f, HalfGap, AsphaltHud::ShopSectionGap - HalfGap)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									AsphaltHud::MakeText(NSLOCTEXT("DIAHud", "ShopTitle", "SHOP"), EDIAFontWeight::ExtraBold, AsphaltHud::ShopTitleFont, FSlateColor(AsphaltHud::SignYellow))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									AsphaltHud::MakeText(NSLOCTEXT("DIAHud", "ShopSubtitle", "Tools and machines for the car park"), EDIAFontWeight::Bold, AsphaltHud::ShopSubtitleFont, FSlateColor(AsphaltHud::SignWhite))
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[
								SNew(SSpacer)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								AsphaltHud::MakePlate(AsphaltHud::SignYellow, AsphaltHud::SignBlack, Inset(24.f, 8.f, 24.f, 10.f),
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										AsphaltHud::MakeText(TAttribute<FText>::CreateLambda([WeakSubsystem]()
										{
											const UDIAWorldSubsystem* S = WeakSubsystem.Get();
											return S ? DIAFormat::Credits(S->GetGame().GetCredits()) : FText::GetEmpty();
										}), EDIAFontWeight::ExtraBold, AsphaltHud::ShopCreditsFont, FSlateColor(AsphaltHud::SignBlack))
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12.f, 0.f, 0.f, 6.f)
									[
										AsphaltHud::MakeText(NSLOCTEXT("DIAHud", "ShopCredits", "credits"), EDIAFontWeight::Bold, AsphaltHud::ShopCreditsCaptionFont, FSlateColor(AsphaltHud::SignBlack))
									],
									AsphaltHud::HudRingWidth)
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							Grid
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, AsphaltHud::ShopSectionGap - HalfGap, 0.f, 0.f)
						[
							AsphaltHud::MakeText(NSLOCTEXT("DIAHud", "ShopHint", "Tab / Esc - close"), EDIAFontWeight::Bold, AsphaltHud::ShopHintFont, FSlateColor(AsphaltHud::SignWhite))
						]
					]
				]
			]
		]
	];
}

void SDIAShopPanel::Unbind()
{
	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		S->OnStateChanged.Remove(StateHandle);
	}
	StateHandle.Reset();
}

void SDIAShopPanel::Refresh()
{
	Pressed = FDIAShopTarget();
	if (const UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
	}
}

void SDIAShopPanel::HandleStateChanged()
{
	// Fires every tick; the model builds new rows only when a shown value changed.
	if (const UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
	}
}

void SDIAShopPanel::HandlePressed(int32 Row)
{
	// What is under the cursor when the button goes down is what the click buys, whatever the rows do until it is released.
	if (const UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Pressed = Model.TargetOf(Row, S->GetGame());
	}
}

FReply SDIAShopPanel::HandleBuy(int32 Row)
{
	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		// Only what was pressed is bought; a release without a press that was seen buys nothing.
		const FDIAShopTarget Target = Pressed;
		Pressed = FDIAShopTarget();
		// Buying goes through the subsystem, which broadcasts it: an item gives its toast, the model and the character refresh.
		if (FDIAShopModel::Buy(Target, *S) && Target.bPlant)
		{
			if (ADIAHUD* H = Hud.Get())
			{
				H->ShowToast(DIAFormat::PlantToast(S->GetGame().GetPlantLevel()));
			}
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAShopPanel::BuildCard(int32 Row)
{
	const int32 Index = Row;
	auto StateOf = [this, Index]() { return Model.GetRow(Index).State; };
	auto Look = [StateOf]() -> const FDIAShopLook& { return LookOf(StateOf()); };

	const TAttribute<FLinearColor> Plate = TAttribute<FLinearColor>::CreateLambda([Look]() { return Look().Plate; });
	const TAttribute<FLinearColor> Ring = TAttribute<FLinearColor>::CreateLambda([Look]() { return Look().Ring; });

	// A name that is wider than the card shrinks to fit on one line: two lines of name would push the value, the reason and the
	// price into each other on a card that is only 172 high.
	const TSharedRef<STextBlock> Name = AsphaltHud::MakeText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Name; }),
		EDIAFontWeight::ExtraBold, AsphaltHud::ShopNameFont,
		TAttribute<FSlateColor>::CreateLambda([Look]() { return FSlateColor(Look().Text); }));

	const TSharedRef<SWidget> Content =
		SNew(SHorizontalBox)
		// The tile: the first letter of the name on a disc.
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, AsphaltHud::ShopTileGap, 0.f)
		[
			SNew(SBox).WidthOverride(AsphaltHud::ShopIconSize).HeightOverride(AsphaltHud::ShopIconSize)
			[
				SNew(SBorder)
				.BorderImage(&TileBrush)
				.BorderBackgroundColor_Lambda([Look]() { return FSlateColor(Look().TileFill); })
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					AsphaltHud::MakeText(TAttribute<FText>::CreateLambda([this, Index]() { return FText::FromString(Model.GetRow(Index).Name.ToString().Left(1)); }),
						EDIAFontWeight::ExtraBold, AsphaltHud::ShopTileFont,
						TAttribute<FSlateColor>::CreateLambda([Look]() { return FSlateColor(Look().TileText); }))
				]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::DownOnly)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					Name
				]
			]
			// What it gives and the reason it is locked keep their case, as in the mockup: units and sentences are not capitals.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this, Index]() { return Model.GetRow(Index).Value; })
				.Font(AsphaltHud::Font(EDIAFontWeight::SemiBold, AsphaltHud::ShopValueTextFont))
				.ColorAndOpacity_Lambda([Look]() { return FSlateColor(Look().Text); })
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.Visibility_Lambda([this, Index]() { return Model.GetRow(Index).Reason.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				[
					SNew(STextBlock)
					.Text_Lambda([this, Index]() { return Model.GetRow(Index).Reason; })
					.Font(AsphaltHud::Font(EDIAFontWeight::SemiBold, AsphaltHud::ShopValueTextFont))
					.ColorAndOpacity(FSlateColor(AsphaltHud::SignYellow))
					.AutoWrapText(true)
					]
			]
		];

	// The bottom line: the price on the left, the button (or OWNED with a tick) on the right.
	const TSharedRef<SWidget> BottomLine =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			AsphaltHud::MakeText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Price; }),
				EDIAFontWeight::ExtraBold, AsphaltHud::ShopPriceFont,
				TAttribute<FSlateColor>::CreateLambda([Look]() { return FSlateColor(Look().Price); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBox)
				.WidthOverride(AsphaltHud::ShopButtonWidth)
				.HeightOverride(AsphaltHud::ShopButtonHeight)
				.Visibility_Lambda([Look]() { return Look().bShowsButton ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SDIAShopButton)
					.ButtonStyle(&ButtonStyle)
					// Focus would take the Tab key from the game and the shop could not be closed with it.
					.IsFocusable(false)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FMargin(0.f))
					.IsEnabled_Lambda([Look]() { return Look().bButtonEnabled; })
					.OnPressed(FSimpleDelegate::CreateSP(this, &SDIAShopPanel::HandlePressed, Index))
					.OnClicked(FOnClicked::CreateSP(this, &SDIAShopPanel::HandleBuy, Index))
					[
						SNew(STextBlock)
						.Text_Lambda([Look]() { return Look().ButtonLabel; })
						.Font_Lambda([Look]() { return AsphaltHud::Font(EDIAFontWeight::ExtraBold, Look().bButtonEnabled ? AsphaltHud::ShopBuyFont : AsphaltHud::ShopOffButtonFont); })
						.ColorAndOpacity_Lambda([Look]() { return FSlateColor(Look().bButtonEnabled ? AsphaltHud::SignYellow : AsphaltHud::SignDimText); })
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Look]() { return Look().bShowsButton ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SDIAShopTick)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					AsphaltHud::MakeText(NSLOCTEXT("DIAHud", "ShopOwned", "OWNED"), EDIAFontWeight::ExtraBold, AsphaltHud::ShopOwnedFont, FSlateColor(AsphaltHud::SignWhite))
				]
			]
		];

	// The plate is the card; its content fills the card's height, the bottom line sits at the bottom.
	return SNew(SBox).HeightOverride(AsphaltHud::ShopCardHeight)
		[
			AsphaltHud::MakePlate(Plate, Ring, Inset(AsphaltHud::ShopCardPaddingH, AsphaltHud::ShopCardPaddingV, AsphaltHud::ShopCardPaddingH, AsphaltHud::ShopCardPaddingV),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					Content
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SSpacer)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					BottomLine
				],
				AsphaltHud::HudRingWidth)
		];
}
