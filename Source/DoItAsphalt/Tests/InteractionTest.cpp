// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIASaveGame.h"
#include "Game/DIAWorldSubsystem.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "Player/DIAInput.h"
#include "World/DIAInteractable.h"
#include "World/DIAPaverCar.h"
#include "World/DIAShopPoint.h"

namespace
{
	/** An interactable with a fixed prompt; the box is its collision, which the range is measured to. */
	struct FInteractionTestThing : public IDIAInteractable
	{
		FText Prompt;
		int32 Uses = 0;

		virtual FText GetPrompt(const ADIACharacter* By) const override { return Prompt; }
		virtual void Interact(ADIACharacter* By) override { ++Uses; }
	};

	/** Half the side of the collision box of the fake interactable (cm). */
	constexpr double InteractionTestHalfSideCm = 30.0;

	/** An actor with a blocking box of half side InteractionTestHalfSideCm at Where. */
	AActor* SpawnBoxActor(UWorld* World, FVector2D Where)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(Where.X, Where.Y, 0.0)), Params);
		if (!Actor)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Box"));
		Box->SetBoxExtent(FVector(InteractionTestHalfSideCm, InteractionTestHalfSideCm, 50.0));
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->SetRootComponent(Box);
		Box->RegisterComponent();
		// Level with the character's centre, so that the range is the horizontal distance to the box.
		Actor->SetActorLocation(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm));
		return Actor;
	}

	void PlaceAt(AActor* Actor, FVector2D Where)
	{
		Actor->SetActorLocation(FVector(Where.X, Where.Y, DIACharacter::CapsuleHalfHeightCm));
	}
}

DIA_TEST(FInteractionSearchTest, "DoItAsphalt.Game.InteractionSearch")

bool FInteractionSearchTest::RunTest(const FString& Parameters)
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
	const FVector2D ThingPos(-3000.0, 500.0);
	AActor* ThingActor = SpawnBoxActor(World, ThingPos);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, ThingPos + FVector2D(100.0, 0.0));
	if (!TestNotNull(TEXT("interactable actor"), ThingActor) || !TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	FInteractionTestThing Thing;
	Thing.Prompt = FText::FromString(TEXT("E - Use"));
	Subsystem->RegisterInteractable(ThingActor, &Thing);

	AActor* Found = nullptr;
	TestTrue(TEXT("found when close"), Subsystem->FindInteractable(Character, &Found) == &Thing && Found == ThingActor);

	// The range is measured to the collision box, not to the origin: 175 cm from the origin is 145 cm from the box, 190 cm is 160 cm.
	PlaceAt(Character, ThingPos + FVector2D(175.0, 0.0));
	TestTrue(TEXT("still in range 145 cm from the collision"), Subsystem->FindInteractable(Character) != nullptr);
	PlaceAt(Character, ThingPos + FVector2D(190.0, 0.0));
	TestTrue(TEXT("out of range 160 cm from the collision"), Subsystem->FindInteractable(Character) == nullptr);
	PlaceAt(Character, LotLayout::PlayerStart);
	TestTrue(TEXT("nothing at the player's start"), Subsystem->FindInteractable(Character) == nullptr);

	// An empty prompt means nothing to do right now.
	PlaceAt(Character, ThingPos + FVector2D(100.0, 0.0));
	Thing.Prompt = FText::GetEmpty();
	TestTrue(TEXT("an empty prompt is not offered"), Subsystem->FindInteractable(Character) == nullptr);
	Thing.Prompt = FText::FromString(TEXT("E - Use"));

	// The player controller uses the same search, but not while an input mode other than the game takes the keys.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params))
	{
		Controller->Possess(Character);
		TestTrue(TEXT("the controller finds it"), Controller->FindInteractable() == &Thing);
		Controller->SetMenuInput(true);
		TestTrue(TEXT("with the menu open nothing can be used"), Controller->FindInteractable() == nullptr);
		Controller->SetMenuInput(false);
		TestTrue(TEXT("with the menu closed again it is found"), Controller->FindInteractable() == &Thing);
	}
	else
	{
		AddError(TEXT("no controller"));
	}

	// An actor that goes away is forgotten.
	Subsystem->UnregisterInteractable(ThingActor);
	TestTrue(TEXT("an unregistered thing is not found"), Subsystem->FindInteractable(Character) == nullptr);
	return true;
}

DIA_TEST(FWinScreenShownTest, "DoItAsphalt.Game.WinScreenShown")

bool FWinScreenShownTest::RunTest(const FString& Parameters)
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
	ADIAPlayerController* Controller = World->SpawnActor<ADIAPlayerController>(Params);
	ADIACharacter* Character = DIATest::SpawnCharacter(World, LotLayout::PlayerStart);
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	Controller->Possess(Character);
	Params.Owner = Controller;
	ADIAHUD* Hud = World->SpawnActor<ADIAHUD>(Params);
	if (!TestNotNull(TEXT("HUD"), Hud))
	{
		return false;
	}
	AActor* ThingActor = SpawnBoxActor(World, LotLayout::PlayerStart + FVector2D(100.0, 0.0));
	FInteractionTestThing Thing;
	Thing.Prompt = FText::FromString(TEXT("E - Use"));
	if (TestNotNull(TEXT("interactable actor"), ThingActor))
	{
		Subsystem->RegisterInteractable(ThingActor, &Thing);
	}

	TestFalse(TEXT("no win screen at first"), Hud->IsWinScreenShown());
	TestFalse(TEXT("and the controller is in game input"), Controller->IsWinInput());
	TestTrue(TEXT("a thing can be used"), Controller->FindInteractable() == &Thing);

	// A win before the exit is open shows nothing.
	Subsystem->Win();
	TestFalse(TEXT("an early win shows nothing"), Hud->IsWinScreenShown());
	Subsystem->Dispatch(Subsystem->GetGame().DebugCompleteCells(AsphaltLot::NumCells));
	TestTrue(TEXT("the exit is open"), Subsystem->GetGame().IsExitOpen());
	Subsystem->Win();
	TestTrue(TEXT("the win shows the win screen"), Hud->IsWinScreenShown());
	TestTrue(TEXT("the controller now takes UI input"), Controller->IsWinInput());
	TestTrue(TEXT("nothing can be used"), Controller->FindInteractable() == nullptr);

	// The HUD going away gives the input back.
	Hud->Destroy();
	TestFalse(TEXT("the controller gave the win input back"), Controller->IsWinInput());
	return true;
}

namespace
{
	ELotShop ShopOfIndex(int32 I) { return static_cast<ELotShop>(I); }

	/** The test slot for the length of a test: the real slot is never touched, and the override is gone afterwards. */
	struct FShopPointTestSlot
	{
		static const FString& Name()
		{
			static const FString SlotName(TEXT("DIA_Test_ShopPoint"));
			return SlotName;
		}
		FShopPointTestSlot()
		{
			UDIAWorldSubsystem::SetSlotNameOverride(Name());
			Delete();
		}
		~FShopPointTestSlot()
		{
			Delete();
			UDIAWorldSubsystem::SetSlotNameOverride(FString());
		}
		static void Delete()
		{
			UGameplayStatics::DeleteGameInSlot(Name(), DIASave::UserIndex);
			UGameplayStatics::DeleteGameInSlot(Name() + DIASave::BackupSuffix, DIASave::UserIndex);
		}
	};

	/** A game world with a controller, its HUD and a character that it possesses, standing at the player's start. */
	struct FShopPointFixture
	{
		FShopPointTestSlot Slot;
		FTestWorldWrapper Wrapper;
		UWorld* World = nullptr;
		UDIAWorldSubsystem* Subsystem = nullptr;
		ADIAPlayerController* Controller = nullptr;
		ADIAHUD* Hud = nullptr;
		ADIACharacter* Character = nullptr;

		bool Start(FAutomationTestBase& Test)
		{
			if (!DIATest::StartGameWorld(Wrapper, Test))
			{
				return false;
			}
			World = Wrapper.GetTestWorld();
			Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
			if (!Test.TestNotNull(TEXT("world subsystem"), Subsystem))
			{
				return false;
			}
			Subsystem->SetAutosaveEnabled(false);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<ADIAPlayerController>(Params);
			Character = DIATest::SpawnCharacter(World, LotLayout::PlayerStart);
			if (!Test.TestNotNull(TEXT("controller"), Controller) || !Test.TestNotNull(TEXT("character"), Character))
			{
				return false;
			}
			Controller->Possess(Character);
			Params.Owner = Controller;
			Hud = World->SpawnActor<ADIAHUD>(Params);
			if (!Test.TestNotNull(TEXT("HUD"), Hud))
			{
				return false;
			}
			Controller->MyHUD = Hud;
			return true;
		}

		/** The player stands here (the capsule centre height; no sweep). */
		void Stand(FVector2D Where) const { PlaceAt(Character, Where); }

		ADIAShopPoint* PointOf(ELotShop Kind) const
		{
			for (TActorIterator<ADIAShopPoint> It(World); It; ++It)
			{
				if (It->Kind == Kind)
				{
					return *It;
				}
			}
			return nullptr;
		}
	};
}

DIA_TEST(FShopPointPromptAndOpenTest, "DoItAsphalt.Game.Interaction.ShopPoint.PromptAndOpen")

bool FShopPointPromptAndOpenTest::RunTest(const FString& Parameters)
{
	FShopPointFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	for (int32 I = 0; I < static_cast<int32>(ELotShop::Count); ++I)
	{
		const ELotShop Kind = ShopOfIndex(I);
		const ADIAShopPoint* Point = F.PointOf(Kind);
		if (!TestNotNull(TEXT("the game mode spawns a buy point"), Point))
		{
			continue;
		}
		F.Stand(LotLayout::ShopPoint(Kind));
		IDIAInteractable* Found = F.Controller->FindInteractable();
		if (!TestNotNull(TEXT("the point is found"), Found))
		{
			continue;
		}
		TestEqual(TEXT("the prompt names the building"), Found->GetPrompt(F.Character).ToString(), DIAFormat::ShopPrompt(Kind).ToString());
		TestTrue(TEXT("a prompt is not empty"), !DIAFormat::ShopPrompt(Kind).IsEmpty());
		TestFalse(TEXT("closed before E"), F.Hud->IsShopOpen());
		F.Controller->OnInteract();
		TestTrue(TEXT("E opens that building's panel"), F.Hud->GetOpenShop() == Kind);
		F.Hud->SetShopOpen(false);
	}
	TestEqual(TEXT("the prompts"), DIAFormat::ShopPrompt(ELotShop::Tools).ToString() + DIAFormat::ShopPrompt(ELotShop::Robots).ToString() + DIAFormat::ShopPrompt(ELotShop::Garage).ToString(),
		FString(TEXT("E – ToolsE – RobotsE – Garage")));
	return true;
}

DIA_TEST(FShopPointECloseTest, "DoItAsphalt.Game.Interaction.ShopPoint.ECloses")

bool FShopPointECloseTest::RunTest(const FString& Parameters)
{
	FShopPointFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Stand(LotLayout::ShopPoint(ELotShop::Robots));
	F.Controller->OnInteract();
	TestTrue(TEXT("opened"), F.Hud->GetOpenShop() == ELotShop::Robots);
	F.Controller->OnInteract();
	TestFalse(TEXT("the second E closes it"), F.Hud->IsShopOpen());
	TestFalse(TEXT("and gives the input back"), F.Controller->IsShopInput());
	F.Controller->OnInteract();
	TestTrue(TEXT("a third E opens it again"), F.Hud->GetOpenShop() == ELotShop::Robots);

	// Where another interactable would be found, E still closes the panel first.
	AActor* ThingActor = SpawnBoxActor(F.World, LotLayout::ShopPoint(ELotShop::Robots) + FVector2D(100.0, 0.0));
	FInteractionTestThing Thing;
	Thing.Prompt = FText::FromString(TEXT("E - Use"));
	if (TestNotNull(TEXT("another interactable"), ThingActor))
	{
		F.Subsystem->RegisterInteractable(ThingActor, &Thing);
		F.Controller->OnInteract();
		TestFalse(TEXT("E closes the panel"), F.Hud->IsShopOpen());
		TestEqual(TEXT("and uses nothing else"), Thing.Uses, 0);
		F.Subsystem->UnregisterInteractable(ThingActor);
	}
	return true;
}

DIA_TEST(FShopPointClosesWhenAwayTest, "DoItAsphalt.Game.Interaction.ShopPoint.ClosesWhenAway")

bool FShopPointClosesWhenAwayTest::RunTest(const FString& Parameters)
{
	FShopPointFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	const FVector2D Point = LotLayout::ShopPoint(ELotShop::Tools);
	F.Stand(Point);
	F.Controller->OnInteract();
	TestTrue(TEXT("opened"), F.Hud->GetOpenShop() == ELotShop::Tools);

	F.Stand(Point + FVector2D(DIAShopPoint::CloseRangeCm - 1.0, 0.0));
	F.Controller->CloseShopWhenAway();
	TestTrue(TEXT("299 cm away it stays open"), F.Hud->IsShopOpen());
	// A jump does not count: the distance is on the ground plane.
	F.Character->SetActorLocation(F.Character->GetActorLocation() + FVector(0.0, 0.0, 400.0));
	F.Controller->CloseShopWhenAway();
	TestTrue(TEXT("height does not close it"), F.Hud->IsShopOpen());

	F.Stand(Point + FVector2D(0.0, DIAShopPoint::CloseRangeCm + 1.0));
	F.Controller->CloseShopWhenAway();
	TestFalse(TEXT("301 cm away the panel closes"), F.Hud->IsShopOpen());
	TestFalse(TEXT("the input is the game's again"), F.Controller->IsShopInput());
	TestFalse(TEXT("not a menu either"), F.Controller->IsMenuInput());

	// The distance is to the building that is open, not to the nearest one.
	F.Stand(LotLayout::ShopPoint(ELotShop::Tools));
	F.Hud->OpenShop(ELotShop::Garage);
	F.Controller->CloseShopWhenAway();
	TestFalse(TEXT("standing at another building's point closes the panel of the open one"), F.Hud->IsShopOpen());
	return true;
}

DIA_TEST(FShopPointClosesOnResetTest, "DoItAsphalt.Game.Interaction.ShopPoint.ClosesOnReset")

bool FShopPointClosesOnResetTest::RunTest(const FString& Parameters)
{
	FShopPointFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	F.Stand(LotLayout::ShopPoint(ELotShop::Garage));
	F.Hud->OpenShop(ELotShop::Garage);
	TestTrue(TEXT("open"), F.Hud->IsShopOpen() && F.Controller->IsShopInput());
	F.Subsystem->NewGame();
	TestFalse(TEXT("a new game closes the panel"), F.Hud->IsShopOpen());
	TestFalse(TEXT("and the input is the game's"), F.Controller->IsShopInput());

	F.Subsystem->SaveGame();
	F.Hud->OpenShop(ELotShop::Robots);
	TestTrue(TEXT("open again"), F.Hud->IsShopOpen() && F.Controller->IsShopInput());
	if (TestTrue(TEXT("the save loads"), F.Subsystem->LoadGame()))
	{
		TestFalse(TEXT("a loaded game closes the panel"), F.Hud->IsShopOpen());
		TestFalse(TEXT("and the input is the game's"), F.Controller->IsShopInput());
	}
	return true;
}

DIA_TEST(FShopPointGarageWithT4Test, "DoItAsphalt.Game.Interaction.ShopPoint.GarageWithT4")

bool FShopPointGarageWithT4Test::RunTest(const FString& Parameters)
{
	FShopPointFixture F;
	if (!F.Start(*this))
	{
		return false;
	}
	for (int32 Tier = 0; Tier <= 3; ++Tier)
	{
		// As a purchase does it: the car appears (and is replaced by the next tier) on OnItemBought.
		const ELotItem Item = static_cast<ELotItem>(static_cast<int32>(ELotItem::Car) + Tier);
		F.Subsystem->GetGame().DebugGrantItem(Item);
		F.Subsystem->OnItemBought.Broadcast(Item);
		F.Subsystem->OnStateChanged.Broadcast();
	}
	ADIAPaverCar* Car = nullptr;
	for (TActorIterator<ADIAPaverCar> It(F.World); It; ++It)
	{
		Car = *It;
	}
	if (!TestNotNull(TEXT("the car"), Car))
	{
		return false;
	}
	TestEqual(TEXT("the biggest car stands in the bay"), Car->GetShownLevel(), 3);
	TestTrue(TEXT("on the bay"), FVector2D(Car->GetActorLocation()).Equals(LotLayout::ShopPos(ELotShop::Garage), 0.5));

	// The player's capsule at the buy point does not touch the car.
	const FVector2D Point = LotLayout::ShopPoint(ELotShop::Garage);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShopPointOverlap), false);
	F.World->OverlapMultiByChannel(Overlaps, FVector(Point.X, Point.Y, DIACharacter::CapsuleHalfHeightCm), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm), QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		TestTrue(TEXT("the capsule overlaps nothing but the character itself"), Overlap.GetActor() != Car);
	}

	F.Stand(Point);
	IDIAInteractable* Found = F.Controller->FindInteractable();
	const ADIAShopPoint* GaragePoint = F.PointOf(ELotShop::Garage);
	TestTrue(TEXT("E finds the buy point, not the car"), GaragePoint && Found == static_cast<const IDIAInteractable*>(GaragePoint));
	return true;
}

DIA_TEST(FShopPointTabUnmappedTest, "DoItAsphalt.Game.Interaction.ShopPoint.TabUnmapped")

bool FShopPointTabUnmappedTest::RunTest(const FString& Parameters)
{
	const FDIAInput Input = FDIAInput::Create(GetTransientPackage());
	if (!TestNotNull(TEXT("the context"), Input.Context.Get()))
	{
		return false;
	}
	int32 Mappings = 0;
	for (const FEnhancedActionKeyMapping& Mapping : Input.Context->GetMappings())
	{
		++Mappings;
		TestTrue(TEXT("no key is Tab"), Mapping.Key != EKeys::Tab);
	}
	TestTrue(TEXT("the context has its mappings"), Mappings > 10);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
