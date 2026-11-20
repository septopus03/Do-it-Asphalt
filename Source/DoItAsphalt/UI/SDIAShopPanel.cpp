// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAShopPanel.h"
#include "Game/DIAWorldSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAButtonStyle.h"
#include "UI/DIAHUD.h"
#include "UI/DIAHudStyle.h"
#include "UI/DIAHudWidgets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** The button's label for the state of its row. */
	FText ButtonLabel(EDIAShopState State)
	{
		switch (State)
		{
		case EDIAShopState::Locked: return NSLOCTEXT("DIAHud", "ShopLocked", "LOCKED");
		case EDIAShopState::Owned: return NSLOCTEXT("DIAHud", "ShopOwned", "OWNED");
		default: return NSLOCTEXT("DIAHud", "ShopBuy", "BUY");
		}
	}

	/** The colour of the price: gold when it can be paid, red when the credits are short, dim when it does not matter yet. */
	FLinearColor PriceColor(EDIAShopState State)
	{
		switch (State)
		{
		case EDIAShopState::Buy: return AsphaltHud::CreditsColor;
		case EDIAShopState::NoCredits: return AsphaltHud::ShopLackColor;
		default: return AsphaltHud::ShopDimColor;
		}
	}
}

void SDIAShopPanel::Construct(const FArguments& InArgs)
{
	Hud = InArgs._Hud;
	Subsystem = InArgs._Subsystem;

	ButtonStyle = AsphaltHud::MakeButtonStyle();

	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
		StateHandle = S->OnStateChanged.AddSP(this, &SDIAShopPanel::HandleStateChanged);
	}

	TSharedRef<SScrollBox> Rows = SNew(SScrollBox).Orientation(Orient_Vertical);
	for (int32 Row = 0; Row < FDIAShopModel::NumRows; ++Row)
	{
		Rows->AddSlot().Padding(0.f, 0.f, 0.f, AsphaltHud::ShopRowSpacing)
		[
			BuildRow(Row)
		];
	}

	// The root takes no mouse, so that the game is clickable around the panel; the panel itself is hit-testable.
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::ShopPanelColor)
			.Padding(AsphaltHud::ShopPadding)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ShopTitle", "SHOP"), AsphaltHud::BigFontSize, AsphaltHud::TextColor)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(24.f, 0.f, 0.f, 4.f)
					[
						AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ShopHint", "Tab - close"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).MaxDesiredHeight(AsphaltHud::ShopMaxListHeight)
					[
						Rows
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

void SDIAShopPanel::HandleStateChanged()
{
	// Fires every tick; the model builds new rows only when a shown value changed.
	if (const UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
	}
}

FReply SDIAShopPanel::HandleBuy(int32 Row)
{
	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		const bool bPlant = Model.GetRow(Row).bPlant;
		// Buying goes through the subsystem, which broadcasts it: an item gives its toast, the model and the character refresh.
		if (Model.Buy(Row, *S) && bPlant)
		{
			if (ADIAHUD* H = Hud.Get())
			{
				H->ShowToast(DIAFormat::PlantToast(S->GetGame().GetPlantLevel()));
			}
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAShopPanel::BuildRow(int32 Row)
{
	const int32 Index = Row;
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::ShopFontSize);
	return SNew(SBorder)
		.BorderImage(&RowBrush)
		.BorderBackgroundColor(AsphaltHud::ShopRowColor)
		.Padding(AsphaltHud::PanelPaddingH, AsphaltHud::PanelPaddingV)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::ShopNameWidth)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Name; }), AsphaltHud::ShopFontSize, AsphaltHud::TextColor)
					]
					// The reason a machine is locked, under its name (collapsed when there is none).
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.Visibility_Lambda([this, Index]() { return Model.GetRow(Index).Reason.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Reason; }), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
						]
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::ShopValueWidth)
				[
					AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Value; }), AsphaltHud::ShopFontSize, AsphaltHud::CaptionColor)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::ShopPriceWidth)
				[
					SNew(STextBlock)
					.Text_Lambda([this, Index]() { return Model.GetRow(Index).Price; })
					.Font(RowFont)
					.ColorAndOpacity_Lambda([this, Index]() { return FSlateColor(PriceColor(Model.GetRow(Index).State)); })
					.ShadowOffset(AsphaltHud::ShadowOffset)
					.ShadowColorAndOpacity(AsphaltHud::ShadowColor)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::ShopButtonWidth)
				[
					SNew(SButton)
					.ButtonStyle(&ButtonStyle)
					// Focus would take the Tab key from the game and the shop could not be closed with it.
					.IsFocusable(false)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FMargin(0.f, 4.f))
					.IsEnabled_Lambda([this, Index]() { return Model.GetRow(Index).State == EDIAShopState::Buy; })
					.OnClicked(FOnClicked::CreateSP(this, &SDIAShopPanel::HandleBuy, Index))
					[
						SNew(STextBlock)
						.Text_Lambda([this, Index]() { return ButtonLabel(Model.GetRow(Index).State); })
						.Font(RowFont)
						.ColorAndOpacity_Lambda([this, Index]() { return Model.GetRow(Index).State == EDIAShopState::Buy ? AsphaltHud::ButtonTextColor : AsphaltHud::ButtonTextDisabledColor; })
					]
				]
			]
		];
}
