// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltBalance.h"
#include "AsphaltGame.h"
#include "AsphaltLayout.h"
#include "HexCoord.h"

DEFINE_LOG_CATEGORY_STATIC(LogBalanceSim, Log, All);

DIA_TEST(FBalanceSimPlaythroughTest, "DoItAsphalt.Core.BalanceSim.Playthrough")

/*
 * Balance playthrough: a scripted, deterministic player plays a whole game through the public FAsphaltGame API
 * (Tick, RefillBackpack / RefillCar, PourFromBackpack / PourFromCar, Buy, BuySpecial) and the test checks that the
 * prices in AsphaltBalance produce a game of the intended length and pace. No random numbers, no wall clock: the
 * only clock is the game's own playtime.
 *
 * Player policy
 *  - Buying: after every simulation step it buys whatever it can. Backpack levels (shards) are bought as soon as
 *    affordable; with money it always buys the cheapest affordable option among the next Tool level, the next Plant
 *    level and every special whose road is complete.
 *  - Target: roads are tried in the order of the specials (3, 2, 4, 1, 0). The first road whose special is not owned
 *    and whose road is incomplete is paved when the money the player has plus the litres still missing on that road
 *    would pay for the special; the nearest unfinished cell of that road (to the plant) is the target. Otherwise the
 *    target is the nearest unfinished cell to the plant (ties: lower index).
 *
 * Trip model (one trip = refill, walk out, pour, walk back)
 *  - Refill: the player stands in the refill zone and takes asphalt (backpack, or the car tank once the car is owned)
 *    at the game's own refill rate. It leaves when the container is full or the plant stock is dry, but never before
 *    the container holds min(capacity, litres missing to complete the target cell); that is the "wait for stock".
 *  - Walking: 2 x distance / speed over the whole trip. Speed is WalkSpeedCmS, times SprintMultiplier once Sprint is
 *    owned, or CarSpeedCmS once the car is owned. Distance out is the distance from the edge of the refill zone
 *    (RefillRadiusCm around RefillCenter) to the target cell centre. With the portable station the station always
 *    stands 300 cm from the last poured cell, so the distance is (target to last poured cell) + 300 cm, and the way
 *    back is 300 cm. Moving the station and the first drive of the car to the zone cost no time.
 *  - Pouring: the bar sweeps parallel lanes (full tool width apart) across the target cell until the container is
 *    empty or the cell is complete, using the real stamp. Wide bars therefore also paint neighbouring cells. The
 *    pour takes litres poured / pour rate, where pour rate = width x speed x LitresPerCell / area of a cell, with
 *    speed PourSpeedCmS for the backpack and CarSpeedCmS for the car. Turning at the end of a lane is free.
 *  - If at least ChainMinL litres are left after the cell, the player walks on to the next target in the same trip.
 *  - Time passes only through FAsphaltGame::Tick (in MaxStepSeconds chunks); the auto paver therefore works during
 *    every walk, wait and pour.
 */
namespace
{
	/** Area of one cell in cm^2 (the cell area is 1 m^2). */
	constexpr double CellAreaCm2 = 10000.0;
	/** Below this amount the container counts as empty. */
	constexpr double MinPourL = 0.5;
	/** The player walks on to another cell only with at least this much asphalt left. */
	constexpr double ChainMinL = 1.0;
	/** The portable station stands this far from the last poured cell. */
	constexpr double StationOffsetCm = 300.0;
	/** Hard stop so that a broken balance cannot run for ever. */
	constexpr double SimLimitSeconds = 6.0 * 3600.0;
	/** Trips in a row that pour nothing before the simulation gives up. */
	constexpr int32 MaxIdleTrips = 50;
	constexpr int32 NumSpecials = static_cast<int32>(EAsphaltSpecial::Count);
	constexpr int32 NumMilestones = 10;

	const TCHAR* const SpecialNames[NumSpecials] = {TEXT("Sprint"), TEXT("TwoHands"), TEXT("PortableStation"), TEXT("AutoPaver"), TEXT("PaverCar")};

	FString Clock(double Seconds)
	{
		if (Seconds < 0.0)
		{
			return TEXT("--:--");
		}
		const int32 Total = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}

	struct FPurchase
	{
		double Time = 0.0;
		FString Name;
		int32 CellsDone = 0;
	};

	class FSimPlayer
	{
	public:
		FSimPlayer()
		{
			const FHexMap& Map = G.GetMap();
			CellPos.SetNum(Map.Num());
			for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
			{
				CellPos[Cell] = AsphaltHex::ToWorld(Map.GetCell(Cell).Coord);
			}
			// Cells by distance to the plant; the stable sort keeps the lower index first on ties.
			NearestToPlant.SetNum(Map.Num());
			for (int32 Cell = 0; Cell < Map.Num(); ++Cell)
			{
				NearestToPlant[Cell] = Cell;
			}
			NearestToPlant.StableSort([this](int32 A, int32 B)
			{
				return FVector2D::DistSquared(CellPos[A], AsphaltLayout::PlantPos) < FVector2D::DistSquared(CellPos[B], AsphaltLayout::PlantPos);
			});
			LastPour = AsphaltLayout::RefillCenter;
			MilestoneSeconds.Init(-1.0, NumMilestones);
		}

		/** Plays until the exit opens, or gives up (see bGaveUp). */
		void Run()
		{
			int32 IdleTrips = 0;
			while (!G.IsExitOpen())
			{
				if (G.GetPlaytimeSeconds() > SimLimitSeconds)
				{
					bGaveUp = true;
					return;
				}
				const double Litres = G.GetTotalLitresPoured();
				RunTrip();
				IdleTrips = G.GetTotalLitresPoured() > Litres ? 0 : IdleTrips + 1;
				if (IdleTrips >= MaxIdleTrips)
				{
					bGaveUp = true;
					return;
				}
			}
		}

		FAsphaltGame G;
		TArray<FPurchase> Purchases;
		/** Playtime at which 10 %, 20 %, ... of the cells were complete; -1 if never. */
		TArray<double> MilestoneSeconds;
		bool bGaveUp = false;

	private:
		// --- time ---------------------------------------------------------------------------------------------

		/** Lets game time pass in Tick chunks; the player shops after every chunk. */
		void Advance(double Seconds)
		{
			while (Seconds > 0.0)
			{
				const double Step = FMath::Min(Seconds, AsphaltBalance::MaxStepSeconds);
				G.Tick(Step);
				Seconds -= Step;
				Shop();
				RecordProgress();
			}
		}

		void RecordProgress()
		{
			const int32 Done = G.GetGrid().NumCompleteCells();
			for (int32 I = 0; I < NumMilestones; ++I)
			{
				if (MilestoneSeconds[I] < 0.0 && Done * NumMilestones >= (I + 1) * FHexMap::NumCells)
				{
					MilestoneSeconds[I] = G.GetPlaytimeSeconds();
				}
			}
		}

		// --- shopping -----------------------------------------------------------------------------------------

		void Record(const FString& Name)
		{
			FPurchase P;
			P.Time = G.GetPlaytimeSeconds();
			P.Name = Name;
			P.CellsDone = G.GetGrid().NumCompleteCells();
			Purchases.Add(P);
		}

		void Shop()
		{
			for (;;)
			{
				if (G.Buy(EAsphaltUpgrade::Backpack))
				{
					Record(FString::Printf(TEXT("Backpack %d"), G.GetLevel(EAsphaltUpgrade::Backpack)));
					continue;
				}

				// Cheapest affordable purchase with money: -1 Tool, -2 Plant, 0.. a special.
				int32 Best = -100;
				double BestPrice = 0.0;
				const auto Consider = [&](int32 Id, double Price)
				{
					if (Best == -100 || Price < BestPrice)
					{
						Best = Id;
						BestPrice = Price;
					}
				};
				if (G.CanBuy(EAsphaltUpgrade::Tool))
				{
					Consider(-1, G.GetNextPrice(EAsphaltUpgrade::Tool));
				}
				if (G.CanBuy(EAsphaltUpgrade::Plant))
				{
					Consider(-2, G.GetNextPrice(EAsphaltUpgrade::Plant));
				}
				for (int32 I = 0; I < NumSpecials; ++I)
				{
					if (G.CanBuySpecial(static_cast<EAsphaltSpecial>(I)))
					{
						Consider(I, G.GetSpecialPrice(static_cast<EAsphaltSpecial>(I)));
					}
				}

				if (Best == -1 && G.Buy(EAsphaltUpgrade::Tool))
				{
					Record(FString::Printf(TEXT("Tool %d"), G.GetLevel(EAsphaltUpgrade::Tool)));
				}
				else if (Best == -2 && G.Buy(EAsphaltUpgrade::Plant))
				{
					Record(FString::Printf(TEXT("Plant %d"), G.GetLevel(EAsphaltUpgrade::Plant)));
				}
				else if (Best >= 0 && G.BuySpecial(static_cast<EAsphaltSpecial>(Best)))
				{
					Record(SpecialNames[Best]);
				}
				else
				{
					return;
				}
			}
		}

		// --- choosing the target ------------------------------------------------------------------------------

		/** Litres that still have to be poured into the cell before it completes. */
		double MissingLitres(int32 Cell) const
		{
			return FMath::Max(0.0, AsphaltBalance::CompletionThreshold - G.GetGrid().GetCellFill(Cell)) * AsphaltBalance::LitresPerCell;
		}

		int32 NearestUnfinished(int32 Road) const
		{
			for (const int32 Cell : NearestToPlant)
			{
				if (!G.GetGrid().IsCellComplete(Cell) && (Road == INDEX_NONE || G.GetMap().GetCell(Cell).RoadIndex == Road))
				{
					return Cell;
				}
			}
			return INDEX_NONE;
		}

		int32 ChooseTarget() const
		{
			for (int32 I = 0; I < NumSpecials; ++I)
			{
				const EAsphaltSpecial S = static_cast<EAsphaltSpecial>(I);
				const int32 Road = G.GetSpecialRoad(S);
				if (G.HasSpecial(S) || G.IsRoadComplete(Road))
				{
					continue;
				}
				double Missing = 0.0;
				for (const int32 Cell : G.GetMap().GetRoadCells(Road))
				{
					Missing += G.GetGrid().IsCellComplete(Cell) ? 0.0 : MissingLitres(Cell);
				}
				if (G.GetMoney() + Missing * AsphaltBalance::MoneyPerLitre >= G.GetSpecialPrice(S))
				{
					return NearestUnfinished(Road);
				}
			}
			return NearestUnfinished(INDEX_NONE);
		}

		// --- one trip -----------------------------------------------------------------------------------------

		bool HasCar() const
		{
			return G.HasSpecial(EAsphaltSpecial::PaverCar);
		}

		double WalkSpeed() const
		{
			if (HasCar())
			{
				return AsphaltBalance::CarSpeedCmS;
			}
			return G.HasSpecial(EAsphaltSpecial::Sprint) ? AsphaltBalance::WalkSpeedCmS * AsphaltBalance::SprintMultiplier : AsphaltBalance::WalkSpeedCmS;
		}

		double ContainerLitres() const
		{
			return HasCar() ? G.GetCarLitres() : G.GetBackpackLitres();
		}

		double ContainerCapacity() const
		{
			return HasCar() ? AsphaltBalance::CarTankL : G.GetBackpackCapacity();
		}

		double BarWidthCm() const
		{
			return HasCar() ? AsphaltBalance::CarWidthCm : G.GetToolWidthCm();
		}

		/** Litres per second while pouring. */
		double PourRate() const
		{
			const double Speed = HasCar() ? AsphaltBalance::CarSpeedCmS : AsphaltBalance::PourSpeedCmS;
			return BarWidthCm() * Speed * AsphaltBalance::LitresPerCell / CellAreaCm2;
		}

		/** Distance between the player's workplace (refill zone or station) and a point; see the header comment. */
		double OriginDistanceCm(const FVector2D& Point) const
		{
			if (G.HasSpecial(EAsphaltSpecial::PortableStation))
			{
				return FVector2D::Distance(Point, LastPour) + StationOffsetCm;
			}
			return FMath::Max(0.0, FVector2D::Distance(Point, AsphaltLayout::RefillCenter) - AsphaltLayout::RefillRadiusCm);
		}

		void FillContainer(double Wanted)
		{
			const double Threshold = FMath::Min(ContainerCapacity(), FMath::Max(Wanted, MinPourL));
			for (;;)
			{
				const double Step = AsphaltBalance::MaxStepSeconds;
				Advance(Step);
				if (HasCar())
				{
					G.RefillCar(Step);
				}
				else
				{
					G.RefillBackpack(Step);
				}
				const bool bFull = ContainerLitres() >= ContainerCapacity() - 1e-9;
				const bool bDry = G.GetPlantStock() <= 1e-9;
				if (bFull || (bDry && ContainerLitres() >= Threshold))
				{
					return;
				}
			}
		}

		/** Sweeps lanes across the cell until the container is empty or the cell is complete; returns the litres used. */
		double PourCell(int32 Cell)
		{
			const FVector2D Centre = CellPos[Cell];
			const double HalfHeight = AsphaltHex::SideCm;
			const double HalfWidth = AsphaltHex::SideCm * 0.5 * FMath::Sqrt(3.0);
			const double Width = BarWidthCm();
			const int32 Lanes = FMath::Max(1, FMath::CeilToInt(2.0 * HalfHeight / Width - 1e-9));
			double Used = 0.0;
			for (int32 Lane = 0; Lane < Lanes && ContainerLitres() >= MinPourL && !G.GetGrid().IsCellComplete(Cell); ++Lane)
			{
				const double Y = Lanes == 1 ? Centre.Y : Centre.Y - HalfHeight + (Lane + 0.5) * Width;
				const FVector2D A(Centre.X - HalfWidth, Y);
				const FVector2D B(Centre.X + HalfWidth, Y);
				Used += (HasCar() ? G.PourFromCar(A, B) : G.PourFromBackpack(A, B)).LitresPoured;
			}
			return Used;
		}

		void RunTrip()
		{
			const int32 First = ChooseTarget();
			if (First == INDEX_NONE)
			{
				return;
			}

			FillContainer(MissingLitres(First));
			Advance(OriginDistanceCm(CellPos[First]) / WalkSpeed());

			int32 Cell = First;
			for (;;)
			{
				const double Used = PourCell(Cell);
				LastPour = CellPos[Cell];
				Advance(Used / PourRate());
				if (Used <= 0.0 || G.IsExitOpen() || ContainerLitres() < ChainMinL)
				{
					break;
				}
				const int32 Next = ChooseTarget();
				if (Next == INDEX_NONE)
				{
					break;
				}
				Advance(FVector2D::Distance(CellPos[Cell], CellPos[Next]) / WalkSpeed());
				Cell = Next;
			}
			Advance(OriginDistanceCm(LastPour) / WalkSpeed());
		}

		TArray<FVector2D> CellPos;
		TArray<int32> NearestToPlant;
		FVector2D LastPour;
	};
}

bool FBalanceSimPlaythroughTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FSimPlayer> Sim = MakeUnique<FSimPlayer>();
	Sim->Run();
	const FAsphaltGame& G = Sim->G;

	for (const FPurchase& P : Sim->Purchases)
	{
		const FString Line = FString::Printf(TEXT("[%s] bought %s"), *Clock(P.Time), *P.Name);
		UE_LOG(LogBalanceSim, Display, TEXT("%s"), *Line);
		AddInfo(Line);
	}

	// Pace: the longest stretch without a purchase during the first 30 minutes.
	constexpr double WindowSeconds = 30.0 * 60.0;
	double MaxGap = 0.0;
	double Previous = 0.0;
	for (const FPurchase& P : Sim->Purchases)
	{
		const double At = FMath::Min(P.Time, WindowSeconds);
		MaxGap = FMath::Max(MaxGap, At - Previous);
		Previous = At;
	}
	MaxGap = FMath::Max(MaxGap, WindowSeconds - Previous);

	const double TotalMinutes = G.GetPlaytimeSeconds() / 60.0;
	const int32 CellsAtLastPurchase = Sim->Purchases.Num() > 0 ? Sim->Purchases.Last().CellsDone : 0;
	const double LastPurchaseSeconds = Sim->Purchases.Num() > 0 ? Sim->Purchases.Last().Time : 0.0;

	FString Milestones;
	for (int32 I = 0; I < Sim->MilestoneSeconds.Num(); ++I)
	{
		Milestones += FString::Printf(TEXT("%d%%=%s "), (I + 1) * 10, *Clock(Sim->MilestoneSeconds[I]));
	}
	const FString Summary = FString::Printf(
		TEXT("total %.1f min, %d purchases, last at %s with %d / %d cells, longest gap in the first 30 min %.0f s, %.0f l poured, money left %.0f, shards left %d; %s"),
		TotalMinutes, Sim->Purchases.Num(), *Clock(LastPurchaseSeconds), CellsAtLastPurchase, FHexMap::NumCells, MaxGap,
		G.GetTotalLitresPoured(), G.GetMoney(), G.GetShards(), *Milestones);
	UE_LOG(LogBalanceSim, Display, TEXT("%s"), *Summary);
	AddInfo(Summary);

	TestFalse(TEXT("the simulated player does not give up"), Sim->bGaveUp);
	TestTrue(TEXT("the exit opens"), G.IsExitOpen());
	TestTrue(TEXT("total time at least 45 minutes"), TotalMinutes >= 45.0);
	TestTrue(TEXT("total time at most 100 minutes"), TotalMinutes <= 100.0);
	TestEqual(TEXT("all 35 upgrades are bought"), Sim->Purchases.Num(), 35);
	TestTrue(TEXT("all upgrades before 75 % of the cells"), CellsAtLastPurchase < 0.75 * FHexMap::NumCells);
	TestTrue(TEXT("no purchase gap above 4 minutes in the first 30 minutes"), MaxGap <= 4.0 * 60.0);
	return true;
}
