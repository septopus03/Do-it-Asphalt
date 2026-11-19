// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltBalance.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAWorldSubsystem.h"
#include "Tests/DIATestWorld.h"
#include "World/DIAAutoPaver.h"

namespace
{
	constexpr int32 PaverRoad = 1;
	constexpr int32 RoadCellCount = 5;
	constexpr double PaverPrice = 450.0;
	constexpr double MovedCm = 1.0;
	constexpr double YawToleranceDeg = 0.5;
	constexpr double FollowToleranceCm = 1.0;
	/** Frames of one second (the longest the subsystem gives the core at once), then frames of 0.1 s: 10 s each. */
	constexpr float LongFrameSeconds = 1.0f;
	constexpr int32 LongFrames = 10;
	constexpr float ShortFrameSeconds = 0.1f;
	constexpr int32 ShortFrames = 100;

	TArray<ADIAAutoPaver*> FindAutoPavers(UWorld* World)
	{
		TArray<ADIAAutoPaver*> Result;
		for (TActorIterator<ADIAAutoPaver> It(World); It; ++It)
		{
			Result.Add(*It);
		}
		return Result;
	}

	FVector2D PaverWhere(const AActor* Actor)
	{
		return FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y);
	}
}

DIA_TEST(FAutoPaverActorFollowsCoreTest, "DoItAsphalt.Game.AutoPaverActorFollowsCore")

bool FAutoPaverActorFollowsCoreTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	FAsphaltGame& Game = Subsystem->GetGame();

	TestEqual(TEXT("no paver before the special is owned"), FindAutoPavers(World).Num(), 0);
	Mode->EnsureAutoPaver();
	TestEqual(TEXT("EnsureAutoPaver spawns nothing without the special"), FindAutoPavers(World).Num(), 0);

	// Road 1 complete (the cells pay out money, so it is zeroed first), then exactly the price: bought like at the kiosk.
	for (const int32 Cell : Game.GetMap().GetRoadCells(PaverRoad))
	{
		Subsystem->Dispatch(Game.DebugCompleteCell(Cell));
	}
	TestEqual(TEXT("road 1 has five cells"), Game.GetMap().GetRoadCells(PaverRoad).Num(), RoadCellCount);
	Game.DebugGive(-Game.GetMoney() + PaverPrice, 0);
	TestEqual(TEXT("money is 450"), Game.GetMoney(), PaverPrice);
	if (!TestTrue(TEXT("the paver is bought"), Subsystem->BuySpecial(EAsphaltSpecial::AutoPaver)))
	{
		return false;
	}

	// Exactly one actor, where the core put the paver, visible and without collision.
	const TArray<ADIAAutoPaver*> Pavers = FindAutoPavers(World);
	if (!TestEqual(TEXT("one paver after the purchase"), Pavers.Num(), 1))
	{
		return false;
	}
	ADIAAutoPaver* Paver = Pavers[0];
	TestTrue(TEXT("it starts at the spawn point of road 1"), PaverWhere(Paver).Equals(AsphaltLayout::SpecialSpawn(PaverRoad), FollowToleranceCm));
	TestTrue(TEXT("the same as the core's position"), PaverWhere(Paver).Equals(Game.GetAutoPaverPos(), FollowToleranceCm));
	TestEqual(TEXT("it stands on the ground"), Paver->GetActorLocation().Z, 0.0);
	TestTrue(TEXT("it has a mesh"), Paver->GetBodyMesh()->GetStaticMesh() != nullptr);
	TestTrue(TEXT("the mesh is visible"), Paver->GetBodyMesh()->IsVisible());
	TestEqual(TEXT("it never blocks the player"), Paver->GetBodyMesh()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	// A second announcement never makes a second paver.
	Subsystem->OnSpecialBought.Broadcast(EAsphaltSpecial::AutoPaver);
	Mode->EnsureAutoPaver();
	TestEqual(TEXT("still one paver"), FindAutoPavers(World).Num(), 1);

	// 10 s in frames of one second: the paver turns to face its move (each frame's turn is allowed 360 degrees), and keeps the heading at rest.
	const double LitresBefore = Game.GetTotalLitresPoured();
	int32 MovingFrames = 0;
	for (int32 Frame = 0; Frame < LongFrames; ++Frame)
	{
		const FVector2D Before = PaverWhere(Paver);
		const double YawBefore = Paver->GetActorRotation().Yaw;
		Wrapper.TickTestWorld(LongFrameSeconds);
		const FVector2D Move = PaverWhere(Paver) - Before;
		if (Move.Size() > MovedCm)
		{
			++MovingFrames;
			const double WantedYaw = FMath::RadiansToDegrees(FMath::Atan2(Move.Y, Move.X));
			TestTrue(TEXT("it faces its direction of travel"), FMath::Abs(FMath::FindDeltaAngleDegrees(Paver->GetActorRotation().Yaw, WantedYaw)) <= YawToleranceDeg);
		}
		else
		{
			TestEqual(TEXT("at rest it keeps its heading"), Paver->GetActorRotation().Yaw, YawBefore);
		}
		TestTrue(TEXT("the actor is where the core says"), PaverWhere(Paver).Equals(Game.GetAutoPaverPos(), FollowToleranceCm));
	}
	TestTrue(TEXT("it drove somewhere in 10 s"), MovingFrames > 0);
	TestTrue(TEXT("it poured asphalt"), Game.GetTotalLitresPoured() > LitresBefore);

	// 10 s more in frames of 0.1 s: still within 1 cm of the core after every frame, and still pouring.
	const double LitresMid = Game.GetTotalLitresPoured();
	for (int32 Frame = 0; Frame < ShortFrames; ++Frame)
	{
		Wrapper.TickTestWorld(ShortFrameSeconds);
		if (!PaverWhere(Paver).Equals(Game.GetAutoPaverPos(), FollowToleranceCm))
		{
			AddError(FString::Printf(TEXT("frame %d: the paver is not within 1 cm of the core"), Frame));
			break;
		}
	}
	TestTrue(TEXT("it kept pouring"), Game.GetTotalLitresPoured() > LitresMid);
	TestEqual(TEXT("still one paver"), FindAutoPavers(World).Num(), 1);

	// A paver that is gone (stale pointer) is spawned again where the core says, once.
	Paver->Destroy();
	Mode->EnsureAutoPaver();
	Mode->EnsureAutoPaver();
	const TArray<ADIAAutoPaver*> Again = FindAutoPavers(World);
	if (TestEqual(TEXT("respawned once"), Again.Num(), 1))
	{
		TestTrue(TEXT("at the core's position"), PaverWhere(Again[0]).Equals(Game.GetAutoPaverPos(), FollowToleranceCm));
	}
	return true;
}
