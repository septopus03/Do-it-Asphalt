// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIALotEdge.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Game/DIAAssets.h"
#include "LotLayout.h"
#include "LotMap.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr double HalfCm = AsphaltLot::HalfExtentCm;
	/** The line where the shopping-centre quadrant begins (X and Y alike): 0. */
	constexpr double CutCm = AsphaltLot::Cut * AsphaltLot::CellCm - AsphaltLot::HalfExtentCm;
	/** Centre line of the fence, and the edge of the strip. */
	constexpr double OuterCm = HalfCm + LotLayout::StripCm;
	/** The entries that have an old road leading out of the car park to the fence. */
	constexpr int32 RoadEntries[] = {3, 4, 5};
	constexpr double BasicShapeCm = 100.0;

	using DIASite::FRun;

	struct FGap
	{
		FVector2D Centre;
		double HalfCm;
	};

	double EdgeYawDeg(FVector2D Direction)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	}

	/** The four outer sides of a square with the corner of the mall quadrant cut out: the car park (Half = HalfExtentCm) or the strip's edge (Half = OuterCm). */
	TArray<FRun> OuterSides(double Half)
	{
		return {
			{FVector2D(-Half, -Half), FVector2D(Half, -Half), FVector2D(0.0, -1.0)},
			{FVector2D(Half, -Half), FVector2D(Half, CutCm), FVector2D(1.0, 0.0)},
			{FVector2D(-Half, Half), FVector2D(CutCm, Half), FVector2D(0.0, 1.0)},
			{FVector2D(-Half, -Half), FVector2D(-Half, Half), FVector2D(-1.0, 0.0)},
		};
	}

	/** Appends a side without the gaps that lie on it. */
	void AddWithoutGaps(TArray<FRun>& Out, const FRun& Side, const TArray<FGap>& Gaps)
	{
		const FVector2D Direction = Side.Direction();
		const double Length = Side.Length();
		TArray<TPair<double, double>> Cuts;
		for (const FGap& Gap : Gaps)
		{
			const FVector2D Relative = Gap.Centre - Side.Start;
			const double Along = FVector2D::DotProduct(Relative, Direction);
			if (FMath::Abs(FVector2D::DotProduct(Relative, Side.Outward)) <= 1.0 && Along >= 0.0 && Along <= Length)
			{
				Cuts.Emplace(Along - Gap.HalfCm, Along + Gap.HalfCm);
			}
		}
		Cuts.Sort([](const TPair<double, double>& A, const TPair<double, double>& B) { return A.Key < B.Key; });

		double Cursor = 0.0;
		for (const TPair<double, double>& Cut : Cuts)
		{
			if (Cut.Key > Cursor + 1.0)
			{
				Out.Add({Side.Start + Direction * Cursor, Side.Start + Direction * Cut.Key, Side.Outward});
			}
			Cursor = FMath::Max(Cursor, Cut.Value);
		}
		if (Length > Cursor + 1.0)
		{
			Out.Add({Side.Start + Direction * Cursor, Side.End, Side.Outward});
		}
	}

	/** How the shape that stands in for a missing model has to be placed: instances of a real model need neither. */
	struct FInstanceLook
	{
		FVector Scale = FVector::OneVector;
		double LiftCm = 0.0;
	};

	/** Gives the component a model; a stand-in engine shape is scaled to the size, painted, and has to be lifted by half its height. */
	FInstanceLook SetInstancedModel(UInstancedStaticMeshComponent* Component, FName Name, DIAAssets::EFallback Shape, const FVector& SizeCm, FName PaletteName)
	{
		FInstanceLook Look;
		UStaticMesh* Mesh = DIAAssets::LoadMeshOrFallback(Name, Shape, SizeCm, Look.Scale);
		Component->SetStaticMesh(Mesh);
		if (Mesh && Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
		{
			Look.LiftCm = SizeCm.Z * 0.5;
			if (UMaterialInterface* Material = DIAAssets::Palette(PaletteName))
			{
				Component->SetMaterial(0, Material);
			}
		}
		else
		{
			Look.Scale = FVector::OneVector;
		}
		return Look;
	}

	/** Lays whole pieces along a run, spread evenly (stretched along the run by Stretch); the piece stands Offset beyond the line. */
	void AddPieces(UInstancedStaticMeshComponent* Component, const FInstanceLook& Look, const FRun& Run, double PitchCm, double OffsetCm, double YawOfPiece)
	{
		const double Length = Run.Length();
		const int32 Count = DIASite::NumPieces(Length, PitchCm);
		const double Stretch = Length / Count / PitchCm;
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector2D Where = Run.Start + Run.Direction() * (Length * (I + 0.5) / Count) + Run.Outward * OffsetCm;
			Component->AddInstance(FTransform(FRotator(0.0, YawOfPiece, 0.0), FVector(Where.X, Where.Y, Look.LiftCm), Look.Scale * FVector(1.0, Stretch, 1.0)));
		}
	}

	int32 CountPieces(const TArray<FRun>& Runs, double PitchCm)
	{
		int32 Count = 0;
		for (const FRun& Run : Runs)
		{
			Count += DIASite::NumPieces(Run.Length(), PitchCm);
		}
		return Count;
	}

	UInstancedStaticMeshComponent* CreateInstanced(AActor* Owner, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Component = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(Owner->GetRootComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Component;
	}

	UBoxComponent* AddWall(AActor* Owner, FVector2D Centre, FVector2D HalfSize)
	{
		UBoxComponent* Wall = NewObject<UBoxComponent>(Owner);
		Wall->SetupAttachment(Owner->GetRootComponent());
		Wall->SetBoxExtent(FVector(HalfSize.X, HalfSize.Y, DIASite::WallHeightCm * 0.5));
		Wall->SetRelativeLocation(FVector(Centre.X, Centre.Y, DIASite::WallHeightCm * 0.5));
		Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		// The camera boom is never pushed in by a wall.
		Wall->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Wall->SetHiddenInGame(true);
		Wall->RegisterComponent();
		return Wall;
	}
}

TArray<DIASite::FRun> DIASite::CurbRuns()
{
	TArray<FGap> Gaps;
	Gaps.Add({FVector2D(LotLayout::TroughPos.X, AsphaltLot::EntryCentre(0).Y), CurbTroughGapHalfCm});
	for (const int32 Entry : RoadEntries)
	{
		Gaps.Add({AsphaltLot::EntryCentre(Entry), CurbEntryGapHalfCm});
	}
	TArray<FRun> Runs;
	for (const FRun& Side : OuterSides(HalfCm))
	{
		AddWithoutGaps(Runs, Side, Gaps);
	}
	return Runs;
}

TArray<DIASite::FRun> DIASite::FenceRuns()
{
	const TArray<FGap> Gaps = {{LotLayout::GatePos(), GateGapHalfCm}};
	TArray<FRun> Runs;
	for (const FRun& Side : OuterSides(OuterCm))
	{
		AddWithoutGaps(Runs, Side, Gaps);
	}
	return Runs;
}

ADIALotEdge::ADIALotEdge()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// The instanced pieces are only to be seen: the walls and the barriers keep the player in.
	Curbs = CreateInstanced(this, TEXT("Curbs"));
	Fence = CreateInstanced(this, TEXT("Fence"));
	Frames = CreateInstanced(this, TEXT("Frames"));
	Frames->SetCastShadow(false);
}

int32 ADIALotEdge::NumCurbs()
{
	return CountPieces(DIASite::CurbRuns(), DIASite::CurbPitchCm);
}

int32 ADIALotEdge::NumFencePanels()
{
	return CountPieces(DIASite::FenceRuns(), DIASite::FencePitchCm);
}

void ADIALotEdge::BeginPlay()
{
	Super::BeginPlay();
	BuildCurbs();
	BuildFence();
	BuildFrames();
	BuildWalls();
	BuildRoadEnds();
}

void ADIALotEdge::BuildCurbs()
{
	// A piece stands just outside the outline with its bevel (+X of the model) towards the car park; its origin is the centre of its footprint.
	const FInstanceLook Look = SetInstancedModel(Curbs, TEXT("SM_Curb"), DIAAssets::EFallback::Cube, DIAModelSizes::CurbCm, TEXT("DIA_Concrete"));
	for (const FRun& Run : DIASite::CurbRuns())
	{
		AddPieces(Curbs, Look, Run, DIASite::CurbPitchCm, DIAModelSizes::CurbCm.X * 0.5, EdgeYawDeg(-Run.Outward));
	}
}

void ADIALotEdge::BuildFence()
{
	// A panel's thin axis (X of the model) points out of the site; the panels tile along the run.
	const FInstanceLook Look = SetInstancedModel(Fence, TEXT("SM_Fence"), DIAAssets::EFallback::Cube, DIAModelSizes::FenceCm, TEXT("DIA_Steel"));
	for (const FRun& Run : DIASite::FenceRuns())
	{
		AddPieces(Fence, Look, Run, DIASite::FencePitchCm, 0.0, EdgeYawDeg(Run.Outward));
	}
}

void ADIALotEdge::BuildFrames()
{
	// The frame is four flat boxes on the border of the entry's rectangle, a hair above the ground, painted white, without collision.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_Quiet | LOAD_NoWarn);
	Frames->SetStaticMesh(Cube);
	if (UMaterialInterface* Material = DIAAssets::Palette(TEXT("DIA_White")))
	{
		Frames->SetMaterial(0, Material);
	}
	const double Z = DIASite::FrameLiftCm + DIASite::FrameThicknessCm * 0.5;
	auto AddBox = [this, Z](FVector2D Centre, FVector2D Size)
	{
		Frames->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Centre.X, Centre.Y, Z), FVector(Size.X, Size.Y, DIASite::FrameThicknessCm) / BasicShapeCm));
	};
	for (int32 Entry = 0; Entry < AsphaltLot::NumEntries; ++Entry)
	{
		FBox2D Rect(ForceInit);
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			const FVector2D Centre = AsphaltLot::CellCentre(AsphaltLot::EntryCell(Entry, I));
			Rect += Centre - FVector2D(AsphaltLot::CellCm * 0.5);
			Rect += Centre + FVector2D(AsphaltLot::CellCm * 0.5);
		}
		const FVector2D Size = Rect.GetSize();
		const FVector2D Middle = Rect.GetCenter();
		const double W = DIASite::FrameWidthCm;
		AddBox(FVector2D(Middle.X, Rect.Min.Y + W * 0.5), FVector2D(Size.X, W));
		AddBox(FVector2D(Middle.X, Rect.Max.Y - W * 0.5), FVector2D(Size.X, W));
		AddBox(FVector2D(Rect.Min.X + W * 0.5, Middle.Y), FVector2D(W, Size.Y - 2.0 * W));
		AddBox(FVector2D(Rect.Max.X - W * 0.5, Middle.Y), FVector2D(W, Size.Y - 2.0 * W));
	}
}

void ADIALotEdge::BuildWalls()
{
	const double Thick = DIASite::WallThicknessCm;
	// Along the fence line, with the inner face where the panels stand and the ends running past the corners so that they seal. The
	// side with the exit gate has a gap there (the gate's posts close its ends, its arm the passage while it is closed).
	const FVector2D Gate = LotLayout::GatePos();
	const double GapHalf = DIASite::GateWallGapHalfCm;
	for (const FRun& Side : OuterSides(OuterCm))
	{
		const FVector2D Direction = Side.Direction();
		const bool bAlongX = FMath::Abs(Direction.X) > 0.5;
		const double StartAlong = FVector2D::DotProduct(Side.Start, Direction);
		const double Low = StartAlong - Thick;
		const double High = StartAlong + Side.Length() + Thick;
		const FVector2D Relative = Gate - Side.Start;
		const double GateAlong = FVector2D::DotProduct(Relative, Direction) + StartAlong;
		const bool bGateHere = FMath::Abs(FVector2D::DotProduct(Relative, Side.Outward)) <= 1.0 && GateAlong - GapHalf > Low && GateAlong + GapHalf < High;

		TArray<TPair<double, double>> Spans;
		if (bGateHere)
		{
			Spans.Emplace(Low, GateAlong - GapHalf);
			Spans.Emplace(GateAlong + GapHalf, High);
		}
		else
		{
			Spans.Emplace(Low, High);
		}
		for (const TPair<double, double>& Span : Spans)
		{
			const double Middle = (Span.Key + Span.Value) * 0.5;
			const FVector2D Centre = Side.Start + Direction * (Middle - StartAlong) + Side.Outward * (Thick * 0.5 - DIAModelSizes::FenceCm.X * 0.5);
			const double HalfLength = (Span.Value - Span.Key) * 0.5;
			Walls.Add(AddWall(this, Centre, bAlongX ? FVector2D(HalfLength, Thick * 0.5) : FVector2D(Thick * 0.5, HalfLength)));
		}
	}
	// Along the strip's edges at the mall: from where the car park's side ends to the fence line. The walls are centred on the line
	// where the mall begins, so that they only touch the car park.
	const double Reach = OuterCm + Thick - HalfCm;
	Walls.Add(AddWall(this, FVector2D(HalfCm + Reach * 0.5, CutCm), FVector2D(Reach * 0.5, Thick * 0.5)));
	Walls.Add(AddWall(this, FVector2D(CutCm, HalfCm + Reach * 0.5), FVector2D(Thick * 0.5, Reach * 0.5)));
}

void ADIALotEdge::BuildRoadEnds()
{
	for (const int32 Entry : RoadEntries)
	{
		const FVector2D Origin = AsphaltLot::EntryCentre(Entry);
		const FVector2D Outward = AsphaltLot::EntryOutward(Entry);
		const FRotator Turn(0.0, EdgeYawDeg(Outward), 0.0);

		// The old road runs from the edge of the car park to the fence (the model's +X is its length).
		UStaticMeshComponent* Stub = NewObject<UStaticMeshComponent>(this);
		Stub->SetupAttachment(GetRootComponent());
		Stub->SetRelativeLocationAndRotation(FVector(Origin.X, Origin.Y, 0.0), Turn);
		Stub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stub->RegisterComponent();
		DIAAssets::SetModelOrFallback(Stub, TEXT("SM_RoadStub"), DIAAssets::EFallback::Cube, DIAModelSizes::RoadStubCm,
			Turn.RotateVector(FVector(DIAModelSizes::RoadStubCm.X * 0.5, 0.0, DIAModelSizes::RoadStubCm.Z * 0.5)), TEXT("DIA_Asphalt"));
		Stubs.Add(Stub);

		// The barrier closes it at the fence; the exit has the gate in the gap of the fence instead (ADIAExitGate).
		if (Entry == LotLayout::ExitEntry)
		{
			continue;
		}
		const double Distance = LotLayout::StripCm - DIAModelSizes::FenceCm.X * 0.5 - DIAModelSizes::BarrierCm.X * 0.5;
		const FVector2D Where = Origin + Outward * Distance;
		UStaticMeshComponent* Barrier = NewObject<UStaticMeshComponent>(this);
		Barrier->SetupAttachment(GetRootComponent());
		Barrier->SetRelativeLocationAndRotation(FVector(Where.X, Where.Y, 0.0), Turn);
		Barrier->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Barrier->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Barrier->RegisterComponent();
		DIAAssets::SetModelOrFallback(Barrier, TEXT("SM_Barrier"), DIAAssets::EFallback::Cube, DIAModelSizes::BarrierCm,
			FVector(0.0, 0.0, DIAModelSizes::BarrierCm.Z * 0.5), TEXT("DIA_Red"));
		Barriers.Add(Barrier);
	}
}
