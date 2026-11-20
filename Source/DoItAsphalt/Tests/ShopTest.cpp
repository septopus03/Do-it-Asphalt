// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltBalance.h"
#include "AsphaltGame.h"
#include "AsphaltTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DIACharacter.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "World/DIAPlantActor.h"

namespace
{
	/** The model (or the placeholder) of a part: a real mesh is told by its name, a placeholder by its scale. */
	bool ShowsMesh(const UStaticMeshComponent* Part, const TCHAR* ModelName)
	{
		return Part && Part->GetStaticMesh() && Part->GetStaticMesh()->GetName() == ModelName;
	}
}

DIA_TEST(FBuyRefreshesEquipmentTest, "DoItAsphalt.Game.BuyRefreshesEquipment")

bool FBuyRefreshesEquipmentTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIACharacter* Character = World->SpawnActor<ADIACharacter>(FVector(-1200.0, 0.0, DIACharacter::CapsuleHalfHeightCm), FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Wrapper.TickTestWorld(0.1f);

	Subsystem->GetGame().DebugGive(1000.0, 100);
	TestTrue(TEXT("starts with the 20 cm spreader"), ShowsMesh(Character->GetSpreaderMesh(), TEXT("SM_Spreader_020")) ||
		Character->GetSpreaderMesh()->GetRelativeScale3D().Equals(FVector(0.08, 0.2, 0.08), 0.001));

	// Buying alone (no tick in between) refreshes the equipment: Buy broadcasts OnStateChanged.
	for (int32 I = 0; I < 3; ++I)
	{
		TestTrue(TEXT("tool level is bought"), Subsystem->Buy(EAsphaltUpgrade::Tool));
	}
	TestEqual(TEXT("tool level 3"), Subsystem->GetGame().GetLevel(EAsphaltUpgrade::Tool), 3);
	const UStaticMeshComponent* Spreader = Character->GetSpreaderMesh();
	const bool bSpreader80 = ShowsMesh(Spreader, TEXT("SM_Spreader_080")) || (Spreader && Spreader->GetRelativeScale3D().Equals(FVector(0.08, 0.8, 0.08), 0.001));
	TestTrue(TEXT("the spreader is the 80 cm one"), bSpreader80);

	TestEqual(TEXT("backpack tier 0 before"), AsphaltBalance::BackpackTier(Subsystem->GetGame().GetLevel(EAsphaltUpgrade::Backpack)), 0);
	const UStaticMeshComponent* SmallBackpack = Character->GetBackpackMesh();
	for (int32 I = 0; I < 3; ++I)
	{
		TestTrue(TEXT("backpack level is bought"), Subsystem->Buy(EAsphaltUpgrade::Backpack));
	}
	TestEqual(TEXT("backpack tier 1"), AsphaltBalance::BackpackTier(Subsystem->GetGame().GetLevel(EAsphaltUpgrade::Backpack)), 1);
	const UStaticMeshComponent* Backpack = Character->GetBackpackMesh();
	TestTrue(TEXT("the backpack was swapped"), Backpack != nullptr && Backpack != SmallBackpack);
	const bool bBackpackM = ShowsMesh(Backpack, TEXT("SM_Backpack_M")) || (Backpack && Backpack->GetRelativeScale3D().Equals(DIAModelSizes::BackpackCm[1] / 100.0, 0.001));
	TestTrue(TEXT("the backpack is the M one"), bBackpackM);
	return true;
}

DIA_TEST(FShopModelTest, "DoItAsphalt.Game.ShopModel")

bool FShopModelTest::RunTest(const FString& Parameters)
{
	auto Is = [this](const TCHAR* What, const FText& Actual, const TCHAR* Expected)
	{
		TestEqual(What, Actual.ToString(), FString(Expected));
	};

	FAsphaltGame Game;
	Game.DebugGive(30.0, 3);
	FDIAShopModel Model;
	TestTrue(TEXT("the first update builds the rows"), Model.Update(Game));
	if (!TestEqual(TEXT("three rows"), FDIAShopModel::NumRows, 3))
	{
		return false;
	}

	// Order: Backpack, Sprayer, Plant.
	TestTrue(TEXT("row 0 is the backpack"), Model.GetRow(0).Upgrade == EAsphaltUpgrade::Backpack);
	TestTrue(TEXT("row 1 is the sprayer"), Model.GetRow(1).Upgrade == EAsphaltUpgrade::Tool);
	TestTrue(TEXT("row 2 is the plant"), Model.GetRow(2).Upgrade == EAsphaltUpgrade::Plant);
	Is(TEXT("name 0"), Model.GetRow(0).Name, TEXT("Backpack"));
	Is(TEXT("name 1"), Model.GetRow(1).Name, TEXT("Sprayer"));
	Is(TEXT("name 2"), Model.GetRow(2).Name, TEXT("Plant"));

	// Current value -> next value, from the balance tables.
	Is(TEXT("change 0"), Model.GetRow(0).Change, TEXT("5 L → 7 L"));
	Is(TEXT("change 1"), Model.GetRow(1).Change, TEXT("0.2 m → 0.3 m"));
	Is(TEXT("change 2"), Model.GetRow(2).Change, TEXT("15 L/min → 20 L/min"));
	Is(TEXT("price 0 is in shards"), Model.GetRow(0).Price, TEXT("1 shard"));
	Is(TEXT("price 1"), Model.GetRow(1).Price, TEXT("25"));
	Is(TEXT("price 2"), Model.GetRow(2).Price, TEXT("30"));
	for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
	{
		TestTrue(TEXT("affordable: the button is active"), Model.GetRow(I).bCanBuy);
		TestFalse(TEXT("not at the maximum"), Model.GetRow(I).bMax);
	}

	// Money that grows without crossing a price changes nothing shown: no rebuild.
	const int32 Rebuilds = Model.GetRebuildCount();
	Game.DebugGive(1.0, 0);
	TestFalse(TEXT("more money, same buttons: no rebuild"), Model.Update(Game));
	TestEqual(TEXT("rebuild count is unchanged"), Model.GetRebuildCount(), Rebuilds);

	// Too poor: the plant (30) and the sprayer (25) cannot be bought with 10 money, the backpack not without shards.
	FAsphaltGame Poor;
	Poor.DebugGive(10.0, 0);
	FDIAShopModel PoorModel;
	PoorModel.Update(Poor);
	for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
	{
		TestFalse(TEXT("too poor: the button is inactive"), PoorModel.GetRow(I).bCanBuy);
	}
	Poor.DebugGive(20.0, 0);
	TestTrue(TEXT("30 money: the buttons change"), PoorModel.Update(Poor));
	TestFalse(TEXT("backpack still needs a shard"), PoorModel.GetRow(0).bCanBuy);
	TestTrue(TEXT("sprayer is affordable at 25"), PoorModel.GetRow(1).bCanBuy);
	TestTrue(TEXT("plant is affordable at 30"), PoorModel.GetRow(2).bCanBuy);

	// Buying moves the row on to the next level.
	TestTrue(TEXT("buy the backpack"), Game.Buy(EAsphaltUpgrade::Backpack));
	TestTrue(TEXT("a purchase changes the rows"), Model.Update(Game));
	Is(TEXT("change after a purchase"), Model.GetRow(0).Change, TEXT("7 L → 10 L"));
	Is(TEXT("price after a purchase"), Model.GetRow(0).Price, TEXT("2 shards"));

	// The maximum: only the value, MAX instead of the price, no button.
	FAsphaltGame Maxed;
	Maxed.DebugGive(100000.0, 1000);
	Maxed.DebugSetLevel(EAsphaltUpgrade::Backpack, 12);
	Maxed.DebugSetLevel(EAsphaltUpgrade::Tool, 6);
	Maxed.DebugSetLevel(EAsphaltUpgrade::Plant, 12);
	FDIAShopModel MaxModel;
	MaxModel.Update(Maxed);
	Is(TEXT("max change 0"), MaxModel.GetRow(0).Change, TEXT("200 L"));
	Is(TEXT("max change 1"), MaxModel.GetRow(1).Change, TEXT("2.0 m"));
	Is(TEXT("max change 2"), MaxModel.GetRow(2).Change, TEXT("1,000 L/min"));
	for (int32 I = 0; I < FDIAShopModel::NumRows; ++I)
	{
		Is(TEXT("max price"), MaxModel.GetRow(I).Price, TEXT("MAX"));
		TestTrue(TEXT("max flag"), MaxModel.GetRow(I).bMax);
		TestFalse(TEXT("max: the button is inactive even with plenty of money"), MaxModel.GetRow(I).bCanBuy);
		TestFalse(TEXT("max: no arrow"), MaxModel.GetRow(I).Change.ToString().Contains(TEXT("→")));
	}

	// Single values of the three upgrades.
	Is(TEXT("backpack value"), DIAFormat::UpgradeValue(EAsphaltUpgrade::Backpack, 5), TEXT("30 L"));
	Is(TEXT("tool value"), DIAFormat::UpgradeValue(EAsphaltUpgrade::Tool, 4), TEXT("1.0 m"));
	Is(TEXT("plant value"), DIAFormat::UpgradeValue(EAsphaltUpgrade::Plant, 9), TEXT("300 L/min"));
	return true;
}

DIA_TEST(FShopToggleTest, "DoItAsphalt.Game.ShopToggle")

bool FShopToggleTest::RunTest(const FString& Parameters)
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

	TestFalse(TEXT("the shop starts closed"), Hud->IsShopOpen());
	Hud->ToggleShop();
	TestTrue(TEXT("Tab opens the shop"), Hud->IsShopOpen());

	// The game runs on while the shop is open: the plant keeps producing.
	const double Stock = Subsystem->GetGame().GetPlantStock();
	for (int32 I = 0; I < 10; ++I)
	{
		Wrapper.TickTestWorld(0.1f);
	}
	TestTrue(TEXT("the plant produces while the shop is open"), Subsystem->GetGame().GetPlantStock() > Stock);
	TestTrue(TEXT("still open"), Hud->IsShopOpen());

	Hud->ToggleShop();
	TestFalse(TEXT("Tab closes the shop"), Hud->IsShopOpen());
	Hud->SetShopOpen(true);
	Hud->SetShopOpen(true);
	TestTrue(TEXT("SetShopOpen(true) is idempotent"), Hud->IsShopOpen());
	Hud->SetShopOpen(false);
	TestFalse(TEXT("SetShopOpen(false) closes"), Hud->IsShopOpen());
	return true;
}

DIA_TEST(FPlantAddonsTest, "DoItAsphalt.Game.PlantAddons")

bool FPlantAddonsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	ADIAPlantActor* Plant = nullptr;
	for (TActorIterator<ADIAPlantActor> It(World); It; ++It)
	{
		Plant = *It;
	}
	if (!TestNotNull(TEXT("plant"), Plant) || !TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FAsphaltGame& Game = Subsystem->GetGame();
	Wrapper.TickTestWorld(0.1f);

	// The addons stand at fixed spots next to the body, but only from plant level 5 (A) and 10 (B).
	TestFalse(TEXT("no addon A at level 0"), Plant->GetAddonA()->IsVisible());
	TestFalse(TEXT("no addon B at level 0"), Plant->GetAddonB()->IsVisible());
	TestTrue(TEXT("addon A is at (-150, 280, 0)"), Plant->GetAddonA()->GetRelativeLocation().Equals(FVector(-150.0, 280.0, 0.0), 0.01));
	TestTrue(TEXT("addon B is at (-150, -290, 0)"), Plant->GetAddonB()->GetRelativeLocation().Equals(FVector(-150.0, -290.0, 0.0), 0.01));

	Game.DebugSetLevel(EAsphaltUpgrade::Plant, 4);
	Wrapper.TickTestWorld(0.1f);
	TestFalse(TEXT("no addon A at level 4"), Plant->GetAddonA()->IsVisible());

	Game.DebugSetLevel(EAsphaltUpgrade::Plant, 5);
	Wrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("addon A from level 5"), Plant->GetAddonA()->IsVisible());
	TestFalse(TEXT("no addon B at level 5"), Plant->GetAddonB()->IsVisible());

	Game.DebugSetLevel(EAsphaltUpgrade::Plant, 9);
	Wrapper.TickTestWorld(0.1f);
	TestFalse(TEXT("no addon B at level 9"), Plant->GetAddonB()->IsVisible());

	Game.DebugSetLevel(EAsphaltUpgrade::Plant, 10);
	Wrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("addon A at level 10"), Plant->GetAddonA()->IsVisible());
	TestTrue(TEXT("addon B from level 10"), Plant->GetAddonB()->IsVisible());

	// A visible addon is solid for pawns but never blocks the camera; a hidden one is not there at all.
	TestTrue(TEXT("addon A is solid"), Plant->GetAddonA()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	TestEqual(TEXT("addon A ignores the camera"), static_cast<int32>(Plant->GetAddonA()->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));

	// Buying moves it along: level 4 -> 5 through the subsystem shows addon A without a tick in between.
	Game.DebugSetLevel(EAsphaltUpgrade::Plant, 4);
	Wrapper.TickTestWorld(0.1f);
	Game.DebugGive(1000.0, 0);
	TestTrue(TEXT("buy the plant level"), Subsystem->Buy(EAsphaltUpgrade::Plant));
	TestTrue(TEXT("addon A appears at once"), Plant->GetAddonA()->IsVisible());
	TestEqual(TEXT("no pawn blocking for a hidden addon"), static_cast<int32>(Plant->GetAddonB()->GetCollisionEnabled()), static_cast<int32>(ECollisionEnabled::NoCollision));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
