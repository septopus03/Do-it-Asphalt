// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAHudOverlay.h"
#include "Game/DIAWorldSubsystem.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "Player/DIACharacter.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	/** A horizontal bar: dim track and a fill whose width follows Fraction (0..1). */
	TSharedRef<SWidget> MakeBar(TAttribute<float> Fraction, const FLinearColor& Fill, float Width)
	{
		return SNew(SBox)
			.WidthOverride(Width)
			.HeightOverride(AsphaltHud::BarHeight)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(AsphaltHud::BarTrackColor)
				]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride_Lambda([Fraction, Width]() { return FMath::Clamp(Fraction.Get(), 0.f, 1.f) * Width; })
					[
						SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Fill)
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

TSharedRef<SWidget> SDIAHudOverlay::BuildTopLeft()
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(AsphaltHud::PanelColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Credits; }), AsphaltHud::BigFontSize, AsphaltHud::CreditsColor)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f, 0.f, 0.f, 4.f)
			[
				AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "CreditsCaption", "credits"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildTopRight()
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(AsphaltHud::PanelColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Progress; }), AsphaltHud::BigFontSize, AsphaltHud::TextColor)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ProgressCaption", "cells paved"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 6.f, 0.f, 0.f)
			[
				MakeBar(TAttribute<float>::CreateLambda([this]() { return Model.GetView().ProgressFraction; }), AsphaltHud::ProgressColor, AsphaltHud::ProgressBarWidth)
			]
			// The next unlock step (hidden once every step is open).
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return Model.GetView().Next.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				[
					AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Next; }), AsphaltHud::CaptionFontSize, AsphaltHud::PromptColor)
				]
			]
			// The auto paver (hidden until it is owned).
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return Model.GetView().Paver.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				[
					AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Paver; }), AsphaltHud::CaptionFontSize, AsphaltHud::TextColor)
				]
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildHotbar()
{
	TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < static_cast<int32>(ELotHand::Count); ++Index)
	{
		Slots->AddSlot().AutoWidth().Padding(AsphaltHud::HotbarGap * 0.5f, 0.f)
		[
			BuildHotbarSlot(static_cast<ELotHand>(Index))
		];
	}
	return SNew(SVerticalBox)
		// What is in the hands with how much it holds; a movable that is held or the car replaces the tool's name here.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::PanelColor)
			.Padding(AsphaltHud::PanelPaddingH, 4.f)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]()
				{
					const FDIAHudView& View = Model.GetView();
					return View.HandGauge.IsEmpty() ? View.HandLabel : FText::Format(NSLOCTEXT("DIAHud", "HandLine", "{0}   {1}"), View.HandLabel, View.HandGauge);
				}), AsphaltHud::CaptionFontSize + 2, AsphaltHud::TextColor)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			Slots
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
	const bool bHasGauge = LotBalance::HandCapacityL[static_cast<int32>(Hand)] > 0.0;
	const FText Key = FText::AsNumber(static_cast<int32>(Hand) + 1);

	return SNew(SBorder)
		.BorderImage(&SlotBrush)
		.BorderBackgroundColor_Lambda([IsSelected]() { return FSlateColor(IsSelected() ? AsphaltHud::ButtonColor : AsphaltHud::HotbarOutlineColor); })
		.Padding(2.f)
		[
			SNew(SBorder)
			.BorderImage(&SlotBrush)
			.BorderBackgroundColor_Lambda([IsSelected]() { return FSlateColor(IsSelected() ? AsphaltHud::HotbarSelectedColor : AsphaltHud::HotbarSlotColor); })
			.Padding(8.f, 4.f, 8.f, 6.f)
			[
				SNew(SBox).WidthOverride(AsphaltHud::HotbarSlotWidth - 20.f).HeightOverride(AsphaltHud::HotbarSlotHeight - 14.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Key)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::CaptionFontSize - 3))
						.ColorAndOpacity_Lambda([IsSelected]() { return FSlateColor(IsSelected() ? AsphaltHud::ButtonColor : AsphaltHud::CaptionColor); })
					]
					+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(DIAFormat::HandName(Hand))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::HotbarNameFontSize))
						.ColorAndOpacity_Lambda([IsAvailable]() { return FSlateColor(IsAvailable() ? AsphaltHud::TextColor : AsphaltHud::ShopDimColor); })
						.ShadowOffset(AsphaltHud::ShadowOffset)
						.ShadowColorAndOpacity(AsphaltHud::ShadowColor)
					]
					// The fill of a vessel; a tool without content has an empty track, a slot that cannot be taken shows no gauge.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(AsphaltHud::HotbarGaugeHeight)
						.Visibility_Lambda([bHasGauge, IsAvailable]() { return bHasGauge && IsAvailable() ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(AsphaltHud::BarTrackColor)
							]
							+ SOverlay::Slot().HAlign(HAlign_Left)
							[
								SNew(SBox)
								.WidthOverride_Lambda([Fill]() { return FMath::Clamp(Fill(), 0.f, 1.f) * (AsphaltHud::HotbarSlotWidth - 20.f); })
								[
									SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(AsphaltHud::CreditsColor)
								]
							]
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildCheckView()
{
	TWeakObjectPtr<ADIAHUD> WeakHud = Hud;
	auto View = [WeakHud]() { return WeakHud.IsValid() ? WeakHud->GetCheckView() : FDIACheckView(); };
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(AsphaltHud::PanelColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		.Visibility_Lambda([View]() { return View().Nearest.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		[
			SNew(SHorizontalBox)
			// The arrow: "→" turned so that straight up on the screen is straight ahead.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("DIAHud", "CheckArrow", "→"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::CheckArrowFontSize))
				.ColorAndOpacity(AsphaltHud::CheckColor)
				.RenderTransformPivot(FVector2D(0.5, 0.5))
				.RenderTransform_Lambda([WeakHud, View]()
				{
					const APlayerController* Controller = WeakHud.IsValid() ? WeakHud->GetOwningPlayerController() : nullptr;
					// Whatever the player is: the character on foot, the car at the wheel.
					const APawn* Character = Controller ? Controller->GetPawn() : nullptr;
					const FDIACheckView Check = View();
					const double Yaw = Controller ? Controller->GetControlRotation().Yaw : 0.0;
					const FVector2D From = Character ? FVector2D(Character->GetActorLocation()) : FVector2D::ZeroVector;
					// Right is 0 for the glyph, ahead (up) is -90.
					const double Degrees = DIAFormat::ArrowDegrees(From, Check.Target, Yaw) - 90.0;
					return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Degrees))));
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([View]() { return View().Nearest; }), AsphaltHud::MediumFontSize, AsphaltHud::CheckColor)
			]
		];
}

TSharedRef<SWidget> SDIAHudOverlay::BuildBottom()
{
	TWeakObjectPtr<ADIAHUD> WeakHud = Hud;
	return SNew(SVerticalBox)
		// The plant's stock, while a refill is within reach (hidden elsewhere).
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::PanelColor)
			.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
			.Visibility_Lambda([this]() { return Model.GetView().Refill.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "RefillCaption", "Plant stock"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this]() { return Model.GetView().Refill; }), AsphaltHud::MediumFontSize, AsphaltHud::TextColor)
				]
			]
		]
		// Contextual prompt (hidden while empty).
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::PanelColor)
			.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
			.Visibility_Lambda([WeakHud]() { return WeakHud.IsValid() && !WeakHud->GetPrompt().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([WeakHud]() { return WeakHud.IsValid() ? WeakHud->GetPrompt() : FText::GetEmpty(); }), AsphaltHud::MediumFontSize, AsphaltHud::PromptColor)
			]
		]
		// The inventory bar, at the very bottom of the screen.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BuildHotbar()
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
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor_Lambda([Alpha]() { FLinearColor C = AsphaltHud::PanelColor; C.A *= Alpha(); return FSlateColor(C); })
			.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
			.Visibility_Lambda([Alpha]() { return Alpha() > 0.f ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(STextBlock)
				.Text_Lambda([WeakHud, Index]() { return WeakHud.IsValid() ? WeakHud->GetToasts().GetText(Index) : FText::GetEmpty(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::MediumFontSize))
				.ColorAndOpacity_Lambda([Alpha]() { FLinearColor C = AsphaltHud::TextColor; C.A = Alpha(); return FSlateColor(C); })
				.ShadowOffset(AsphaltHud::ShadowOffset)
				.ShadowColorAndOpacity_Lambda([Alpha]() { FLinearColor C = AsphaltHud::ShadowColor; C.A *= Alpha(); return C; })
			]
		];
	}
	return Box;
}
