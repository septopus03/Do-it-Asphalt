// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"
#include "LotBalance.h"
#include "LotGame.h"
#include "LotGrid.h"
#include "LotLayout.h"
#include "LotMap.h"
#include <limits>

DIA_TEST(FLotSaveFreshIsSmallTest, "DoItAsphalt.Lot.Save.FreshIsSmall")
DIA_TEST(FLotSaveRoundTripTest, "DoItAsphalt.Lot.Save.RoundTrip")
DIA_TEST(FLotSaveHeldAndWonTest, "DoItAsphalt.Lot.Save.HeldAndWon")
DIA_TEST(FLotSaveGridRoundTripTest, "DoItAsphalt.Lot.Save.GridRoundTrip")
DIA_TEST(FLotSaveGridReadIsAtomicTest, "DoItAsphalt.Lot.Save.GridReadIsAtomic")
DIA_TEST(FLotSaveDerivedStateTest, "DoItAsphalt.Lot.Save.DerivedState")
DIA_TEST(FLotSaveRejectsCorruptDataTest, "DoItAsphalt.Lot.Save.RejectsCorruptData")
DIA_TEST(FLotSaveRejectsInconsistentTest, "DoItAsphalt.Lot.Save.RejectsInconsistent")
DIA_TEST(FLotSaveRejectsCorruptGridTest, "DoItAsphalt.Lot.Save.RejectsCorruptGrid")
DIA_TEST(FLotSaveRejectsV1Test, "DoItAsphalt.Lot.Save.RejectsV1")

namespace
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();

	/** Byte offsets of the fields in a save; the table is the format, so a layout drift fails the tests loudly. */
	namespace Off
	{
		constexpr int32 Magic = 0;
		constexpr int32 Version = 4;
		constexpr int32 NumCells = 8;
		constexpr int32 GridSize = 12;
		constexpr int32 Credits = 16;
		constexpr int32 Playtime = 24;
		constexpr int32 TotalLitres = 32;
		constexpr int32 PlantLevel = 40;
		constexpr int32 PlantStock = 44;
		constexpr int32 Owned = 52; // 11 bytes in ELotItem order
		constexpr int32 Hand = 63;
		constexpr int32 Bowl = 67;
		constexpr int32 Bucket = 75;
		constexpr int32 Canister = 83;
		constexpr int32 Tub = 91;
		constexpr int32 Swap = 99;
		constexpr int32 Suitcase = 107;
		constexpr int32 Cart = 115;
		constexpr int32 Held = 123;
		constexpr int32 PaverSector = 127;
		constexpr int32 PaverStep = 131;
		constexpr int32 PaverPos = 135;
		constexpr int32 PaverPhase = 151;
		constexpr int32 PaverBudget = 155;
		constexpr int32 PaverTank = 163;
		constexpr int32 CarTank = 171;
		constexpr int32 Won = 179;
		constexpr int32 PlayerPos = 180;
		constexpr int32 PlayerYaw = 196;
		constexpr int32 Movables = 204; // 3 x (position 16 bytes, yaw 8 bytes)
		constexpr int32 CarPos = 276;
		constexpr int32 CarYaw = 292;
		constexpr int32 InCar = 300;
		constexpr int32 Grid = 301;
		constexpr int32 CompleteBitsBytes = 3750;
		constexpr int32 InProgressCount = Grid + CompleteBitsBytes;
		constexpr int32 Records = InProgressCount + 4;
		/** A cell index and one bit per texel of the cell. */
		constexpr int32 RecordBytes = 4 + 50;
	}

	void PatchU8(TArray<uint8>& Blob, int32 At, uint8 Value)
	{
		Blob[At] = Value;
	}

	void PatchI32(TArray<uint8>& Blob, int32 At, int32 Value)
	{
		for (int32 I = 0; I < 4; ++I)
		{
			Blob[At + I] = static_cast<uint8>(static_cast<uint32>(Value) >> (8 * I));
		}
	}

	void PatchDouble(TArray<uint8>& Blob, int32 At, double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		for (int32 I = 0; I < 8; ++I)
		{
			Blob[At + I] = static_cast<uint8>(Bits >> (8 * I));
		}
	}

	int32 ReadI32(const TArray<uint8>& Blob, int32 At)
	{
		uint32 Value = 0;
		for (int32 I = 0; I < 4; ++I)
		{
			Value |= static_cast<uint32>(Blob[At + I]) << (8 * I);
		}
		return static_cast<int32>(Value);
	}

	bool SameBytes(TConstArrayView<uint8> A, TConstArrayView<uint8> B)
	{
		return A.Num() == B.Num() && FMemory::Memcmp(A.GetData(), B.GetData(), A.Num()) == 0;
	}

	/** A strip that covers exactly the cell. */
	void CellStrip(int32 Cell, FVector2D& A, FVector2D& B)
	{
		const FVector2D Centre = AsphaltLot::CellCentre(Cell);
		A = Centre + FVector2D(-45.0, 0.0);
		B = Centre + FVector2D(45.0, 0.0);
	}

	/** Cell in the middle of the lot, far from the first cells that the debug completion takes. */
	int32 FarCell(int32 Col)
	{
		return AsphaltLot::CellIndex(Col, 150 - 60);
	}

	/** Three in-progress cells, each part-covered by a different amount. */
	void AddInProgressCells(FLotGame& G)
	{
		FVector2D A, B;
		CellStrip(FarCell(40), A, B);
		G.GetGrid().Stamp(A, B, 100.0, 2.0);
		CellStrip(FarCell(41), A, B);
		G.GetGrid().Stamp(A, B, 100.0, 1.5);
		CellStrip(FarCell(42), A, B);
		G.GetGrid().Stamp(A, B, 100.0, 1.0);
		G.GetGrid().DepositIntoCell(FarCell(42), 0.75, AsphaltLot::CellCentre(FarCell(42)));
	}

	/**
	 * A game that has been played in every respect that the save keeps. With bHeldCart the player holds the cart,
	 * otherwise sits in the car.
	 */
	FLotGame MakeRichGame(bool bHeldCart)
	{
		FLotGame G;
		G.DebugGive(777.5);
		G.DebugSetPlantLevel(2);
		G.DebugSetPlantStock(G.GetPlantStorage());
		for (ELotItem Item : {ELotItem::Canister, ELotItem::Bucket, ELotItem::Tub, ELotItem::Suitcase, ELotItem::Cart, ELotItem::Station,
			ELotItem::AutoPaver, ELotItem::AutoPaverFast, ELotItem::Car})
		{
			G.DebugGrantItem(Item);
		}
		// 500 cells in the middle of the lot; the entry segments stay open, so the quick swap does not.
		for (int32 Cell = 8000; Cell < 8500; ++Cell)
		{
			G.DebugCompleteCell(Cell);
		}
		AddInProgressCells(G);

		G.ScoopFromTrough(1.3);
		G.SelectHand(ELotHand::Canister);
		G.Tick(2.0);
		G.ScoopFromTrough(1.3);
		G.FillVessel(ELotHand::Bucket, ELotSource::Plant, 1.0);
		G.DebugFillTanks();
		G.FillMovable(ELotMovable::Suitcase, 2.0);
		G.FillMovable(ELotMovable::Cart, 6.0);

		// The auto paver works on sector 7 from step 123: everything before is already complete.
		for (int32 Step = 0; Step < 123; ++Step)
		{
			G.DebugCompleteCell(AsphaltLot::SectorCell(7, Step));
		}
		G.AssignAutoPaverSector(7);
		for (int32 I = 0; I < 400 && G.GetAutoPaverStep() < 123; ++I)
		{
			G.Tick(0.25);
		}

		FLotWorldState& World = G.World();
		World.PlayerPos = FVector2D(123.5, -456.25);
		World.PlayerYawDeg = 77.5;
		for (int32 I = 0; I < static_cast<int32>(ELotMovable::Count); ++I)
		{
			World.Movables[I].Pos = FVector2D(100.0 * I + 0.5, -2000.0 - I);
			World.Movables[I].YawDeg = 10.0 * I - 15.0;
		}
		World.CarPos = FVector2D(1500.0, -300.0);
		World.CarYawDeg = -135.0;
		G.SelectHand(ELotHand::Bucket); // a swap in progress is part of the state
		if (bHeldCart)
		{
			G.Grab(ELotMovable::Cart);
		}
		else
		{
			World.bPlayerInCar = true;
		}
		return G;
	}

	/** A fresh game with nothing bought: the blobs of the "needs an owned item" cases start here. */
	TArray<uint8> FreshBlob()
	{
		FLotGame G;
		return G.SaveToBytes();
	}

	/** Expects the load to fail and the target game to stay as it was. */
	void ExpectRejected(FAutomationTestBase& T, FLotGame& Target, const TArray<uint8>& Blob, const TCHAR* What)
	{
		const TArray<uint8> Before = Target.SaveToBytes();
		T.TestFalse(What, Target.LoadFromBytes(Blob));
		T.TestTrue(FString::Printf(TEXT("%s: target untouched"), What), Target.SaveToBytes() == Before);
	}

	/** Patches a copy of Base and expects it to be rejected. */
	template <typename PatchFn>
	void ExpectPatchRejected(FAutomationTestBase& T, FLotGame& Target, const TArray<uint8>& Base, const TCHAR* What, PatchFn Patch)
	{
		TArray<uint8> Blob = Base;
		Patch(Blob);
		ExpectRejected(T, Target, Blob, What);
	}
}

bool FLotSaveFreshIsSmallTest::RunTest(const FString& Parameters)
{
	FLotGame G;
	const TArray<uint8> Bytes = G.SaveToBytes();
	TestTrue(TEXT("fresh save is below 8 kB"), Bytes.Num() > 0 && Bytes.Num() < 8 * 1024);
	if (!TestEqual(TEXT("header + empty grid"), Bytes.Num(), Off::Records))
	{
		return false;
	}
	TestEqual(TEXT("magic"), static_cast<uint32>(ReadI32(Bytes, Off::Magic)), FLotGame::SaveMagic);
	TestEqual(TEXT("version"), ReadI32(Bytes, Off::Version), FLotGame::SaveVersion);
	TestEqual(TEXT("magic is DIA2 in the file"), FString::Printf(TEXT("%c%c%c%c"), Bytes[0], Bytes[1], Bytes[2], Bytes[3]), FString(TEXT("DIA2")));

	FLotGame Loaded;
	TestTrue(TEXT("fresh save loads"), Loaded.LoadFromBytes(Bytes));
	TestTrue(TEXT("and saves the same"), Loaded.SaveToBytes() == Bytes);
	return true;
}

bool FLotSaveRoundTripTest::RunTest(const FString& Parameters)
{
	for (bool bHeldCart : {false, true})
	{
		const FLotGame Source = MakeRichGame(bHeldCart);
		TestEqual(TEXT("sector assigned"), Source.GetAutoPaverSector(), 7);
		TestTrue(TEXT("step reached"), Source.GetAutoPaverStep() >= 123);
		TestTrue(TEXT("swap in progress"), Source.IsHandBusy());
		TestEqual(TEXT("three in-progress cells at least"), Source.GetGrid().GetCellDone(FarCell(41)) > 0 && Source.GetGrid().GetCellDone(FarCell(40)) > 0, true);

		const TArray<uint8> Bytes = Source.SaveToBytes();
		FLotGame Loaded;
		TestTrue(TEXT("loads"), Loaded.LoadFromBytes(Bytes));
		TestTrue(TEXT("save -> load -> save is byte-identical"), Loaded.SaveToBytes() == Bytes);
		TestTrue(TEXT("texels equal"), SameBytes(Loaded.GetGrid().GetTexels(), Source.GetGrid().GetTexels()));

		TestEqual(TEXT("credits"), Loaded.GetCredits(), Source.GetCredits());
		TestEqual(TEXT("playtime"), Loaded.GetPlaytimeSeconds(), Source.GetPlaytimeSeconds());
		TestEqual(TEXT("total litres"), Loaded.GetTotalLitres(), Source.GetTotalLitres());
		TestEqual(TEXT("plant level"), Loaded.GetPlantLevel(), 2);
		TestEqual(TEXT("plant stock"), Loaded.GetPlantStock(), Source.GetPlantStock());
		for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
		{
			TestEqual(FString::Printf(TEXT("owned %d"), I), Loaded.Owns(static_cast<ELotItem>(I)), Source.Owns(static_cast<ELotItem>(I)));
		}
		TestEqual(TEXT("hand"), Loaded.GetHand(), ELotHand::Bucket);
		TestTrue(TEXT("hand busy"), Loaded.IsHandBusy());
		for (int32 I = 0; I < static_cast<int32>(ELotHand::Count); ++I)
		{
			TestEqual(FString::Printf(TEXT("hand litres %d"), I), Loaded.GetHandLitres(static_cast<ELotHand>(I)), Source.GetHandLitres(static_cast<ELotHand>(I)));
		}
		TestTrue(TEXT("bowl has something"), Loaded.GetHandLitres(ELotHand::Bowl) > 0.0);
		TestEqual(TEXT("suitcase"), Loaded.GetMovableLitres(ELotMovable::Suitcase), Source.GetMovableLitres(ELotMovable::Suitcase));
		TestEqual(TEXT("cart"), Loaded.GetMovableLitres(ELotMovable::Cart), Source.GetMovableLitres(ELotMovable::Cart));
		TestTrue(TEXT("cart filled"), Loaded.GetMovableLitres(ELotMovable::Cart) > 0.0);
		TestEqual(TEXT("held"), Loaded.GetHeld(), Source.GetHeld());
		TestEqual(TEXT("sector"), Loaded.GetAutoPaverSector(), Source.GetAutoPaverSector());
		TestEqual(TEXT("step"), Loaded.GetAutoPaverStep(), Source.GetAutoPaverStep());
		TestEqual(TEXT("paver position"), Loaded.GetAutoPaverPos(), Source.GetAutoPaverPos());
		TestEqual(TEXT("travelling"), Loaded.IsAutoPaverTravelling(), Source.IsAutoPaverTravelling());
		TestEqual(TEXT("completed cells"), Loaded.GetCompletedCells(), Source.GetCompletedCells());

		const FLotWorldState& A = Loaded.World();
		const FLotWorldState& B = Source.World();
		TestEqual(TEXT("player"), A.PlayerPos, B.PlayerPos);
		TestEqual(TEXT("player yaw"), A.PlayerYawDeg, B.PlayerYawDeg);
		for (int32 I = 0; I < static_cast<int32>(ELotMovable::Count); ++I)
		{
			TestEqual(FString::Printf(TEXT("movable %d position"), I), A.Movables[I].Pos, B.Movables[I].Pos);
			TestEqual(FString::Printf(TEXT("movable %d yaw"), I), A.Movables[I].YawDeg, B.Movables[I].YawDeg);
			TestEqual(FString::Printf(TEXT("movable %d held"), I), A.Movables[I].bHeld, B.Movables[I].bHeld);
		}
		TestEqual(TEXT("car"), A.CarPos, B.CarPos);
		TestEqual(TEXT("car yaw"), A.CarYawDeg, B.CarYawDeg);
		TestEqual(TEXT("in car"), A.bPlayerInCar, bHeldCart ? false : true);
	}
	return true;
}

bool FLotSaveHeldAndWonTest::RunTest(const FString& Parameters)
{
	// bHeld is not saved; it comes back from the held movable.
	{
		const FLotGame Source = MakeRichGame(true);
		FLotGame Loaded;
		TestTrue(TEXT("loads"), Loaded.LoadFromBytes(Source.SaveToBytes()));
		TestEqual(TEXT("held cart"), Loaded.GetHeld(), ELotMovable::Cart);
		for (int32 I = 0; I < static_cast<int32>(ELotMovable::Count); ++I)
		{
			TestEqual(FString::Printf(TEXT("movable %d held flag"), I), Loaded.World().Movables[I].bHeld, I == static_cast<int32>(ELotMovable::Cart));
		}
		Loaded.Release(FVector2D(5.0, 6.0), 7.0);
		TestFalse(TEXT("released"), Loaded.World().Movables[static_cast<int32>(ELotMovable::Cart)].bHeld);
	}

	// A won game: everything complete, exit open.
	FLotGame Won;
	Won.DebugCompleteCells(AsphaltLot::NumCells);
	Won.SetWon();
	TestTrue(TEXT("won"), Won.HasWon());
	const TArray<uint8> Bytes = Won.SaveToBytes();
	FLotGame Loaded;
	TestTrue(TEXT("won game loads"), Loaded.LoadFromBytes(Bytes));
	TestTrue(TEXT("still won"), Loaded.HasWon());
	TestTrue(TEXT("exit open"), Loaded.IsExitOpen());
	TestEqual(TEXT("all cells"), Loaded.GetCompletedCells(), AsphaltLot::NumCells);
	TestTrue(TEXT("same bytes"), Loaded.SaveToBytes() == Bytes);
	TestEqual(TEXT("a full lot stores no texels"), Bytes.Num(), Off::Records);
	return true;
}

bool FLotSaveGridRoundTripTest::RunTest(const FString& Parameters)
{
	FLotGame Source;
	Source.DebugCompleteCells(40);
	AddInProgressCells(Source);

	TArray<uint8> Bytes;
	Source.GetGrid().Write(Bytes);
	if (!TestEqual(TEXT("bitset + count + 3 records"), Bytes.Num(), Off::CompleteBitsBytes + 4 + 3 * Off::RecordBytes))
	{
		return false;
	}
	TestEqual(TEXT("bits of the first 8 cells"), static_cast<int32>(Bytes[0]), 0xFF);
	TestEqual(TEXT("count"), ReadI32(Bytes, Off::CompleteBitsBytes), 3);
	TestEqual(TEXT("records ascend"), ReadI32(Bytes, Off::CompleteBitsBytes + 4), FarCell(40));

	// Trailing data after the grid is left for the caller.
	TArray<uint8> WithTail = Bytes;
	WithTail.Add(0xAB);
	FLotGrid Loaded;
	int32 BytesRead = -1;
	TestTrue(TEXT("grid reads"), Loaded.Read(WithTail, BytesRead));
	TestEqual(TEXT("bytes read"), BytesRead, Bytes.Num());
	TestTrue(TEXT("texels equal"), SameBytes(Loaded.GetTexels(), Source.GetGrid().GetTexels()));
	TestEqual(TEXT("complete count"), Loaded.NumCompleteCells(), 40);
	for (int32 Cell : {0, 39, 40, FarCell(40), FarCell(41), FarCell(42)})
	{
		TestEqual(FString::Printf(TEXT("done counter of %d"), Cell), Loaded.GetCellDone(Cell), Source.GetGrid().GetCellDone(Cell));
		TestEqual(FString::Printf(TEXT("complete flag of %d"), Cell), Loaded.IsCellComplete(Cell), Source.GetGrid().IsCellComplete(Cell));
	}

	// Every sector is dirty after a load, so the picture is uploaded whole.
	TArray<FIntRect> Rects;
	Loaded.ConsumeDirtyRects(Rects);
	TestEqual(TEXT("one rectangle per sector"), Rects.Num(), AsphaltLot::NumSectors);
	int64 Area = 0;
	for (const FIntRect& Rect : Rects)
	{
		Area += static_cast<int64>(Rect.Width()) * Rect.Height();
	}
	TestEqual(TEXT("they cover the whole car park"), Area, static_cast<int64>(AsphaltLot::NumCells) * FLotGrid::TexelsPerCell);
	Loaded.ConsumeDirtyRects(Rects);
	TestEqual(TEXT("consumed"), Rects.Num(), 0);

	// The loaded grid works on: completing the partial cell still takes the right litres.
	const FLotPour Pour = Loaded.CompleteCell(FarCell(40));
	TestEqual(TEXT("free part of the half-done cell"), Pour.TexelsChanged, FLotGrid::TexelsPerCell - Source.GetGrid().GetCellDone(FarCell(40)));
	return true;
}

bool FLotSaveGridReadIsAtomicTest::RunTest(const FString& Parameters)
{
	FLotGame Source;
	AddInProgressCells(Source);
	TArray<uint8> Good;
	Source.GetGrid().Write(Good);
	if (!TestEqual(TEXT("grid size"), Good.Num(), Off::CompleteBitsBytes + 4 + 3 * Off::RecordBytes))
	{
		return false;
	}

	FLotGrid Target;
	FVector2D A, B;
	CellStrip(FarCell(10), A, B);
	Target.Stamp(A, B, 100.0, 3.0);
	const TArray<uint8> TexelsBefore(Target.GetTexels().GetData(), Target.GetTexels().Num());
	TArray<uint8> BytesBefore;
	Target.Write(BytesBefore);

	// The damage is in the last record, after two good ones.
	TArray<uint8> Bad = Good;
	const int32 LastRecord = Off::CompleteBitsBytes + 4 + 2 * Off::RecordBytes;
	PatchI32(Bad, LastRecord, ReadI32(Bad, LastRecord - Off::RecordBytes));
	int32 BytesRead = 77;
	TestFalse(TEXT("a repeated index in the last record"), Target.Read(Bad, BytesRead));
	TestEqual(TEXT("bytes read untouched"), BytesRead, 77);
	TestTrue(TEXT("texels untouched"), SameBytes(TexelsBefore, Target.GetTexels()));
	TArray<uint8> BytesAfter;
	Target.Write(BytesAfter);
	TestTrue(TEXT("counters untouched"), BytesAfter == BytesBefore);
	TestEqual(TEXT("done counter untouched"), Target.GetCellDone(FarCell(10)), 120);

	TArray<FIntRect> Rects;
	Target.ConsumeDirtyRects(Rects);
	TestEqual(TEXT("dirty state untouched (only the sector of the earlier stamp)"), Rects.Num(), 1);

	TArray<uint8> Short(Good.GetData(), Good.Num() - 1);
	TestFalse(TEXT("truncated by a byte"), Target.Read(Short, BytesRead));
	TestFalse(TEXT("empty"), Target.Read(TConstArrayView<uint8>(), BytesRead));
	TestEqual(TEXT("bytes read still untouched"), BytesRead, 77);
	return true;
}

bool FLotSaveDerivedStateTest::RunTest(const FString& Parameters)
{
	// Complete entry segments 0 and 1 and enough cells to open QuickSwap and Sprint.
	FLotGame Source;
	for (int32 Entry = 0; Entry < 2; ++Entry)
	{
		for (int32 I = 0; I < AsphaltLot::CellsPerEntry; ++I)
		{
			Source.DebugCompleteCell(AsphaltLot::EntryCell(Entry, I));
		}
	}
	Source.DebugCompleteCells(100);
	TestTrue(TEXT("source: quick swap open"), Source.IsUnlocked(ELotUnlock::QuickSwap));
	TestTrue(TEXT("source: sprint open"), Source.IsUnlocked(ELotUnlock::Sprint));
	TestFalse(TEXT("source: exit closed"), Source.IsExitOpen());

	FLotGame Loaded;
	TestTrue(TEXT("loads"), Loaded.LoadFromBytes(Source.SaveToBytes()));
	TestEqual(TEXT("completed cells"), Loaded.GetCompletedCells(), Source.GetCompletedCells());
	TestEqual(TEXT("entry 0 progress"), Loaded.GetEntryProgress(0), AsphaltLot::CellsPerEntry);
	TestEqual(TEXT("entry 2 progress"), Loaded.GetEntryProgress(2), Source.GetEntryProgress(2));
	TestTrue(TEXT("quick swap open"), Loaded.IsUnlocked(ELotUnlock::QuickSwap));
	TestTrue(TEXT("sprint open"), Loaded.IsUnlocked(ELotUnlock::Sprint));
	TestFalse(TEXT("exit closed"), Loaded.IsExitOpen());
	TestEqual(TEXT("nearest incomplete cell"), Loaded.FindNearestIncomplete(FVector2D::ZeroVector), Source.FindNearestIncomplete(FVector2D::ZeroVector));

	// Quick swap is open: a swap is instant in the loaded game.
	Loaded.DebugGrantItem(ELotItem::Canister);
	TestTrue(TEXT("scoop"), Loaded.SelectHand(ELotHand::Canister));
	TestFalse(TEXT("no swap time"), Loaded.IsHandBusy());
	return true;
}

bool FLotSaveRejectsCorruptDataTest::RunTest(const FString& Parameters)
{
	const FLotGame Rich = MakeRichGame(false);
	const TArray<uint8> Base = Rich.SaveToBytes();
	if (!TestTrue(TEXT("a rich save has in-progress cells"), Base.Num() >= Off::Records + 3 * Off::RecordBytes))
	{
		return false;
	}
	FLotGame Target = MakeRichGame(true);

	{
		FLotGame Probe;
		TestTrue(TEXT("the base blob is valid"), Probe.LoadFromBytes(Base));
	}

	// Truncations and extra data.
	for (int32 Length : {0, 3, 40, Base.Num() / 2, Base.Num() - 1, Off::Grid, Off::InProgressCount + 2})
	{
		ExpectRejected(*this, Target, TArray<uint8>(Base.GetData(), Length), *FString::Printf(TEXT("truncated to %d"), Length));
	}
	{
		TArray<uint8> Longer = Base;
		Longer.Add(0);
		ExpectRejected(*this, Target, Longer, TEXT("one byte too many"));
	}
	TestFalse(TEXT("from an empty view"), Target.LoadFromBytes(TConstArrayView<uint8>()));

	// Header.
	ExpectPatchRejected(*this, Target, Base, TEXT("bad magic"), [](TArray<uint8>& B) { PatchU8(B, Off::Magic, 'X'); });
	ExpectPatchRejected(*this, Target, Base, TEXT("version 1"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("version 2"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 2); });
	ExpectPatchRejected(*this, Target, Base, TEXT("version 0"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("wrong number of cells"), [](TArray<uint8>& B) { PatchI32(B, Off::NumCells, 29999); });
	ExpectPatchRejected(*this, Target, Base, TEXT("wrong grid size"), [](TArray<uint8>& B) { PatchI32(B, Off::GridSize, 2000); });

	// Numbers.
	ExpectPatchRejected(*this, Target, Base, TEXT("credits NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::Credits, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("credits -1"), [](TArray<uint8>& B) { PatchDouble(B, Off::Credits, -1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("credits infinite"), [](TArray<uint8>& B) { PatchDouble(B, Off::Credits, Infinity); });
	ExpectPatchRejected(*this, Target, Base, TEXT("playtime NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::Playtime, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("playtime -1"), [](TArray<uint8>& B) { PatchDouble(B, Off::Playtime, -1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("total litres NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::TotalLitres, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("total litres -1"), [](TArray<uint8>& B) { PatchDouble(B, Off::TotalLitres, -1.0); });

	// Plant.
	ExpectPatchRejected(*this, Target, Base, TEXT("plant level 6"), [](TArray<uint8>& B) { PatchI32(B, Off::PlantLevel, 6); });
	ExpectPatchRejected(*this, Target, Base, TEXT("plant level -1"), [](TArray<uint8>& B) { PatchI32(B, Off::PlantLevel, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("stock NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::PlantStock, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("stock -1"), [](TArray<uint8>& B) { PatchDouble(B, Off::PlantStock, -1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("stock above the storage"), [&Rich](TArray<uint8>& B) { PatchDouble(B, Off::PlantStock, Rich.GetPlantStorage() + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("level lowered under the stock"), [](TArray<uint8>& B) { PatchI32(B, Off::PlantLevel, 0); });

	// Items.
	ExpectPatchRejected(*this, Target, Base, TEXT("owned flag 2"), [](TArray<uint8>& B) { PatchU8(B, Off::Owned + static_cast<int32>(ELotItem::Bucket), 2); });
	ExpectPatchRejected(*this, Target, Base, TEXT("owned flag 255"), [](TArray<uint8>& B) { PatchU8(B, Off::Owned, 255); });

	// Hand and vessels.
	ExpectPatchRejected(*this, Target, Base, TEXT("hand 4"), [](TArray<uint8>& B) { PatchI32(B, Off::Hand, 4); });
	ExpectPatchRejected(*this, Target, Base, TEXT("hand -1"), [](TArray<uint8>& B) { PatchI32(B, Off::Hand, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("bowl 3 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Bowl, 3.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("bowl -0.5 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Bowl, -0.5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("bowl NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::Bowl, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("canister 20.5 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Canister, 20.5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("tub 36.5 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Tub, 36.5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("bucket 10.5 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Bucket, 10.5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("swap remainder above the swap time"), [](TArray<uint8>& B) { PatchDouble(B, Off::Swap, LotBalance::SwapSeconds + 0.5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("swap remainder negative"), [](TArray<uint8>& B) { PatchDouble(B, Off::Swap, -0.1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("swap remainder NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::Swap, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("suitcase 61 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Suitcase, LotBalance::SuitcaseCapacityL + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("suitcase -1 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Suitcase, -1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("cart 201 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Cart, LotBalance::CartCapacityL + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("cart NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::Cart, NaN); });

	// Held movable.
	ExpectPatchRejected(*this, Target, Base, TEXT("held 5"), [](TArray<uint8>& B) { PatchI32(B, Off::Held, 5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("held -1"), [](TArray<uint8>& B) { PatchI32(B, Off::Held, -1); });

	// Auto paver.
	ExpectPatchRejected(*this, Target, Base, TEXT("sector 75"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSector, 75); });
	ExpectPatchRejected(*this, Target, Base, TEXT("sector -2"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSector, -2); });
	ExpectPatchRejected(*this, Target, Base, TEXT("step 400"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverStep, 400); });
	ExpectPatchRejected(*this, Target, Base, TEXT("step -1"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverStep, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("paver position NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverPos, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("paver position infinite"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverPos + 8, Infinity); });
	ExpectPatchRejected(*this, Target, Base, TEXT("paver phase 5"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverPhase, 5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("paver phase -1"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverPhase, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("paver tank above the tank"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverTank, LotBalance::AutoPaverTankL + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("paver tank negative"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverTank, -1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("car tank above the tank"), [](TArray<uint8>& B) { PatchDouble(B, Off::CarTank, LotBalance::CarTankL[0] + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("car tank NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::CarTank, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("budget above the cap"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverBudget, LotBalance::AutoPaverBudgetCapL + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("budget negative"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverBudget, -1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("budget NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverBudget, NaN); });

	// Won, and the world.
	ExpectPatchRejected(*this, Target, Base, TEXT("won flag 2"), [](TArray<uint8>& B) { PatchU8(B, Off::Won, 2); });
	ExpectPatchRejected(*this, Target, Base, TEXT("won with the exit closed"), [](TArray<uint8>& B) { PatchU8(B, Off::Won, 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("player x NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::PlayerPos, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("player y infinite"), [](TArray<uint8>& B) { PatchDouble(B, Off::PlayerPos + 8, Infinity); });
	ExpectPatchRejected(*this, Target, Base, TEXT("player yaw NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::PlayerYaw, NaN); });
	for (int32 I = 0; I < static_cast<int32>(ELotMovable::Count); ++I)
	{
		ExpectPatchRejected(*this, Target, Base, *FString::Printf(TEXT("movable %d position NaN"), I), [I](TArray<uint8>& B) { PatchDouble(B, Off::Movables + 24 * I, NaN); });
		ExpectPatchRejected(*this, Target, Base, *FString::Printf(TEXT("movable %d yaw infinite"), I), [I](TArray<uint8>& B) { PatchDouble(B, Off::Movables + 24 * I + 16, Infinity); });
	}
	ExpectPatchRejected(*this, Target, Base, TEXT("car position NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::CarPos + 8, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("car yaw NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::CarYaw, NaN); });
	ExpectPatchRejected(*this, Target, Base, TEXT("in-car flag 2"), [](TArray<uint8>& B) { PatchU8(B, Off::InCar, 2); });
	return true;
}

bool FLotSaveRejectsInconsistentTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Fresh = FreshBlob();
	if (!TestEqual(TEXT("fresh save size"), Fresh.Num(), Off::Records))
	{
		return false;
	}
	FLotGame Target = MakeRichGame(true);

	// Things that need an item the fresh game does not own.
	ExpectPatchRejected(*this, Target, Fresh, TEXT("bucket in hand, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Hand, static_cast<int32>(ELotHand::Bucket)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("canister in hand, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Hand, static_cast<int32>(ELotHand::Canister)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("tub in hand, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Hand, static_cast<int32>(ELotHand::Tub)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("canister content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Canister, 0.5); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("tub content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Tub, 0.5); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("paver tank without the paver"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverTank, 5.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("car tank without the car"), [](TArray<uint8>& B) { PatchDouble(B, Off::CarTank, 5.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("bucket content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Bucket, 0.5); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("suitcase content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Suitcase, 1.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("cart content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Cart, 1.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("held cart, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Held, static_cast<int32>(ELotMovable::Cart)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("held station, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Held, static_cast<int32>(ELotMovable::Station)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("in the car, no car"), [](TArray<uint8>& B) { PatchU8(B, Off::InCar, 1); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("sector, no auto paver"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSector, 3); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("step without sector"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverStep, 5); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("a phase without sector"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverPhase, 1); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("budget without sector"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverBudget, 1.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("won on a fresh lot"), [](TArray<uint8>& B) { PatchU8(B, Off::Won, 1); });

	// Item requirements: the faster auto paver without the first one, the wide bar without the car.
	for (ELotItem Wanted : {ELotItem::AutoPaverFast, ELotItem::AutoPaverFaster, ELotItem::CarWideBar})
	{
		ExpectPatchRejected(*this, Target, Fresh, TEXT("item without its requirement"), [Wanted](TArray<uint8>& B) { PatchU8(B, Off::Owned + static_cast<int32>(Wanted), 1); });
	}

	// A held movable and sitting in the car exclude each other.
	{
		const FLotGame Held = MakeRichGame(true);
		const TArray<uint8> HeldBlob = Held.SaveToBytes();
		if (!TestTrue(TEXT("held save size"), HeldBlob.Num() >= Off::Records))
		{
			return false;
		}
		FLotGame Probe;
		TestTrue(TEXT("held base is valid"), Probe.LoadFromBytes(HeldBlob));
		ExpectPatchRejected(*this, Target, HeldBlob, TEXT("holding the cart and sitting in the car"), [](TArray<uint8>& B) { PatchU8(B, Off::InCar, 1); });
	}

	// The swap remainder may be as large as a whole swap.
	{
		TArray<uint8> Blob = Fresh;
		PatchDouble(Blob, Off::Swap, LotBalance::SwapSeconds);
		FLotGame Probe;
		TestTrue(TEXT("a full swap remainder is allowed"), Probe.LoadFromBytes(Blob));
	}
	return true;
}

bool FLotSaveRejectsCorruptGridTest::RunTest(const FString& Parameters)
{
	FLotGame Source;
	Source.DebugCompleteCells(3);
	AddInProgressCells(Source);
	const TArray<uint8> Base = Source.SaveToBytes();
	if (!TestEqual(TEXT("size"), Base.Num(), Off::Records + 3 * Off::RecordBytes))
	{
		return false;
	}
	TestEqual(TEXT("three records"), ReadI32(Base, Off::InProgressCount), 3);
	{
		FLotGame Probe;
		TestTrue(TEXT("the base blob is valid"), Probe.LoadFromBytes(Base));
	}
	FLotGame Target;
	Target.DebugCompleteCells(5);
	AddInProgressCells(Target);

	const int32 Rec0 = Off::Records;
	const int32 Rec1 = Off::Records + Off::RecordBytes;
	const int32 Rec2 = Off::Records + 2 * Off::RecordBytes;
	const int32 Cell0 = ReadI32(Base, Rec0);

	// Count field.
	ExpectPatchRejected(*this, Target, Base, TEXT("count -1"), [](TArray<uint8>& B) { PatchI32(B, Off::InProgressCount, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("count 4 (one record missing)"), [](TArray<uint8>& B) { PatchI32(B, Off::InProgressCount, 4); });
	ExpectPatchRejected(*this, Target, Base, TEXT("count 2 (one record too many)"), [](TArray<uint8>& B) { PatchI32(B, Off::InProgressCount, 2); });
	ExpectPatchRejected(*this, Target, Base, TEXT("count INT_MAX"), [](TArray<uint8>& B) { PatchI32(B, Off::InProgressCount, MAX_int32); });
	ExpectPatchRejected(*this, Target, Base, TEXT("count above the number of cells"), [](TArray<uint8>& B) { PatchI32(B, Off::InProgressCount, AsphaltLot::NumCells + 1); });

	// Indices.
	ExpectPatchRejected(*this, Target, Base, TEXT("index 30000"), [Rec2](TArray<uint8>& B) { PatchI32(B, Rec2, AsphaltLot::NumCells); });
	ExpectPatchRejected(*this, Target, Base, TEXT("index -1"), [Rec0](TArray<uint8>& B) { PatchI32(B, Rec0, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("two equal indices"), [Rec0, Rec1](TArray<uint8>& B) { PatchI32(B, Rec1, ReadI32(B, Rec0)); });
	ExpectPatchRejected(*this, Target, Base, TEXT("descending indices"), [Rec0, Rec1](TArray<uint8>& B) { PatchI32(B, Rec0, ReadI32(B, Rec1) + 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("in-progress cell also marked complete"), [Cell0](TArray<uint8>& B) { B[Off::Grid + Cell0 / 8] |= static_cast<uint8>(1 << (Cell0 % 8)); });

	// Texels: one bit each; any pattern is a texel, but the count must be below the threshold and above nothing.
	ExpectPatchRejected(*this, Target, Base, TEXT("a cell with every texel covered"), [Rec2](TArray<uint8>& B) { FMemory::Memset(&B[Rec2 + 4], 0xFF, 50); });
	ExpectPatchRejected(*this, Target, Base, TEXT("a cell at the 95 % threshold"), [Rec2](TArray<uint8>& B)
	{
		FMemory::Memset(&B[Rec2 + 4], 0x00, 50);
		FMemory::Memset(&B[Rec2 + 4], 0xFF, 47);
		PatchU8(B, Rec2 + 4 + 47, 0x0F); // 47 * 8 + 4 = 380 covered texels
	});
	ExpectPatchRejected(*this, Target, Base, TEXT("a record with only free texels (not canonical)"), [Rec1](TArray<uint8>& B) { FMemory::Memset(&B[Rec1 + 4], 0x00, 50); });

	// Just below the threshold is a valid in-progress cell.
	{
		TArray<uint8> Blob = Base;
		FMemory::Memset(&Blob[Rec2 + 4], 0x00, 50);
		FMemory::Memset(&Blob[Rec2 + 4], 0xFF, 47);
		PatchU8(Blob, Rec2 + 4 + 47, 0x07); // 47 * 8 + 3 = 379
		FLotGame Probe;
		TestTrue(TEXT("379 covered texels are valid"), Probe.LoadFromBytes(Blob));
		TestEqual(TEXT("counted"), Probe.GetGrid().GetCellDone(ReadI32(Blob, Rec2)), 379);
	}

	return true;
}

bool FLotSaveRejectsV1Test::RunTest(const FString& Parameters)
{
	FLotGame G;
	TArray<uint8> Blob = G.SaveToBytes();
	if (!TestEqual(TEXT("fresh save size"), Blob.Num(), Off::Records))
	{
		return false;
	}
	FLotGame Target = MakeRichGame(false);
	PatchI32(Blob, Off::Magic, 0x31414944);
	ExpectRejected(*this, Target, Blob, TEXT("v1 magic on a v2 body"));

	TArray<uint8> OnlyMagic;
	OnlyMagic.SetNum(4);
	PatchI32(OnlyMagic, 0, 0x31414944);
	ExpectRejected(*this, Target, OnlyMagic, TEXT("v1 magic alone"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
