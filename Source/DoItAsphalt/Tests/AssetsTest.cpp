// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "Engine/StaticMesh.h"
#include "Game/DIAAssets.h"
#include "Materials/MaterialInterface.h"

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
