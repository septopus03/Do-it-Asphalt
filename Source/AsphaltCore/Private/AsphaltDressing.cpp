// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltDressing.h"
#include "AsphaltLayout.h"
#include "HexMap.h"

namespace
{
	/** Seed and constants of the small linear congruential generator (Knuth's MMIX) that every layout starts from. */
	constexpr uint64 DressingSeed = 20261119;
	constexpr uint64 DressingLcgMultiplier = 6364136223846793005ULL;
	constexpr uint64 DressingLcgIncrement = 1442695040888963407ULL;
	constexpr int32 DressingLcgShift = 32;
	constexpr double DressingLcgRange = 4294967296.0;

	/** Rejection sampling gives up on an item after this many tries; the layout is then short (the test notices). */
	constexpr int32 DressingMaxTries = 20000;
	/** A lamp that cannot stand at its corner slides along the outline in steps of this angle, to the side first, up to the limit. */
	constexpr double DressingLampStepDeg = 0.5;
	constexpr int32 DressingLampMaxSteps = 90;

	constexpr double DressingFullCircleDeg = 360.0;
	constexpr double DressingHalfSqrt3 = 0.86602540378443864676;

	/** Footprint radii of the props (from their models); a prop keeps its radius extra from everything it avoids. */
	constexpr double DressingRadiusCm[] = {15.0, 30.0, 20.0, 100.0};
	/** Free space between the footprints of two neighbouring props, if that is more than MinSpacingCm. */
	constexpr double DressingGapCm = 30.0;

	double DressingRadius(EDressingKind Kind)
	{
		return DressingRadiusCm[static_cast<int32>(Kind)];
	}

	/** Deterministic generator: the same seed gives the same numbers on every machine. */
	struct FDressingRandom
	{
		uint64 State = DressingSeed;

		/** A number in [0, 1). */
		double Next()
		{
			State = State * DressingLcgMultiplier + DressingLcgIncrement;
			return static_cast<double>(State >> DressingLcgShift) / DressingLcgRange;
		}
	};

	/** Distance from a point to the segment A-B. */
	double DressingSegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / AB.SizeSquared(), 0.0, 1.0);
		return FVector2D::Distance(P, A + AB * T);
	}

	/** Everything the placing needs, computed once per layout. */
	struct FDressingSite
	{
		TArray<FVector2D> CellCentres;
		TArray<FVector2D> Spots;
		FVector2D RoadEnds[FHexMap::NumRoads];
		FVector2D Gates[FHexMap::NumRoads];

		explicit FDressingSite(const FHexMap& Map)
		{
			for (int32 Index = 0; Index < Map.Num(); ++Index)
			{
				CellCentres.Add(AsphaltHex::ToWorld(Map.GetCell(Index).Coord));
			}
			Spots.Add(AsphaltLayout::RefillCenter);
			Spots.Add(AsphaltLayout::PlayerStart);
			for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
			{
				Spots.Add(AsphaltLayout::KioskPos(Road));
				Spots.Add(AsphaltLayout::GatePos(Road));
				Spots.Add(AsphaltLayout::SpecialSpawn(Road));
				RoadEnds[Road] = AsphaltLayout::RoadEnd(Road);
				Gates[Road] = AsphaltLayout::GatePos(Road);
			}
		}

		/** The prop may stand here as far as the map, the layout and the fence are concerned (not yet as far as other props are). */
		bool IsFree(EDressingKind Kind, const FVector2D& P) const
		{
			const double Radius = DressingRadius(Kind);
			if (P.Size() > AsphaltLayout::SiteRadiusCm - AsphaltDressing::FenceMarginCm - Radius)
			{
				return false;
			}
			// 150 cm from the outline of a cell (so not on one either): from its centre that is the cell's circumradius (its side) more.
			const double CellLimit = AsphaltDressing::CellClearCm + AsphaltHex::SideCm + Radius;
			for (const FVector2D& Centre : CellCentres)
			{
				if (FVector2D::Distance(P, Centre) < CellLimit)
				{
					return false;
				}
			}
			for (const FVector2D& Spot : Spots)
			{
				if (FVector2D::Distance(P, Spot) < AsphaltDressing::SpotClearCm + Radius)
				{
					return false;
				}
			}
			if (FVector2D::Distance(P, AsphaltLayout::PlantPos) < AsphaltDressing::PlantClearCm + Radius)
			{
				return false;
			}
			for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
			{
				if (DressingSegmentDistance(P, RoadEnds[Road], Gates[Road]) < AsphaltDressing::RoadClearCm + Radius)
				{
					return false;
				}
			}
			return true;
		}
	};

	bool DressingFitsAmongOthers(const FDressingItem& Item, const TArray<FDressingItem>& Placed)
	{
		for (const FDressingItem& Other : Placed)
		{
			const double Limit = FMath::Max(AsphaltDressing::MinSpacingCm, DressingRadius(Item.Kind) + DressingRadius(Other.Kind) + DressingGapCm);
			if (FVector2D::Distance(Item.Pos, Other.Pos) < Limit)
			{
				return false;
			}
		}
		return true;
	}

	/** A lamp faces the lot: its arm (+Y) points at the centre. */
	double DressingLampYaw(const FVector2D& P)
	{
		const FVector2D ToCentre = -P.GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Atan2(-ToCentre.X, ToCentre.Y));
	}

	double DressingWrapYaw(double Deg)
	{
		const double Wrapped = FMath::Fmod(Deg, DressingFullCircleDeg);
		return Wrapped < 0.0 ? Wrapped + DressingFullCircleDeg : Wrapped;
	}

	/** The six lamps: at the corners of the lot (the corner cells are Directions[i] * LotRadius), beyond its outline. */
	void DressingPlaceLamps(const FDressingSite& Site, TArray<FDressingItem>& Items)
	{
		const double CornerRadius = AsphaltHex::ToWorld(AsphaltHex::Directions[0] * FHexMap::LotRadius).Size();
		// The outline of the lot, seen from the centre through a corner, lies the apothem of the corner cell beyond the cell's centre.
		const double LampRadius = CornerRadius + AsphaltHex::SideCm * DressingHalfSqrt3 + AsphaltDressing::LampBeyondEdgeCm;

		for (int32 Corner = 0; Corner < AsphaltDressing::NumLamps; ++Corner)
		{
			const FVector2D Centre = AsphaltHex::ToWorld(AsphaltHex::Directions[Corner] * FHexMap::LotRadius);
			const double CornerDeg = FMath::RadiansToDegrees(FMath::Atan2(Centre.Y, Centre.X));
			// The corner by the plant is where the player stands and the plant looms: that lamp slides along the outline to the nearest free place.
			for (int32 Step = 0; Step <= DressingLampMaxSteps; ++Step)
			{
				bool bPlaced = false;
				for (const double Side : {1.0, -1.0})
				{
					const double Angle = FMath::DegreesToRadians(CornerDeg + Side * Step * DressingLampStepDeg);
					FDressingItem Lamp;
					Lamp.Kind = EDressingKind::Lamp;
					Lamp.Pos = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * LampRadius;
					Lamp.YawDeg = DressingWrapYaw(DressingLampYaw(Lamp.Pos));
					if (Site.IsFree(Lamp.Kind, Lamp.Pos) && DressingFitsAmongOthers(Lamp, Items))
					{
						Items.Add(Lamp);
						bPlaced = true;
						break;
					}
				}
				if (bPlaced)
				{
					break;
				}
			}
		}
	}

	/** Scatters Count props of one kind over the ring between the lot and the fence. */
	void DressingScatter(const FDressingSite& Site, FDressingRandom& Random, EDressingKind Kind, int32 Count, TArray<FDressingItem>& Items)
	{
		const double OuterRadius = AsphaltLayout::SiteRadiusCm - AsphaltDressing::FenceMarginCm;
		for (int32 Placed = 0; Placed < Count; ++Placed)
		{
			for (int32 Try = 0; Try < DressingMaxTries; ++Try)
			{
				// Uniform over the area of the disc: the radius is the root of a uniform number.
				const double Radius = OuterRadius * FMath::Sqrt(Random.Next());
				const double Angle = Random.Next() * 2.0 * UE_DOUBLE_PI;
				FDressingItem Item;
				Item.Kind = Kind;
				Item.Pos = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
				Item.YawDeg = Random.Next() * DressingFullCircleDeg;
				if (Site.IsFree(Kind, Item.Pos) && DressingFitsAmongOthers(Item, Items))
				{
					Items.Add(Item);
					break;
				}
			}
		}
	}
}

TArray<FDressingItem> AsphaltDressing::Build(const FHexMap& Map)
{
	const FDressingSite Site(Map);
	FDressingRandom Random;

	TArray<FDressingItem> Items;
	Items.Reserve(NumItems);
	// The biggest and the most constrained first, so that the small ones fill the gaps.
	DressingPlaceLamps(Site, Items);
	DressingScatter(Site, Random, EDressingKind::DirtPile, NumDirtPiles, Items);
	DressingScatter(Site, Random, EDressingKind::Barrel, NumBarrels, Items);
	DressingScatter(Site, Random, EDressingKind::Cone, NumCones, Items);
	return Items;
}
