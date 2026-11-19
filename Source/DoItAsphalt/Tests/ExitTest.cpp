// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "HexMap.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "UI/DIAHUD.h"
#include "World/DIAExitGate.h"
#include "World/DIAPaverCar.h"
#include "World/DIASiteFence.h"

namespace
{
	constexpr float ExitTickSeconds = 0.1f;
	/** The arm takes 1.5 s to rise; two seconds is safely after it. */
	constexpr int32 ExitTicksAfterOpening = 20;
	constexpr int32 ExitTicksHalfway = 8;
	constexpr int32 ExitNumDirections = 72;
	constexpr double ExitDirectionStepDeg = 5.0;
	constexpr double ExitSweepLengthCm = 4000.0;
	constexpr double ExitFurthestHitCm = 2700.0;
	/** The sweep's capsule is lifted a little, so that the ground does not count as an obstacle. */
	constexpr double ExitSweepLiftCm = 2.0;
	constexpr double ExitDistanceToleranceCm = 0.5;
	constexpr int32 ExitDaySeconds = 3725;

	void ExitCompleteEverything(UDIAWorldSubsystem* Subsystem)
	{
		FAsphaltGame& Game = Subsystem->GetGame();
		for (int32 Cell = 0; Cell < Game.GetMap().Num(); ++Cell)
		{
			if (!Game.GetGrid().IsCellComplete(Cell))
			{
				Subsystem->Dispatch(Game.DebugCompleteCell(Cell));
			}
		}
	}

	void ExitTickFor(FTestWorldWrapper& Wrapper, int32 Ticks)
	{
		for (int32 I = 0; I < Ticks; ++I)
		{
			Wrapper.TickTestWorld(ExitTickSeconds);
		}
	}

	template <typename T>
	TArray<T*> ExitFind(UWorld* World)
	{
		TArray<T*> Result;
		for (TActorIterator<T> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		return Result;
	}

	/** Sweeps the player's capsule from the middle of the lot along Dir; true on a blocking hit (the distance travelled is returned). */
	bool ExitSweep(UWorld* World, FVector2D Dir, double& OutDistance, AActor** OutActor = nullptr)
	{
		const FVector Start(0.0, 0.0, DIACharacter::CapsuleHalfHeightCm + ExitSweepLiftCm);
		const FVector End = Start + FVector(Dir.X, Dir.Y, 0.0) * ExitSweepLengthCm;
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(DIACharacter::CapsuleRadiusCm, DIACharacter::CapsuleHalfHeightCm));
		OutDistance = bHit ? static_cast<double>(Hit.Distance) : ExitSweepLengthCm;
		if (OutActor)
		{
			*OutActor = bHit ? Hit.GetActor() : nullptr;
		}
		return bHit;
	}

	FVector2D ExitPlace(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}

	FVector2D ExitPlace(const USceneComponent* Component)
	{
		return FVector2D(Component->GetComponentLocation().X, Component->GetComponentLocation().Y);
	}
}

DIA_TEST(FGateOpensAndWinsTest, "DoItAsphalt.Game.GateOpensAndWins")

bool FGateOpensAndWinsTest::RunTest(const FString& Parameters)
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
	FAsphaltGame& Game = Subsystem->GetGame();
	const TArray<ADIAExitGate*> Gates = ExitFind<ADIAExitGate>(World);
	if (!TestEqual(TEXT("exactly one exit gate"), Gates.Num(), 1))
	{
		return false;
	}
	ADIAExitGate* Gate = Gates[0];
	const FVector2D Direction = FHexMap::RoadDirection(AsphaltBalance::ExitRoad);

	TestTrue(TEXT("the gate stands at GatePos of the exit road"), ExitPlace(Gate).Equals(AsphaltLayout::GatePos(AsphaltBalance::ExitRoad), 0.1));
	TestTrue(TEXT("it stands across the road"), FVector2D(Gate->GetActorForwardVector()).Equals(Direction, 0.001));
	TestTrue(TEXT("the trigger is centred at ExitTrigger"), FVector2D(Gate->GetTrigger()->GetComponentLocation()).Equals(AsphaltLayout::ExitTrigger(), 0.1));
	TestEqual(TEXT("the trigger is 150 cm"), Gate->GetTrigger()->GetScaledSphereRadius(), 150.0f);

	// Closed: the arm blocks the way out.
	TestFalse(TEXT("the gate starts closed"), Gate->IsOpen());
	TestEqual(TEXT("the arm is down"), Gate->GetOpenFraction(), 0.0);
	TestTrue(TEXT("the arm blocks pawns"), Gate->GetArmCollision()->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	double Distance = 0.0;
	AActor* Hit = nullptr;
	TestTrue(TEXT("a pawn walking out along the exit road is stopped"), ExitSweep(World, Direction, Distance, &Hit));
	TestTrue(TEXT("by the gate"), Hit == Gate);
	TestTrue(TEXT("in the fence line"), Distance <= ExitFurthestHitCm && Distance > AsphaltLayout::SiteRadiusCm - 200.0);

	int32 Won = 0;
	Subsystem->OnWon.AddLambda([&Won]() { ++Won; });

	// Winning is only possible once every cell is done.
	Subsystem->Win();
	TestFalse(TEXT("Win does nothing while the exit is closed"), Game.HasWon());
	TestEqual(TEXT("and announces nothing"), Won, 0);

	// All 499 cells: the exit opens and the arm rises for 1.5 s.
	ExitCompleteEverything(Subsystem);
	TestTrue(TEXT("the exit is open"), Game.IsExitOpen());
	ExitTickFor(Wrapper, ExitTicksHalfway);
	TestTrue(TEXT("halfway the arm is rising"), Gate->GetOpenFraction() > 0.0 && Gate->GetOpenFraction() < 1.0);
	TestFalse(TEXT("but not up yet"), Gate->IsOpen());
	ExitTickFor(Wrapper, ExitTicksAfterOpening);
	TestTrue(TEXT("two seconds later the gate is open"), Gate->IsOpen());
	TestTrue(TEXT("the arm no longer blocks"), Gate->GetArmCollision()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("a pawn walks through the open gate"), ExitSweep(World, Direction, Distance));
	// The arm really is up: its tip is far above where it was.
	const FBox Arm = Gate->GetArmCollision()->Bounds.GetBox();
	TestTrue(TEXT("the raised arm stands (nearly) upright"), Arm.Max.Z - Arm.Min.Z > DIAModelSizes::ExitGateBarrierCm.Y * 0.9);
	TestTrue(TEXT("and it rose, it did not sink into the ground"), Arm.Min.Z > DIAExitGate::ArmHeightCm - DIAModelSizes::ExitGateBarrierCm.Z);

	// Winning: the character stepping into the trigger wins, once.
	ADIACharacter* Character = DIATest::SpawnCharacter(World, AsphaltLayout::PlayerStart);
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	TestFalse(TEXT("not won before reaching the trigger"), Game.HasWon());
	Character->SetActorLocation(FVector(AsphaltLayout::ExitTrigger().X, AsphaltLayout::ExitTrigger().Y, DIACharacter::CapsuleHalfHeightCm));
	TestTrue(TEXT("the character in the trigger wins"), Game.HasWon());
	TestEqual(TEXT("OnWon fired once"), Won, 1);
	Character->SetActorLocation(FVector(AsphaltLayout::PlayerStart.X, AsphaltLayout::PlayerStart.Y, DIACharacter::CapsuleHalfHeightCm));
	Character->SetActorLocation(FVector(AsphaltLayout::ExitTrigger().X, AsphaltLayout::ExitTrigger().Y, DIACharacter::CapsuleHalfHeightCm));
	Subsystem->Win();
	TestEqual(TEXT("leaving and entering again announces nothing more"), Won, 1);
	return true;
}

DIA_TEST(FGateWinsFromCarTest, "DoItAsphalt.Game.GateWinsFromCar")

bool FGateWinsFromCarTest::RunTest(const FString& Parameters)
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
	FAsphaltGame& Game = Subsystem->GetGame();
	Game.DebugGrantSpecial(EAsphaltSpecial::PaverCar);
	Subsystem->OnSpecialBought.Broadcast(EAsphaltSpecial::PaverCar);
	const TArray<ADIAPaverCar*> Cars = ExitFind<ADIAPaverCar>(World);
	if (!TestEqual(TEXT("one truck"), Cars.Num(), 1))
	{
		return false;
	}
	ADIAPaverCar* Car = Cars[0];
	int32 Won = 0;
	Subsystem->OnWon.AddLambda([&Won]() { ++Won; });

	// The truck in front of the gate while the exit is still closed changes nothing.
	const FVector2D Trigger = AsphaltLayout::ExitTrigger();
	Car->SetActorLocation(FVector(Trigger.X, Trigger.Y, DIAPaverCar::OriginHeightCm()));
	TestFalse(TEXT("not won with the exit closed"), Game.HasWon());

	ExitCompleteEverything(Subsystem);
	Car->SetActorLocation(FVector(AsphaltLayout::PlayerStart.X, AsphaltLayout::PlayerStart.Y, DIAPaverCar::OriginHeightCm()));
	TestFalse(TEXT("still not won"), Game.HasWon());
	Car->SetActorLocation(FVector(Trigger.X, Trigger.Y, DIAPaverCar::OriginHeightCm()));
	TestTrue(TEXT("the truck in the trigger wins"), Game.HasWon());
	TestEqual(TEXT("OnWon fired once"), Won, 1);
	return true;
}

DIA_TEST(FGateOpenAtLoadTest, "DoItAsphalt.Game.GateOpenAtLoad")

bool FGateOpenAtLoadTest::RunTest(const FString& Parameters)
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
	ExitCompleteEverything(Subsystem);

	// A gate that comes into a game whose exit is already open stands open at once, without a single frame passing.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADIAExitGate* Late = World->SpawnActor<ADIAExitGate>(Params);
	if (!TestNotNull(TEXT("a late gate"), Late))
	{
		return false;
	}
	TestTrue(TEXT("it is open immediately"), Late->IsOpen());
	TestEqual(TEXT("fully raised"), Late->GetOpenFraction(), 1.0);
	TestTrue(TEXT("and does not block"), Late->GetArmCollision()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	return true;
}

DIA_TEST(FSiteIsClosedTest, "DoItAsphalt.Game.SiteIsClosed")

bool FSiteIsClosedTest::RunTest(const FString& Parameters)
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

	// The six directions of the roads are among the 72 (every 5 degrees); they end at a barrier or the closed arm.
	TArray<int32> RoadSteps;
	for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
	{
		const FVector2D Dir = FHexMap::RoadDirection(Road);
		const double Step = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) / ExitDirectionStepDeg;
		const int32 Rounded = FMath::RoundToInt(Step);
		TestEqual(TEXT("a road leaves in one of the 72 directions"), Step, static_cast<double>(Rounded), 1e-6);
		RoadSteps.Add(((Rounded % ExitNumDirections) + ExitNumDirections) % ExitNumDirections);
	}
	TestEqual(TEXT("six different road directions"), TSet<int32>(RoadSteps).Num(), FHexMap::NumRoads);

	auto Direction = [](int32 Step)
	{
		const double Angle = FMath::DegreesToRadians(Step * ExitDirectionStepDeg);
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	};

	int32 Blocked = 0;
	for (int32 Step = 0; Step < ExitNumDirections; ++Step)
	{
		double Distance = 0.0;
		AActor* Hit = nullptr;
		if (ExitSweep(World, Direction(Step), Distance, &Hit))
		{
			++Blocked;
			TestTrue(*FString::Printf(TEXT("direction %d stops within 2700 cm"), Step), Distance <= ExitFurthestHitCm);
		}
		else
		{
			AddError(FString::Printf(TEXT("direction %d (%.0f deg) leads out of the site"), Step, Step * ExitDirectionStepDeg));
		}
	}
	TestEqual(TEXT("all 72 directions are closed"), Blocked, ExitNumDirections);

	// Once the exit is open the way along road 5 is free, and the other 71 are still closed.
	ExitCompleteEverything(Subsystem);
	ExitTickFor(Wrapper, ExitTicksAfterOpening);
	double Distance = 0.0;
	TestFalse(TEXT("the open gate lets the road-5 sweep through"), ExitSweep(World, FHexMap::RoadDirection(AsphaltBalance::ExitRoad), Distance));
	int32 StillClosed = 0;
	for (int32 Step = 0; Step < ExitNumDirections; ++Step)
	{
		if (Step == RoadSteps[AsphaltBalance::ExitRoad])
		{
			continue;
		}
		StillClosed += ExitSweep(World, Direction(Step), Distance) ? 1 : 0;
	}
	TestEqual(TEXT("the other 71 directions stay closed"), StillClosed, ExitNumDirections - 1);
	return true;
}

DIA_TEST(FSiteFenceBuildsTest, "DoItAsphalt.Game.SiteFenceBuilds")

bool FSiteFenceBuildsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	const TArray<ADIASiteFence*> Fences = ExitFind<ADIASiteFence>(World);
	if (!TestEqual(TEXT("exactly one fence"), Fences.Num(), 1) || !TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	Mode->EnsureSiteFence();
	TestEqual(TEXT("EnsureSiteFence does not spawn a second one"), ExitFind<ADIASiteFence>(World).Num(), 1);
	Mode->EnsureExitGate();
	TestEqual(TEXT("EnsureExitGate does not spawn a second gate"), ExitFind<ADIAExitGate>(World).Num(), 1);

	const ADIASiteFence* Fence = Fences[0];
	TestTrue(TEXT("there are fence panels (about 70)"), Fence->GetNumPanels() >= 60 && Fence->GetNumPanels() <= 80);
	TestTrue(TEXT("there are wall segments"), Fence->GetNumWallSegments() >= 60 && Fence->GetNumWallSegments() <= DIASite::WallSlots);
	for (int32 I = 0; I < Fence->GetNumPanels(); ++I)
	{
		const FVector2D Where = ExitPlace(Fence->GetPanel(I));
		TestEqual(TEXT("a panel stands on the fence circle"), Where.Size(), AsphaltLayout::SiteRadiusCm, ExitDistanceToleranceCm);
	}
	// No panel and no wall in the gap of a road: the gap is 380 cm wide around the road's axis.
	for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
	{
		const FVector2D Dir = FHexMap::RoadDirection(Road);
		const FVector2D Gate = AsphaltLayout::GatePos(Road);
		for (int32 I = 0; I < Fence->GetNumWallSegments(); ++I)
		{
			const FVector2D Centre = ExitPlace(Fence->GetWallSegment(I));
			const double Along = FVector2D::DotProduct(Centre - Gate, FVector2D(-Dir.Y, Dir.X));
			const bool bNear = FVector2D::Distance(Centre, Gate) < DIASite::GapWidthCm;
			if (bNear)
			{
				TestTrue(TEXT("a wall segment near a gate stays out of its gap"), FMath::Abs(Along) >= DIASite::GapWidthCm * 0.5 - 1.0);
			}
		}
	}
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
	ADIACharacter* Character = DIATest::SpawnCharacter(World, AsphaltLayout::PlayerStart);
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

	TestFalse(TEXT("no win screen at first"), Hud->IsWinScreenShown());
	TestFalse(TEXT("and the controller is in game input"), Controller->IsWinInput());

	// The shop is open when the win comes: it closes, and cannot be opened again.
	Hud->SetShopOpen(true);
	TestTrue(TEXT("the shop is open"), Hud->IsShopOpen());
	Subsystem->Win();
	TestFalse(TEXT("an early win shows nothing"), Hud->IsWinScreenShown());
	ExitCompleteEverything(Subsystem);
	Subsystem->Win();
	TestTrue(TEXT("the win shows the win screen"), Hud->IsWinScreenShown());
	TestFalse(TEXT("the shop is closed"), Hud->IsShopOpen());
	TestTrue(TEXT("the controller now takes UI input"), Controller->IsWinInput());
	Hud->ToggleShop();
	TestFalse(TEXT("the shop cannot be opened over the win screen"), Hud->IsShopOpen());
	Controller->OnPourStarted();
	TestFalse(TEXT("pouring is off"), Character->IsPouring());
	TestTrue(TEXT("nothing can be used"), Controller->FindInteractable() == nullptr);
	return true;
}

DIA_TEST(FWinModelTest, "DoItAsphalt.Game.WinModel")

bool FWinModelTest::RunTest(const FString& Parameters)
{
	auto Is = [this](const TCHAR* What, const FText& Actual, const TCHAR* Expected)
	{
		TestEqual(What, Actual.ToString(), FString(Expected));
	};

	// The pieces of text.
	Is(TEXT("duration 0"), DIAFormat::Duration(0.0), TEXT("0:00"));
	Is(TEXT("duration 59.9 s rounds down"), DIAFormat::Duration(59.9), TEXT("0:59"));
	Is(TEXT("duration 754 s"), DIAFormat::Duration(754.0), TEXT("12:34"));
	Is(TEXT("duration below an hour"), DIAFormat::Duration(3599.0), TEXT("59:59"));
	Is(TEXT("duration 1 h 2 min 5 s"), DIAFormat::Duration(3725.0), TEXT("1:02:05"));
	Is(TEXT("duration of a long game"), DIAFormat::Duration(10.0 * 3600.0 + 5.0 * 60.0), TEXT("10:05:00"));
	Is(TEXT("duration negative"), DIAFormat::Duration(-5.0), TEXT("0:00"));
	Is(TEXT("litres poured"), DIAFormat::LitresPoured(12345.9), TEXT("12,345 L"));
	Is(TEXT("litres poured, none"), DIAFormat::LitresPoured(0.0), TEXT("0 L"));
	Is(TEXT("upgrades"), DIAFormat::Upgrades(8, 35), TEXT("8 / 35"));

	// A fresh game: nothing happened yet.
	FAsphaltGame Game;
	FDIAWinModel Model;
	TestTrue(TEXT("the first update builds the view"), Model.Update(Game));
	Is(TEXT("time"), Model.GetView().Time, TEXT("0:00"));
	Is(TEXT("poured"), Model.GetView().Poured, TEXT("0 L"));
	Is(TEXT("money"), Model.GetView().Money, TEXT("0"));
	Is(TEXT("upgrades"), Model.GetView().Upgrades, TEXT("0 / 35"));
	TestEqual(TEXT("35 upgrades in all: 12 + 6 + 12 levels and 5 specials"), FDIAWinModel::UpgradesTotal(Game), 35);

	// Bought upgrades: the three levels plus the specials; time passes; everything is poured.
	Game.DebugSetLevel(EAsphaltUpgrade::Backpack, 3);
	Game.DebugSetLevel(EAsphaltUpgrade::Tool, 2);
	Game.DebugSetLevel(EAsphaltUpgrade::Plant, 1);
	Game.DebugGrantSpecial(EAsphaltSpecial::Sprint);
	Game.DebugGrantSpecial(EAsphaltSpecial::AutoPaver);
	Game.Tick(static_cast<double>(ExitDaySeconds));
	for (int32 Cell = 0; Cell < Game.GetMap().Num(); ++Cell)
	{
		Game.DebugCompleteCell(Cell);
	}
	TestEqual(TEXT("six levels and two specials"), FDIAWinModel::UpgradesBought(Game), 8);
	// What was earned is what was poured, whatever has been spent since.
	const double Poured = Game.GetTotalLitresPoured();
	TestTrue(TEXT("a lot was poured"), Poured > 5000.0);
	Game.DebugGive(-Game.GetMoney(), 0);
	TestTrue(TEXT("a second update with changed numbers rebuilds"), Model.Update(Game));
	Is(TEXT("time after 3725 s"), Model.GetView().Time, TEXT("1:02:05"));
	TestEqual(TEXT("poured"), Model.GetView().Poured.ToString(), DIAFormat::LitresPoured(Poured).ToString());
	TestEqual(TEXT("money earned is the money for the litres, not the empty purse"), Model.GetView().Money.ToString(),
		DIAFormat::Money(Poured * AsphaltBalance::MoneyPerLitre).ToString());
	Is(TEXT("upgrades"), Model.GetView().Upgrades, TEXT("8 / 35"));

	// An unchanged game costs nothing: the texts are rebuilt only when something shown changed.
	const int32 Rebuilds = Model.GetRebuildCount();
	TestFalse(TEXT("nothing changed, nothing rebuilt"), Model.Update(Game));
	TestEqual(TEXT("rebuild count unchanged"), Model.GetRebuildCount(), Rebuilds);
	return true;
}
