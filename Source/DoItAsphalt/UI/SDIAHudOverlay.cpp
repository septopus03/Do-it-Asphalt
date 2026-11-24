// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAHudOverlay.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "Player/DIACharacter.h"
#include "UI/DIAControlsLegend.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAIcons.h"
#include "UI/DIAHudText.h"
#include "UI/DIAHudWidgets.h"
#include "UI/SDIAHazardStripes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	/** HUD text in the given weight, size and colour (capitals, no shadow). */
	TSharedRef<STextBlock> Text(TAttribute<FText> Value, EDIAFontWeight Weight, int32 Size, const FLinearColor& Color)
	{
		return AsphaltHud::MakeText(Value, Weight, Size, FSlateColor(Color));
	}

	/** The progress bar: a yellow frame around a hatched track and a yellow fill whose width follows Fraction (0..1). */
	TSharedRef<SWidget> MakeProgressBar(TAttribute<float> Fraction)
	{
		const float Frame = AsphaltHud::ProgressFrameWidth;
		const float InnerWidth = AsphaltHud::ProgressBarWidth - 2.f * Frame;
		const float InnerHeight = AsphaltHud::BarHeight - 2.f * Frame;
		return SNew(SBox)
			.WidthOverride(AsphaltHud::ProgressBarWidth)
			.HeightOverride(AsphaltHud::BarHeight)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(AsphaltHud::SignYellow)
				.Padding(Frame)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SDIAHazardStripes)
						.Band(AsphaltHud::ProgressStripeBand)
						.Height(InnerHeight)
						.StripeColor(AsphaltHud::SignStripeDark)
						.GroundColor(AsphaltHud::SignBlack)
					]
					+ SOverlay::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride_Lambda([Fraction, InnerWidth]() { return FMath::Clamp(Fraction.Get(), 0.f, 1.f) * InnerWidth; })
						.HeightOverride(InnerHeight)
						[
							SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(AsphaltHud::SignYellow)
						]
					]
				]
			];
	}
}

void SDIAHudOverlay::Construct(const FArguments& InArgs)
{
	Hud = InArgs._Hud;
	Subsystem = InArgs._Subsystem;

	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
		StateHandle = S->OnStateChanged.AddSP(this, &SDIAHudOverlay::HandleStateChanged);
	}

	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(AsphaltHud::ScreenMargin)
		[
			BuildTopLeft()
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(AsphaltHud::ScreenMargin)
		[
			BuildTopRight()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			BuildCrosshair()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, AsphaltHud::ToastTop, 0.f, 0.f)
		[
			BuildToasts()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, AsphaltHud::CheckTop, 0.f, 0.f)
		[
			BuildCheckView()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, AsphaltHud::BottomMargin)
		[
			BuildBottom()
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(AsphaltHud::ScreenMargin, 0.f, 0.f, AsphaltHud::BottomMargin)
		[
			BuildControlsLegend()
		]
	];
}

void SDIAHudOverlay::Unbind()
{
	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		S->OnStateChanged.Remove(StateHandle);
	}
	StateHandle.Reset();
}

void SDIAHudOverlay::HandleStateChanged()
{
	// Fires every tick; the model builds new texts only when a shown value changed.
	if (const UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
	}
}

const TArray<FDIAPromptLine>& SDIAHudOverlay::GetPromptLines()
{
	const FText Prompt = Hud.IsValid() ? Hud->GetPrompt() : FText::GetEmpty();
	if (!PromptSource.Equals(Prompt.ToString(), ESearchCase::CaseSensitive))
	{
		PromptSource = Prompt.ToString();
		PromptLines = DIAHudText::SplitPrompt(Prompt);
	}
	return PromptLines;
}

const FText& SDIAHudOverlay::GetNextText()
{
	const FText& Next = Model.GetView().Next;
	if (!NextSource.Equals(Next.ToString(), ESearchCase::CaseSensitive))
	{
		NextSource = Next.ToString();
		NextText = DIAHudText::StripLabel(Next);
	}
	return NextText;
}

TSharedRef<SWidget> SDIAHudOverlay::BuildTopLeft()
{
	return AsphaltHud::MakePlate(AsphaltHud::SignYellow, AsphaltHud::SignBlack, AsphaltHud::Inset(24.f, 8.f, 24.f, 10.f),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
		[
			Text(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Credits; }), EDIAFontWeight::ExtraBold, AsphaltHud::HudCreditsFont, AsphaltHud::SignBlack)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12.f, 0.f, 0.f, 6.f)
		[
			Text(NSLOCTEXT("DIAHud", "CreditsCaption", "credits"), EDIAFontWeight::Bold, AsphaltHud::HudCreditsCaptionFont, AsphaltHud::SignBlack)
		]);
}

TSharedRef<SWidget> SDIAHudOverlay::BuildTopRight()
{
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			Text(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Progress; }), EDIAFontWeight::ExtraBold, AsphaltHud::HudProgressFont, AsphaltHud::SignWhite)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 6.f, 0.f, 0.f)
		[
			Text(NSLOCTEXT("DIAHud", "ProgressCaption", "cells paved"), EDIAFontWeight::Bold, AsphaltHud::HudProgressCaptionFont, AsphaltHud::SignYellow)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 6.f, 0.f, 0.f)
		[
			MakeProgressBar(TAttribute<float>::CreateLambda([this]() { return Model.GetView().ProgressFraction; }))
		]
		// The next unlock step (hidden once every step is open): a yellow tag and the step without its "Next:" label.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return Model.GetView().Next.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(AsphaltHud::SignYellow)
				.Padding(8.f, 0.f)
				[
					Text(NSLOCTEXT("DIAHud", "NextTag", "Next"), EDIAFontWeight::ExtraBold, AsphaltHud::HudNextTagFont, AsphaltHud::SignBlack)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Text(TAttribute<FText>::CreateLambda([this]() { return GetNextText(); }), EDIAFontWeight::Bold, AsphaltHud::HudNextFont, AsphaltHud::SignWhite)
			]
		];
	// One line per robot, stacked in robot order (a robot that is not owned has no text and takes no room).
	for (int32 Paver = 0; Paver < MaxAutoPavers; ++Paver)
	{
		Lines->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this, Paver]() { return Model.GetView().Pavers[Paver].IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				Text(TAttribute<FText>::CreateLambda([this, Paver]() { return Model.GetView().Pavers[Paver]; }), EDIAFontWeight::Bold, AsphaltHud::HudNextFont, AsphaltHud::SignWhite)
			]
		];
	}
	return AsphaltHud::MakePlate(AsphaltHud::SignBlack, AsphaltHud::SignYellow, AsphaltHud::Inset(22.f, 12.f, 22.f, 16.f), Lines);
}

TSharedRef<SWidget> SDIAHudOverlay::BuildHotbar()
{
	TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < static_cast<int32>(ELotHand::Count); ++Index)
	{
		// Only what the player owns is shown; the bowl is always there. The gap goes with the slot, so that a hidden one leaves none.
		const ELotHand Hand = static_cast<ELotHand>(Index);
		TWeakObjectPtr<UDIAWorldSubsystem> WeakSubsystem = Subsystem;
		Slots->AddSlot().AutoWidth()
		[
			SNew(SBox)
			.Padding(Index == 0 ? 0.f : AsphaltHud::HotbarGap, 0.f, 0.f, 0.f)
			.Visibility_Lambda([WeakSubsystem, Hand]()
			{
				const UDIAWorldSubsystem* S = WeakSubsystem.Get();
				return S && S->GetGame().CanSelectHand(Hand) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
			})
			[
				BuildHotbarSlot(Hand)
			]
		];
	}
	return SNew(SVerticalBox)
		// What is in the hands with how much it holds; a movable that is held or the car replaces the tool's name here.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::BottomStackGap)
		[
			AsphaltHud::MakePlate(AsphaltHud::SignBlack, AsphaltHud::SignYellow, AsphaltHud::Inset(22.f, 4.f, 22.f, 6.f),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					Text(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().HandLabel; }), EDIAFontWeight::ExtraBold, AsphaltHud::HudHandFont, AsphaltHud::SignWhite)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(14.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox)
					.Visibility_Lambda([this]() { return Model.GetView().HandGauge.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					[
						Text(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().HandGauge; }), EDIAFontWeight::Bold, AsphaltHud::HudHandFont, AsphaltHud::SignYellow)
					]
				])
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
		[
			Slots
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SDIAHazardStripes)
			.Band(AsphaltHud::HotbarStripeBand)
			.Height(AsphaltHud::HotbarStripeHeight)
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildHotbarSlot(ELotHand Hand)
{
	TWeakObjectPtr<UDIAWorldSubsystem> WeakSubsystem = Subsystem;
	// The tool in the hands is lit; while a movable is held or the player is in the car no tool is.
	auto IsSelected = [WeakSubsystem, Hand]()
	{
		const UDIAWorldSubsystem* S = WeakSubsystem.Get();
		return S && S->GetGame().GetHand() == Hand && S->GetGame().GetHeld() == ELotMovable::Count && !S->GetGame().World().bPlayerInCar;
	};
	auto IsAvailable = [WeakSubsystem, Hand]()
	{
		const UDIAWorldSubsystem* S = WeakSubsystem.Get();
		return S && S->GetGame().CanSelectHand(Hand);
	};
	auto Fill = [WeakSubsystem, Hand]()
	{
		const UDIAWorldSubsystem* S = WeakSubsystem.Get();
		const double Capacity = S ? S->GetGame().GetHandCapacity(Hand) : 0.0;
		return Capacity > 0.0 ? static_cast<float>(S->GetGame().GetHandLitres(Hand) / Capacity) : 0.f;
	};
	// The selected slot turns the sign round: a yellow plate with black ring, key square and fill.
	auto PlateColor = [IsSelected]() { return IsSelected() ? AsphaltHud::SignYellow : AsphaltHud::SignBlack; };
	auto InkColor = [IsSelected]() { return IsSelected() ? AsphaltHud::SignBlack : AsphaltHud::SignYellow; };
	const bool bHasGauge = LotBalance::HandCapacityL[static_cast<int32>(Hand)] > 0.0;
	const FText Key = FText::AsNumber(static_cast<int32>(Hand) + 1);
	// The gauge's width: the slot minus the border and padding on both sides and the gauge's own side margin of 2.
	const float SidePadding = 9.f;
	const float GaugeWidth = AsphaltHud::HotbarSlotWidth - 2.f * (AsphaltHud::HudRingWidth + SidePadding) - 2.f * 2.f;

	return SNew(SBox)
		.WidthOverride(AsphaltHud::HotbarSlotWidth)
		.HeightOverride(AsphaltHud::HotbarSlotHeight)
		[
			AsphaltHud::MakePlate(TAttribute<FLinearColor>::CreateLambda(PlateColor), TAttribute<FLinearColor>::CreateLambda(InkColor), AsphaltHud::Inset(SidePadding, 9.f, SidePadding, 12.f),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(AsphaltHud::HotbarKeySize).HeightOverride(AsphaltHud::HotbarKeySize)
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor_Lambda([InkColor]() { return FSlateColor(InkColor()); })
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.Padding(0.f)
							[
								AsphaltHud::MakeText(Key, EDIAFontWeight::ExtraBold, AsphaltHud::HotbarKeyFont, TAttribute<FSlateColor>::CreateLambda([PlateColor]() { return FSlateColor(PlateColor()); }))
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						AsphaltHud::MakeText(DIAFormat::HandName(Hand), EDIAFontWeight::ExtraBold, AsphaltHud::HotbarNameFontSize,
							TAttribute<FSlateColor>::CreateLambda([IsSelected, IsAvailable]()
							{
								return FSlateColor(!IsAvailable() ? AsphaltHud::SignDimText : (IsSelected() ? AsphaltHud::SignBlack : AsphaltHud::SignWhite));
							}))
					]
				]
				// The picture of the tool or vessel.
				+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(DIAIcons::HandIconSize).HeightOverride(DIAIcons::HandIconSize)
					[
						SNew(SImage).Image(DIAIcons::HandBrush(Hand))
					]
				]
				// The fill of a vessel; a tool without content has an empty track, a slot that cannot be taken shows no gauge.
				+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 0.f)
				[
					SNew(SBox)
					.HeightOverride(AsphaltHud::HotbarGaugeHeight)
					.Visibility_Lambda([bHasGauge, IsAvailable]() { return bHasGauge && IsAvailable() ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage).Image(WhiteBrush())
							.ColorAndOpacity_Lambda([IsSelected]() { return IsSelected() ? FSlateColor(FLinearColor(FColor(0x11, 0x12, 0x14, 64))) : FSlateColor(FLinearColor(FColor(0xFF, 0xFF, 0xFF, 56))); })
						]
						+ SOverlay::Slot().HAlign(HAlign_Left)
						[
							SNew(SBox)
							.WidthOverride_Lambda([Fill, GaugeWidth]() { return FMath::Clamp(Fill(), 0.f, 1.f) * GaugeWidth; })
							[
								SNew(SImage).Image(WhiteBrush()).ColorAndOpacity_Lambda([InkColor]() { return FSlateColor(InkColor()); })
							]
						]
					]
				])
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildCheckView()
{
	TWeakObjectPtr<ADIAHUD> WeakHud = Hud;
	auto View = [WeakHud]() { return WeakHud.IsValid() ? WeakHud->GetCheckView() : FDIACheckView(); };
	// The diamond is a square plate turned by 45 degrees; the arrow inside is turned back by as much.
	const float DiamondTurn = 45.f;
	return SNew(SHorizontalBox)
		.Visibility_Lambda([View]() { return View().Nearest.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(AsphaltHud::CheckDiamondSize)
			.HeightOverride(AsphaltHud::CheckDiamondSize)
			.RenderTransformPivot(FVector2D(0.5, 0.5))
			.RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(DiamondTurn)))))
			[
				AsphaltHud::MakePlate(AsphaltHud::SignOrange, AsphaltHud::SignBlack, FMargin(0.f),
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						// The arrow: "→" turned so that straight up on the screen is straight ahead.
						SNew(STextBlock)
						.Text(NSLOCTEXT("DIAHud", "CheckArrow", "→"))
						.Font(AsphaltHud::Font(EDIAFontWeight::Bold, AsphaltHud::CheckArrowFontSize))
						.ColorAndOpacity(AsphaltHud::SignBlack)
						.RenderTransformPivot(FVector2D(0.5, 0.5))
						.RenderTransform_Lambda([WeakHud, View, DiamondTurn]()
						{
							const APlayerController* Controller = WeakHud.IsValid() ? WeakHud->GetOwningPlayerController() : nullptr;
							// Whatever the player is: the character on foot, the car at the wheel.
							const APawn* Character = Controller ? Controller->GetPawn() : nullptr;
							const FDIACheckView Check = View();
							const double Yaw = Controller ? Controller->GetControlRotation().Yaw : 0.0;
							const FVector2D From = Character ? FVector2D(Character->GetActorLocation()) : FVector2D::ZeroVector;
							// Right is 0 for the glyph, ahead (up) is -90; the diamond already turned it by DiamondTurn.
							const double Degrees = DIAFormat::ArrowDegrees(From, Check.Target, Yaw) - 90.0 - DiamondTurn;
							return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Degrees))));
						})
					], AsphaltHud::CheckDiamondRing)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(AsphaltHud::CheckGap, 0.f, 0.f, 0.f)
		[
			AsphaltHud::MakePlate(AsphaltHud::SignBlack, AsphaltHud::SignOrange, AsphaltHud::Inset(22.f, 6.f, 22.f, 8.f),
				Text(TAttribute<FText>::CreateLambda([View]() { return View().Nearest; }), EDIAFontWeight::Bold, AsphaltHud::HudCheckFont, AsphaltHud::SignOrangeText))
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildBottom()
{
	TSharedRef<SVerticalBox> Prompt = SNew(SVerticalBox);
	for (int32 Line = 0; Line < DIAHudText::MaxPromptLines; ++Line)
	{
		auto LineText = [this, Line](bool bKey)
		{
			const TArray<FDIAPromptLine>& Lines = GetPromptLines();
			return Lines.IsValidIndex(Line) ? (bKey ? Lines[Line].Key : Lines[Line].Action) : FText::GetEmpty();
		};
		// Rows are left aligned so that the keys of stacked lines form a column.
		Prompt->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, Line == 0 ? 0.f : AsphaltHud::PromptLineGap, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this, Line]() { return GetPromptLines().IsValidIndex(Line) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			// The key on a square yellow plate (not shown for a line that has no key).
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, AsphaltHud::PromptLineGap, 0.f)
			[
				SNew(SBox)
				.MinDesiredWidth(AsphaltHud::PromptKeyWidth)
				.Visibility_Lambda([this, Line]() { return GetPromptLines().IsValidIndex(Line) && !GetPromptLines()[Line].Key.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					AsphaltHud::MakePlate(AsphaltHud::SignYellow, AsphaltHud::SignBlack, FMargin(0.f),
						SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							Text(TAttribute<FText>::CreateLambda([LineText]() { return LineText(true); }), EDIAFontWeight::ExtraBold, AsphaltHud::HudPromptFont, AsphaltHud::SignBlack)
						])
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				AsphaltHud::MakePlate(AsphaltHud::SignBlack, AsphaltHud::SignYellow, AsphaltHud::Inset(24.f, 6.f, 24.f, 8.f),
					Text(TAttribute<FText>::CreateLambda([LineText]() { return LineText(false); }), EDIAFontWeight::Bold, AsphaltHud::HudPromptFont, AsphaltHud::SignWhite))
			]
		];
	}

	return SNew(SVerticalBox)
		// The plant's stock, while a refill is within reach (hidden elsewhere).
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::BottomStackGap)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return Model.GetView().Refill.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				AsphaltHud::MakePlate(AsphaltHud::SignBlack, AsphaltHud::SignYellow, AsphaltHud::Inset(22.f, 6.f, 22.f, 8.f),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 14.f, 2.f)
					[
						Text(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().RefillCaption; }), EDIAFontWeight::Bold, AsphaltHud::HudRefillCaptionFont, AsphaltHud::SignYellow)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						Text(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Refill; }), EDIAFontWeight::Bold, AsphaltHud::HudRefillFont, AsphaltHud::SignWhite)
					])
			]
		]
		// Contextual prompt: a key plate and an action plate per line (the whole block is hidden while the prompt is empty).
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::BottomStackGap)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return GetPromptLines().Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				Prompt
			]
		]
		// The hand line and the inventory bar with its stripes, at the very bottom of the screen.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BuildHotbar()
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildControlsLegend()
{
	// One column of lines (key caps, then the action) per variant; the one that does not apply is collapsed and takes no room.
	auto BuildLines = [](const TArray<DIAControlsLegend::FLine>& Lines)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			const DIAControlsLegend::FLine& Line = Lines[Index];
			TSharedRef<SHorizontalBox> Caps = SNew(SHorizontalBox);
			for (int32 Cap = 0; Cap < Line.Caps.Num(); ++Cap)
			{
				Caps->AddSlot().AutoWidth().Padding(Cap == 0 ? 0.f : AsphaltHud::LegendKeyGap, 0.f, 0.f, 0.f)
				[
					SNew(SBox)
					.MinDesiredWidth(AsphaltHud::LegendKeyMinWidth)
					.HeightOverride(AsphaltHud::LegendKeyHeight)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(AsphaltHud::SignYellow)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(FMargin(6.f, 0.f))
						[
							Text(Line.Caps[Cap].Label, EDIAFontWeight::ExtraBold, AsphaltHud::LegendKeyFont, AsphaltHud::SignBlack)
						]
					]
				];
			}
			Box->AddSlot().AutoHeight().Padding(0.f, Index == 0 ? 0.f : AsphaltHud::LegendLineGap, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(AsphaltHud::LegendKeyColumnWidth)
					[
						Caps
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(AsphaltHud::LegendActionGap, 0.f, 0.f, 0.f)
				[
					Text(Line.Action, EDIAFontWeight::Bold, AsphaltHud::LegendActionFont, AsphaltHud::SignWhite)
				]
			];
		}
		return Box;
	};

	TWeakObjectPtr<UDIAWorldSubsystem> WeakSubsystem = Subsystem;
	auto InCar = [WeakSubsystem]()
	{
		const UDIAWorldSubsystem* S = WeakSubsystem.Get();
		return S && S->GetGame().World().bPlayerInCar;
	};
	return SAssignNew(ControlsLegend, SBox)
		.Visibility_Lambda([this]()
		{
			const ADIAHUD* H = Hud.Get();
			return (H && H->IsControlsLegendShown()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			AsphaltHud::MakePlate(AsphaltHud::SignBlack, AsphaltHud::SignYellow, AsphaltHud::Inset(18.f, 10.f, 20.f, 14.f),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					Text(DIAControlsLegend::Title(), EDIAFontWeight::ExtraBold, AsphaltHud::LegendTitleFont, AsphaltHud::SignYellow)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(LegendOnFoot, SBox)
					.Visibility_Lambda([InCar]() { return InCar() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					[
						BuildLines(DIAControlsLegend::OnFoot())
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(LegendInCar, SBox)
					.Visibility_Lambda([InCar]() { return InCar() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						BuildLines(DIAControlsLegend::InCar())
					]
				])
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildCrosshair()
{
	return SNew(SBox)
		.WidthOverride(AsphaltHud::CrosshairOuterSize)
		.HeightOverride(AsphaltHud::CrosshairOuterSize)
		.Visibility_Lambda([this]()
		{
			const ADIAHUD* H = Hud.Get();
			return (H && !H->IsMenuOpen() && !H->IsShopOpen() && !H->IsWinScreenShown()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(&DotBrush).BorderBackgroundColor(AsphaltHud::CrosshairRingColor)
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(AsphaltHud::CrosshairInnerSize)
				.HeightOverride(AsphaltHud::CrosshairInnerSize)
				[
					SNew(SBorder).BorderImage(&DotBrush).BorderBackgroundColor(AsphaltHud::CrosshairColor)
				]
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildToasts()
{
	TWeakObjectPtr<ADIAHUD> WeakHud = Hud;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (int32 Index = 0; Index < AsphaltHud::MaxToasts; ++Index)
	{
		auto Alpha = [WeakHud, Index]() { return WeakHud.IsValid() ? WeakHud->GetToasts().GetAlpha(Index) : 0.f; };
		TSharedRef<STextBlock> Label = AsphaltHud::MakeText(
			TAttribute<FText>::CreateLambda([WeakHud, Index]() { return WeakHud.IsValid() ? WeakHud->GetToasts().GetText(Index) : FText::GetEmpty(); }),
			EDIAFontWeight::ExtraBold, AsphaltHud::HudToastFont,
			TAttribute<FSlateColor>::CreateLambda([Alpha]() { return FSlateColor(AsphaltHud::Faded(AsphaltHud::SignBlack, Alpha())); }));
		// A long toast (an unlock: the step and its effect) wraps at a fixed width and is centred. No auto-wrap: it would wrap at the
		// width the slot was last painted at, and a slot is as wide as its last text.
		Label->SetWrapTextAt(AsphaltHud::ToastWrapWidth);
		Label->SetJustification(ETextJustify::Center);
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, AsphaltHud::ToastGap)
		[
			SNew(SBox)
			.Visibility_Lambda([Alpha]() { return Alpha() > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				AsphaltHud::MakePlate(AsphaltHud::SignYellow, AsphaltHud::SignBlack, AsphaltHud::Inset(22.f, 6.f, 22.f, 8.f), Label, AsphaltHud::HudRingWidth, TAttribute<float>::CreateLambda(Alpha))
			]
		];
	}
	return Box;
}
