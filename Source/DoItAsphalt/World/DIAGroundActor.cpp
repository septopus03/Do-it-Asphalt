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
#include "HexCoord.h"
#include "HexMap.h"
#include "Async/ParallelFor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Both textures are plain linear 8-bit masks read from the red channel (the M_Ground contract). */
	UTexture2D* MakeMaskTexture(int32 Size, TConstArrayView64<uint8> Data, const TCHAR* Name)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_G8, FName(Name), Data);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_Masks;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		// The properties above change how the resource is created, so create it again.
		Texture->UpdateResource();
		return Texture;
	}

	/** Centre of texel I of a texture that spans the plot, in cm. */
	double TexelCentreCm(int32 I, int32 TexSize)
	{
		return -FCoverageGrid::HalfExtentCm + (I + 0.5) * (DIAGround::PlotSizeCm / TexSize);
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

void ADIAGroundActor::BuildHexLines(TArray<uint8>& OutTexels)
{
	constexpr int32 Size = DIAGround::HexTexSize;
	static const FHexMap Map;

	OutTexels.Reset();
	OutTexels.SetNumZeroed(Size * Size);
	uint8* Data = OutTexels.GetData();

	ParallelFor(Size, [Data](int32 Y)
	{
		const double Wy = TexelCentreCm(Y, Size);
		for (int32 X = 0; X < Size; ++X)
		{
			const FVector2D P(TexelCentreCm(X, Size), Wy);
			// The edge distance is cheap; the map lookup only happens near an edge.
			if (AsphaltHex::EdgeDistanceCm(P) < DIAGround::HexLineHalfWidthCm &&
				Map.FindCell(AsphaltHex::FromWorld(P)) != INDEX_NONE)
			{
				Data[Y * Size + X] = 255;
			}
		}
	});
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

	// Textures.
	TArray<uint8> HexLines;
	const double Start = FPlatformTime::Seconds();
	BuildHexLines(HexLines);
	UE_LOG(LogDoItAsphalt, Log, TEXT("Ground: hex outline texture built in %.0f ms"), (FPlatformTime::Seconds() - Start) * 1000.0);
	HexLinesTexture = MakeMaskTexture(DIAGround::HexTexSize, HexLines, TEXT("T_Runtime_HexLines"));

	UDIAWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogDoItAsphalt, Error, TEXT("Ground: no UDIAWorldSubsystem in this world"));
		return;
	}
	CoverageTexture = MakeMaskTexture(FCoverageGrid::Size, Subsystem->GetGame().GetGrid().GetTexels(), TEXT("T_Runtime_Coverage"));
	// The texture now shows the whole grid; whatever was dirty before is included.
	FIntRect Ignored;
	Subsystem->GetGame().GetGrid().ConsumeDirtyRect(Ignored);

	// Material.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Ground.M_Ground"), nullptr, LOAD_Quiet | LOAD_NoWarn);
	if (Base)
	{
		GroundMaterial = UMaterialInstanceDynamic::Create(Base, this);
		GroundMaterial->SetTextureParameterValue(TEXT("CoverageTex"), CoverageTexture);
		GroundMaterial->SetTextureParameterValue(TEXT("HexLinesTex"), HexLinesTexture);
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

void ADIAGroundActor::SyncCoverage(bool bFull)
{
	UDIAWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDIAWorldSubsystem>() : nullptr;
	if (!Subsystem || !CoverageTexture)
	{
		return;
	}
	FCoverageGrid& Grid = Subsystem->GetGame().GetGrid();

	FIntRect Rect;
	const bool bDirty = Grid.ConsumeDirtyRect(Rect);
	if (bFull)
	{
		Rect = FIntRect(0, 0, FCoverageGrid::Size, FCoverageGrid::Size);
		++NumFullUploads;
	}
	else if (!bDirty)
	{
		return;
	}

	const int32 Width = Rect.Width();
	const int32 Height = Rect.Height();
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	// The render thread reads the copy later, so it must not point into the grid.
	const TConstArrayView<uint8> Texels = Grid.GetTexels();
	uint8* Copy = static_cast<uint8*>(FMemory::Malloc(static_cast<SIZE_T>(Width) * Height));
	for (int32 Row = 0; Row < Height; ++Row)
	{
		FMemory::Memcpy(Copy + static_cast<SIZE_T>(Row) * Width, &Texels[(Rect.Min.Y + Row) * FCoverageGrid::Size + Rect.Min.X], Width);
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(Rect.Min.X, Rect.Min.Y, 0, 0, Width, Height);
	CoverageTexture->UpdateTextureRegions(0, 1, Region, Width, 1, Copy,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(SrcData);
			delete Regions;
		});
}
