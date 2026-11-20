// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Game/DIAAssets.h"
#include "Game/DIAModelSizes.h"
#include "PhysicsEngine/BodySetup.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Proves the GLB -> Unreal conversion (Tools/UE/import_models.py): SM_AxisTest is a 100 x 50 x 25 cm box whose
// centre lies on +X, +Y and +Z. A mirrored axis or a swapped pair shows up as a wrong size or a negative centre.
DIA_TEST(FAxisTestImportedTest, "DoItAsphalt.Game.AxisTestImported")

bool FAxisTestImportedTest::RunTest(const FString& Parameters)
{
	FVector Scale = FVector(7.0);
	UStaticMesh* Mesh = DIAAssets::LoadMeshOrFallback(TEXT("SM_AxisTest"), DIAAssets::EFallback::Cube, FVector(100.0), Scale);
	if (!TestNotNull(TEXT("SM_AxisTest loads"), Mesh))
	{
		return false;
	}
	// The fallback is the engine cube (/Engine/BasicShapes); an imported model lives in /Game/Models.
	TestTrue(TEXT("mesh comes from /Game/Models, not the fallback"), Mesh->GetPathName().StartsWith(TEXT("/Game/Models/SM_AxisTest.")));
	TestEqual(TEXT("a real model keeps scale 1"), Scale, FVector::OneVector);

	const FBox Box = Mesh->GetBoundingBox();
	const FVector Size = Box.GetSize();
	const FVector Center = Box.GetCenter();
	const double Tol = 1.0;
	TestEqual(TEXT("size X"), Size.X, 100.0, Tol);
	TestEqual(TEXT("size Y"), Size.Y, 50.0, Tol);
	TestEqual(TEXT("size Z"), Size.Z, 25.0, Tol);
	TestEqual(TEXT("centre X"), Center.X, 50.0, Tol);
	TestEqual(TEXT("centre Y"), Center.Y, 25.0, Tol);
	TestEqual(TEXT("centre Z"), Center.Z, 12.5, Tol);

	// Palette slot: DIA_Yellow -> /Game/Materials/MI_DIA_Yellow.
	if (TestEqual(TEXT("one material slot"), Mesh->GetStaticMaterials().Num(), 1))
	{
		UMaterialInterface* Material = Mesh->GetMaterial(0);
		UMaterialInterface* Expected = DIAAssets::Palette(TEXT("DIA_Yellow"));
		TestNotNull(TEXT("MI_DIA_Yellow exists"), Expected);
		TestTrue(TEXT("slot uses MI_DIA_Yellow"), Material == Expected);
	}
	return true;
}

// Every one of the 30 models of docs/03-3d-modely.md and the 12 of docs/04-plan-zmien-asphalt-self.md (chapter 4.1) is imported: /Game/Models/<Name> is a real mesh (not the placeholder shape
// of the game) whose envelope matches the manifest of the model generator (metres there, centimetres in the engine).
DIA_TEST(FAllModelsImportedTest, "DoItAsphalt.Game.AllModelsImported")

namespace
{
	/** How far the imported envelope may be from the manifest, in cm (the manifest itself allows 2 cm; the import rounds a little). */
	constexpr double AssetsTestEnvelopeToleranceCm = 3.0;
	constexpr double AssetsTestMetreToCm = 100.0;
	/** The placeholder size passed to LoadMeshOrFallback; it has no effect on a real model. */
	constexpr double AssetsTestPlaceholderCm = 100.0;

	/** The names of docs/03-3d-modely.md, chapter 4: the chain check, the player, the plant and pickup, roads and exit, machines, props. */
	const TCHAR* const AssetsTestModelNames[] = {
		TEXT("SM_AxisTest"),
		TEXT("SM_Spreader_020"), TEXT("SM_Spreader_030"), TEXT("SM_Spreader_050"), TEXT("SM_Spreader_080"),
		TEXT("SM_Spreader_100"), TEXT("SM_Spreader_150"), TEXT("SM_Spreader_200"),
		TEXT("SM_Backpack_S"), TEXT("SM_Backpack_M"), TEXT("SM_Backpack_L"), TEXT("SM_Backpack_XL"), TEXT("SM_Helmet"),
		TEXT("SM_Plant"), TEXT("SM_Plant_AddonA"), TEXT("SM_Plant_AddonB"), TEXT("SM_PortableStation"),
		TEXT("SM_Kiosk"), TEXT("SM_Lock"), TEXT("SM_RoadStub"), TEXT("SM_Barrier"), TEXT("SM_ExitGate_Frame"), TEXT("SM_ExitGate_Barrier"),
		TEXT("SM_AutoPaver"), TEXT("SM_PaverCar"),
		TEXT("SM_Fence"), TEXT("SM_Cone"), TEXT("SM_Barrel"), TEXT("SM_Lamp"), TEXT("SM_DirtPile"),
		// Asphalt-self (docs/04, chapter 4.1): hand tools, containers, finisher, trough, curb, wide bar, mall.
		TEXT("SM_Spoon"), TEXT("SM_Bowl"), TEXT("SM_Trowel"), TEXT("SM_Scoop"), TEXT("SM_Bucket"), TEXT("SM_Suitcase"),
		TEXT("SM_Cart"), TEXT("SM_Finisher"), TEXT("SM_Trough"), TEXT("SM_Curb"), TEXT("SM_PaverCar_WideBar"), TEXT("SM_Mall"),
	};
	constexpr int32 AssetsTestNumModels = 42;

	bool AssetsTestReadVector(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Entry->TryGetArrayField(Field, Values) || Values->Num() != 3)
		{
			return false;
		}
		Out = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber()) * AssetsTestMetreToCm;
		return true;
	}
}

bool FAllModelsImportedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("the model list has 42 names"), static_cast<int32>(UE_ARRAY_COUNT(AssetsTestModelNames)), AssetsTestNumModels);

	const FString ManifestPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/ModelGen/manifest.json"));
	FString Text;
	TArray<TSharedPtr<FJsonValue>> Entries;
	if (!TestTrue(TEXT("manifest can be read"), FFileHelper::LoadFileToString(Text, *ManifestPath))
		|| !TestTrue(TEXT("manifest is a JSON array"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Entries)))
	{
		return false;
	}
	TestEqual(TEXT("the manifest lists the same number of models"), Entries.Num(), AssetsTestNumModels);

	TMap<FString, TSharedPtr<FJsonObject>> ByName;
	for (const TSharedPtr<FJsonValue>& Value : Entries)
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		if (Entry.IsValid())
		{
			ByName.Add(Entry->GetStringField(TEXT("name")), Entry);
		}
	}

	for (const TCHAR* Name : AssetsTestModelNames)
	{
		const TSharedPtr<FJsonObject>* Found = ByName.Find(Name);
		FVector ExpectedMin, ExpectedMax;
		if (!TestTrue(FString::Printf(TEXT("%s is in the manifest with min and max"), Name), Found && AssetsTestReadVector(*Found, TEXT("min"), ExpectedMin) && AssetsTestReadVector(*Found, TEXT("max"), ExpectedMax)))
		{
			continue;
		}

		FVector Scale = FVector(7.0);
		UStaticMesh* Mesh = DIAAssets::LoadMeshOrFallback(Name, DIAAssets::EFallback::Cube, FVector(AssetsTestPlaceholderCm), Scale);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), Name), Mesh))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s comes from /Game/Models, not the fallback"), Name), Mesh->GetPathName().StartsWith(FString::Printf(TEXT("/Game/Models/%s."), Name)));
		TestEqual(*FString::Printf(TEXT("%s keeps scale 1"), Name), Scale, FVector::OneVector);

		const FBox Box = Mesh->GetBoundingBox();
		TestTrue(*FString::Printf(TEXT("%s min %s is within %.0f cm of %s"), Name, *Box.Min.ToString(), AssetsTestEnvelopeToleranceCm, *ExpectedMin.ToString()), Box.Min.Equals(ExpectedMin, AssetsTestEnvelopeToleranceCm));
		TestTrue(*FString::Printf(TEXT("%s max %s is within %.0f cm of %s"), Name, *Box.Max.ToString(), AssetsTestEnvelopeToleranceCm, *ExpectedMax.ToString()), Box.Max.Equals(ExpectedMax, AssetsTestEnvelopeToleranceCm));
	}
	return true;
}

// The mall is a 100 x 100 m block: its collision is one box over the envelope (Tools/UE/import_models.py, BOX_COLLISION_MODELS), not the
// convex hull of its facade relief, so the player stops at the wall plane of the footprint and cannot slip into a relief niche.
DIA_TEST(FMallCollisionTest, "DoItAsphalt.Game.MallCollision")

bool FMallCollisionTest::RunTest(const FString& Parameters)
{
	FVector Scale = FVector::OneVector;
	UStaticMesh* Mesh = DIAAssets::LoadMeshOrFallback(TEXT("SM_Mall"), DIAAssets::EFallback::Cube, FVector(AssetsTestPlaceholderCm), Scale);
	if (!TestNotNull(TEXT("SM_Mall loads"), Mesh) || !TestNotNull(TEXT("SM_Mall has a body setup"), Mesh->GetBodySetup()))
	{
		return false;
	}
	const FKAggregateGeom& Geometry = Mesh->GetBodySetup()->AggGeom;
	TestEqual(TEXT("exactly one box element"), Geometry.BoxElems.Num(), 1);
	TestEqual(TEXT("no convex element"), Geometry.ConvexElems.Num(), 0);
	if (Geometry.BoxElems.Num() == 1)
	{
		// The box covers the envelope: 100 x 100 x 14.3 m, centred on X = Y = 0.
		const FKBoxElem& Box = Geometry.BoxElems[0];
		TestEqual(TEXT("box length"), static_cast<double>(Box.X), DIAModelSizes::MallCm.X, AssetsTestEnvelopeToleranceCm);
		TestEqual(TEXT("box width"), static_cast<double>(Box.Y), DIAModelSizes::MallCm.Y, AssetsTestEnvelopeToleranceCm);
		TestEqual(TEXT("box height"), static_cast<double>(Box.Z), DIAModelSizes::MallCm.Z, AssetsTestEnvelopeToleranceCm);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
