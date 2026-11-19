// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIASiteFence.h"
#include "AsphaltLayout.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "HexMap.h"

namespace
{
	constexpr double FullCircleDeg = 360.0;

	double RoadAngleDeg(int32 Road)
	{
		const FVector2D Dir = FHexMap::RoadDirection(Road);
		const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		return Angle < 0.0 ? Angle + FullCircleDeg : Angle;
	}

	/** A point of the circle at an angle, and the transform of a piece standing there with its X axis pointing outwards. */
	FVector2D CirclePoint(double AngleDeg)
	{
		const double Angle = FMath::DegreesToRadians(AngleDeg);
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * AsphaltLayout::SiteRadiusCm;
	}
}

double DIASite::GapHalfAngleDeg()
{
	return FMath::RadiansToDegrees(FMath::Asin(GapWidthCm * 0.5 / AsphaltLayout::SiteRadiusCm));
}

TArray<DIASite::FArc> DIASite::FenceArcs()
{
	TArray<double> Roads;
	for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
	{
		Roads.Add(RoadAngleDeg(Road));
	}
	Roads.Sort();

	TArray<FArc> Arcs;
	const double Half = GapHalfAngleDeg();
	for (int32 I = 0; I < Roads.Num(); ++I)
	{
		const bool bWraps = I + 1 == Roads.Num();
		const double Next = bWraps ? Roads[0] + FullCircleDeg : Roads[I + 1];
		Arcs.Add({Roads[I] + Half, Next - Half});
	}
	return Arcs;
}

TArray<DIASite::FArc> DIASite::WallArcs()
{
	const TArray<FArc> Fence = FenceArcs();
	const double Slot = FullCircleDeg / WallSlots;

	TArray<FArc> Arcs;
	for (int32 I = 0; I < WallSlots; ++I)
	{
		const double Low = I * Slot - Slot * 0.5;
		const double High = I * Slot + Slot * 0.5;
		for (const FArc& Stretch : Fence)
		{
			// The stretch and its copies a turn either side, so that the one across +X meets the slots there.
			for (const double Turn : {-FullCircleDeg, 0.0, FullCircleDeg})
			{
				const double Start = FMath::Max(Low, Stretch.StartDeg + Turn);
				const double End = FMath::Min(High, Stretch.EndDeg + Turn);
				if (End - Start > MinWallArcDeg)
				{
					Arcs.Add({Start, End});
				}
			}
		}
	}
	return Arcs;
}

ADIASiteFence::ADIASiteFence()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADIASiteFence::BeginPlay()
{
	Super::BeginPlay();
	BuildPanels();
	BuildWall();
}

void ADIASiteFence::BuildPanels()
{
	// Each stretch between two gaps gets whole panels, spaced evenly at about the model's 2 m pitch (a little closer rather than apart).
	for (const DIASite::FArc& Stretch : DIASite::FenceArcs())
	{
		const double Length = FMath::DegreesToRadians(Stretch.LengthDeg()) * AsphaltLayout::SiteRadiusCm;
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / DIAModelSizes::FenceCm.Y));
		for (int32 I = 0; I < Count; ++I)
		{
			const double Angle = Stretch.StartDeg + Stretch.LengthDeg() * (I + 0.5) / Count;
			const FVector2D Where = CirclePoint(Angle);

			UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(this);
			Panel->SetupAttachment(GetRootComponent());
			Panel->SetRelativeLocationAndRotation(FVector(Where.X, Where.Y, 0.0), FRotator(0.0, Angle, 0.0));
			Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Panel->RegisterComponent();
			// The real panel stands on the ground at its origin; the placeholder is a box lifted by half its height.
			DIAAssets::SetModelOrFallback(Panel, TEXT("SM_Fence"), DIAAssets::EFallback::Cube, DIAModelSizes::FenceCm,
				FVector(0.0, 0.0, DIAModelSizes::FenceCm.Z * 0.5), TEXT("DIA_Steel"));
			Panels.Add(Panel);
		}
	}
}

void ADIASiteFence::BuildWall()
{
	for (const DIASite::FArc& Arc : DIASite::WallArcs())
	{
		// A straight box standing on the tangent: its length is the tangent's, so that neighbouring boxes meet without a gap.
		const double Length = 2.0 * AsphaltLayout::SiteRadiusCm * FMath::Tan(FMath::DegreesToRadians(Arc.LengthDeg() * 0.5));
		const FVector2D Where = CirclePoint(Arc.MidDeg());

		UBoxComponent* Wall = NewObject<UBoxComponent>(this);
		Wall->SetupAttachment(GetRootComponent());
		Wall->SetBoxExtent(FVector(DIASite::WallThicknessCm * 0.5, Length * 0.5, DIASite::WallHeightCm * 0.5));
		Wall->SetRelativeLocationAndRotation(FVector(Where.X, Where.Y, DIASite::WallHeightCm * 0.5), FRotator(0.0, Arc.MidDeg(), 0.0));
		Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		// The camera boom is never pushed in by the wall.
		Wall->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Wall->SetHiddenInGame(true);
		Wall->RegisterComponent();
		Walls.Add(Wall);
	}
}
