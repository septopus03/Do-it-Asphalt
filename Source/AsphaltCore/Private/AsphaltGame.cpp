// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltGame.h"
#include "AsphaltBalance.h"

namespace
{
	/** Metres to centimetres: the balance tables give the tool width in metres. */
	constexpr double CmPerMetre = 100.0;
	/**
	 * Longest span a single Tick call simulates (one day). Only a guard against absurd Dt values such as 1e15,
	 * which would otherwise keep the loop running for ever; real callers pass a frame time.
	 */
	constexpr double MaxTickSeconds = 86400.0;
	/** Coordinates beyond this are far outside the site; they are clamped so the stamp geometry stays finite. */
	constexpr double MaxCoordinateCm = 1.0e6;

	int32 MaxLevelOf(EAsphaltUpgrade Upgrade)
	{
		switch (Upgrade)
		{
		case EAsphaltUpgrade::Backpack:
			return static_cast<int32>(UE_ARRAY_COUNT(AsphaltBalance::BackpackCapacityL)) - 1;
		case EAsphaltUpgrade::Tool:
			return static_cast<int32>(UE_ARRAY_COUNT(AsphaltBalance::ToolWidthM)) - 1;
		case EAsphaltUpgrade::Plant:
			return static_cast<int32>(UE_ARRAY_COUNT(AsphaltBalance::PlantRateLPerMin)) - 1;
		}
		return 0;
	}

	bool IsFinite(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}

	FVector2D ClampToWorld(const FVector2D& V)
	{
		return FVector2D(FMath::Clamp(V.X, -MaxCoordinateCm, MaxCoordinateCm), FMath::Clamp(V.Y, -MaxCoordinateCm, MaxCoordinateCm));
	}
}

void FAsphaltEvents::Append(const FAsphaltEvents& Other)
{
	LitresPoured += Other.LitresPoured;
	MoneyEarned += Other.MoneyEarned;
	ShardsEarned += Other.ShardsEarned;
	CompletedCells.Append(Other.CompletedCells);
	CompletedRoads.Append(Other.CompletedRoads);
	bExitOpened = bExitOpened || Other.bExitOpened;
}

FAsphaltGame::FAsphaltGame()
	: Grid(Map)
{
}

const FHexMap& FAsphaltGame::GetMap() const
{
	return Map;
}

FCoverageGrid& FAsphaltGame::GetGrid()
{
	return Grid;
}

const FCoverageGrid& FAsphaltGame::GetGrid() const
{
	return Grid;
}

double FAsphaltGame::GetMoney() const
{
	return Money;
}

int32 FAsphaltGame::GetShards() const
{
	return Shards;
}

double FAsphaltGame::GetPlaytimeSeconds() const
{
	return PlaytimeSeconds;
}

double FAsphaltGame::GetTotalLitresPoured() const
{
	return TotalLitresPoured;
}

int32 FAsphaltGame::GetLevel(EAsphaltUpgrade Upgrade) const
{
	return Levels[static_cast<int32>(Upgrade)];
}

int32 FAsphaltGame::GetMaxLevel(EAsphaltUpgrade Upgrade) const
{
	return MaxLevelOf(Upgrade);
}

double FAsphaltGame::GetNextPrice(EAsphaltUpgrade Upgrade) const
{
	const int32 Level = GetLevel(Upgrade);
	if (Level >= MaxLevelOf(Upgrade))
	{
		return -1.0;
	}
	switch (Upgrade)
	{
	case EAsphaltUpgrade::Backpack:
		return AsphaltBalance::BackpackPriceShards[Level];
	case EAsphaltUpgrade::Tool:
		return AsphaltBalance::ToolPriceMoney[Level];
	case EAsphaltUpgrade::Plant:
		return AsphaltBalance::PlantPriceMoney[Level];
	}
	return -1.0;
}

bool FAsphaltGame::CanBuy(EAsphaltUpgrade Upgrade) const
{
	const double Price = GetNextPrice(Upgrade);
	if (Price < 0.0)
	{
		return false;
	}
	return Upgrade == EAsphaltUpgrade::Backpack ? Shards >= Price : Money >= Price;
}

bool FAsphaltGame::Buy(EAsphaltUpgrade Upgrade)
{
	if (!CanBuy(Upgrade))
	{
		return false;
	}
	const double Price = GetNextPrice(Upgrade);
	if (Upgrade == EAsphaltUpgrade::Backpack)
	{
		Shards -= static_cast<int32>(Price);
	}
	else
	{
		Money -= Price;
	}
	++Levels[static_cast<int32>(Upgrade)];
	ClampToCapacities();
	return true;
}

double FAsphaltGame::GetBackpackCapacity() const
{
	return AsphaltBalance::BackpackCapacityL[GetLevel(EAsphaltUpgrade::Backpack)];
}

double FAsphaltGame::GetBackpackLitres() const
{
	return BackpackLitres;
}

double FAsphaltGame::GetToolWidthCm() const
{
	return CmPerMetre * AsphaltBalance::ToolWidthM[GetLevel(EAsphaltUpgrade::Tool)];
}

double FAsphaltGame::GetPlantRatePerMin() const
{
	return AsphaltBalance::PlantRateLPerMin[GetLevel(EAsphaltUpgrade::Plant)];
}

double FAsphaltGame::GetPlantStorage() const
{
	return GetPlantRatePerMin() * AsphaltBalance::PlantStorageMinutes;
}

double FAsphaltGame::GetPlantStock() const
{
	return PlantStock;
}

FAsphaltEvents FAsphaltGame::Tick(double Dt)
{
	FAsphaltEvents Events;
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return Events;
	}

	double Remaining = FMath::Min(Dt, MaxTickSeconds);
	while (Remaining > 0.0)
	{
		const double Step = FMath::Min(Remaining, MaxStepSeconds);
		TickStep(Step);
		Remaining -= Step;
	}
	return Events;
}

void FAsphaltGame::TickStep(double Step)
{
	PlaytimeSeconds += Step;
	PlantStock = FMath::Min(PlantStock + GetPlantRatePerMin() / 60.0 * Step, GetPlantStorage());
}

double FAsphaltGame::RefillBackpack(double Dt)
{
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return 0.0;
	}

	const double Capacity = GetBackpackCapacity();
	const double ByRate = Capacity / AsphaltBalance::RefillSeconds * Dt;
	const double Moved = FMath::Max(0.0, FMath::Min3(ByRate, Capacity - BackpackLitres, PlantStock));
	BackpackLitres = FMath::Min(BackpackLitres + Moved, Capacity);
	PlantStock = FMath::Max(0.0, PlantStock - Moved);
	return Moved;
}

FAsphaltEvents FAsphaltGame::PourFromBackpack(FVector2D A, FVector2D B)
{
	if (!(BackpackLitres > 0.0) || !IsFinite(A) || !IsFinite(B))
	{
		return FAsphaltEvents();
	}

	const FPourResult Result = Grid.Stamp(ClampToWorld(A), ClampToWorld(B), GetToolWidthCm(), BackpackLitres);
	BackpackLitres = FMath::Max(0.0, BackpackLitres - Result.LitresUsed);
	return ApplyPourResult(Result);
}

FAsphaltEvents FAsphaltGame::ApplyPourResult(const FPourResult& Result)
{
	FAsphaltEvents Events;
	Events.LitresPoured = Result.LitresUsed;
	Events.MoneyEarned = Result.LitresUsed * AsphaltBalance::MoneyPerLitre;
	Events.ShardsEarned = Result.CompletedCells.Num() * AsphaltBalance::ShardsPerCell;
	Events.CompletedCells = Result.CompletedCells;

	TotalLitresPoured += Events.LitresPoured;
	Money += Events.MoneyEarned;
	Shards += Events.ShardsEarned;
	return Events;
}

void FAsphaltGame::DebugGive(double InMoney, int32 InShards)
{
	if (FMath::IsFinite(InMoney))
	{
		Money = FMath::Max(0.0, Money + InMoney);
	}
	Shards = FMath::Max(0, Shards + InShards);
}

void FAsphaltGame::DebugSetLevel(EAsphaltUpgrade Upgrade, int32 Level)
{
	Levels[static_cast<int32>(Upgrade)] = FMath::Clamp(Level, 0, MaxLevelOf(Upgrade));
	ClampToCapacities();
}

FAsphaltEvents FAsphaltGame::DebugCompleteCell(int32 Cell)
{
	return ApplyPourResult(Grid.CompleteCell(Cell));
}

void FAsphaltGame::ClampToCapacities()
{
	BackpackLitres = FMath::Min(BackpackLitres, GetBackpackCapacity());
	PlantStock = FMath::Min(PlantStock, GetPlantStorage());
}
