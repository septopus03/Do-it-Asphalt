// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltDressing.h"
#include "AsphaltLayout.h"
#include "AsphaltTest.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Game/DIAGameMode.h"
#include "Game/DIAModelSizes.h"
#include "Player/DIACharacter.h"
#include "Player/DIAPlayerController.h"
#include "Tests/DIATestWorld.h"
#include "UI/DIAFormat.h"
#include "World/DIAExitGate.h"
#include "World/DIASiteDressing.h"

namespace
{
	constexpr double DressTestToleranceCm = 0.5;
	constexpr double DressTestYawToleranceDeg = 0.5;
	constexpr double DressTestLevelStartZCm = 100.0;
}

DIA_TEST(FSiteDressingBuildsTest, "DoItAsphalt.Game.SiteDressingBuilds")

bool FSiteDressingBuildsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	TArray<ADIASiteDressing*> Found;
	for (TActorIterator<ADIASiteDressing> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	if (!TestEqual(TEXT("exactly one dressing actor"), Found.Num(), 1) || !TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	Mode->EnsureDressing();
	int32 Count = 0;
	for (TActorIterator<ADIASiteDressing> It(World); It; ++It)
	{
		++Count;
	}
	TestEqual(TEXT("EnsureDressing does not spawn a second one"), Count, 1);

	const ADIASiteDressing* Dressing = Found[0];
	if (!TestEqual(TEXT("64 items"), Dressing->GetNumItems(), AsphaltDressing::NumItems))
	{
		return false;
	}
	const FHexMap Map;
	const TArray<FDressingItem> Expected = AsphaltDressing::Build(Map);
	int32 NumPoles = 0;
	for (int32 I = 0; I < Dressing->GetNumItems(); ++I)
	{
		const FDressingItem& Item = Dressing->GetItem(I);
		TestTrue(TEXT("the item is the one the core lays out"), Item.Kind == Expected[I].Kind && Item.Pos == Expected[I].Pos && Item.YawDeg == Expected[I].YawDeg);

		const UStaticMeshComponent* Mesh = Dressing->GetMesh(I);
		const UStaticMesh* Model = Mesh ? Mesh->GetStaticMesh().Get() : nullptr;
		if (!TestNotNull(TEXT("a mesh"), Mesh) || !TestNotNull(TEXT("with a static mesh"), Model))
		{
			continue;
		}
		// A real model (not the placeholder shape) that stands on the ground at its origin.
		TestTrue(TEXT("the model comes from /Game/Models"), Model->GetPathName().StartsWith(TEXT("/Game/Models/")));
		TestEqual(TEXT("the model's feet are at its origin"), Model->GetBoundingBox().Min.Z, 0.0, DressTestToleranceCm);
		const FVector Where = Mesh->GetComponentLocation();
		TestTrue(TEXT("it stands where the core says, on the ground"), Where.Equals(FVector(Item.Pos.X, Item.Pos.Y, 0.0), DressTestToleranceCm));
		TestEqual(TEXT("turned by the layout's yaw"), FMath::Abs(FMath::FindDeltaAngleDegrees(Mesh->GetComponentRotation().Yaw, Item.YawDeg)), 0.0, DressTestYawToleranceDeg);

		const bool bSolid = Item.Kind == EDressingKind::Barrel || Item.Kind == EDressingKind::DirtPile;
		TestTrue(TEXT("barrels and dirt piles block pawns, cones and the lamp model do not"),
			(Mesh->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision) == bSolid);
		if (bSolid)
		{
			TestEqual(TEXT("a solid prop does not block the camera"), static_cast<int32>(Mesh->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));
		}

		const UBoxComponent* Pole = Dressing->GetPole(I);
		if (Item.Kind == EDressingKind::Lamp)
		{
			++NumPoles;
			if (TestNotNull(TEXT("a lamp has a pole"), Pole))
			{
				TestEqual(TEXT("the pole blocks pawns"), static_cast<int32>(Pole->GetCollisionResponseToChannel(ECC_Pawn)), static_cast<int32>(ECR_Block));
				TestEqual(TEXT("the pole stands on the ground"), Pole->GetComponentLocation().Z - Pole->GetScaledBoxExtent().Z, 0.0, DressTestToleranceCm);
				TestEqual(TEXT("the pole is the size of the model's pole"), Pole->GetScaledBoxExtent().Z * 2.0, DIAModelSizes::LampPoleCm.Z, DressTestToleranceCm);
			}
			// The arm (+Y of the model) points at the lot.
			const FVector Arm = Mesh->GetComponentTransform().TransformVectorNoScale(FVector(0.0, 1.0, 0.0));
			TestTrue(TEXT("the lamp faces the lot"), FVector2D::DotProduct(FVector2D(Arm), -Item.Pos.GetSafeNormal()) > 0.999);
		}
		else
		{
			TestNull(TEXT("only lamps have a pole"), Pole);
		}
	}
	TestEqual(TEXT("six poles"), NumPoles, AsphaltDressing::NumLamps);
	return true;
}

DIA_TEST(FExitSignTextTest, "DoItAsphalt.Game.ExitSignText")

bool FExitSignTextTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	const ADIAExitGate* Gate = nullptr;
	for (TActorIterator<ADIAExitGate> It(World); It; ++It)
	{
		Gate = *It;
	}
	if (!TestNotNull(TEXT("exit gate"), Gate))
	{
		return false;
	}
	// The sign board has the word on both faces; each text looks away from the beam, towards the lot (-X) and towards the road (+X).
	TestNull(TEXT("no sign beyond the faces"), Gate->GetSignText(DIAExitGate::SignFaceCount));
	for (int32 Side = 0; Side < DIAExitGate::SignFaceCount; ++Side)
	{
		const UTextRenderComponent* Sign = Gate->GetSignText(Side);
		if (!TestNotNull(TEXT("sign text"), Sign))
		{
			continue;
		}
		TestEqual(TEXT("the board says EXIT"), Sign->Text.ToString(), TEXT("EXIT"));
		TestEqual(TEXT("the same as the format helper"), Sign->Text.ToString(), DIAFormat::ExitSign().ToString());
		const double Facing = Side == 0 ? -1.0 : 1.0;
		TestTrue(TEXT("the text looks away from the beam"), FVector::DotProduct(Sign->GetForwardVector(), Gate->GetActorForwardVector() * Facing) > 0.999);
		TestTrue(TEXT("the text stands just in front of its face of the board"),
			FMath::IsNearlyEqual(FVector::DotProduct(Sign->GetComponentLocation() - Gate->GetActorLocation(), Gate->GetActorForwardVector() * Facing),
				DIAExitGate::SignFaceXCm + DIAExitGate::SignTextOffsetCm, DressTestToleranceCm));
		TestEqual(TEXT("at the middle of the board"), Sign->GetComponentLocation().Z, DIAExitGate::SignCentreZCm, DressTestToleranceCm);
		TestEqual(TEXT("the letters have the sign colour"), Sign->TextRenderColor, DIAExitGate::SignColor);
		TestEqual(TEXT("the letters have the sign height"), static_cast<double>(Sign->WorldSize), DIAExitGate::SignTextSizeCm, 0.01);
	}
	return true;
}

DIA_TEST(FSpawnFacingTest, "DoItAsphalt.Game.SpawnFacing")

bool FSpawnFacingTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Wrapper;
	if (!DIATest::StartGameWorld(Wrapper, *this))
	{
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	ADIAGameMode* Mode = World->GetAuthGameMode<ADIAGameMode>();
	if (!TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}

	// The real level's PlayerStart sits at the layout's start but looks along +X (yaw 0). A new player must still end up facing the plant.
	FActorSpawnParameters StartParams;
	StartParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* LevelStart = World->SpawnActor<APlayerStart>(FVector(AsphaltLayout::PlayerStart.X, AsphaltLayout::PlayerStart.Y, DressTestLevelStartZCm), FRotator::ZeroRotator, StartParams);
	if (!TestNotNull(TEXT("level player start"), LevelStart))
	{
		return false;
	}
	APlayerController* Controller = World->SpawnActor<ADIAPlayerController>();
	AActor* Start = Mode->ChoosePlayerStart(Controller);
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("start"), Start))
	{
		return false;
	}
	TestTrue(TEXT("the level's start is the one used"), Start == LevelStart);
	TestEqual(TEXT("the level's start looks along +X"), FMath::Abs(FMath::FindDeltaAngleDegrees(Start->GetActorRotation().Yaw, 0.0)), 0.0, DressTestYawToleranceDeg);
	Mode->RestartPlayer(Controller);
	const APawn* Pawn = Controller->GetPawn();
	if (!TestNotNull(TEXT("the player has a pawn"), Pawn))
	{
		return false;
	}
	TestTrue(TEXT("at the layout's start"), FVector2D(Pawn->GetActorLocation()).Equals(AsphaltLayout::PlayerStart, 1.0));
	const double Yaw = AsphaltLayout::PlayerStartYawDeg;
	TestEqual(TEXT("the body faces the plant"), FMath::Abs(FMath::FindDeltaAngleDegrees(Pawn->GetActorRotation().Yaw, Yaw)), 0.0, DressTestYawToleranceDeg);
	TestEqual(TEXT("the camera looks at the plant"), FMath::Abs(FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Yaw, Yaw)), 0.0, DressTestYawToleranceDeg);
	const FVector2D Ahead(Pawn->GetActorForwardVector());
	TestTrue(TEXT("the plant is ahead of the player"), FVector2D::DotProduct(Ahead, (AsphaltLayout::PlantPos - AsphaltLayout::PlayerStart).GetSafeNormal()) > 0.999);
	return true;
}
