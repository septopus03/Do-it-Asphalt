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
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** The button's label for the state of its card. */
	FText ButtonLabel(EDIAShopState State)
	{
		switch (State)
		{
		case EDIAShopState::Locked: return NSLOCTEXT("DIAHud", "ShopLocked", "LOCKED");
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

	/** The outline of a card: bright for what can be bought, faint for what cannot, green for what is owned. */
	FLinearColor OutlineColor(EDIAShopState State)
	{
		switch (State)
		{
		case EDIAShopState::Buy: return AsphaltHud::ShopOutlineBuyColor;
		case EDIAShopState::NoCredits: return AsphaltHud::ShopOutlineLackColor;
		case EDIAShopState::Owned: return AsphaltHud::ShopOutlineOwnedColor;
		default: return AsphaltHud::ShopOutlineLockedColor;
		}
	}

	/** The colour of the tile in front of a card's name: the plant, tools, things to carry, machines, the car. */
	FLinearColor IconColor(const FDIAShopRow& Row)
	{
		if (Row.bPlant)
		{
			return AsphaltHud::ShopIconPlantColor;
		}
		switch (Row.Item)
		{
		case ELotItem::Scoop:
		case ELotItem::Bucket:
			return AsphaltHud::ShopIconToolColor;
		case ELotItem::Suitcase:
		case ELotItem::Cart:
		case ELotItem::Station:
			return AsphaltHud::ShopIconCarrierColor;
		case ELotItem::Car:
		case ELotItem::CarWideBar:
			return AsphaltHud::ShopIconCarColor;
		default:
			return AsphaltHud::ShopIconMachineColor;
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

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(AsphaltHud::ShopCardGap))
		.MinDesiredSlotWidth(AsphaltHud::ShopCardWidth)
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
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(AsphaltHud::ShopPanelColor)
			.Padding(AsphaltHud::ShopPadding)
			[
				SNew(SVerticalBox)
				// The header: the title with what the shop is for, and the credits to spend.
				+ SVerticalBox::Slot().AutoHeight().Padding(AsphaltHud::ShopCardGap, 0.f, AsphaltHud::ShopCardGap, 14.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ShopTitle", "SHOP"), AsphaltHud::BigFontSize, AsphaltHud::ScreenTitleColor)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ShopSubtitle", "Tools and machines for the car park"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.BorderImage(&BadgeBrush)
						.BorderBackgroundColor(AsphaltHud::ShopCardColor)
						.Padding(18.f, 8.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								AsphaltHud::MakeShadowedText(TAttribute<FText>::CreateLambda([WeakSubsystem]()
								{
									const UDIAWorldSubsystem* S = WeakSubsystem.Get();
									return S ? DIAFormat::Credits(S->GetGame().GetCredits()) : FText::GetEmpty();
								}), AsphaltHud::BigFontSize, AsphaltHud::CreditsColor)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f, 0.f, 0.f, 4.f)
							[
								AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ShopCredits", "credits"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Grid
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)
				[
					AsphaltHud::MakeShadowedText(NSLOCTEXT("DIAHud", "ShopHint", "Tab / Esc - close"), AsphaltHud::CaptionFontSize, AsphaltHud::CaptionColor)
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
	const FSlateFontInfo CardFont = FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::ShopFontSize);
	const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle("Regular", AsphaltHud::ShopValueFontSize);
	auto StateOf = [this, Index]() { return Model.GetRow(Index).State; };

	// The outline is the outer border; the card sits inside it.
	return SNew(SBorder)
		.BorderImage(&CardBrush)
		.BorderBackgroundColor_Lambda([this, Index]() { return FSlateColor(OutlineColor(Model.GetRow(Index).State)); })
		.Padding(AsphaltHud::ShopCardBorder)
		[
			SNew(SBorder)
			.BorderImage(&CardBrush)
			.BorderBackgroundColor_Lambda([StateOf]() { return FSlateColor(StateOf() == EDIAShopState::Owned ? AsphaltHud::ShopCardOwnedColor : AsphaltHud::ShopCardColor); })
			.Padding(AsphaltHud::ShopCardPadding)
			[
				SNew(SHorizontalBox)
				// The tile: the first letter of the name on the colour of what it is.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(SBox).WidthOverride(AsphaltHud::ShopIconSize).HeightOverride(AsphaltHud::ShopIconSize)
					[
						SNew(SBorder)
						.BorderImage(&IconBrush)
						.BorderBackgroundColor_Lambda([this, Index]()
						{
							const FDIAShopRow& R = Model.GetRow(Index);
							FLinearColor C = IconColor(R);
							// What is not on offer fades back.
							C.A = R.State == EDIAShopState::Locked ? 0.28f : 1.f;
							return FSlateColor(C);
						})
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this, Index]() { return FText::FromString(Model.GetRow(Index).Name.ToString().Left(1)); })
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
							.ColorAndOpacity(AsphaltHud::ButtonTextColor)
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text_Lambda([this, Index]() { return Model.GetRow(Index).Name; })
						.Font(CardFont)
						.ColorAndOpacity_Lambda([StateOf]() { return FSlateColor(StateOf() == EDIAShopState::Locked ? AsphaltHud::ShopDimColor : AsphaltHud::TextColor); })
						.ShadowOffset(AsphaltHud::ShadowOffset)
						.ShadowColorAndOpacity(AsphaltHud::ShadowColor)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([this, Index]() { return Model.GetRow(Index).Value; })
						.Font(ValueFont)
						.ColorAndOpacity(AsphaltHud::CaptionColor)
					]
					// The reason a machine is locked, under what it gives (collapsed when there is none).
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(SBox)
						.Visibility_Lambda([this, Index]() { return Model.GetRow(Index).Reason.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						[
							SNew(STextBlock)
							.Text_Lambda([this, Index]() { return Model.GetRow(Index).Reason; })
							.Font(ValueFont)
							.ColorAndOpacity(AsphaltHud::PromptColor)
							.AutoWrapText(true)
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SSpacer)
					]
					// The bottom line: the price on the left, the button (or the OWNED badge) on the right.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this, Index]() { return Model.GetRow(Index).Price; })
							.Font(CardFont)
							.ColorAndOpacity_Lambda([StateOf]() { return FSlateColor(PriceColor(StateOf())); })
							.ShadowOffset(AsphaltHud::ShadowOffset)
							.ShadowColorAndOpacity(AsphaltHud::ShadowColor)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SBox)
								.WidthOverride(AsphaltHud::ShopButtonWidth)
								.Visibility_Lambda([StateOf]() { return StateOf() == EDIAShopState::Owned ? EVisibility::Collapsed : EVisibility::Visible; })
								[
									SNew(SDIAShopButton)
									.ButtonStyle(&ButtonStyle)
									// Focus would take the Tab key from the game and the shop could not be closed with it.
									.IsFocusable(false)
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.ContentPadding(FMargin(0.f, 3.f))
									.IsEnabled_Lambda([StateOf]() { return StateOf() == EDIAShopState::Buy; })
									.OnPressed(FSimpleDelegate::CreateSP(this, &SDIAShopPanel::HandlePressed, Index))
									.OnClicked(FOnClicked::CreateSP(this, &SDIAShopPanel::HandleBuy, Index))
									[
										SNew(STextBlock)
										.Text_Lambda([StateOf]() { return ButtonLabel(StateOf()); })
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::CaptionFontSize))
										.ColorAndOpacity_Lambda([StateOf]() { return StateOf() == EDIAShopState::Buy ? AsphaltHud::ButtonTextColor : AsphaltHud::ButtonTextDisabledColor; })
									]
								]
							]
							+ SOverlay::Slot()
							[
								SNew(SBox)
								.WidthOverride(AsphaltHud::ShopButtonWidth)
								.Visibility_Lambda([StateOf]() { return StateOf() == EDIAShopState::Owned ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								[
									SNew(SBorder)
									.BorderImage(&BadgeBrush)
									.BorderBackgroundColor(AsphaltHud::ShopOutlineOwnedColor)
									.HAlign(HAlign_Center)
									.Padding(0.f, 5.f)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("DIAHud", "ShopOwned", "OWNED"))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", AsphaltHud::CaptionFontSize))
										.ColorAndOpacity(AsphaltHud::ButtonTextColor)
									]
								]
							]
						]
					]
				]
			]
		];
}
