// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;

namespace DIAAssets
{
	/** Engine basic shape (all 100 cm) used while a model has not been imported yet. */
	enum class EFallback : uint8
	{
		Cube,
		Cylinder,
		Cone,
		Sphere,
	};

	/**
	 * Loads /Game/Models/<Name>. If it does not exist, returns the engine basic shape and sets OutScale so that the
	 * shape measures SizeCm; a real model keeps OutScale = 1. Never returns null unless the engine shape is missing.
	 */
	DOITASPHALT_API UStaticMesh* LoadMeshOrFallback(FName Name, EFallback Shape, FVector SizeCm, FVector& OutScale);

	/** Loads the palette material instance /Game/Materials/MI_<Name>; null if it does not exist. */
	DOITASPHALT_API UMaterialInterface* Palette(FName Name);
}
