// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotLayout.h"
#include "LotMap.h"

DEFINE_LOG_CATEGORY_STATIC(LogLotBalanceSim, Log, All);

DIA_TEST(FLotBalanceSimPlaythroughTest, "DoItAsphalt.Lot.BalanceSim.Playthrough")

/*
 * Balance playthrough of the whole game: a scripted, deterministic player plays from the first spoonful to the
 * open exit through the public FLotGame API only (Tick, scooping, pouring, filling, grabbing, strips,
 * the auto paver, the car), and the test checks the milestones of the design's pacing curve. No random numbers and
 * no Debug* calls; the only clock is GetPlaytimeSeconds(), and time passes only through Tick.
 *
 * Measured times (game time, with the tunables of LotBalance.h as committed; asphalt is not smoothed, so the cart lays
 * finished strips on its own and the game is shorter than it was with the finisher):
 *   milestone      measured   allowed
 *   1st cell       0:42 min   40 - 80 s
 *   10 cells       1:55       up to 6:00
 *   100 cells      6:34       up to 18:00
 *   350 cells      13:33      (logged only)
 *   800 cells      23:04      up to 42:00
 *   6 000 cells    48:41      up to 78:00
 *   30 000 cells   141:12     105 - 165 min (the exit opens at the same moment)
 *   Unlock steps: quick swap 2:01, sprint 6:38, strip work 13:38, auto paver 23:05, car 49:39, exit 141:12.
 *   The auto paver and the car draw from tanks that they refill at the stations, which makes the second half longer than it was.
 *
 * Player policy
 *  - Buying: after every step it buys the cheapest offered thing it can afford (items and plant levels).
 *  - Target: while the next unlock step is within 12 cells of its cell count and its entry segment is not complete,
 *    it paves that segment. Otherwise the nearest incomplete cell to the source (the trough, or the portable station
 *    once owned), not counting the sector the auto paver works in (unless nothing else is left).
 *  - Method: the best one owned: bowl (scoop, walk, pour on a grid of aim points) -> bucket (fill, walk, dump; with
 *    the suitcase as a stock for the bucket when the walk is long) -> cart -> car. The auto paver always gets the nearest incomplete sector that the player is not working in.
 *  - Walking: WalkSpeedCmS, times SprintMultiplier once the sprint is unlocked. A vessel is filled within
 *    RefillRadiusCm of the source; an aim point must lie within ReachCm of the player.
 *
 * Modelling choices the core leaves open (none of them is a game number):
 *  - Carrying the station uses its own tunable; pulling the suitcase SuitcasePullSpeedCmS.
 *  - The core has no "enter the car" action; the player walks to the car and sets World().bPlayerInCar.
 *  - The station is moved only when the next target lies farther than StationRelocateCm from it, to 5 m from the target.
 *  - The car (3 m or 3.5 m bar) lays bands along the X axis and waits whenever the stock cannot pay the next step.
 */
namespace
{
	/** Hard stop, as in the brief: six hours of game time. */
	constexpr double SimLimitSeconds = 6.0 * 3600.0;
	/** Step of hand work (scooping, pouring, filling). */
	constexpr double HandStep = 0.1;
	/** Step of walking, driving, laying and finishing. */
	constexpr double MoveStep = LotBalance::MaxStepSeconds;
	/** Jobs in a row that change nothing before the simulation gives up. */
	constexpr int32 MaxIdleJobs = 60;
	/** Guard against a job that loops without progress. */
	constexpr int32 MaxJobLoops = 60;
	/** The player works on an entry segment this many cells before its cell threshold. */
	constexpr int32 UnlockLeadCells = 12;
	/** A suitcase trip pays off when the walk from the source to the work spot is longer than this. */
	constexpr double SuitcaseMinWalkCm = 1000.0;
	/** The suitcase is reused for cells within this distance of where it stands. */
	constexpr double SuitcaseReuseCm = 1500.0;
	/** The suitcase is put down this far before the target. */
	constexpr double SuitcaseStandOffCm = 200.0;
	/** The station is placed this far from the target. */
	constexpr double StationOffsetCm = 500.0;
	/** The station is moved when the target lies farther than this from it. */
	constexpr double StationRelocateCm = 1200.0;
	/** With the car the station is moved only when the target lies farther than this from it (the driver has to leave the car for it). */
	constexpr double CarStationRelocateCm = 8000.0;
	/** The cart goes on with another strip when the next target is nearer than this to where it stands. */
	constexpr double CartContinueCm = 3000.0;
	/** Longest run of cells the car lays in one band. */
	constexpr int32 CarMaxCells = 50;
	/** Aim points per side of the pouring grid of one cell: spacing 20 cm, so the 15 cm bowl footprint covers every texel. */
	constexpr int32 PourPointsPerSide = 5;
	/** Margin that keeps every point of a cell within ReachCm: half a cell diagonal. */
	constexpr double CellReachMarginCm = AsphaltLot::CellCm * 0.71;

	constexpr int32 NumItems = static_cast<int32>(ELotItem::Count);
	constexpr int32 NumUnlocks = static_cast<int32>(ELotUnlock::Count);

	const TCHAR* const ItemNames[NumItems] = {TEXT("Bucket"), TEXT("Canister"), TEXT("Tub"), TEXT("Suitcase"), TEXT("Cart"), TEXT("Station"),
		TEXT("AutoPaver"), TEXT("AutoPaverFast"), TEXT("AutoPaverFaster"), TEXT("Car"), TEXT("CarWideBar")};
	const TCHAR* const UnlockNames[NumUnlocks] = {TEXT("QuickSwap"), TEXT("Sprint"), TEXT("StripWork"), TEXT("AutoPaver"), TEXT("Car"), TEXT("Exit")};
	/** Cell counts at which the time is recorded; the test asserts those named in the brief. */
	constexpr int32 MilestoneCells[] = {1, 10, 100, 350, 800, 6000, 30000};
	constexpr int32 NumMilestones = UE_ARRAY_COUNT(MilestoneCells);

	FString Clock(double Seconds)
	{
		if (Seconds < 0.0)
		{
			return TEXT("--:--:--");
		}
		const int32 Total = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%02d:%02d:%02d"), Total / 3600, (Total / 60) % 60, Total % 60);
	}

	/** What the player's time is spent on; only reported, never asserted. */
	enum class EAct : uint8 { Walk, Transport, Drive, Swap, Scoop, Pour, Fill, Lay, CarLay, CarWait, Count };
	constexpr int32 NumActs = static_cast<int32>(EAct::Count);
	const TCHAR* const ActNames[NumActs] = {TEXT("walk"), TEXT("transport"), TEXT("drive"), TEXT("swap"), TEXT("scoop"), TEXT("pour"),
		TEXT("fill"), TEXT("lay"), TEXT("carlay"), TEXT("carwait")};

	/** A strip of cells along a row or a column: axis A->B (inset by the stamp pad so that the stamp ends on the cell edges). */
	struct FStrip
	{
		FVector2D A = FVector2D::ZeroVector;
		FVector2D B = FVector2D::ZeroVector;
		/** Unit vector from A to B: along X or along Y. */
		FVector2D Dir = FVector2D(1.0, 0.0);
		int32 Cells = 0;
		/** Y of the lower edge of the first cell. */
		double RowBottom = 0.0;
	};

	class FSimPlayer
	{
	public:
		FSimPlayer()
		{
			Centres.SetNum(AsphaltLot::NumCells);
			for (int32 Cell = 0; Cell < AsphaltLot::NumCells; ++Cell)
			{
				Centres[Cell] = AsphaltLot::CellCentre(Cell);
			}
			Pos = LotLayout::PlayerStart;
			MilestoneSeconds.Init(-1.0, NumMilestones);
			UnlockSeconds.Init(-1.0, NumUnlocks);
			SectorDone.Init(false, AsphaltLot::NumSectors);
			ActSeconds.Init(0.0, NumActs);
		}

		/** Plays until the exit opens, or gives up (see bGaveUp). */
		void Run()
		{
			int32 IdleJobs = 0;
			while (!G.IsExitOpen() && !bAborted)
			{
				const int32 CellsBefore = G.GetCompletedCells();
				const double LitresBefore = G.GetTotalLitres();
				const int32 Target = ChooseTarget(SourcePos());
				if (Target == INDEX_NONE)
				{
					break;
				}
				WorkSector = AsphaltLot::SectorOf(Target);
				PlaceStation(Centres[Target], G.Owns(ELotItem::Car) ? CarStationRelocateCm : StationRelocateCm);
				RunJob(Target);
				IdleJobs = (G.GetCompletedCells() > CellsBefore || G.GetTotalLitres() > LitresBefore) ? 0 : IdleJobs + 1;
				if (IdleJobs >= MaxIdleJobs)
				{
					bGaveUp = true;
					return;
				}
			}
			bGaveUp = bGaveUp || bAborted;
		}

		FString ActReport() const
		{
			FString Out;
			for (int32 I = 0; I < NumActs; ++I)
			{
				Out += FString::Printf(TEXT("%s %.0f; "), ActNames[I], ActSeconds[I]);
			}
			return Out;
		}

		FLotGame G;
		FVector2D Pos;
		TArray<FString> Log;
		TArray<double> MilestoneSeconds;
		TArray<double> UnlockSeconds;
		double MinStock = TNumericLimits<double>::Max();
		bool bGaveUp = false;
		/** Strips the cart laid and the cells they hold. */
		int32 NumStrips = 0;
		int32 StripCells = 0;
		/** Seconds spent per activity so far. */
		TArray<double> ActSeconds;

	private:
		// --- time ---------------------------------------------------------------------------------------------

		/** Lets game time pass (Seconds at most one sub-step) and lets the player shop and the machines be served. */
		void Advance(double Seconds)
		{
			if (bAborted)
			{
				return;
			}
			if (Seconds > 0.0)
			{
				G.Tick(Seconds);
				ActSeconds[static_cast<int32>(Cur)] += Seconds;
			}
			MinStock = FMath::Min(MinStock, G.GetPlantStock());
			Shop();
			ManagePaver();
			Observe();
			if (G.GetPlaytimeSeconds() > SimLimitSeconds)
			{
				bAborted = true;
			}
		}

		void Note(const FString& Line)
		{
			Log.Add(Line);
			UE_LOG(LogLotBalanceSim, Display, TEXT("%s"), *Line);
		}

		void Observe()
		{
			const int32 Cells = G.GetCompletedCells();
			for (int32 I = 0; I < NumMilestones; ++I)
			{
				if (MilestoneSeconds[I] < 0.0 && Cells >= MilestoneCells[I])
				{
					MilestoneSeconds[I] = G.GetPlaytimeSeconds();
					Note(FString::Printf(TEXT("[%s] milestone: %d cells; seconds so far: %s"), *Clock(MilestoneSeconds[I]), MilestoneCells[I], *ActReport()));
				}
			}
			for (int32 I = 0; I < NumUnlocks; ++I)
			{
				if (UnlockSeconds[I] < 0.0 && G.IsUnlocked(static_cast<ELotUnlock>(I)))
				{
					UnlockSeconds[I] = G.GetPlaytimeSeconds();
					Note(FString::Printf(TEXT("[%s] unlocked %s (%d cells)"), *Clock(UnlockSeconds[I]), UnlockNames[I], Cells));
				}
			}
		}

		// --- shopping and machines ------------------------------------------------------------------------------

		/** Buys the cheapest offered thing it can afford, until nothing is left to buy. */
		void Shop()
		{
			for (;;)
			{
				int32 Best = INDEX_NONE;
				double BestPrice = 0.0;
				for (int32 I = 0; I < NumItems; ++I)
				{
					const ELotItem Item = static_cast<ELotItem>(I);
					if (G.CanBuyItem(Item) && (Best == INDEX_NONE || G.GetItemPrice(Item) < BestPrice))
					{
						Best = I;
						BestPrice = G.GetItemPrice(Item);
					}
				}
				const bool bPlantBest = G.CanBuyPlant() && (Best == INDEX_NONE || G.GetPlantNextPrice() < BestPrice);
				if (bPlantBest)
				{
					const double Price = G.GetPlantNextPrice();
					G.BuyPlant();
					Note(FString::Printf(TEXT("[%s] bought plant level %d for %.0f (%d cells)"), *Clock(G.GetPlaytimeSeconds()), G.GetPlantLevel(), Price, G.GetCompletedCells()));
				}
				else if (Best != INDEX_NONE)
				{
					G.BuyItem(static_cast<ELotItem>(Best));
					Note(FString::Printf(TEXT("[%s] bought %s for %.0f (%d cells)"), *Clock(G.GetPlaytimeSeconds()), ItemNames[Best], BestPrice, G.GetCompletedCells()));
				}
				else
				{
					return;
				}
			}
		}

		bool IsSectorDone(int32 Sector)
		{
			if (!SectorDone[Sector])
			{
				bool bAll = true;
				for (int32 Step = 0; Step < AsphaltLot::CellsPerSector && bAll; ++Step)
				{
					bAll = G.GetGrid().IsCellComplete(AsphaltLot::SectorCell(Sector, Step));
				}
				SectorDone[Sector] = bAll;
			}
			return SectorDone[Sector];
		}

		/** Whenever the auto paver waits, it gets the nearest unfinished sector that the player does not work in. */
		void ManagePaver()
		{
			if (!G.Owns(ELotItem::AutoPaver) || G.GetAutoPaverSector() != INDEX_NONE || G.GetPlaytimeSeconds() < NextPaverCheck)
			{
				return;
			}
			int32 Best = INDEX_NONE;
			double BestDistance = TNumericLimits<double>::Max();
			for (int32 Sector = 0; Sector < AsphaltLot::NumSectors; ++Sector)
			{
				if (Sector == WorkSector || IsSectorDone(Sector))
				{
					continue;
				}
				const double Distance = FVector2D::DistSquared(AsphaltLot::SectorCentre(Sector), LotLayout::TroughPos);
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					Best = Sector;
				}
			}
			if (Best != INDEX_NONE)
			{
				G.AssignAutoPaverSector(Best);
			}
			else
			{
				NextPaverCheck = G.GetPlaytimeSeconds() + 1.0;
			}
		}

		// --- targets ----------------------------------------------------------------------------------------------

		/** Where asphalt is taken from: the portable station once owned (and no car), else the trough. */
		FVector2D SourcePos() const
		{
			if (G.Owns(ELotItem::Station) && !G.Owns(ELotItem::Car))
			{
				return G.World().Movables[static_cast<int32>(ELotMovable::Station)].Pos;
			}
			return LotLayout::TroughPos;
		}

		double WalkSpeed() const
		{
			return LotBalance::WalkSpeedCmS * (G.IsUnlocked(ELotUnlock::Sprint) ? LotBalance::SprintMultiplier : 1.0);
		}

		/** Incomplete cell nearest to Ref, leaving out one sector (INDEX_NONE: none). */
		int32 NearestIncomplete(FVector2D Ref, int32 ExcludeSector) const
		{
			int32 Best = INDEX_NONE;
			double BestDistance = TNumericLimits<double>::Max();
			const FLotGrid& Grid = G.GetGrid();
			for (int32 Cell = 0; Cell < AsphaltLot::NumCells; ++Cell)
			{
				if (Grid.IsCellComplete(Cell))
				{
					continue;
				}
				const double Distance = FVector2D::DistSquared(Ref, Centres[Cell]);
				if (Distance < BestDistance && (ExcludeSector == INDEX_NONE || AsphaltLot::SectorOf(Cell) != ExcludeSector))
				{
					BestDistance = Distance;
					Best = Cell;
				}
			}
			return Best;
		}

		/** Picks the next cell to work on; GoalEntry says whether it belongs to an entry segment being paved. */
		int32 ChooseTarget(FVector2D Ref)
		{
			GoalEntry = INDEX_NONE;
			int32 Step = INDEX_NONE;
			for (int32 I = 0; I < NumUnlocks; ++I)
			{
				if (!G.IsUnlocked(static_cast<ELotUnlock>(I)))
				{
					Step = I;
					break;
				}
			}
			if (Step != INDEX_NONE && G.GetCompletedCells() >= LotBalance::UnlockCells[Step] - UnlockLeadCells && !G.IsEntryComplete(Step))
			{
				int32 Best = INDEX_NONE;
				double BestDistance = TNumericLimits<double>::Max();
				for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
				{
					const int32 Cell = AsphaltLot::EntryCell(Step, I);
					const double Distance = FVector2D::DistSquared(Ref, Centres[Cell]);
					if (!G.GetGrid().IsCellComplete(Cell) && Distance < BestDistance)
					{
						BestDistance = Distance;
						Best = Cell;
					}
				}
				if (Best != INDEX_NONE)
				{
					GoalEntry = Step;
					return Best;
				}
			}
			const int32 Cell = NearestIncomplete(Ref, G.GetAutoPaverSector());
			return Cell != INDEX_NONE ? Cell : NearestIncomplete(Ref, INDEX_NONE);
		}

		/** Litres still missing for the cell to reach the completion threshold. */
		double MissingLitres(int32 Cell) const
		{
			return FMath::Max(0.0, (FLotGrid::ThresholdTexels - G.GetGrid().GetCellDone(Cell)) * FLotGrid::LitresPerTexel);
		}

		// --- movement ---------------------------------------------------------------------------------------------

		/** Moves to To at Speed. Per step, Ready(Dt) says whether the step may be taken (else the time just passes) and Act(prev, cur) does the work. */
		void MoveAlong(FVector2D To, double Speed, TFunctionRef<bool(double)> Ready, TFunctionRef<void(FVector2D, FVector2D)> Act)
		{
			const FVector2D From = Pos;
			const double Distance = FVector2D::Distance(From, To);
			if (Distance < 1e-6)
			{
				return;
			}
			const double Total = Distance / Speed;
			double Done = 0.0;
			while (Done < Total - 1e-12 && !bAborted)
			{
				const double Dt = FMath::Min(MoveStep, Total - Done);
				if (!Ready(Dt))
				{
					Advance(MoveStep);
					continue;
				}
				const FVector2D Next = Done + Dt >= Total - 1e-12 ? To : From + (To - From) * ((Done + Dt) / Total);
				Act(Pos, Next);
				Pos = Next;
				Advance(Dt);
				Done += Dt;
			}
			Pos = To;
		}

		void Travel(FVector2D To, double Speed)
		{
			Cur = Speed == LotBalance::CarSpeedCmS ? EAct::Drive : (Speed == WalkSpeed() ? EAct::Walk : EAct::Transport);
			MoveAlong(To, Speed, [](double) { return true; }, [](FVector2D, FVector2D) {});
		}

		/** Walks (or pushes at Speed) until within Radius of Centre; does nothing if already there. */
		void GoWithin(FVector2D Centre, double Radius, double Speed)
		{
			const double Distance = FVector2D::Distance(Pos, Centre);
			if (Distance > Radius)
			{
				Travel(Centre + (Pos - Centre) / Distance * (Radius - 1.0), Speed);
			}
		}

		void ReleaseHeld()
		{
			if (G.GetHeld() != ELotMovable::Count)
			{
				G.Release(Pos, 0.0);
			}
		}

		void EnsureHand(ELotHand Hand)
		{
			Cur = EAct::Swap;
			ReleaseHeld();
			while (!bAborted && G.IsHandBusy())
			{
				Advance(HandStep);
			}
			if (G.GetHand() != Hand)
			{
				G.SelectHand(Hand);
			}
			while (!bAborted && G.IsHandBusy())
			{
				Advance(HandStep);
			}
		}

		/** Moves the station to 5 m from a far target (the player walks to it, carries it and puts it down). */
		void PlaceStation(FVector2D Target, double RelocateCm)
		{
			if (!G.Owns(ELotItem::Station))
			{
				return;
			}
			const FVector2D Station = G.World().Movables[static_cast<int32>(ELotMovable::Station)].Pos;
			const double Distance = FVector2D::Distance(Station, Target);
			if (Distance <= RelocateCm)
			{
				return;
			}
			if (G.World().bPlayerInCar)
			{
				// Out of the car, where it stands.
				G.World().CarPos = Pos;
				G.World().bPlayerInCar = false;
			}
			ReleaseHeld();
			GoWithin(Station, LotBalance::InteractionRangeCm, WalkSpeed());
			if (G.Grab(ELotMovable::Station))
			{
				Travel(Target - (Target - Station) / Distance * StationOffsetCm, LotBalance::StationCarrySpeedCmS);
				G.Release(Pos, 0.0);
			}
		}

		// --- jobs ---------------------------------------------------------------------------------------------------

		void RunJob(int32 Target)
		{
			ReleaseHeld();
			if (G.Owns(ELotItem::Car))
			{
				CarJob(Target);
			}
			else if (G.Owns(ELotItem::Cart))
			{
				CartJob(Target);
			}
			else if (G.Owns(ELotItem::Bucket))
			{
				const double Walk = FVector2D::Distance(SourcePos(), Centres[Target]) - LotLayout::RefillRadiusCm - WorkReach();
				if (G.Owns(ELotItem::Suitcase) && Walk > SuitcaseMinWalkCm)
				{
					SuitcaseJob(Target);
				}
				else
				{
					BucketJob(Target);
				}
			}
			else
			{
				BowlJob(Target);
			}
		}

		static double WorkReach()
		{
			return LotBalance::ReachCm - CellReachMarginCm;
		}

		/** Scoops from the trough into the bowl, then pours it onto the cell, until the cell is complete. */
		void BowlJob(int32 Cell)
		{
			for (int32 Loop = 0; Loop < MaxJobLoops && !bAborted && !G.GetGrid().IsCellComplete(Cell); ++Loop)
			{
				EnsureHand(ELotHand::Bowl);
				const double Want = FMath::Min(G.GetHandCapacity(ELotHand::Bowl), MissingLitres(Cell));
				if (G.GetHandLitres(ELotHand::Bowl) < Want - 1e-9)
				{
					GoWithin(LotLayout::TroughPos, LotBalance::ReachCm, WalkSpeed());
					Cur = EAct::Scoop;
					while (!bAborted && G.GetHandLitres(ELotHand::Bowl) < Want - 1e-9)
					{
						G.ScoopFromTrough(HandStep);
						Advance(HandStep);
					}
				}
				GoWithin(Centres[Cell], WorkReach(), WalkSpeed());
				PourBowl(Cell);
			}
		}

		/** Pours the bowl over a grid of aim points of the cell until it is empty or the cell is complete. */
		void PourBowl(int32 Cell)
		{
			const FVector2D Origin = Centres[Cell] - FVector2D(AsphaltLot::CellCm * 0.5, AsphaltLot::CellCm * 0.5);
			const double Spacing = AsphaltLot::CellCm / PourPointsPerSide;
			Cur = EAct::Pour;
			for (int32 Row = 0; Row < PourPointsPerSide; ++Row)
			{
				for (int32 K = 0; K < PourPointsPerSide; ++K)
				{
					const int32 Col = Row % 2 == 0 ? K : PourPointsPerSide - 1 - K;
					const FVector2D Aim = Origin + FVector2D((Col + 0.5) * Spacing, (Row + 0.5) * Spacing);
					// Stay at the aim point while it still takes asphalt.
					while (!bAborted && !G.GetGrid().IsCellComplete(Cell) && G.GetHandLitres(ELotHand::Bowl) > 1e-9)
					{
						const FLotEvents Events = G.PourHand(Aim, Pos, HandStep);
						Advance(HandStep);
						if (Events.LitresPoured <= 1e-12)
						{
							break;
						}
					}
				}
			}
		}

		/** Fills the bucket at the source (or from a stock) and dumps it into the cell, until the cell is complete. */
		void BucketJob(int32 Cell)
		{
			for (int32 Loop = 0; Loop < MaxJobLoops && !bAborted && !G.GetGrid().IsCellComplete(Cell); ++Loop)
			{
				EnsureHand(ELotHand::Bucket);
				GoWithin(SourcePos(), LotLayout::RefillRadiusCm, WalkSpeed());
				Cur = EAct::Fill;
				while (!bAborted && G.GetHandLitres(ELotHand::Bucket) < G.GetHandCapacity(ELotHand::Bucket) - 1e-9)
				{
					G.FillVessel(ELotHand::Bucket, ELotSource::Plant, HandStep);
					Advance(HandStep);
				}
				DumpInto(Cell);
			}
		}

		void DumpInto(int32 Cell)
		{
			EnsureHand(ELotHand::Bucket);
			GoWithin(Centres[Cell], WorkReach(), WalkSpeed());
			G.DumpVessel(Centres[Cell], Pos);
			Advance(0.0);
		}

		/** Walks to a movable, takes it and brings it to the source (put down within the refill radius). */
		bool FetchMovable(ELotMovable Movable, double Speed)
		{
			ReleaseHeld();
			GoWithin(G.World().Movables[static_cast<int32>(Movable)].Pos, LotBalance::InteractionRangeCm, WalkSpeed());
			if (!G.Grab(Movable))
			{
				return false;
			}
			GoWithin(SourcePos(), LotLayout::RefillRadiusCm, Speed);
			G.Release(Pos, 0.0);
			return true;
		}

		/** Fills a movable to the brim; waits for the plant when the stock is dry. */
		void FillMovableFull(ELotMovable Movable)
		{
			Cur = EAct::Fill;
			while (!bAborted && G.GetMovableLitres(Movable) < G.GetMovableCapacity(Movable) - 1e-9)
			{
				G.FillMovable(Movable, HandStep);
				Advance(HandStep);
			}
		}

		/** The suitcase carries up to 60 l out to the work and feeds the bucket there; it is left standing. */
		void SuitcaseJob(int32 Target)
		{
			const ELotMovable Suitcase = ELotMovable::Suitcase;
			const FVector2D TargetPos = Centres[Target];
			const FVector2D SuitcasePos = G.World().Movables[static_cast<int32>(Suitcase)].Pos;
			if (G.GetMovableLitres(Suitcase) < LotBalance::LitresPerCell || FVector2D::Distance(SuitcasePos, TargetPos) > SuitcaseReuseCm)
			{
				if (!FetchMovable(Suitcase, LotBalance::SuitcasePullSpeedCmS))
				{
					return;
				}
				FillMovableFull(Suitcase);
				if (!G.Grab(Suitcase))
				{
					return;
				}
				const double Distance = FVector2D::Distance(Pos, TargetPos);
				Travel(TargetPos - (TargetPos - Pos) / Distance * SuitcaseStandOffCm, LotBalance::SuitcasePullSpeedCmS);
				G.Release(Pos, 0.0);
			}

			const FVector2D Stand = G.World().Movables[static_cast<int32>(Suitcase)].Pos;
			int32 Cell = Target;
			for (int32 Loop = 0; Loop < MaxJobLoops && !bAborted && Cell != INDEX_NONE; ++Loop)
			{
				if (G.GetGrid().IsCellComplete(Cell))
				{
					Cell = ChooseTarget(Stand);
					if (Cell == INDEX_NONE || FVector2D::Distance(Stand, Centres[Cell]) > SuitcaseReuseCm)
					{
						return;
					}
					continue;
				}
				if (G.GetMovableLitres(Suitcase) <= 1e-9 && G.GetHandLitres(ELotHand::Bucket) <= 1e-9)
				{
					return;
				}
				EnsureHand(ELotHand::Bucket);
				GoWithin(Stand, LotBalance::InteractionRangeCm, WalkSpeed());
				Cur = EAct::Fill;
				while (!bAborted && G.GetMovableLitres(Suitcase) > 1e-9 && G.GetHandLitres(ELotHand::Bucket) < G.GetHandCapacity(ELotHand::Bucket) - 1e-9)
				{
					G.FillVessel(ELotHand::Bucket, ELotSource::Suitcase, HandStep);
					Advance(HandStep);
				}
				DumpInto(Cell);
			}
		}

		/** The cart lays finished strips from its load. */
		void CartJob(int32 Target)
		{
			const ELotMovable Cart = ELotMovable::Cart;
			if (G.GetMovableLitres(Cart) < LotBalance::LitresPerCell)
			{
				if (!FetchMovable(Cart, LotBalance::CartPushSpeedCmS))
				{
					return;
				}
				FillMovableFull(Cart);
			}
			int32 Cell = Target;
			for (int32 Loop = 0; Loop < MaxJobLoops && !bAborted && Cell != INDEX_NONE && G.GetMovableLitres(Cart) >= LotBalance::LitresPerCell; ++Loop)
			{
				LayStrip(Cell);
				Cell = ChooseTarget(Pos);
				if (Cell != INDEX_NONE && FVector2D::Distance(Pos, Centres[Cell]) > CartContinueCm)
				{
					return;
				}
			}
		}

		/**
		 * Strip over up to MaxCells incomplete cells in a straight line from Cell, in the direction (along X, or also
		 * along Y when bVertical) with the longest run. OnlyEntry restricts the run to one entry segment (INDEX_NONE: any cell).
		 */
		FStrip PlanStrip(int32 Cell, int32 MaxCells, int32 OnlyEntry, bool bVertical) const
		{
			const FIntPoint Coord = AsphaltLot::CellCoord(Cell);
			const FIntPoint Dirs[4] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};
			int32 BestRun = 0;
			FIntPoint BestDir = Dirs[0];
			for (int32 D = 0; D < (bVertical ? 4 : 2); ++D)
			{
				int32 Count = 0;
				while (Count < MaxCells)
				{
					const int32 Next = AsphaltLot::CellIndex(Coord.X + Dirs[D].X * Count, Coord.Y + Dirs[D].Y * Count);
					if (Next == INDEX_NONE || G.GetGrid().IsCellComplete(Next) || (OnlyEntry != INDEX_NONE && AsphaltLot::EntryOf(Next) != OnlyEntry))
					{
						break;
					}
					++Count;
				}
				if (Count > BestRun)
				{
					BestRun = Count;
					BestDir = Dirs[D];
				}
			}
			FStrip Strip;
			Strip.Cells = FMath::Max(1, BestRun);
			Strip.Dir = FVector2D(BestDir.X, BestDir.Y);
			Strip.RowBottom = -AsphaltLot::HalfExtentCm + Coord.Y * AsphaltLot::CellCm;
			const double HalfInset = AsphaltLot::CellCm * 0.5 - LotBalance::StampPadCm;
			Strip.A = Centres[Cell] - Strip.Dir * HalfInset;
			Strip.B = Centres[Cell] + Strip.Dir * ((Strip.Cells - 1) * AsphaltLot::CellCm + HalfInset);
			return Strip;
		}

		void LayStrip(int32 Cell)
		{
			const ELotMovable Cart = ELotMovable::Cart;
			const int32 MaxCells = FMath::FloorToInt(G.GetMovableLitres(Cart) / LotBalance::LitresPerCell + 1e-9);
			const FStrip Strip = PlanStrip(Cell, MaxCells, GoalEntry, true);
			ReleaseHeld();
			GoWithin(G.World().Movables[static_cast<int32>(Cart)].Pos, LotBalance::InteractionRangeCm, WalkSpeed());
			if (!G.Grab(Cart))
			{
				return;
			}
			++NumStrips;
			StripCells += Strip.Cells;
			Travel(Strip.A, LotBalance::CartPushSpeedCmS);
			Cur = EAct::Lay;
			MoveAlong(Strip.B, LotBalance::CartLaySpeedCmS, [](double) { return true; }, [this](FVector2D From, FVector2D To)
			{
				G.LayCartStrip(From, To);
			});
			G.Release(Pos, 0.0);
		}

		/** The car lays a band (3 m or 3.5 m wide) along a row, starting at the row's lower edge; it waits when the stock cannot pay a step. */
		void CarJob(int32 Target)
		{
			ReleaseHeld();
			if (!G.World().bPlayerInCar)
			{
				GoWithin(G.World().CarPos, LotBalance::InteractionRangeCm, WalkSpeed());
				Pos = G.World().CarPos;
				G.World().bPlayerInCar = true;
			}
			const int32 Level = G.GetCarLevel();
			const double Width = LotBalance::CarWidthCm[Level];
			const double Speed = LotBalance::CarPaveSpeedCmS(Level);
			FStrip Strip = PlanStrip(Target, CarMaxCells, INDEX_NONE, false);
			const double Y = Strip.RowBottom + Width * 0.5;
			Strip.A.Y = Y;
			Strip.B.Y = Y;
			// A band needs about this much; with less in the tank the car drives to a station first.
			const double Needed = Strip.Cells * Width / AsphaltLot::CellCm * LotBalance::LitresPerCell;
			if (G.GetCarTank() < FMath::Min(Needed, G.GetCarTankCapacity()))
			{
				LoadCarAtStation();
			}
			Travel(Strip.A, LotBalance::CarSpeedCmS);
			MoveAlong(Strip.B, Speed, [this, Width, Speed](double Dt)
			{
				// A step is taken only when the tank can pay for all of it; an empty tank is filled at a station and the band goes on from where it stopped.
				const double Cost = Width * Speed * Dt / (AsphaltLot::CellCm * AsphaltLot::CellCm) * LotBalance::LitresPerCell;
				if (G.GetCarTank() < Cost)
				{
					const FVector2D Back = Pos;
					LoadCarAtStation();
					Travel(Back, LotBalance::CarSpeedCmS);
				}
				const bool bReady = G.GetCarTank() >= Cost;
				Cur = bReady ? EAct::CarLay : EAct::CarWait;
				return bReady;
			}, [this](FVector2D From, FVector2D To)
			{
				G.PourFromCar(From, To);
			});
			G.World().CarPos = Pos;
		}

		/** Drives to the nearest station and waits there until the tank is full. */
		void LoadCarAtStation()
		{
			Travel(G.GetStationPos(G.FindNearestStation(Pos)), LotBalance::CarSpeedCmS);
			Cur = EAct::CarWait;
			while (!bAborted && G.GetCarTank() < G.GetCarTankCapacity() - 1e-9)
			{
				G.World().CarPos = Pos;
				G.LoadCar(HandStep);
				Advance(HandStep);
			}
		}

		TArray<FVector2D> Centres;
		TArray<bool> SectorDone;
		int32 WorkSector = INDEX_NONE;
		/** Entry segment the current target belongs to while the player paves it; else INDEX_NONE. */
		int32 GoalEntry = INDEX_NONE;
		double NextPaverCheck = 0.0;
		bool bAborted = false;
		EAct Cur = EAct::Walk;
	};
}

bool FLotBalanceSimPlaythroughTest::RunTest(const FString& Parameters)
{
	const double WallStart = FPlatformTime::Seconds();
	TUniquePtr<FSimPlayer> Sim = MakeUnique<FSimPlayer>();
	Sim->Run();
	const double WallSeconds = FPlatformTime::Seconds() - WallStart;
	const FLotGame& G = Sim->G;

	for (const FString& Line : Sim->Log)
	{
		AddInfo(Line);
	}
	UE_LOG(LogLotBalanceSim, Display, TEXT("seconds per activity: %s cart strips: %d with %d cells"), *Sim->ActReport(), Sim->NumStrips, Sim->StripCells);
	FString Table;
	for (int32 I = 0; I < NumMilestones; ++I)
	{
		Table += FString::Printf(TEXT("%d cells=%s "), MilestoneCells[I], *Clock(Sim->MilestoneSeconds[I]));
	}
	const FString Summary = FString::Printf(TEXT("total %s game time, %.2f s wall clock, %d items, plant level %d, %.0f l poured, credits left %.0f, minimal stock %.3f; %s"),
		*Clock(G.GetPlaytimeSeconds()), WallSeconds, G.GetNumItemsOwned(), G.GetPlantLevel(), G.GetTotalLitres(), G.GetCredits(), Sim->MinStock, *Table);
	UE_LOG(LogLotBalanceSim, Display, TEXT("%s"), *Summary);
	AddInfo(Summary);

	const auto Minutes = [&](int32 MilestoneIndex) { return Sim->MilestoneSeconds[MilestoneIndex] / 60.0; };
	// Indices into MilestoneCells: 1, 10, 100, 350, 800, 6000, 30000.
	TestFalse(TEXT("the simulated player does not give up"), Sim->bGaveUp);
	TestTrue(TEXT("the exit opens"), G.IsExitOpen());
	TestTrue(TEXT("1st cell between 40 and 80 s"), Sim->MilestoneSeconds[0] >= 40.0 && Sim->MilestoneSeconds[0] <= 80.0);
	TestTrue(TEXT("10 cells within 6 min"), Sim->MilestoneSeconds[1] >= 0.0 && Minutes(1) <= 6.0);
	TestTrue(TEXT("100 cells within 18 min"), Sim->MilestoneSeconds[2] >= 0.0 && Minutes(2) <= 18.0);
	TestTrue(TEXT("800 cells within 42 min"), Sim->MilestoneSeconds[4] >= 0.0 && Minutes(4) <= 42.0);
	TestTrue(TEXT("6000 cells within 78 min"), Sim->MilestoneSeconds[5] >= 0.0 && Minutes(5) <= 78.0);
	TestTrue(TEXT("30000 cells between 105 and 165 min"), Sim->MilestoneSeconds[6] >= 0.0 && Minutes(6) >= 105.0 && Minutes(6) <= 165.0);
	TestEqual(TEXT("all items are bought"), G.GetNumItemsOwned(), static_cast<int32>(ELotItem::Count));
	TestEqual(TEXT("all 5 plant levels are bought"), G.GetPlantLevel(), 5);
	TestTrue(TEXT("the stock is never negative"), Sim->MinStock >= 0.0);
	TestTrue(TEXT("the game ends within the six hour cap"), G.GetPlaytimeSeconds() <= 6.0 * 3600.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
