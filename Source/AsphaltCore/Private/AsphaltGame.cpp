// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltGame.h"
#include "AsphaltBalance.h"
#include "HexCoord.h"

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

	constexpr int32 NumSpecials = static_cast<int32>(EAsphaltSpecial::Count);

	bool IsValidSpecial(EAsphaltSpecial S)
	{
		return static_cast<int32>(S) >= 0 && static_cast<int32>(S) < NumSpecials;
	}

	FVector2D ClampToWorld(const FVector2D& V)
	{
		return FVector2D(FMath::Clamp(V.X, -MaxCoordinateCm, MaxCoordinateCm), FMath::Clamp(V.Y, -MaxCoordinateCm, MaxCoordinateCm));
	}

	/** Bytes of the packed coverage mask (one bit per texel). */
	constexpr int32 CoverageBytes = FCoverageGrid::Size * FCoverageGrid::Size / 8;
	/** Rounding slack for comparing a loaded amount of litres with its capacity. */
	constexpr double LoadSlackL = 1e-6;

	/** Appends plain values to a byte array in native (little-endian on all supported platforms) order. */
	class FSaveWriter
	{
	public:
		explicit FSaveWriter(TArray<uint8>& InOut)
			: Out(InOut)
		{
		}

		template <typename T>
		void Write(T Value)
		{
			static_assert(TIsTriviallyCopyConstructible<T>::Value, "plain values only");
			const int32 At = Out.AddUninitialized(sizeof(T));
			FMemory::Memcpy(Out.GetData() + At, &Value, sizeof(T));
		}

		void WriteBool(bool bValue)
		{
			Write<uint8>(bValue ? 1 : 0);
		}

		void WriteVector(const FVector2D& V)
		{
			Write<double>(V.X);
			Write<double>(V.Y);
		}

	private:
		TArray<uint8>& Out;
	};

	/** Reads plain values from a byte view; every read reports whether enough bytes were left. */
	class FSaveReader
	{
	public:
		explicit FSaveReader(TConstArrayView<uint8> InData)
			: Data(InData)
		{
		}

		template <typename T>
		bool Read(T& Value)
		{
			if (Data.Num() - Position < static_cast<int32>(sizeof(T)))
			{
				return false;
			}
			FMemory::Memcpy(&Value, Data.GetData() + Position, sizeof(T));
			Position += sizeof(T);
			return true;
		}

		/** A bool is stored as exactly 0 or 1; any other byte is corrupt. */
		bool ReadBool(bool& bValue)
		{
			uint8 Byte = 0;
			if (!Read(Byte) || Byte > 1)
			{
				return false;
			}
			bValue = Byte == 1;
			return true;
		}

		bool ReadVector(FVector2D& V)
		{
			return Read(V.X) && Read(V.Y) && IsFinite(V);
		}

		/** Takes the next Count bytes as a view; false if fewer are left. */
		bool ReadBlock(int32 Count, TConstArrayView<uint8>& Block)
		{
			if (Count < 0 || Data.Num() - Position < Count)
			{
				return false;
			}
			Block = TConstArrayView<uint8>(Data.GetData() + Position, Count);
			Position += Count;
			return true;
		}

		bool AtEnd() const
		{
			return Position == Data.Num();
		}

	private:
		TConstArrayView<uint8> Data;
		int32 Position = 0;
	};

	bool IsNonNegativeFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
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
	const double Width = CmPerMetre * AsphaltBalance::ToolWidthM[GetLevel(EAsphaltUpgrade::Tool)];
	return HasSpecial(EAsphaltSpecial::TwoHands) ? Width * AsphaltBalance::TwoHandsWidthMultiplier : Width;
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
		const double Step = FMath::Min(Remaining, AsphaltBalance::MaxStepSeconds);
		Events.Append(TickStep(Step));
		Remaining -= Step;
	}
	return Events;
}

FAsphaltEvents FAsphaltGame::TickStep(double Step)
{
	PlaytimeSeconds += Step;
	PlantStock = FMath::Min(PlantStock + GetPlantRatePerMin() / 60.0 * Step, GetPlantStorage());
	return TickAutoPaver(Step);
}

FAsphaltEvents FAsphaltGame::TickAutoPaver(double Step)
{
	if (!HasSpecial(EAsphaltSpecial::AutoPaver))
	{
		return FAsphaltEvents();
	}

	if (AutoPaverTarget == INDEX_NONE || Grid.IsCellComplete(AutoPaverTarget))
	{
		AutoPaverTarget = PickAutoPaverTarget();
	}
	if (AutoPaverTarget == INDEX_NONE)
	{
		return FAsphaltEvents();
	}

	const FVector2D Center = AsphaltHex::ToWorld(Map.GetCell(AutoPaverTarget).Coord);
	const FVector2D ToTarget = Center - AutoPaverPos;
	const double Distance = ToTarget.Size();
	if (Distance > AsphaltBalance::AutoPaverReachCm)
	{
		// Walking: the paver earns no litres on the way.
		const double Reach = AsphaltBalance::AutoPaverSpeedCmS * Step;
		AutoPaverPos = Distance <= Reach ? Center : AutoPaverPos + ToTarget * (Reach / Distance);
		return FAsphaltEvents();
	}

	AutoPaverBudget += AsphaltBalance::AutoPaverRateLPerMin / 60.0 * Step;
	const FPourResult Result = Grid.DepositIntoCell(AutoPaverTarget, AutoPaverBudget);
	AutoPaverBudget = FMath::Clamp(AutoPaverBudget - Result.LitresUsed, 0.0, AsphaltBalance::AutoPaverMaxBudgetL);
	return ApplyPourResult(Result);
}

int32 FAsphaltGame::PickAutoPaverTarget() const
{
	if (Grid.NumCompleteCells() >= Map.Num())
	{
		return INDEX_NONE;
	}

	int32 Best = INDEX_NONE;
	double BestDistSq = 0.0;
	for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
	{
		if (Grid.IsCellComplete(Cell))
		{
			continue;
		}
		const double DistSq = FVector2D::DistSquared(AsphaltHex::ToWorld(Map.GetCell(Cell).Coord), AutoPaverPos);
		if (Best == INDEX_NONE || DistSq < BestDistSq)
		{
			Best = Cell;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

double FAsphaltGame::MoveFromPlant(double Capacity, double& Container, double Dt)
{
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return 0.0;
	}

	const double ByRate = Capacity / AsphaltBalance::RefillSeconds * Dt;
	const double Moved = FMath::Max(0.0, FMath::Min3(ByRate, Capacity - Container, PlantStock));
	Container = FMath::Min(Container + Moved, Capacity);
	PlantStock = FMath::Max(0.0, PlantStock - Moved);
	return Moved;
}

double FAsphaltGame::RefillBackpack(double Dt)
{
	return MoveFromPlant(GetBackpackCapacity(), BackpackLitres, Dt);
}

double FAsphaltGame::RefillCar(double Dt)
{
	return HasSpecial(EAsphaltSpecial::PaverCar) ? MoveFromPlant(AsphaltBalance::CarTankL, CarLitres, Dt) : 0.0;
}

FAsphaltEvents FAsphaltGame::PourFromCar(FVector2D A, FVector2D B)
{
	if (!HasSpecial(EAsphaltSpecial::PaverCar) || !(CarLitres > 0.0) || !IsFinite(A) || !IsFinite(B))
	{
		return FAsphaltEvents();
	}

	const FPourResult Result = Grid.Stamp(ClampToWorld(A), ClampToWorld(B), AsphaltBalance::CarWidthCm, CarLitres);
	CarLitres = FMath::Max(0.0, CarLitres - Result.LitresUsed);
	return ApplyPourResult(Result);
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

	// Roads and the exit follow from the grid, so a road is reported by the result that completed its last cell.
	for (const int32 Cell : Result.CompletedCells)
	{
		const FAsphaltCell& Info = Map.GetCell(Cell);
		if (Info.Kind == EAsphaltCellKind::Road && IsRoadComplete(Info.RoadIndex))
		{
			Events.CompletedRoads.AddUnique(Info.RoadIndex);
		}
	}
	Events.bExitOpened = Result.CompletedCells.Num() > 0 && IsExitOpen();

	TotalLitresPoured += Events.LitresPoured;
	Money += Events.MoneyEarned;
	Shards += Events.ShardsEarned;
	return Events;
}

bool FAsphaltGame::HasSpecial(EAsphaltSpecial S) const
{
	return IsValidSpecial(S) && bOwned[static_cast<int32>(S)];
}

double FAsphaltGame::GetSpecialPrice(EAsphaltSpecial S) const
{
	return IsValidSpecial(S) ? AsphaltBalance::SpecialPriceMoney[static_cast<int32>(S)] : -1.0;
}

int32 FAsphaltGame::GetSpecialRoad(EAsphaltSpecial S) const
{
	return IsValidSpecial(S) ? AsphaltBalance::SpecialRoad[static_cast<int32>(S)] : INDEX_NONE;
}

bool FAsphaltGame::CanBuySpecial(EAsphaltSpecial S) const
{
	return IsValidSpecial(S) && !HasSpecial(S) && IsRoadComplete(GetSpecialRoad(S)) && Money >= GetSpecialPrice(S);
}

bool FAsphaltGame::BuySpecial(EAsphaltSpecial S)
{
	if (!CanBuySpecial(S))
	{
		return false;
	}
	Money -= GetSpecialPrice(S);
	GrantSpecial(S);
	return true;
}

void FAsphaltGame::GrantSpecial(EAsphaltSpecial S)
{
	if (!IsValidSpecial(S) || HasSpecial(S))
	{
		return;
	}
	bOwned[static_cast<int32>(S)] = true;

	const FVector2D Spawn = AsphaltLayout::SpecialSpawn(GetSpecialRoad(S));
	switch (S)
	{
	case EAsphaltSpecial::PortableStation:
		WorldState.StationPos = Spawn;
		break;
	case EAsphaltSpecial::PaverCar:
		WorldState.CarPos = Spawn;
		break;
	case EAsphaltSpecial::AutoPaver:
		AutoPaverPos = Spawn;
		AutoPaverTarget = INDEX_NONE;
		AutoPaverBudget = 0.0;
		break;
	default:
		break;
	}
}

int32 FAsphaltGame::GetRoadProgress(int32 Road) const
{
	if (Road < 0 || Road >= FHexMap::NumRoads)
	{
		return 0;
	}
	int32 Done = 0;
	for (const int32 Cell : Map.GetRoadCells(Road))
	{
		Done += Grid.IsCellComplete(Cell) ? 1 : 0;
	}
	return Done;
}

bool FAsphaltGame::IsRoadComplete(int32 Road) const
{
	return GetRoadProgress(Road) == FHexMap::CellsPerRoad;
}

bool FAsphaltGame::IsExitOpen() const
{
	return Grid.NumCompleteCells() >= Map.Num();
}

bool FAsphaltGame::HasWon() const
{
	return bWon;
}

void FAsphaltGame::SetWon()
{
	if (IsExitOpen())
	{
		bWon = true;
	}
}

double FAsphaltGame::GetCarLitres() const
{
	return CarLitres;
}

FVector2D FAsphaltGame::GetAutoPaverPos() const
{
	return AutoPaverPos;
}

int32 FAsphaltGame::GetAutoPaverTarget() const
{
	return AutoPaverTarget;
}

FAsphaltWorldState& FAsphaltGame::World()
{
	return WorldState;
}

const FAsphaltWorldState& FAsphaltGame::World() const
{
	return WorldState;
}

void FAsphaltGame::DebugGrantSpecial(EAsphaltSpecial S)
{
	GrantSpecial(S);
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

TArray<uint8> FAsphaltGame::SaveToBytes() const
{
	TArray<uint8> Bytes;
	Bytes.Reserve(CoverageBytes + 512);
	FSaveWriter W(Bytes);

	W.Write<uint32>(SaveMagic);
	W.Write<int32>(SaveVersion);
	W.Write<int32>(Map.Num());
	W.Write<int32>(FCoverageGrid::Size);

	W.Write<double>(Money);
	W.Write<int32>(Shards);
	W.Write<double>(PlaytimeSeconds);
	W.Write<double>(TotalLitresPoured);

	for (const int32 Level : Levels)
	{
		W.Write<int32>(Level);
	}

	uint8 OwnedMask = 0;
	for (int32 I = 0; I < NumSpecials; ++I)
	{
		OwnedMask |= bOwned[I] ? static_cast<uint8>(1u << I) : 0;
	}
	W.Write<uint8>(OwnedMask);
	W.WriteBool(bWon);

	W.Write<double>(PlantStock);
	W.Write<double>(BackpackLitres);
	W.Write<double>(CarLitres);

	W.WriteVector(AutoPaverPos);
	W.Write<int32>(AutoPaverTarget);
	W.Write<double>(AutoPaverBudget);

	W.WriteVector(WorldState.PlayerPos);
	W.Write<double>(WorldState.PlayerYawDeg);
	W.WriteVector(WorldState.StationPos);
	W.WriteBool(WorldState.bStationCarried);
	W.WriteVector(WorldState.CarPos);
	W.Write<double>(WorldState.CarYawDeg);
	W.WriteBool(WorldState.bPlayerInCar);

	TArray<uint8> Coverage;
	Grid.WriteBits(Coverage);
	W.Write<int32>(Coverage.Num());
	Bytes.Append(Coverage);
	return Bytes;
}

bool FAsphaltGame::LoadFromBytes(TConstArrayView<uint8> Bytes)
{
	static_assert(NumSpecials <= 8, "the owned specials are stored in one byte");

	// Everything is read and checked into locals first; the game is only touched once all of it is valid.
	FSaveReader R(Bytes);

	uint32 Magic = 0;
	int32 Version = 0;
	int32 NumCells = 0;
	int32 GridSize = 0;
	if (!R.Read(Magic) || Magic != SaveMagic || !R.Read(Version) || Version != SaveVersion
		|| !R.Read(NumCells) || NumCells != Map.Num() || !R.Read(GridSize) || GridSize != FCoverageGrid::Size)
	{
		return false;
	}

	double NewMoney = 0.0;
	int32 NewShards = 0;
	double NewPlaytime = 0.0;
	double NewTotalLitres = 0.0;
	if (!R.Read(NewMoney) || !IsNonNegativeFinite(NewMoney) || !R.Read(NewShards) || NewShards < 0
		|| !R.Read(NewPlaytime) || !IsNonNegativeFinite(NewPlaytime)
		|| !R.Read(NewTotalLitres) || !IsNonNegativeFinite(NewTotalLitres))
	{
		return false;
	}

	int32 NewLevels[3] = {0, 0, 0};
	for (int32 I = 0; I < 3; ++I)
	{
		if (!R.Read(NewLevels[I]) || NewLevels[I] < 0 || NewLevels[I] > MaxLevelOf(static_cast<EAsphaltUpgrade>(I)))
		{
			return false;
		}
	}

	uint8 OwnedMask = 0;
	bool bNewWon = false;
	if (!R.Read(OwnedMask) || (OwnedMask >> NumSpecials) != 0 || !R.ReadBool(bNewWon))
	{
		return false;
	}

	double NewPlantStock = 0.0;
	double NewBackpack = 0.0;
	double NewCar = 0.0;
	const double BackpackCapacity = AsphaltBalance::BackpackCapacityL[NewLevels[static_cast<int32>(EAsphaltUpgrade::Backpack)]];
	const double PlantStorage = AsphaltBalance::PlantRateLPerMin[NewLevels[static_cast<int32>(EAsphaltUpgrade::Plant)]]
		* AsphaltBalance::PlantStorageMinutes;
	if (!R.Read(NewPlantStock) || !IsNonNegativeFinite(NewPlantStock) || NewPlantStock > PlantStorage + LoadSlackL
		|| !R.Read(NewBackpack) || !IsNonNegativeFinite(NewBackpack) || NewBackpack > BackpackCapacity + LoadSlackL
		|| !R.Read(NewCar) || !IsNonNegativeFinite(NewCar) || NewCar > AsphaltBalance::CarTankL + LoadSlackL)
	{
		return false;
	}

	FVector2D NewPaverPos;
	int32 NewPaverTarget = INDEX_NONE;
	double NewPaverBudget = 0.0;
	if (!R.ReadVector(NewPaverPos) || !R.Read(NewPaverTarget) || (NewPaverTarget != INDEX_NONE && (NewPaverTarget < 0 || NewPaverTarget >= Map.Num()))
		|| !R.Read(NewPaverBudget) || !IsNonNegativeFinite(NewPaverBudget) || NewPaverBudget > AsphaltBalance::AutoPaverMaxBudgetL + LoadSlackL)
	{
		return false;
	}

	FAsphaltWorldState NewWorld;
	if (!R.ReadVector(NewWorld.PlayerPos) || !R.Read(NewWorld.PlayerYawDeg) || !FMath::IsFinite(NewWorld.PlayerYawDeg)
		|| !R.ReadVector(NewWorld.StationPos) || !R.ReadBool(NewWorld.bStationCarried)
		|| !R.ReadVector(NewWorld.CarPos) || !R.Read(NewWorld.CarYawDeg) || !FMath::IsFinite(NewWorld.CarYawDeg)
		|| !R.ReadBool(NewWorld.bPlayerInCar))
	{
		return false;
	}

	int32 CoverageLength = 0;
	TConstArrayView<uint8> Coverage;
	if (!R.Read(CoverageLength) || CoverageLength != CoverageBytes || !R.ReadBlock(CoverageLength, Coverage) || !R.AtEnd())
	{
		return false;
	}

	// The coverage is the only part that can still refuse (it checks its size first and only then rewrites itself),
	// so it goes first: a refusal leaves the game as it was.
	if (!Grid.ReadBits(Coverage))
	{
		return false;
	}

	Money = NewMoney;
	Shards = NewShards;
	PlaytimeSeconds = NewPlaytime;
	TotalLitresPoured = NewTotalLitres;
	for (int32 I = 0; I < 3; ++I)
	{
		Levels[I] = NewLevels[I];
	}
	for (int32 I = 0; I < NumSpecials; ++I)
	{
		bOwned[I] = (OwnedMask & (1u << I)) != 0;
	}
	bWon = bNewWon;
	PlantStock = FMath::Min(NewPlantStock, PlantStorage);
	BackpackLitres = FMath::Min(NewBackpack, BackpackCapacity);
	CarLitres = FMath::Min(NewCar, AsphaltBalance::CarTankL);
	AutoPaverPos = NewPaverPos;
	AutoPaverTarget = NewPaverTarget;
	AutoPaverBudget = FMath::Min(NewPaverBudget, AsphaltBalance::AutoPaverMaxBudgetL);
	WorldState = NewWorld;
	return true;
}

void FAsphaltGame::ClampToCapacities()
{
	BackpackLitres = FMath::Min(BackpackLitres, GetBackpackCapacity());
	PlantStock = FMath::Min(PlantStock, GetPlantStorage());
}
