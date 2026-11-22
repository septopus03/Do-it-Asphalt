// Copyright Do it Asphalt. All Rights Reserved.

#include "LotFluid.h"
#include "LotBalance.h"
#include "LotGrid.h"
#include <type_traits>

static_assert(FLotFluid::GridSide == FLotGrid::Size, "the fluid lives on the grid of the lot");
static_assert(FLotFluid::GridSide % FLotFluid::TileSide == 0, "the grid is a whole number of tiles");

namespace
{
	constexpr int32 S = FLotFluid::TileSide;
	constexpr int32 Half = FLotFluid::GridSide / 2;

	/** Thickness below which asphalt is dust and is dropped (it keeps tiles from living on rounding residue). */
	constexpr float DustUnits = 1e-3f;
	/** Flows smaller than this are zero. */
	constexpr float DustFlow = 1e-5f;

	const float Film = static_cast<float>(LotBalance::FluidFilmUnits);
	const float Cover = static_cast<float>(LotBalance::FluidCoverUnits);
	const float Stiffness = static_cast<float>(LotBalance::FluidStiffness);
	const float Momentum = static_cast<float>(LotBalance::FluidMomentum);
	const float FreezeHeat = static_cast<float>(LotBalance::FluidFreezeHeat);
	/** Share of the heat that is left after one step: the heat falls to 1/e in FluidCoolSeconds. */
	const float CoolPerStep = static_cast<float>(FMath::Exp(-LotBalance::FluidStepSeconds / LotBalance::FluidCoolSeconds));

	inline int32 Idx(int32 Lx, int32 Ly)
	{
		return Ly * S + Lx;
	}

	inline bool Pourable(int32 X, int32 Y)
	{
		return X >= 0 && Y >= 0 && X < FLotFluid::GridSide && Y < FLotFluid::GridSide && !(X >= Half && Y >= Half);
	}

	/** Value of an array of the tile at local (Lx, Ly), which may lie one texel outside the tile on one axis; zero where there is no tile. */
	template <typename TileT, typename GetArr>
	auto Sample(const TileT* L, const TileT* R, const TileT* D, const TileT* U, const TileT& C, int32 Lx, int32 Ly, GetArr Get)
	{
		using V = std::remove_cv_t<std::remove_reference_t<decltype(Get(C)[0])>>;
		if (Lx < 0)
		{
			return L ? static_cast<V>(Get(*L)[Idx(S - 1, Ly)]) : static_cast<V>(0);
		}
		if (Lx >= S)
		{
			return R ? static_cast<V>(Get(*R)[Idx(0, Ly)]) : static_cast<V>(0);
		}
		if (Ly < 0)
		{
			return D ? static_cast<V>(Get(*D)[Idx(Lx, S - 1)]) : static_cast<V>(0);
		}
		if (Ly >= S)
		{
			return U ? static_cast<V>(Get(*U)[Idx(Lx, 0)]) : static_cast<V>(0);
		}
		return static_cast<V>(Get(C)[Idx(Lx, Ly)]);
	}

	/** The flow across the face between a texel and its neighbour (positive from the texel to the neighbour). */
	float FlowAcross(float Prev, float Hi, float Ei, float Hj, float Ej, bool bOpen)
	{
		const float Sum = Hi + Hj;
		if (!bOpen || Sum <= 0.f)
		{
			return 0.f;
		}
		// Only what stands above the film runs; the heat of the face is the average of the two, and the run is slowed by the square of it.
		const float Heat = FMath::Min(1.f, (Ei + Ej) / Sum);
		const float Drive = FMath::Max(Hi, Film) - FMath::Max(Hj, Film);
		const float Flow = Momentum * Prev + Stiffness * Heat * Heat * Drive;
		return FMath::Abs(Flow) < DustFlow ? 0.f : Flow;
	}

	inline float HeatOf(float H, float E)
	{
		return H > 0.f ? FMath::Min(1.f, E / H) : 0.f;
	}
}

FLotFluid::FLotFluid()
{
	TileTable.Init(-1, TilesPerSide * TilesPerSide);
}

FLotFluid::FLotFluid(const FLotFluid& Other)
{
	*this = Other;
}

FLotFluid& FLotFluid::operator=(const FLotFluid& Other)
{
	if (this != &Other)
	{
		Tiles.Reset();
		for (const TUniquePtr<FTile>& Tile : Other.Tiles)
		{
			Tiles.Add(Tile ? MakeUnique<FTile>(*Tile) : TUniquePtr<FTile>());
		}
		TileTable = Other.TileTable;
		FreeSlots = Other.FreeSlots;
		NumUsedTiles = Other.NumUsedTiles;
	}
	return *this;
}

bool FLotFluid::IsPourable(int32 X, int32 Y)
{
	return Pourable(X, Y);
}

const FLotFluid::FTile* FLotFluid::TileOf(int32 X, int32 Y) const
{
	return Pourable(X, Y) ? TileAt(X >> TileShift, Y >> TileShift) : nullptr;
}

FLotFluid::FTile* FLotFluid::TileAt(int32 Tx, int32 Ty) const
{
	if (Tx < 0 || Ty < 0 || Tx >= TilesPerSide || Ty >= TilesPerSide)
	{
		return nullptr;
	}
	const int32 Slot = TileTable[Ty * TilesPerSide + Tx];
	return Slot >= 0 ? Tiles[Slot].Get() : nullptr;
}

FLotFluid::FTile* FLotFluid::EnsureTile(int32 Tx, int32 Ty)
{
	if (Tx < 0 || Ty < 0 || Tx >= TilesPerSide || Ty >= TilesPerSide)
	{
		return nullptr;
	}
	int32& Entry = TileTable[Ty * TilesPerSide + Tx];
	if (Entry < 0)
	{
		TUniquePtr<FTile> Tile = MakeUnique<FTile>();
		FMemory::Memzero(Tile.Get(), sizeof(FTile));
		Tile->Tx = Tx;
		Tile->Ty = Ty;
		if (FreeSlots.Num() > 0)
		{
			Entry = FreeSlots.Pop(EAllowShrinking::No);
			Tiles[Entry] = MoveTemp(Tile);
		}
		else
		{
			Entry = Tiles.Add(MoveTemp(Tile));
		}
		++NumUsedTiles;
	}
	return Tiles[Entry].Get();
}

void FLotFluid::ReleaseTile(int32 Slot)
{
	FTile* Tile = Tiles[Slot].Get();
	TileTable[Tile->Ty * TilesPerSide + Tile->Tx] = -1;
	Tiles[Slot].Reset();
	FreeSlots.Add(Slot);
	--NumUsedTiles;
}

void FLotFluid::Clear()
{
	Tiles.Reset();
	FreeSlots.Reset();
	TileTable.Init(-1, TilesPerSide * TilesPerSide);
	NumUsedTiles = 0;
}

bool FLotFluid::Add(int32 X, int32 Y, float Units)
{
	if (!Pourable(X, Y) || !(Units > 0.f) || !FMath::IsFinite(Units))
	{
		return false;
	}
	FTile* Tile = EnsureTile(X >> TileShift, Y >> TileShift);
	const int32 I = Idx(X & (TileSide - 1), Y & (TileSide - 1));
	Tile->H[I] += Units;
	// New asphalt is as hot as it gets.
	Tile->E[I] += Units;
	return true;
}

float FLotFluid::GetUnits(int32 X, int32 Y) const
{
	const FTile* Tile = TileOf(X, Y);
	return Tile ? Tile->H[Idx(X & (TileSide - 1), Y & (TileSide - 1))] : 0.f;
}

float FLotFluid::GetHeat(int32 X, int32 Y) const
{
	const FTile* Tile = TileOf(X, Y);
	if (!Tile)
	{
		return 0.f;
	}
	const int32 I = Idx(X & (TileSide - 1), Y & (TileSide - 1));
	return HeatOf(Tile->H[I], Tile->E[I]);
}

double FLotFluid::GetTotalUnits() const
{
	double Total = 0.0;
	for (const TUniquePtr<FTile>& Tile : Tiles)
	{
		if (Tile)
		{
			for (int32 I = 0; I < TileTexels; ++I)
			{
				Total += Tile->H[I];
			}
		}
	}
	return Total;
}

void FLotFluid::GrowBorders()
{
	const int32 NumSlots = Tiles.Num();
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		const FTile* Tile = Tiles[Slot].Get();
		if (!Tile)
		{
			continue;
		}
		bool bLeft = false, bRight = false, bDown = false, bUp = false;
		for (int32 K = 0; K < S; ++K)
		{
			bLeft |= Tile->H[Idx(0, K)] > 0.f;
			bRight |= Tile->H[Idx(S - 1, K)] > 0.f;
			bDown |= Tile->H[Idx(K, 0)] > 0.f;
			bUp |= Tile->H[Idx(K, S - 1)] > 0.f;
		}
		// EnsureTile may add to Tiles, so the tile is copied out of the way first.
		const int32 Tx = Tile->Tx;
		const int32 Ty = Tile->Ty;
		if (bLeft)
		{
			EnsureTile(Tx - 1, Ty);
		}
		if (bRight)
		{
			EnsureTile(Tx + 1, Ty);
		}
		if (bDown)
		{
			EnsureTile(Tx, Ty - 1);
		}
		if (bUp)
		{
			EnsureTile(Tx, Ty + 1);
		}
	}
}

void FLotFluid::Step(TConstArrayView<uint8> Texels, TFunctionRef<void(int32, int32)> OnSet)
{
	if (NumUsedTiles == 0)
	{
		return;
	}
	check(Texels.Num() == GridSide * GridSide);
	GrowBorders();

	struct FNeighbours
	{
		const FTile* L;
		const FTile* R;
		const FTile* D;
		const FTile* U;
	};
	TArray<FTile*> Used;
	TArray<FNeighbours> Nb;
	Used.Reserve(NumUsedTiles);
	Nb.Reserve(NumUsedTiles);
	TArray<int32> UsedSlots;
	UsedSlots.Reserve(NumUsedTiles);
	for (int32 Slot = 0; Slot < Tiles.Num(); ++Slot)
	{
		if (FTile* Tile = Tiles[Slot].Get())
		{
			Used.Add(Tile);
			UsedSlots.Add(Slot);
		}
	}
	for (const FTile* Tile : Used)
	{
		Nb.Add({TileAt(Tile->Tx - 1, Tile->Ty), TileAt(Tile->Tx + 1, Tile->Ty), TileAt(Tile->Tx, Tile->Ty - 1), TileAt(Tile->Tx, Tile->Ty + 1)});
	}

	const auto GetH = [](const FTile& T) { return T.H; };
	const auto GetE = [](const FTile& T) { return T.E; };
	const auto GetFx = [](const FTile& T) { return T.Fx; };
	const auto GetFy = [](const FTile& T) { return T.Fy; };
	const auto GetS = [](const FTile& T) { return T.S; };

	// A: how hard each face pushes (the old flow carries on, which is the inertia).
	for (int32 T = 0; T < Used.Num(); ++T)
	{
		FTile& C = *Used[T];
		const FNeighbours& N = Nb[T];
		for (int32 Ly = 0; Ly < S; ++Ly)
		{
			for (int32 Lx = 0; Lx < S; ++Lx)
			{
				const int32 I = Idx(Lx, Ly);
				const int32 Gx = C.Tx * S + Lx;
				const int32 Gy = C.Ty * S + Ly;
				const float Hi = C.H[I];
				const float Ei = C.E[I];
				const float Hr = Sample(N.L, N.R, N.D, N.U, C, Lx + 1, Ly, GetH);
				const float Er = Sample(N.L, N.R, N.D, N.U, C, Lx + 1, Ly, GetE);
				const float Hu = Sample(N.L, N.R, N.D, N.U, C, Lx, Ly + 1, GetH);
				const float Eu = Sample(N.L, N.R, N.D, N.U, C, Lx, Ly + 1, GetE);
				const bool bHere = Pourable(Gx, Gy);
				C.Fx[I] = FlowAcross(C.Fx[I], Hi, Ei, Hr, Er, bHere && Pourable(Gx + 1, Gy));
				C.Fy[I] = FlowAcross(C.Fy[I], Hi, Ei, Hu, Eu, bHere && Pourable(Gx, Gy + 1));
			}
		}
	}

	// B: a texel cannot give more than it holds; the share it can give.
	for (int32 T = 0; T < Used.Num(); ++T)
	{
		FTile& C = *Used[T];
		const FNeighbours& N = Nb[T];
		for (int32 Ly = 0; Ly < S; ++Ly)
		{
			for (int32 Lx = 0; Lx < S; ++Lx)
			{
				const int32 I = Idx(Lx, Ly);
				const float FromLeft = Sample(N.L, N.R, N.D, N.U, C, Lx - 1, Ly, GetFx);
				const float FromDown = Sample(N.L, N.R, N.D, N.U, C, Lx, Ly - 1, GetFy);
				const float Out = FMath::Max(C.Fx[I], 0.f) + FMath::Max(-FromLeft, 0.f) + FMath::Max(C.Fy[I], 0.f) + FMath::Max(-FromDown, 0.f);
				C.S[I] = Out > C.H[I] ? C.H[I] / Out : 1.f;
			}
		}
	}

	// C: the flows that really happen (the share of the texel they leave).
	for (int32 T = 0; T < Used.Num(); ++T)
	{
		FTile& C = *Used[T];
		const FNeighbours& N = Nb[T];
		for (int32 Ly = 0; Ly < S; ++Ly)
		{
			for (int32 Lx = 0; Lx < S; ++Lx)
			{
				const int32 I = Idx(Lx, Ly);
				const bool bRightIn = Lx + 1 < S || N.R;
				const bool bUpIn = Ly + 1 < S || N.U;
				const float Sr = bRightIn ? Sample(N.L, N.R, N.D, N.U, C, Lx + 1, Ly, GetS) : 1.f;
				const float Su = bUpIn ? Sample(N.L, N.R, N.D, N.U, C, Lx, Ly + 1, GetS) : 1.f;
				C.Fx[I] *= C.Fx[I] > 0.f ? C.S[I] : Sr;
				C.Fy[I] *= C.Fy[I] > 0.f ? C.S[I] : Su;
			}
		}
	}

	// D: what each texel holds after the flows, its heat (carried by the flow).
	for (int32 T = 0; T < Used.Num(); ++T)
	{
		FTile& C = *Used[T];
		const FNeighbours& N = Nb[T];
		for (int32 Ly = 0; Ly < S; ++Ly)
		{
			for (int32 Lx = 0; Lx < S; ++Lx)
			{
				const int32 I = Idx(Lx, Ly);
				const float Hi = C.H[I];
				const float Ei = C.E[I];
				const float HeatI = HeatOf(Hi, Ei);

				const float Er = C.Fx[I];
				const float Eu = C.Fy[I];
				const float El = Sample(N.L, N.R, N.D, N.U, C, Lx - 1, Ly, GetFx);
				const float Ed = Sample(N.L, N.R, N.D, N.U, C, Lx, Ly - 1, GetFy);
				const float Hl = Sample(N.L, N.R, N.D, N.U, C, Lx - 1, Ly, GetH);
				const float Hr = Sample(N.L, N.R, N.D, N.U, C, Lx + 1, Ly, GetH);
				const float Hd = Sample(N.L, N.R, N.D, N.U, C, Lx, Ly - 1, GetH);
				const float Hu = Sample(N.L, N.R, N.D, N.U, C, Lx, Ly + 1, GetH);
				const float HeatL = HeatOf(Hl, Sample(N.L, N.R, N.D, N.U, C, Lx - 1, Ly, GetE));
				const float HeatR = HeatOf(Hr, Sample(N.L, N.R, N.D, N.U, C, Lx + 1, Ly, GetE));
				const float HeatD = HeatOf(Hd, Sample(N.L, N.R, N.D, N.U, C, Lx, Ly - 1, GetE));
				const float HeatU = HeatOf(Hu, Sample(N.L, N.R, N.D, N.U, C, Lx, Ly + 1, GetE));

				// A positive flow leaves the texel to the right/up and enters it from the left/down.
				float NewH = Hi - Er + El - Eu + Ed;
				float NewE = Ei - Er * (Er > 0.f ? HeatI : HeatR) + El * (El > 0.f ? HeatL : HeatI) - Eu * (Eu > 0.f ? HeatI : HeatU) + Ed * (Ed > 0.f ? HeatD : HeatI);

				NewH = FMath::Max(NewH, 0.f);
				NewE = FMath::Clamp(NewE, 0.f, NewH);
				C.Hn[I] = NewH;
				C.En[I] = NewE;
			}
		}
	}

	// E: commit, cool, let the asphalt that covers a free texel set, and freeze what has cooled.
	TArray<int32> Empty;
	for (int32 T = 0; T < Used.Num(); ++T)
	{
		FTile& C = *Used[T];
		bool bAny = false;
		for (int32 Ly = 0; Ly < S; ++Ly)
		{
			for (int32 Lx = 0; Lx < S; ++Lx)
			{
				const int32 I = Idx(Lx, Ly);
				float H = C.Hn[I];
				float E = C.En[I] * CoolPerStep;
				if (H < DustUnits)
				{
					H = 0.f;
				}
				if (H > 0.f)
				{
					const int32 Gx = C.Tx * S + Lx;
					const int32 Gy = C.Ty * S + Ly;
					if (H >= Cover && Texels[Gy * GridSide + Gx] == 0)
					{
						OnSet(Gx, Gy);
					}
					if (HeatOf(H, E) < FreezeHeat)
					{
						H = 0.f;
					}
				}
				if (H <= 0.f)
				{
					H = 0.f;
					E = 0.f;
				}
				else
				{
					bAny = true;
				}
				C.H[I] = H;
				C.E[I] = FMath::Min(E, H);
			}
		}
		if (!bAny)
		{
			Empty.Add(UsedSlots[T]);
		}
	}
	for (const int32 Slot : Empty)
	{
		ReleaseTile(Slot);
	}
}
