// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Brush.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Game/DIAWorldSubsystem.h"
#include "GameFramework/Pawn.h"
#include "LotBalance.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Tests/DIATestWorld.h"
#include "World/DIAEntryBoard.h"
#include "World/DIAGroundActor.h"
#include "World/DIALotEdge.h"
#include "World/DIAMallActor.h"

namespace
{
	constexpr double SiteTestHalfCm = AsphaltLot::HalfExtentCm;
	/** The line where the shopping-centre quadrant begins (X and Y alike). */
	constexpr double SiteTestCutCm = AsphaltLot::Cut * AsphaltLot::CellCm - AsphaltLot::HalfExtentCm;
	/** An actor may overlap the car park by this much (the mall only touches it). */
	constexpr double SiteTestOverlapCm = 1.0;
	/** The wall must stop the player at the latest this far beyond the car park's outline. */
	constexpr double SiteTestWallReachCm = 1250.0;
	/** Height of the traces above the ground: where the player's body is. */
	constexpr double SiteTestTraceZCm = 90.0;
	constexpr double SiteTestTouchCm = 1.0;

	/** The part of a box (2D) that lies over a rectangle of the car park: the smaller of the two overlaps, 0 if they do not meet. */
	double OverlapDepth(const FBox& Box, double MinX, double MinY, double MaxX, double MaxY)
	{
		const double OverX = FMath::Min(Box.Max.X, MaxX) - FMath::Max(Box.Min.X, MinX);
		const double OverY = FMath::Min(Box.Max.Y, MaxY) - FMath::Max(Box.Min.Y, MinY);
		return FMath::Min(FMath::Max(OverX, 0.0), FMath::Max(OverY, 0.0));
	}

	/** Pieces of a stretch of an edge at a pitch, worked out here from the outline and not by the code under test. */
	int32 Pieces(double Length, double Pitch)
	{
		return FMath::Max(1, FMath::RoundToInt(Length / Pitch));
	}

	/** First hit of a pawn-channel line, or false. */
	bool TraceHit(UWorld* World, FVector2D From, FVector2D To, FHitResult& Hit)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SiteTest), false);
		return World->LineTraceSingleByChannel(Hit, FVector(From.X, From.Y, SiteTestTraceZCm), FVector(To.X, To.Y, SiteTestTraceZCm), ECC_Pawn, Params);
	}
}

// Nothing that blocks the player stands on the car park, except the ground itself: the mall only touches it.
DIA_TEST(FSiteLotIsClearTest, "DoItAsphalt.Game.Site.LotIsClear")

bool FSiteLotIsClearTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	int32 NumBlocking = 0;
	bool bMallSeen = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		// The test world's own default brush is a builder aid, not part of the site.
		if (It->IsA<ADIAGroundActor>() || It->IsA<APawn>() || It->IsA<ABrush>())
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Components(*It);
		for (const UPrimitiveComponent* Component : Components)
		{
			if (Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
			{
				continue;
			}
			++NumBlocking;
			bMallSeen |= It->IsA<ADIAMallActor>();
			const FBox Box = Component->Bounds.GetBox();
			// The car park: the whole square without the shopping-centre quadrant, as two rectangles.
			const double Lower = OverlapDepth(Box, -SiteTestHalfCm, -SiteTestHalfCm, SiteTestHalfCm, SiteTestCutCm);
			const double Upper = OverlapDepth(Box, -SiteTestHalfCm, SiteTestCutCm, SiteTestCutCm, SiteTestHalfCm);
			TestTrue(*FString::Printf(TEXT("%s / %s does not stand on the car park"), *It->GetName(), *Component->GetName()),
				Lower <= SiteTestOverlapCm && Upper <= SiteTestOverlapCm);
		}
	}
	TestTrue(TEXT("the check saw the mall"), bMallSeen);
	TestTrue(TEXT("the check saw the walls, boards and barriers too"), NumBlocking >= 10);
	return true;
}

DIA_TEST(FSiteCountsTest, "DoItAsphalt.Game.Site.Counts")

bool FSiteCountsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	Mode->EnsureSite();

	int32 NumMalls = 0;
	for (TActorIterator<ADIAMallActor> It(World); It; ++It)
	{
		++NumMalls;
		TestTrue(TEXT("the mall stands at the layout position"), FVector2D(It->GetActorLocation()).Equals(LotLayout::MallCentre, 1.0));
		TestTrue(TEXT("without a turn"), It->GetActorRotation().IsNearlyZero(0.1));
	}
	TestEqual(TEXT("one mall"), NumMalls, 1);

	TArray<int32> Seen;
	for (TActorIterator<ADIAEntryBoard> It(World); It; ++It)
	{
		Seen.AddUnique(It->Entry);
	}
	TestEqual(TEXT("six distinct boards"), Seen.Num(), AsphaltLot::NumEntries);

	TArray<ADIALotEdge*> Edges;
	for (TActorIterator<ADIALotEdge> It(World); It; ++It)
	{
		Edges.Add(*It);
	}
	if (!TestEqual(TEXT("one edge actor"), Edges.Num(), 1))
	{
		return false;
	}
	const ADIALotEdge* Edge = Edges[0];

	// The pieces that fit the sides minus the gaps, counted from the outline here: bottom (gap at the trough), left, top and right
	// (gaps at the entries 3, 4, 5), each side in two stretches.
	const double CurbPitch = DIAModelSizes::CurbCm.Y;
	auto TwoStretches = [&](double Low, double High, double GapCentre, double GapHalf)
	{
		return Pieces(GapCentre - GapHalf - Low, CurbPitch) + Pieces(High - (GapCentre + GapHalf), CurbPitch);
	};
	const double Strip = LotLayout::StripCm;
	const int32 ExpectedCurbs =
		TwoStretches(-SiteTestHalfCm, SiteTestHalfCm, LotLayout::TroughPos.X, 300.0)
		+ TwoStretches(-SiteTestHalfCm, SiteTestHalfCm, AsphaltLot::EntryCentre(3).Y, 200.0)
		+ TwoStretches(-SiteTestHalfCm, SiteTestCutCm, AsphaltLot::EntryCentre(4).X, 200.0)
		+ TwoStretches(-SiteTestHalfCm, SiteTestCutCm, AsphaltLot::EntryCentre(5).Y, 200.0);
	TestEqual(TEXT("NumCurbs is the tiling of the sides minus the gaps"), ADIALotEdge::NumCurbs(), ExpectedCurbs);
	TestEqual(TEXT("the kerb instances are NumCurbs"), Edge->GetCurbs()->GetInstanceCount(), ADIALotEdge::NumCurbs());

	const double Outer = SiteTestHalfCm + Strip;
	const double FencePitch = DIAModelSizes::FenceCm.Y;
	const int32 ExpectedPanels = Pieces(2.0 * Outer, FencePitch) * 2 + Pieces(Outer - SiteTestCutCm, FencePitch)
		+ TwoStretches(-Outer, SiteTestCutCm, LotLayout::GatePos().Y, 200.0);
	TestEqual(TEXT("NumFencePanels is the tiling of the strip's edge minus the gate gap"), ADIALotEdge::NumFencePanels(), ExpectedPanels);
	TestEqual(TEXT("the fence instances are NumFencePanels"), Edge->GetFence()->GetInstanceCount(), ADIALotEdge::NumFencePanels());

	TestEqual(TEXT("four boxes of white frame per entry"), Edge->GetFrames()->GetInstanceCount(), 4 * AsphaltLot::NumEntries);
	TestEqual(TEXT("old road at the entries 3, 4, 5"), Edge->GetStubs().Num(), 3);
	TestEqual(TEXT("a barrier at each of them"), Edge->GetBarriers().Num(), 3);
	return true;
}

// From the middle of the car park out through each outer side the player is stopped within 12.5 m of the outline, and the mall
// stops him on its walls.
DIA_TEST(FSiteWallsTest, "DoItAsphalt.Game.Site.Walls")

bool FSiteWallsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	struct FOutward
	{
		const TCHAR* Name;
		FVector2D From;
		FVector2D Dir;
		double Outline;
	};
	// Outline is the coordinate of a side along Dir (used by the rays into the mall).
	// Only the walls answer these rays: every other component of the world (fence, barriers, trough, plant, kiosks, mall, ground) is
	// ignored, so that a missing or misplaced wall cannot be hidden by something else standing in the way.
	const ADIALotEdge* Edge = nullptr;
	for (TActorIterator<ADIALotEdge> It(World); It; ++It)
	{
		Edge = *It;
	}
	if (!TestNotNull(TEXT("the edge actor"), Edge))
	{
		return false;
	}
	auto IsWall = [Edge](const UPrimitiveComponent* Component)
	{
		return Edge->GetWalls().ContainsByPredicate([Component](const TObjectPtr<UBoxComponent>& Wall) { return Wall == Component; });
	};
	FCollisionQueryParams WallsOnly(SCENE_QUERY_STAT(SiteTestWalls), false);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components(*It);
		for (UPrimitiveComponent* Component : Components)
		{
			if (!IsWall(Component))
			{
				WallsOnly.AddIgnoredComponent(Component);
			}
		}
	}
	auto HitWall = [&](FVector2D From, FVector2D To, FHitResult& Hit)
	{
		return World->LineTraceSingleByChannel(Hit, FVector(From.X, From.Y, SiteTestTraceZCm), FVector(To.X, To.Y, SiteTestTraceZCm), ECC_Pawn, WallsOnly)
			&& IsWall(Hit.GetComponent());
	};
	// How far beyond the car park's outline a point lies (the larger of the two axes, as the outline is a square with a corner cut out).
	auto Beyond = [](const FVector& Point) { return FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) - SiteTestHalfCm; };
	auto ExpectOut = [&](const FString& What, FVector2D From, FVector2D To)
	{
		FHitResult Hit;
		if (TestTrue(*FString::Printf(TEXT("%s: a wall stops the way out"), *What), HitWall(From, To, Hit)))
		{
			TestTrue(*FString::Printf(TEXT("%s: within 12.5 m of the outline (%.0f cm)"), *What, Beyond(Hit.ImpactPoint)), Beyond(Hit.ImpactPoint) <= SiteTestWallReachCm);
		}
	};

	// A sweep every 5 m along each of the four outer sides, straight out of the car park (the ends of the sides run into the strip's corners).
	const double Out = SiteTestHalfCm + SiteTestWallReachCm;
	const double Step = 500.0;
	for (double Along = -11000.0; Along <= 11000.0; Along += Step)
	{
		ExpectOut(FString::Printf(TEXT("bottom, x=%.0f"), Along), FVector2D(Along, -9000.0), FVector2D(Along, -Out));
		ExpectOut(FString::Printf(TEXT("left, y=%.0f"), Along), FVector2D(-9000.0, Along), FVector2D(-Out, Along));
		if (Along <= SiteTestCutCm)
		{
			ExpectOut(FString::Printf(TEXT("top, x=%.0f"), Along), FVector2D(Along, 9000.0), FVector2D(Along, Out));
			ExpectOut(FString::Printf(TEXT("right, y=%.0f"), Along), FVector2D(9000.0, Along), FVector2D(Out, Along));
		}
	}
	// The places where something else stands or the pieces meet: the gaps of the trough and the gate, the slits beside the gate's
	// barrier, the entries' road ends, and a diagonal into each outer corner (and into the two corners beside the mall).
	for (const double X : {LotLayout::TroughPos.X, LotLayout::TroughPos.X - 349.0, LotLayout::TroughPos.X + 299.0, AsphaltLot::EntryCentre(0).X})
	{
		ExpectOut(FString::Printf(TEXT("trough gap, x=%.0f"), X), FVector2D(X, -9000.0), FVector2D(X, -Out));
	}
	for (const double Y : {LotLayout::GatePos().Y, LotLayout::GatePos().Y - 190.0, LotLayout::GatePos().Y + 190.0, LotLayout::GatePos().Y - 170.0, LotLayout::GatePos().Y + 170.0})
	{
		ExpectOut(FString::Printf(TEXT("gate gap, y=%.0f"), Y), FVector2D(9000.0, Y), FVector2D(Out, Y));
	}
	for (const double Offset : {-190.0, 0.0, 190.0})
	{
		ExpectOut(FString::Printf(TEXT("entry 3 road end, y=%.0f"), Offset), FVector2D(-9000.0, AsphaltLot::EntryCentre(3).Y + Offset), FVector2D(-Out, AsphaltLot::EntryCentre(3).Y + Offset));
		ExpectOut(FString::Printf(TEXT("entry 4 road end, x=%.0f"), Offset), FVector2D(AsphaltLot::EntryCentre(4).X + Offset, 9000.0), FVector2D(AsphaltLot::EntryCentre(4).X + Offset, Out));
	}
	ExpectOut(TEXT("corner bottom-left"), FVector2D(-9000.0, -9000.0), FVector2D(-Out, -Out));
	ExpectOut(TEXT("corner bottom-right"), FVector2D(9000.0, -9000.0), FVector2D(Out, -Out));
	ExpectOut(TEXT("corner top-left"), FVector2D(-9000.0, 9000.0), FVector2D(-Out, Out));
	ExpectOut(TEXT("corner beside the mall, east"), FVector2D(9000.0, -1000.0), FVector2D(Out, 250.0));
	ExpectOut(TEXT("corner beside the mall, north"), FVector2D(-1000.0, 9000.0), FVector2D(250.0, Out));

	// The strips beside the mall: just past the end of the car park's side, going along the mall, a wall closes the strip on the line where the mall begins.
	const FOutward Closed[] = {
		{TEXT("strip beside the mall, east"), FVector2D(SiteTestHalfCm + 100.0, -500.0), FVector2D(0.0, 1.0), 0.0},
		{TEXT("strip beside the mall, east, near the fence"), FVector2D(LotLayout::StripCm + SiteTestHalfCm - 100.0, -500.0), FVector2D(0.0, 1.0), 0.0},
		{TEXT("strip beside the mall, north"), FVector2D(-500.0, SiteTestHalfCm + 100.0), FVector2D(1.0, 0.0), 0.0},
		{TEXT("strip beside the mall, north, near the fence"), FVector2D(-500.0, LotLayout::StripCm + SiteTestHalfCm - 100.0), FVector2D(1.0, 0.0), 0.0},
	};
	for (const FOutward& Side : Closed)
	{
		FHitResult Hit;
		if (TestTrue(*FString::Printf(TEXT("%s: a wall closes the strip"), Side.Name), HitWall(Side.From, Side.From + Side.Dir * 1000.0, Hit)))
		{
			TestTrue(*FString::Printf(TEXT("%s: on the line where the mall begins"), Side.Name),
				FMath::Abs(FVector2D::DotProduct(FVector2D(Hit.ImpactPoint), Side.Dir)) <= 100.0);
		}
	}

	// Into the mall: the wall of its body, where its facade stands.
	const FOutward IntoMall[] = {
		{TEXT("mall, facade towards -Y"), FVector2D(5000.0, -3000.0), FVector2D(0.0, 1.0), SiteTestCutCm},
		{TEXT("mall, facade towards -X"), FVector2D(-3000.0, 5000.0), FVector2D(1.0, 0.0), SiteTestCutCm},
	};
	for (const FOutward& Side : IntoMall)
	{
		FHitResult Hit;
		if (TestTrue(*FString::Printf(TEXT("%s: the mall blocks"), Side.Name), TraceHit(World, Side.From, Side.From + Side.Dir * 6000.0, Hit)))
		{
			TestTrue(*FString::Printf(TEXT("%s: it is the mall"), Side.Name), Hit.GetActor() && Hit.GetActor()->IsA<ADIAMallActor>());
			TestEqual(*FString::Printf(TEXT("%s: on its wall"), Side.Name), FVector2D::DotProduct(FVector2D(Hit.ImpactPoint), Side.Dir), Side.Outline, SiteTestTouchCm);
		}
	}
	return true;
}

DIA_TEST(FSiteBoardTextTest, "DoItAsphalt.Game.Site.BoardText")

bool FSiteBoardTextTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	TMap<int32, ADIAEntryBoard*> Boards;
	for (TActorIterator<ADIAEntryBoard> It(World); It; ++It)
	{
		Boards.Add(It->Entry, *It);
	}
	if (!TestNotNull(TEXT("world subsystem"), Subsystem) || !TestEqual(TEXT("six boards"), Boards.Num(), AsphaltLot::NumEntries))
	{
		return false;
	}

	// A new game: the first step needs 12 cells of the entry and 10 cells in all.
	const FString Fresh = Boards[0]->GetBoardText().ToString();
	TestTrue(TEXT("the board of entry 0 says 0 / 12"), Fresh.Contains(TEXT("0 / 12")));
	TestTrue(TEXT("and 0 / 10"), Fresh.Contains(*FString::Printf(TEXT("0 / %d"), LotBalance::UnlockCells[0])));
	TestFalse(TEXT("it is not unlocked"), Fresh.Contains(TEXT("UNLOCKED")));
	const FString Last = Boards[5]->GetBoardText().ToString();
	TestTrue(TEXT("the board of the exit says 0 / 30,000"), Last.Contains(TEXT("0 / 30,000")));

	// What the component shows is what GetBoardText says, and it follows the game without being asked.
	TestEqual(TEXT("the component shows the text"), Boards[0]->GetTextComponent()->Text.ToString(), Fresh);

	// dia.CompleteEntry 0
	FLotEvents Events;
	for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
	{
		Events.Append(Subsystem->GetGame().DebugCompleteCell(AsphaltLot::EntryCell(0, I)));
	}
	Subsystem->Dispatch(Events);
	const FString Done = Boards[0]->GetBoardText().ToString();
	TestTrue(TEXT("entry 0 and 12 cells: UNLOCKED"), Done.Contains(TEXT("UNLOCKED")));
	TestEqual(TEXT("the component follows"), Boards[0]->GetTextComponent()->Text.ToString(), Done);
	TestFalse(TEXT("the other boards stay locked"), Boards[3]->GetBoardText().ToString().Contains(TEXT("UNLOCKED")));
	TestTrue(TEXT("entry 3 still says 0 / 12"), Boards[3]->GetBoardText().ToString().Contains(TEXT("0 / 12")));

	// Where the boards stand: a kiosk beside the entries 0, 3, 4, 5, the text on the mall's sign for 1 and 2.
	for (int32 Entry = 0; Entry < AsphaltLot::NumEntries; ++Entry)
	{
		const ADIAEntryBoard* Board = Boards[Entry];
		if (DIABoard::HasKiosk(Entry))
		{
			TestTrue(*FString::Printf(TEXT("board %d stands at BoardPos"), Entry), FVector2D(Board->GetActorLocation()).Equals(LotLayout::BoardPos(Entry), 1.0));
			const FVector2D Facing(Board->GetActorForwardVector());
			TestTrue(*FString::Printf(TEXT("board %d faces the car park"), Entry), FVector2D::DotProduct(Facing, AsphaltLot::EntryOutward(Entry)) < -0.99);
		}
		else
		{
			const FVector Where = Board->GetActorLocation();
			TestEqual(*FString::Printf(TEXT("sign text %d is at the entry (X)"), Entry), Where.X, AsphaltLot::EntryCentre(Entry).X + (Entry == 2 ? -DIABoard::SignTextStandOffCm : 0.0), 1.0);
			TestEqual(*FString::Printf(TEXT("sign text %d is at the entry (Y)"), Entry), Where.Y, AsphaltLot::EntryCentre(Entry).Y + (Entry == 1 ? -DIABoard::SignTextStandOffCm : 0.0), 1.0);
			TestEqual(*FString::Printf(TEXT("sign text %d is at the height of the sign"), Entry), Where.Z, DIAModelSizes::MallSignCm.Z, 1.0);
			TestTrue(*FString::Printf(TEXT("sign text %d faces the car park"), Entry),
				FVector2D::DotProduct(FVector2D(Board->GetActorForwardVector()), AsphaltLot::EntryOutward(Entry)) < -0.99);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
