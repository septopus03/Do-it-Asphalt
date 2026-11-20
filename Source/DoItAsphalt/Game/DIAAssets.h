// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMeshComponent;
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

	/**
	 * Gives the component the model /Game/Models/<Name>. If it does not exist the engine shape is used instead: scaled to
	 * SizeCm, moved by FallbackOffsetCm (added to the component's relative location, so the shape occupies the place of the
	 * real model) and painted with the palette material. Returns true if the real model was used.
	 */
	DOITASPHALT_API bool SetModelOrFallback(UStaticMeshComponent* Component, FName Name, EFallback Shape, const FVector& SizeCm,
		const FVector& FallbackOffsetCm, FName PaletteName);

	/** Loads the palette material instance /Game/Materials/MI_<Name>; null if it does not exist. */
	DOITASPHALT_API UMaterialInterface* Palette(FName Name);

	/**
	 * Loads /Game/Materials/M_DIA_Text, the unlit text material for UTextRenderComponent (the engine's default one is lit and its specular
	 * haze turns dark letters pale in daylight); null if it does not exist, then the component keeps its default material.
	 */
	DOITASPHALT_API UMaterialInterface* TextMaterial();
}
