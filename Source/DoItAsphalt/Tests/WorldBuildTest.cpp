// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Algo/Count.h"
#include "AsphaltBalance.h"
#include "EngineUtils.h"
#include "GameMapsSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/DIAAssets.h"
#include "Game/DIAGameMode.h"
#include "Tests/DIATestWorld.h"
#include "Game/DIAWorldSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Tests/AutomationCommon.h"
#include "World/DIAGroundActor.h"

namespace
{
	/** Texel of a texture that spans the plot containing the world coordinate. */
	int32 TexelIndex(double WorldX, double WorldY, int32 Size)
	{
		const double TexelCm = DIAGround::PlotSizeCm / Size;
		const int32 X = FMath::FloorToInt((WorldX + FCoverageGrid::HalfExtentCm) / TexelCm);
		const int32 Y = FMath::FloorToInt((WorldY + FCoverageGrid::HalfExtentCm) / TexelCm);
		return Y * Size + X;
	}
}

DIA_TEST(FWorldBuildsTest, "DoItAsphalt.Game.WorldBuilds")

bool FWorldBuildsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("project default game mode"), UGameMapsSettings::GetGlobalDefaultGameMode(), FString(TEXT("/Script/DoItAsphalt.DIAGameMode")));

	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();

	TestNotNull(TEXT("game mode is ADIAGameMode"), Cast<ADIAGameMode>(World->GetAuthGameMode()));

	int32 GroundCount = 0;
	ADIAGroundActor* Ground = nullptr;
	for (TActorIterator<ADIAGroundActor> It(World); It; ++It)
	{
		Ground = *It;
		++GroundCount;
	}
	TestEqual(TEXT("exactly one ground actor"), GroundCount, 1);

	UDIAWorldSubsystem* Subsystem = World->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	TestEqual(TEXT("499 cells"), Subsystem->GetGame().GetMap().Num(), 499);

	if (TestNotNull(TEXT("ground"), Ground))
	{
		TestNotNull(TEXT("ground material built from M_Ground"), Ground->GetGroundMaterial());
		UTexture2D* Coverage = Ground->GetCoverageTexture();
		UTexture2D* Hex = Ground->GetHexLinesTexture();
		if (TestNotNull(TEXT("coverage texture"), Coverage) && TestNotNull(TEXT("hex lines texture"), Hex))
		{
			TestEqual(TEXT("coverage size"), Coverage->GetSizeX(), FCoverageGrid::Size);
			TestEqual(TEXT("hex lines size"), Hex->GetSizeX(), DIAGround::HexTexSize);
			for (UTexture2D* Texture : {Coverage, Hex})
			{
				TestFalse(TEXT("texture is linear"), Texture->SRGB);
				TestTrue(TEXT("texture clamps in X"), Texture->AddressX == TA_Clamp);
				TestTrue(TEXT("texture clamps in Y"), Texture->AddressY == TA_Clamp);
				TestTrue(TEXT("texture is a single channel"), Texture->GetPixelFormat() == PF_G8);
			}
			if (UMaterialInstanceDynamic* Material = Ground->GetGroundMaterial())
			{
				UTexture* Bound = nullptr;
				TestTrue(TEXT("CoverageTex is bound"), Material->GetTextureParameterValue(TEXT("CoverageTex"), Bound) && Bound == Coverage);
				TestTrue(TEXT("HexLinesTex is bound"), Material->GetTextureParameterValue(TEXT("HexLinesTex"), Bound) && Bound == Hex);
			}
		}
	}
	return true;
}

DIA_TEST(FSubsystemDispatchTest, "DoItAsphalt.Game.SubsystemDispatch")

bool FSubsystemDispatchTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = Wrapper.GetTestWorld()->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}

	int32 CellBroadcasts = 0;
	int32 CellsReported = 0;
	TArray<int32> Roads;
	int32 StateChanges = 0;
	Subsystem->OnCellsCompleted.AddLambda([&](const TArray<int32>& Cells)
	{
		++CellBroadcasts;
		CellsReported += Cells.Num();
	});
	Subsystem->OnRoadCompleted.AddLambda([&](int32 Road) { Roads.Add(Road); });
	Subsystem->OnStateChanged.AddLambda([&]() { ++StateChanges; });

	// Road 0 consists of the cells 469..473.
	FAsphaltGame& Game = Subsystem->GetGame();
	for (int32 Cell = 469; Cell <= 473; ++Cell)
	{
		TestFalse(TEXT("road cell starts incomplete"), Game.GetGrid().IsCellComplete(Cell));
		Subsystem->Dispatch(Game.DebugCompleteCell(Cell));
	}

	TestEqual(TEXT("OnCellsCompleted once per dispatch"), CellBroadcasts, 5);
	TestEqual(TEXT("five cells reported"), CellsReported, 5);
	if (TestEqual(TEXT("OnRoadCompleted fires once"), Roads.Num(), 1))
	{
		TestEqual(TEXT("the completed road is road 0"), Roads[0], 0);
	}
	TestTrue(TEXT("OnStateChanged fires for every dispatch"), StateChanges >= 5);
	TestTrue(TEXT("the exit stays closed"), !Game.IsExitOpen());

	// Buying through the subsystem announces the special and the state.
	Game.DebugGive(1000000.0, 0);
	int32 Specials = 0;
	Subsystem->OnSpecialBought.AddLambda([&](EAsphaltSpecial) { ++Specials; });
	const int32 StatesBefore = StateChanges;
	TestTrue(TEXT("paver car (road 0) can be bought"), Subsystem->BuySpecial(EAsphaltSpecial::PaverCar));
	TestEqual(TEXT("OnSpecialBought fired"), Specials, 1);
	TestTrue(TEXT("a purchase changes the state"), StateChanges > StatesBefore);
	TestFalse(TEXT("buying it twice fails"), Subsystem->BuySpecial(EAsphaltSpecial::PaverCar));
	TestEqual(TEXT("a failed purchase announces nothing"), Specials, 1);
	return true;
}

DIA_TEST(FGroundTexturesTest, "DoItAsphalt.Game.GroundTextures")

bool FGroundTexturesTest::RunTest(const FString& Parameters)
{
	// Outline texture content: edge texels are white, cell centres and the outside of the map are black.
	TArray<uint8> Hex;
	const double Start = FPlatformTime::Seconds();
	ADIAGroundActor::BuildHexLines(Hex);
	const double Seconds = FPlatformTime::Seconds() - Start;
	AddInfo(FString::Printf(TEXT("hex outline texture built in %.0f ms"), Seconds * 1000.0));
	TestTrue(TEXT("hex outline texture is built in well under 2 s"), Seconds < 2.0);

	constexpr int32 Size = DIAGround::HexTexSize;
	if (!TestEqual(TEXT("hex texel count"), Hex.Num(), Size * Size))
	{
		return false;
	}
	const double ApothemCm = FMath::Sqrt(3.0) * 0.5 * AsphaltHex::SideCm;
	// Edge between cell {0,0} and cell {1,0}: halfway along +X, a texel width away from the line itself.
	TestEqual(TEXT("edge between two cells is white"), static_cast<int32>(Hex[TexelIndex(ApothemCm - 0.4, 0.0, Size)]), 255);
	TestEqual(TEXT("cell centre is black"), static_cast<int32>(Hex[TexelIndex(0.0, 0.0, Size)]), 0);
	TestEqual(TEXT("cell interior is black"), static_cast<int32>(Hex[TexelIndex(20.0, 15.0, Size)]), 0);
	TestEqual(TEXT("outside the map is black"), static_cast<int32>(Hex[TexelIndex(1500.0, 1500.0, Size)]), 0);
	const int32 Count = Algo::CountIf(Hex, [](uint8 V) { return V != 0; });
	TestTrue(TEXT("a sensible share of texels is outline"), Count > 20000 && Count < 600000);

	// The ground follows the grid: a changed area is consumed (uploaded) when the state changes.
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UDIAWorldSubsystem* Subsystem = Wrapper.GetTestWorld()->GetSubsystem<UDIAWorldSubsystem>();
	if (!TestNotNull(TEXT("world subsystem"), Subsystem))
	{
		return false;
	}
	FAsphaltGame& Game = Subsystem->GetGame();
	FIntRect Rect;
	TestFalse(TEXT("nothing dirty after the ground has uploaded the whole grid"), Game.GetGrid().ConsumeDirtyRect(Rect));
	const FAsphaltEvents Events = Game.DebugCompleteCell(0);
	TestEqual(TEXT("cell 0 completed"), Events.CompletedCells.Num(), 1);
	Subsystem->Dispatch(Events);
	TestFalse(TEXT("the ground consumed the changed area"), Game.GetGrid().ConsumeDirtyRect(Rect));
	return true;
}

DIA_TEST(FAssetFallbackTest, "DoItAsphalt.Game.AssetFallback")

bool FAssetFallbackTest::RunTest(const FString& Parameters)
{
	FVector Scale = FVector::ZeroVector;
	UStaticMesh* Cube = DIAAssets::LoadMeshOrFallback(TEXT("SM_DoesNotExist"), DIAAssets::EFallback::Cube, FVector(200.0, 50.0, 100.0), Scale);
	if (TestNotNull(TEXT("fallback cube"), Cube))
	{
		TestTrue(TEXT("it is the engine cube"), Cube->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube")));
	}
	TestTrue(TEXT("scale yields the requested size"), Scale.Equals(FVector(2.0, 0.5, 1.0)));

	UStaticMesh* Cylinder = DIAAssets::LoadMeshOrFallback(TEXT("SM_DoesNotExist"), DIAAssets::EFallback::Cylinder, FVector(50.0), Scale);
	if (TestNotNull(TEXT("fallback cylinder"), Cylinder))
	{
		TestTrue(TEXT("it is the engine cylinder"), Cylinder->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cylinder")));
	}

	TestNotNull(TEXT("palette material exists"), DIAAssets::Palette(TEXT("DIA_Dirt")));
	TestNull(TEXT("unknown palette material"), DIAAssets::Palette(TEXT("DoesNotExist")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
