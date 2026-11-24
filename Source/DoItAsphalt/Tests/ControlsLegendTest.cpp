// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "EnhancedActionKeyMapping.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "LotGame.h"
#include "Player/DIAInput.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAControlsLegend.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "UI/SDIAHudOverlay.h"
#include "UI/SDIAPaverMap.h"

namespace
{
	/** True when the context maps Key to Action. */
	bool IsMapped(const UInputMappingContext& Context, const FKey& Key, const UInputAction* Action)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context.GetMappings())
		{
			if (Mapping.Key == Key && Mapping.Action == Action)
			{
				return true;
			}
		}
		return false;
	}
}

DIA_TEST(FPaverMapOnMTest, "DoItAsphalt.Game.Input.PaverMapOnM")

bool FPaverMapOnMTest::RunTest(const FString& Parameters)
{
	const FDIAInput Input = FDIAInput::Create(GetTransientPackage());
	if (!TestNotNull(TEXT("the context"), Input.Context.Get()) || !TestNotNull(TEXT("the map action"), Input.PaverMap.Get()))
	{
		return false;
	}
	TestTrue(TEXT("M opens the paver map"), IsMapped(*Input.Context, EKeys::M, Input.PaverMap));
	TestFalse(TEXT("G does not any more"), IsMapped(*Input.Context, EKeys::G, Input.PaverMap));
	for (const FEnhancedActionKeyMapping& Mapping : Input.Context->GetMappings())
	{
		TestTrue(TEXT("nothing is bound to G"), Mapping.Key != EKeys::G);
		TestTrue(TEXT("M belongs to the paver map alone"), Mapping.Key != EKeys::M || Mapping.Action == Input.PaverMap);
	}
	return true;
}

DIA_TEST(FControlsLegendBindingsTest, "DoItAsphalt.Game.Hud.ControlsLegend.Bindings")

bool FControlsLegendBindingsTest::RunTest(const FString& Parameters)
{
	const FDIAInput Input = FDIAInput::Create(GetTransientPackage());
	if (!TestNotNull(TEXT("the context"), Input.Context.Get()))
	{
		return false;
	}
	TestEqual(TEXT("the title"), DIAControlsLegend::Title().ToString(), FString(TEXT("Controls")));

	const TArray<DIAControlsLegend::FLine>* Variants[2] = {&DIAControlsLegend::OnFoot(), &DIAControlsLegend::InCar()};
	const TCHAR* VariantNames[2] = {TEXT("on foot"), TEXT("in the car")};
	for (int32 Variant = 0; Variant < 2; ++Variant)
	{
		const TArray<DIAControlsLegend::FLine>& Lines = *Variants[Variant];
		TestTrue(*FString::Printf(TEXT("%s: the legend has lines"), VariantNames[Variant]), Lines.Num() > 3);
		for (const DIAControlsLegend::FLine& Line : Lines)
		{
			TestFalse(TEXT("a line has an action text"), Line.Action.IsEmpty());
			TestTrue(TEXT("a line has a key cap"), Line.Caps.Num() > 0);
			for (const DIAControlsLegend::FCap& Cap : Line.Caps)
			{
				TestFalse(TEXT("a cap has a label"), Cap.Label.IsEmpty());
				TestTrue(TEXT("a cap stands for a key"), Cap.Bindings.Num() > 0);
				for (const DIAControlsLegend::FBinding& Binding : Cap.Bindings)
				{
					const UInputAction* Action = Binding.Action ? (Input.*Binding.Action).Get() : nullptr;
					if (TestNotNull(*FString::Printf(TEXT("%s: the action of %s"), VariantNames[Variant], *Binding.Key.ToString()), Action))
					{
						TestTrue(*FString::Printf(TEXT("%s: the key %s is bound to the action the legend names (%s)"), VariantNames[Variant], *Binding.Key.ToString(), *Action->GetName()),
							IsMapped(*Input.Context, Binding.Key, Action));
					}
				}
				// A cap of one key letter shows that letter.
				if (Cap.Bindings.Num() == 1 && Cap.Label.ToString().Len() == 1)
				{
					TestEqual(TEXT("a letter cap shows its key"), Cap.Label.ToString(), Cap.Bindings[0].Key.GetDisplayName().ToString());
				}
			}
		}
	}

	// The legend on foot misses no key: every mapping of the context is on one of its caps (so a new binding cannot be left out of the legend).
	for (const FEnhancedActionKeyMapping& Mapping : Input.Context->GetMappings())
	{
		bool bShown = false;
		for (const DIAControlsLegend::FLine& Line : DIAControlsLegend::OnFoot())
		{
			for (const DIAControlsLegend::FCap& Cap : Line.Caps)
			{
				for (const DIAControlsLegend::FBinding& Binding : Cap.Bindings)
				{
					bShown |= Binding.Key == Mapping.Key && (Input.*Binding.Action).Get() == Mapping.Action;
				}
			}
		}
		TestTrue(*FString::Printf(TEXT("the key %s is in the legend"), *Mapping.Key.ToString()), bShown);
	}

	// M is on the legend as the paver map, in both variants; the car's list is the shorter one and has no key that does nothing at the wheel.
	for (int32 Variant = 0; Variant < 2; ++Variant)
	{
		bool bMap = false;
		for (const DIAControlsLegend::FLine& Line : *Variants[Variant])
		{
			for (const DIAControlsLegend::FCap& Cap : Line.Caps)
			{
				bMap |= Cap.Label.ToString() == TEXT("M") && Line.Action.ToString() == TEXT("paver map");
			}
		}
		TestTrue(*FString::Printf(TEXT("%s: M is the paver map"), VariantNames[Variant]), bMap);
	}
	TestTrue(TEXT("the car's list is shorter"), DIAControlsLegend::InCar().Num() < DIAControlsLegend::OnFoot().Num());
	for (const DIAControlsLegend::FLine& Line : DIAControlsLegend::InCar())
	{
		for (const DIAControlsLegend::FCap& Cap : Line.Caps)
		{
			for (const DIAControlsLegend::FBinding& Binding : Cap.Bindings)
			{
				const UInputAction* Action = (Input.*Binding.Action).Get();
				TestTrue(TEXT("the car's keys are the ones that work at the wheel"),
					Action == Input.Move || Action == Input.Look || Action == Input.Use || Action == Input.Interact || Action == Input.PaverMap || Action == Input.Pause);
			}
		}
	}
	return true;
}

DIA_TEST(FControlsLegendVisibilityTest, "DoItAsphalt.Game.Hud.ControlsLegend.Visibility")

bool FControlsLegendVisibilityTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("HUD"), Hud) || !TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	// The HUD has no overlay without a window; this one is built the way the HUD builds it, and Slate polls its attributes once a frame (here: by hand).
	const TSharedRef<SDIAHudOverlay> Overlay = SNew(SDIAHudOverlay).Hud(Hud).Subsystem(Subsystem);
	auto Frame = [&Overlay]()
	{
		Overlay->Invalidate(EInvalidateWidgetReason::Prepass);
		Overlay->SlatePrepass(1.f);
	};
	auto LegendShown = [&]()
	{
		Frame();
		return Overlay->GetControlsLegend().IsValid() && Overlay->GetControlsLegend()->GetVisibility().IsVisible();
	};

	TestTrue(TEXT("the HUD shows the legend by default"), Hud->IsControlsLegendShown());
	TestTrue(TEXT("the legend widget is shown by default"), LegendShown());

	// The shop.
	Hud->OpenShop(ELotShop::Tools);
	TestFalse(TEXT("the HUD hides the legend while the shop is open"), Hud->IsControlsLegendShown());
	TestFalse(TEXT("the widget is hidden while the shop is open"), LegendShown());
	Hud->SetShopOpen(false);
	TestTrue(TEXT("the shop closes: the legend is back"), LegendShown());

	// The paver map.
	Hud->SetPaverMapOpen(true);
	TestTrue(TEXT("the map is open"), Hud->IsPaverMapOpen());
	TestFalse(TEXT("the HUD hides the legend while the map is open"), Hud->IsControlsLegendShown());
	TestFalse(TEXT("the widget is hidden while the map is open"), LegendShown());
	Hud->SetPaverMapOpen(false);
	TestTrue(TEXT("the map closes: the legend is back"), LegendShown());

	// A menu.
	Hud->OpenMenu(EDIAMenuMode::Pause);
	TestFalse(TEXT("the HUD hides the legend while a menu is open"), Hud->IsControlsLegendShown());
	TestFalse(TEXT("the widget is hidden while a menu is open"), LegendShown());
	Hud->CloseMenu();
	TestTrue(TEXT("the menu closes: the legend is back"), LegendShown());

	// On foot the legend shows the foot list; at the wheel the car's.
	Frame();
	TestTrue(TEXT("on foot: the foot list"), Overlay->GetLegendOnFoot()->GetVisibility().IsVisible());
	TestFalse(TEXT("on foot: not the car's list"), Overlay->GetLegendInCar()->GetVisibility().IsVisible());
	Subsystem->GetGame().World().bPlayerInCar = true;
	Frame();
	TestFalse(TEXT("in the car: not the foot list"), Overlay->GetLegendOnFoot()->GetVisibility().IsVisible());
	TestTrue(TEXT("in the car: the car's list"), Overlay->GetLegendInCar()->GetVisibility().IsVisible());
	TestTrue(TEXT("in the car the legend is still shown"), LegendShown());
	Subsystem->GetGame().World().bPlayerInCar = false;

	// The win screen has no way back, so it comes last.
	Hud->ShowWinScreen();
	TestTrue(TEXT("the win screen is up"), Hud->IsWinScreenShown());
	TestFalse(TEXT("the HUD hides the legend on the win screen"), Hud->IsControlsLegendShown());
	TestFalse(TEXT("the widget is hidden on the win screen"), LegendShown());
	return true;
}

DIA_TEST(FPaverMapKeyTextTest, "DoItAsphalt.Game.Hud.PaverMapKeyText")

bool FPaverMapKeyTextTest::RunTest(const FString& Parameters)
{
	// The map's legend names the key that closes it: M, not the old G.
	for (const int32 Pavers : {1, 2, 3})
	{
		const FString Legend = DIAPaverMap::LegendText(Pavers).ToString();
		TestTrue(*FString::Printf(TEXT("%d robots: the legend ends with M / Esc close"), Pavers), Legend.EndsWith(TEXT("M / Esc close")));
		TestFalse(TEXT("no G in the legend"), Legend.Contains(TEXT("G / Esc")));
	}
	TestTrue(TEXT("the toast of a bought robot names M"), DIAFormat::ItemBoughtToast(ELotItem::AutoPaver).ToString().Contains(TEXT("(M = map)")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
