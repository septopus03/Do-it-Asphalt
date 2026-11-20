// Copyright Do it Asphalt. All Rights Reserved.

#include "World/DIAGroundActor.h"
#include "DoItAsphalt.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAWorldSubsystem.h"
#include "Async/ParallelFor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Colour of one grid texel in the R8G8 coverage texture. */
	void EncodeTexel(uint8 State, uint8* Out)
	{
		Out[0] = State != static_cast<uint8>(ELotTexel::Free) ? 255 : 0;
		Out[1] = State == static_cast<uint8>(ELotTexel::Done) ? 255 : 0;
	}
}

ADIAGroundActor::ADIAGroundActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// One slab under the whole area (plot and surround) so that the player can also walk outside the plot.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Root);
	Collision->SetBoxExtent(FVector(DIAGround::SurroundHalfExtentCm, DIAGround::SurroundHalfExtentCm, DIAGround::CollisionDepthCm * 0.5));
	Collision->SetRelativeLocation(FVector(0.0, 0.0, -DIAGround::CollisionDepthCm * 0.5));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	PlotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlotMesh"));
	PlotMesh->SetupAttachment(Root);
	PlotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlotMesh->SetCastShadow(false);

	SurroundMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SurroundMesh"));
	SurroundMesh->SetupAttachment(Root);
	SurroundMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurroundMesh->SetCastShadow(false);
}

void ADIAGroundActor::BeginPlay()
{
	Super::BeginPlay();

	// Plane: 100 x 100 cm in XY, normal +Z. The surround lies a little lower than the plot.
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"), nullptr, LOAD_Quiet | LOAD_NoWarn);
	if (!Plane)
	{
		UE_LOG(LogDoItAsphalt, Error, TEXT("Ground: /Engine/BasicShapes/Plane is missing"));
	}

	const double PlotScale = DIAGround::PlotSizeCm / 100.0;
	PlotMesh->SetStaticMesh(Plane);
	PlotMesh->SetRelativeLocation(FVector::ZeroVector);
	PlotMesh->SetRelativeScale3D(FVector(PlotScale, PlotScale, 1.0));

	const double SurroundScale = 2.0 * DIAGround::SurroundHalfExtentCm / 100.0;
	SurroundMesh->SetStaticMesh(Plane);
	SurroundMesh->SetRelativeScale3D(FVector(SurroundScale, SurroundScale, 1.0));
	SurroundMesh->SetRelativeLocation(FVector(0.0, 0.0, -DIAGround::SurroundDropCm));
	if (UMaterialInterface* Dirt = DIAAssets::Palette(TEXT("DIA_Dirt")))
	{
		SurroundMesh->SetMaterial(0, Dirt);
	}

	// Coverage texture: R8G8, built from the whole grid.
	UDIAWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogDoItAsphalt, Error, TEXT("Ground: no UDIAWorldSubsystem in this world"));
		return;
	}
	constexpr int32 Size = FLotGrid::Size;
	FLotGrid& Grid = Subsystem->GetGame().GetGrid();
	const TConstArrayView<uint8> Texels = Grid.GetTexels();
	Mirror.SetNumUninitialized(static_cast<int64>(Size) * Size * 2);
	uint8* Out = Mirror.GetData();
	ParallelFor(Size, [&Texels, Out](int32 Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			EncodeTexel(Texels[Y * Size + X], Out + (static_cast<int64>(Y) * Size + X) * 2);
		}
	});
	UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_R8G8, FName(TEXT("T_Runtime_Coverage")), Mirror);
	if (Texture)
	{
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_Masks;
#if WITH_EDITORONLY_DATA
		// Editor-only property; a transient texture made at runtime has just the one mip anyway.
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		// The properties above change how the resource is created, so create it again.
		Texture->UpdateResource();
	}
	CoverageTexture = Texture;
	// The texture now shows the whole grid; whatever was dirty before is included.
	TArray<FIntRect> Ignored;
	Grid.ConsumeDirtyRects(Ignored);

	// Material.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Ground.M_Ground"), nullptr, LOAD_Quiet | LOAD_NoWarn);
	if (Base)
	{
		GroundMaterial = UMaterialInstanceDynamic::Create(Base, this);
		GroundMaterial->SetTextureParameterValue(TEXT("CoverageTex"), CoverageTexture);
		PlotMesh->SetMaterial(0, GroundMaterial);
	}
	else
	{
		UE_LOG(LogDoItAsphalt, Error, TEXT("Ground: /Game/Materials/M_Ground is missing"));
	}

	StateSub.Bind(Subsystem, this, &ADIAGroundActor::OnStateChanged, &ADIAGroundActor::HandleGameReset);
}

void ADIAGroundActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StateSub.Unbind(UDIAWorldSubsystem::Get(this));
	Super::EndPlay(EndPlayReason);
}

void ADIAGroundActor::OnStateChanged()
{
	SyncCoverage();
}

void ADIAGroundActor::HandleGameReset()
{
	SyncCoverage(/*bFull*/ true);
}

bool ADIAGroundActor::GetCoverageTexel(int32 X, int32 Y, uint8& OutR, uint8& OutG) const
{
	constexpr int32 Size = FLotGrid::Size;
	if (X < 0 || Y < 0 || X >= Size || Y >= Size || Mirror.Num() != static_cast<int64>(Size) * Size * 2)
	{
		return false;
	}
	const int64 I = (static_cast<int64>(Y) * Size + X) * 2;
	OutR = Mirror[I];
	OutG = Mirror[I + 1];
	return true;
}

void ADIAGroundActor::SyncCoverage(bool bFull)
{
	UDIAWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	if (!Subsystem || !CoverageTexture)
	{
		return;
	}
	constexpr int32 Size = FLotGrid::Size;
	FLotGrid& Grid = Subsystem->GetGame().GetGrid();

	TArray<FIntRect> Rects;
	Grid.ConsumeDirtyRects(Rects);
	if (bFull)
	{
		Rects.Reset();
		Rects.Add(FIntRect(0, 0, Size, Size));
		++NumFullUploads;
	}
	if (Rects.IsEmpty())
	{
		return;
	}

	const double Start = FPlatformTime::Seconds();
	const TConstArrayView<uint8> Texels = Grid.GetTexels();
	int64 NumTexels = 0;
	for (const FIntRect& Rect : Rects)
	{
		const int32 Width = Rect.Width();
		const int32 Height = Rect.Height();
		if (Width <= 0 || Height <= 0)
		{
			continue;
		}
		NumTexels += static_cast<int64>(Width) * Height;

		// Update the CPU copy and a private buffer for the render thread, which reads it later and must not point into the mirror.
		uint8* Copy = static_cast<uint8*>(FMemory::Malloc(static_cast<SIZE_T>(Width) * Height * 2));
		for (int32 Row = 0; Row < Height; ++Row)
		{
			const int32 Y = Rect.Min.Y + Row;
			uint8* Line = Copy + static_cast<SIZE_T>(Row) * Width * 2;
			uint8* MirrorLine = Mirror.GetData() + (static_cast<int64>(Y) * Size + Rect.Min.X) * 2;
			for (int32 Col = 0; Col < Width; ++Col)
			{
				EncodeTexel(Texels[Y * Size + Rect.Min.X + Col], Line + Col * 2);
			}
			FMemory::Memcpy(MirrorLine, Line, static_cast<SIZE_T>(Width) * 2);
		}

		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(Rect.Min.X, Rect.Min.Y, 0, 0, Width, Height);
		CoverageTexture->UpdateTextureRegions(0, 1, Region, Width * 2, 2, Copy,
			[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(SrcData);
				delete Regions;
			});
	}

	const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
	if (NumTexels >= DIAGround::BulkyUploadTexels)
	{
		UE_LOG(LogDoItAsphalt, Log, TEXT("Ground: coverage upload of %d rect(s), %lld texels, %.1f ms"), Rects.Num(), NumTexels, Ms);
	}
	else
	{
		UE_LOG(LogDoItAsphalt, VeryVerbose, TEXT("Ground: coverage upload of %d rect(s), %lld texels, %.1f ms"), Rects.Num(), NumTexels, Ms);
	}
}
