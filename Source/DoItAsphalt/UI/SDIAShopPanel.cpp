// Copyright Do it Asphalt. All Rights Reserved.

#include "UI/SDIAShopPanel.h"
#include "Game/DIAWorldSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/DIAHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TSharedRef<STextBlock> MakeText(TAttribute<FText> Text, int32 Size, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", Size))
			.ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(1.0, 1.0))
			.ShadowColorAndOpacity(AsphaltHud::ShadowColor);
	}
}

void SDIAShopPanel::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;

	ButtonStyle = FButtonStyle()
		.SetNormal(ButtonNormalBrush)
		.SetHovered(ButtonHoverBrush)
		.SetPressed(ButtonPressedBrush)
		.SetDisabled(ButtonDisabledBrush)
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));

	if (UDIAWorldSubsystem* S = Subsystem.Get())
	{
		Model.Update(S->GetGame());
		StateHandle = S->OnStateChanged.AddSP(this, &SDIAShopPanel::HandleStateChanged);
	}

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (int32 Row = 0; Row < FDIAShopModel::NumRows; ++Row)
	{
		Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, AsphaltHud::ShopRowSpacing)
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
						MakeText(NSLOCTEXT("DIAHud", "ShopTitle", "SHOP"), AsphaltHud::BigFontSize, AsphaltHud::TextColor)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(24.f, 0.f, 0.f, 4.f)
					[
						MakeText(NSLOCTEXT("DIAHud", "ShopHint", "Tab - close"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Rows
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
		// Buy broadcasts OnStateChanged, which refreshes this model and the character's equipment.
		S->Buy(FDIAShopModel::RowUpgrade(Row));
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SDIAShopPanel::BuildRow(int32 Row)
{
	const int32 Index = Row;
	const bool bShards = FDIAShopModel::RowUpgrade(Row) == EAsphaltUpgrade::Backpack;
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
					MakeText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Name; }), AsphaltHud::MediumFontSize, AsphaltHud::TextColor)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::ShopChangeWidth)
				[
					MakeText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Change; }), AsphaltHud::MediumFontSize, AsphaltHud::CaptionColor)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(AsphaltHud::ShopPriceWidth)
				[
					MakeText(TAttribute<FText>::CreateLambda([this, Index]() { return Model.GetRow(Index).Price; }), AsphaltHud::MediumFontSize,
						bShards ? AsphaltHud::ShardColor : AsphaltHud::MoneyColor)
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
					.ContentPadding(FMargin(0.f, 6.f))
					.IsEnabled_Lambda([this, Index]() { return Model.GetRow(Index).bCanBuy; })
					.OnClicked(FOnClicked::CreateSP(this, &SDIAShopPanel::HandleBuy, Index))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("DIAHud", "ShopBuy", "BUY"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::MediumFontSize))
						.ColorAndOpacity_Lambda([this, Index]() { return Model.GetRow(Index).bCanBuy ? AsphaltHud::ButtonTextColor : AsphaltHud::ButtonTextDisabledColor; })
					]
				]
			]
		];
}
