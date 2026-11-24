// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LotBalance.h"
#include "DIAShopBuilding.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Where the three shop buildings stand and what their sign boards say (nothing here is a game rule). */
namespace DIAShopBuilding
{
	/** The model of a building: SM_ToolShop, SM_PaverDepot, SM_Garage. */
	DOITASPHALT_API const TCHAR* ModelName(ELotShop Shop);
	/** Centre of the white sign board in the model's axes (cm; the board faces +X of the model). */
	DOITASPHALT_API FVector SignCentreCm(ELotShop Shop);
	/** Width (Y) and height (Z) of the sign board's area that the name may fill (cm). */
	DOITASPHALT_API FVector2D SignAreaCm(ELotShop Shop);
	/** The building's place: LotLayout::ShopPos on the ground, turned by LotLayout::ShopYawDeg. */
	DOITASPHALT_API FTransform MakeTransform(ELotShop Shop);

	/** The text stands this far in front of the board. */
	inline constexpr double SignTextStandOffCm = 1.0;
}

/**
 * One of the three shop buildings along the south strip (the tool shop, the paver depot, the garage). The model's simple collision (boxes
 * made at import: walls, posts, roof, counter, furniture) blocks the player and the car; the garage door and the space inside stay free.
 * The name of the shop is written on the sign board. The building only stands; buying happens at its buy point (ADIAShopPoint).
 */
UCLASS(NotBlueprintable)
class DOITASPHALT_API ADIAShopBuilding : public AActor
{
	GENERATED_BODY()

public:
	ADIAShopBuilding();

	/** Which building; set before BeginPlay (spawn deferred). */
	ELotShop Kind = ELotShop::Tools;

	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UTextRenderComponent* GetTextComponent() const { return Text; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Text;
};
