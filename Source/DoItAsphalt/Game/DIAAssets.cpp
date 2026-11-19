// Copyright Do it Asphalt. All Rights Reserved.

#include "Game/DIAAssets.h"
#include "DoItAsphalt.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace DIAAssets
{
	namespace
	{
		/** Edge length of the engine basic shapes in cm. */
		constexpr double BasicShapeSizeCm = 100.0;

		const TCHAR* FallbackPath(EFallback Shape)
		{
			switch (Shape)
			{
			case EFallback::Cylinder: return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
			case EFallback::Cone: return TEXT("/Engine/BasicShapes/Cone.Cone");
			case EFallback::Sphere: return TEXT("/Engine/BasicShapes/Sphere.Sphere");
			case EFallback::Cube:
			default: return TEXT("/Engine/BasicShapes/Cube.Cube");
			}
		}
	}

	UStaticMesh* LoadMeshOrFallback(FName Name, EFallback Shape, FVector SizeCm, FVector& OutScale)
	{
		const FString Package = FString::Printf(TEXT("/Game/Models/%s"), *Name.ToString());
		if (FPackageName::DoesPackageExist(Package))
		{
			const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *Package, *Name.ToString());
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn))
			{
				OutScale = FVector::OneVector;
				return Mesh;
			}
			UE_LOG(LogDoItAsphalt, Warning, TEXT("Model %s exists but is not a static mesh; using the fallback shape"), *Package);
		}

		OutScale = SizeCm / BasicShapeSizeCm;
		return LoadObject<UStaticMesh>(nullptr, FallbackPath(Shape), nullptr, LOAD_Quiet | LOAD_NoWarn);
	}

	UMaterialInterface* Palette(FName Name)
	{
		const FString Package = FString::Printf(TEXT("/Game/Materials/MI_%s"), *Name.ToString());
		if (!FPackageName::DoesPackageExist(Package))
		{
			return nullptr;
		}
		const FString ObjectPath = FString::Printf(TEXT("%s.MI_%s"), *Package, *Name.ToString());
		return LoadObject<UMaterialInterface>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
	}
}
