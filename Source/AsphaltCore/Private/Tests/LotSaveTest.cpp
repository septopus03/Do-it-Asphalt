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
DIA_TEST(FLotSavePaverAutoTest, "DoItAsphalt.Lot.Save.PaverAuto")
DIA_TEST(FLotSaveGridRoundTripTest, "DoItAsphalt.Lot.Save.GridRoundTrip")
DIA_TEST(FLotSaveGridReadIsAtomicTest, "DoItAsphalt.Lot.Save.GridReadIsAtomic")
DIA_TEST(FLotSaveDerivedStateTest, "DoItAsphalt.Lot.Save.DerivedState")
DIA_TEST(FLotSaveRejectsCorruptDataTest, "DoItAsphalt.Lot.Save.RejectsCorruptData")
DIA_TEST(FLotSaveRejectsInconsistentTest, "DoItAsphalt.Lot.Save.RejectsInconsistent")
DIA_TEST(FLotSaveRejectsCorruptGridTest, "DoItAsphalt.Lot.Save.RejectsCorruptGrid")
DIA_TEST(FLotSaveRejectsV1Test, "DoItAsphalt.Lot.Save.RejectsV1")
DIA_TEST(FLotSaveCarTiersTest, "DoItAsphalt.Lot.Save.CarTiers")
DIA_TEST(FLotSaveLoadsVersion4Test, "DoItAsphalt.Lot.Save.LoadsVersion4")
DIA_TEST(FLotSaveThreePaversRoundTripTest, "DoItAsphalt.Lot.Save.ThreePaversRoundTrip")
DIA_TEST(FLotSaveLoadsOlderPaverTest, "DoItAsphalt.Lot.Save.LoadsOlderPaver")
DIA_TEST(FLotSaveRejectsSharedSectorTest, "DoItAsphalt.Lot.Save.RejectsSharedSector")
DIA_TEST(FLotSavePaverUpgradesRoundTripTest, "DoItAsphalt.Lot.Save.PaverUpgradesRoundTrip")
DIA_TEST(FLotSaveRejectsPaverUpgradesTest, "DoItAsphalt.Lot.Save.RejectsPaverUpgrades")
DIA_TEST(FLotSaveLoadsVersion6Test, "DoItAsphalt.Lot.Save.LoadsVersion6")

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
		constexpr int32 Owned = 52; // 13 bytes in ELotItem order
		constexpr int32 Hand = 65;
		constexpr int32 Bowl = 69;
		constexpr int32 Bucket = 77;
		constexpr int32 Canister = 85;
		constexpr int32 Tub = 93;
		constexpr int32 Swap = 101;
		constexpr int32 Trolley = 109;
		constexpr int32 Cart = 117;
		constexpr int32 Held = 125;
		/** The block of robot 0: speed level, capacity level, sector, step, position, phase, budget, tank; robots 1 and 2 follow it back to back. */
		constexpr int32 PaverSpeed = 129;
		constexpr int32 PaverCapacity = 133;
		constexpr int32 PaverSector = 137;
		constexpr int32 PaverStep = 141;
		constexpr int32 PaverPos = 145;
		constexpr int32 PaverPhase = 161;
		constexpr int32 PaverBudget = 165;
		constexpr int32 PaverTank = 173;
		constexpr int32 PaverBlockBytes = 52;
		constexpr int32 Paver1 = PaverSector + PaverBlockBytes; // the block of robot 1 has its sector here
		constexpr int32 Paver2 = Paver1 + PaverBlockBytes;
		constexpr int32 CarTank = 285;
		constexpr int32 Won = 293;
		constexpr int32 PlayerPos = 294;
		constexpr int32 PlayerYaw = 310;
		constexpr int32 Movables = 318; // 3 x (position 16 bytes, yaw 8 bytes)
		constexpr int32 CarPos = 390;
		constexpr int32 CarYaw = 406;
		constexpr int32 InCar = 414;
		constexpr int32 Grid = 416;
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
		for (ELotItem Item : {ELotItem::Canister, ELotItem::Bucket, ELotItem::Tub, ELotItem::Trolley, ELotItem::Cart, ELotItem::Station,
			ELotItem::AutoPaver, ELotItem::Car})
		{
			G.DebugGrantItem(Item);
		}
		G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 1);
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
		G.FillMovable(ELotMovable::Trolley, 2.0);
		G.FillMovable(ELotMovable::Cart, 6.0);

		// The auto paver works on sector 7 from step 123: everything before is already complete.
		for (int32 Step = 0; Step < 123; ++Step)
		{
			G.DebugCompleteCell(AsphaltLot::SectorCell(7, Step));
		}
		G.AssignAutoPaverSector(0, 7);
		for (int32 I = 0; I < 400 && G.GetAutoPaverStep(0) < 123; ++I)
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
		TestEqual(TEXT("sector assigned"), Source.GetAutoPaverSector(0), 7);
		TestTrue(TEXT("step reached"), Source.GetAutoPaverStep(0) >= 123);
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
		TestEqual(TEXT("trolley"), Loaded.GetMovableLitres(ELotMovable::Trolley), Source.GetMovableLitres(ELotMovable::Trolley));
		TestEqual(TEXT("cart"), Loaded.GetMovableLitres(ELotMovable::Cart), Source.GetMovableLitres(ELotMovable::Cart));
		TestTrue(TEXT("cart filled"), Loaded.GetMovableLitres(ELotMovable::Cart) > 0.0);
		TestEqual(TEXT("held"), Loaded.GetHeld(), Source.GetHeld());
		TestEqual(TEXT("sector"), Loaded.GetAutoPaverSector(0), Source.GetAutoPaverSector(0));
		TestEqual(TEXT("step"), Loaded.GetAutoPaverStep(0), Source.GetAutoPaverStep(0));
		TestEqual(TEXT("paver position"), Loaded.GetAutoPaverPos(0), Source.GetAutoPaverPos(0));
		TestEqual(TEXT("travelling"), Loaded.IsAutoPaverTravelling(0), Source.IsAutoPaverTravelling(0));
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

bool FLotSavePaverAutoTest::RunTest(const FString& Parameters)
{
	// The auto switch of the paver is part of a save, on and off.
	for (const bool bAuto : {true, false})
	{
		FLotGame Source;
		Source.DebugGrantItem(ELotItem::AutoPaver);
		Source.SetAutoPaverAuto(bAuto);
		FLotGame Loaded;
		TestTrue(TEXT("loads"), Loaded.LoadFromBytes(Source.SaveToBytes()));
		TestEqual(TEXT("the switch is kept"), Loaded.IsAutoPaverAuto(), bAuto);
	}
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
	ExpectPatchRejected(*this, Target, Base, TEXT("version 3"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 3); });
	ExpectPatchRejected(*this, Target, Base, TEXT("version 8"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 8); });
	// A current body (13 flags, three robot blocks of the new size) under an older number is of the wrong length.
	ExpectPatchRejected(*this, Target, Base, TEXT("version 6 with a version 7 body"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 6); });
	ExpectPatchRejected(*this, Target, Base, TEXT("version 5 with a version 7 body"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("version 4 with a version 7 body"), [](TArray<uint8>& B) { PatchI32(B, Off::Version, 4); });
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
	ExpectPatchRejected(*this, Target, Base, TEXT("trolley 61 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Trolley, LotBalance::TrolleyCapacityL + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("trolley -1 l"), [](TArray<uint8>& B) { PatchDouble(B, Off::Trolley, -1.0); });
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
	ExpectPatchRejected(*this, Target, Base, TEXT("paver tank above the tank"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverTank, LotBalance::AutoPaverTankLByLevel[0] + 1.0); });
	{
		TArray<uint8> Roomy = Base; // a bigger tank holds more, a smaller one does not
		PatchI32(Roomy, Off::PaverCapacity, 1);
		PatchDouble(Roomy, Off::PaverTank, LotBalance::AutoPaverTankLByLevel[1]);
		FLotGame Probe;
		TestTrue(TEXT("a tank filled to its capacity level is valid"), Probe.LoadFromBytes(Roomy));
		TestEqual(TEXT("the capacity level is read"), Probe.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Capacity), 1);
		ExpectPatchRejected(*this, Target, Roomy, TEXT("the capacity level lowered under the tank"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverCapacity, 0); });
		ExpectPatchRejected(*this, Target, Roomy, TEXT("a tank above the capacity of its level"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverTank, LotBalance::AutoPaverTankLByLevel[1] + 1.0); });
	}
	ExpectPatchRejected(*this, Target, Base, TEXT("speed level 3"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSpeed, MaxPaverUpgradeLevel + 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("speed level -1"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSpeed, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("capacity level 3"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverCapacity, MaxPaverUpgradeLevel + 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("capacity level -1"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverCapacity, -1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("speed level of robot 2 above the maximum"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver2 - 8, MaxPaverUpgradeLevel + 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("speed level on robot 1 that is not bought"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver1 - 8, 1); });
	ExpectPatchRejected(*this, Target, Base, TEXT("capacity level on robot 2 that is not bought"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver2 - 4, 1); });
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
	ExpectPatchRejected(*this, Target, Fresh, TEXT("trolley content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Trolley, 1.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("cart content, not owned"), [](TArray<uint8>& B) { PatchDouble(B, Off::Cart, 1.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("held cart, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Held, static_cast<int32>(ELotMovable::Cart)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("held station, not owned"), [](TArray<uint8>& B) { PatchI32(B, Off::Held, static_cast<int32>(ELotMovable::Station)); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("in the car, no car"), [](TArray<uint8>& B) { PatchU8(B, Off::InCar, 1); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("sector, no auto paver"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSector, 3); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("step without sector"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverStep, 5); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("a phase without sector"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverPhase, 1); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("budget without sector"), [](TArray<uint8>& B) { PatchDouble(B, Off::PaverBudget, 1.0); });
	ExpectPatchRejected(*this, Target, Fresh, TEXT("won on a fresh lot"), [](TArray<uint8>& B) { PatchU8(B, Off::Won, 1); });

	// Item requirements: robots 2 and 3 without the one before, each car tier without the one before (CarT3 and CarT4 with the car alone).
	for (ELotItem Wanted : {ELotItem::AutoPaver2, ELotItem::AutoPaver3, ELotItem::CarT2, ELotItem::CarT3, ELotItem::CarT4})
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

namespace
{
	/**
	 * Byte offsets of the version 6, 5 and 4 saves, written down as they were when those versions were current. They do not
	 * follow Off, so the helpers below produce the old formats from facts that stay put even if the current format changes.
	 */
	namespace OldOff
	{
		/** Version 6: 15 item flags (AutoPaverFast and AutoPaverFaster at 7 and 8), robot blocks without the upgrade levels. */
		constexpr int32 V6Flags = 15;
		constexpr int32 V6Fast = 7;
		constexpr int32 V6Faster = 8;
		constexpr int32 V6PaverSector = 131;
		constexpr int32 V6PaverBlockBytes = 44;
		constexpr int32 V6Paver1 = V6PaverSector + V6PaverBlockBytes;
		constexpr int32 V5PaverSector = 129;
		constexpr int32 V5PaverStep = 133;
		constexpr int32 V5PaverPos = 137;
		constexpr int32 V5PaverPhase = 153;
		constexpr int32 V5PaverBudget = 157;
		constexpr int32 V5PaverTank = 165;
		constexpr int32 V5CarTank = 173;
		constexpr int32 V5FreshSize = 4058; // header, 13 flags, one robot, an empty grid
		constexpr int32 V5CarT3 = 11; // the flags of CarT3 and CarT4 (version 5 only), which version 4 did not have
		constexpr int32 V4CarT2 = 10;
	}

	/**
	 * The blob a version 6 game would have written: the same state, 15 item flags (the global speed items AutoPaverFast and AutoPaverFaster between
	 * AutoPaver and Car, set from the speed level of robot 0), robot blocks without the upgrade levels, and the old version number.
	 */
	TArray<uint8> ToVersion6(const TArray<uint8>& Current)
	{
		TArray<uint8> V6 = Current;
		const int32 Speed = ReadI32(V6, Off::PaverSpeed);
		for (int32 Paver = MaxAutoPavers - 1; Paver >= 0; --Paver)
		{
			V6.RemoveAt(Off::PaverSpeed + Paver * Off::PaverBlockBytes, 8); // before the flags grow, so that the offsets are the current ones
		}
		const uint8 SpeedFlags[2] = {static_cast<uint8>(Speed >= 1), static_cast<uint8>(Speed >= 2)};
		V6.Insert(SpeedFlags, 2, Off::Owned + OldOff::V6Fast);
		PatchI32(V6, Off::Version, 6);
		return V6;
	}

	/** The blob a version 5 game would have written: the version 6 state, 13 item flags and one robot block (robot 0), and the old version number. */
	TArray<uint8> ToVersion5(const TArray<uint8>& Current)
	{
		TArray<uint8> V5 = ToVersion6(Current);
		V5.RemoveAt(OldOff::V6Paver1, 2 * OldOff::V6PaverBlockBytes);
		V5.RemoveAt(Off::Owned + OldOff::V6Flags - 2, 2); // AutoPaver2 and AutoPaver3
		PatchI32(V5, Off::Version, 5);
		return V5;
	}

	/** The blob a version 4 game would have written: the version 5 state, 11 item flags (CarT3 and CarT4 did not exist) and the old version number. */
	TArray<uint8> ToVersion4(const TArray<uint8>& V5)
	{
		TArray<uint8> V4 = V5;
		V4.RemoveAt(Off::Owned + OldOff::V5CarT3, 2);
		PatchI32(V4, Off::Version, 4);
		return V4;
	}
}

bool FLotSaveCarTiersTest::RunTest(const FString& Parameters)
{
	for (int32 Level = 0; Level < LotBalance::NumCarLevels; ++Level)
	{
		FLotGame Source;
		for (int32 Tier = 0; Tier <= Level; ++Tier)
		{
			Source.DebugGrantItem(static_cast<ELotItem>(static_cast<int32>(ELotItem::Car) + Tier));
		}
		Source.DebugFillTanks();
		Source.World().CarPos = FVector2D(-500.0 * Level, 700.0);
		const TArray<uint8> Bytes = Source.SaveToBytes();
		if (!TestEqual(*FString::Printf(TEXT("tier %d: size"), Level + 1), Bytes.Num(), Off::Records))
		{
			return false;
		}
		TestEqual(*FString::Printf(TEXT("tier %d: current version"), Level + 1), ReadI32(Bytes, Off::Version), FLotGame::SaveVersion);
		FLotGame Loaded;
		if (!TestTrue(*FString::Printf(TEXT("tier %d: loads"), Level + 1), Loaded.LoadFromBytes(Bytes)))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("tier %d: level"), Level + 1), Loaded.GetCarLevel(), Level);
		TestEqual(*FString::Printf(TEXT("tier %d: tank"), Level + 1), Loaded.GetCarTank(), LotBalance::CarTankL[Level]);
		TestEqual(*FString::Printf(TEXT("tier %d: tank size"), Level + 1), Loaded.GetCarTankCapacity(), LotBalance::CarTankL[Level]);
		TestTrue(*FString::Printf(TEXT("tier %d: byte-identical"), Level + 1), Loaded.SaveToBytes() == Bytes);

		// The tank of a higher tier does not fit a lower one.
		if (Level > 0)
		{
			TArray<uint8> Lower = Bytes;
			PatchU8(Lower, Off::Owned + static_cast<int32>(ELotItem::Car) + Level, 0);
			FLotGame Probe;
			TestFalse(*FString::Printf(TEXT("tier %d: tank above the lower tank"), Level + 1), Probe.LoadFromBytes(Lower));
		}
	}
	return true;
}

bool FLotSaveLoadsVersion4Test::RunTest(const FString& Parameters)
{
	// A version 4 game with the plain car, and one with the wide bar (which is CarT2 now).
	for (bool bWideBar : {false, true})
	{
		FLotGame Source = MakeRichGame(false);
		if (bWideBar)
		{
			Source.DebugGrantItem(ELotItem::CarT2);
		}
		Source.DebugFillTanks();
		const TArray<uint8> Current = Source.SaveToBytes();
		const TArray<uint8> V5 = ToVersion5(Current);
		const TArray<uint8> V4 = ToVersion4(V5);
		if (!TestEqual(TEXT("a version 4 save is two bytes shorter than version 5"), V4.Num(), V5.Num() - 2))
		{
			return false;
		}
		FLotGame Loaded;
		if (!TestTrue(bWideBar ? TEXT("version 4 with the wide bar loads") : TEXT("version 4 loads"), Loaded.LoadFromBytes(V4)))
		{
			continue;
		}
		TestEqual(TEXT("car level"), Loaded.GetCarLevel(), bWideBar ? 1 : 0);
		TestTrue(TEXT("the old wide bar is CarT2"), Loaded.Owns(ELotItem::CarT2) == bWideBar);
		TestFalse(TEXT("CarT3 is not owned"), Loaded.Owns(ELotItem::CarT3));
		TestFalse(TEXT("CarT4 is not owned"), Loaded.Owns(ELotItem::CarT4));
		TestEqual(TEXT("car tank"), Loaded.GetCarTank(), LotBalance::CarTankL[bWideBar ? 1 : 0]);
		TestEqual(TEXT("credits"), Loaded.GetCredits(), Source.GetCredits());
		TestEqual(TEXT("car position"), Loaded.World().CarPos, Source.World().CarPos);
		TestEqual(TEXT("car yaw"), Loaded.World().CarYawDeg, Source.World().CarYawDeg);
		TestEqual(TEXT("the one robot"), Loaded.GetNumAutoPavers(), 1);
		TestEqual(TEXT("its sector"), Loaded.GetAutoPaverSector(0), Source.GetAutoPaverSector(0));
		TestEqual(TEXT("its tank"), Loaded.GetAutoPaverTank(0), Source.GetAutoPaverTank(0));
		TestTrue(TEXT("the grid is the same"), SameBytes(Loaded.GetGrid().GetTexels(), Source.GetGrid().GetTexels()));
		// Saving it again writes the current version, byte for byte what the current game wrote.
		TestTrue(TEXT("saved again it is the current save"), Loaded.SaveToBytes() == Current);
	}

	// The validation of the old format stays.
	{
		FLotGame Source;
		TArray<uint8> V4 = ToVersion4(ToVersion5(Source.SaveToBytes()));
		FLotGame Target;
		ExpectPatchRejected(*this, Target, V4, TEXT("v4: the wide bar without the car"), [](TArray<uint8>& B) { PatchU8(B, Off::Owned + OldOff::V4CarT2, 1); });
		ExpectPatchRejected(*this, Target, V4, TEXT("v4: car tank without the car"), [](TArray<uint8>& B) { PatchDouble(B, OldOff::V5CarTank - 2, 5.0); });
		ExpectPatchRejected(*this, Target, TArray<uint8>(V4.GetData(), V4.Num() - 1), TEXT("v4: truncated"), [](TArray<uint8>&) {});
	}
	return true;
}

namespace
{
	/** Three robots that have been bought and sent out: robot 0 is working, robot 1 drives to a station, robot 2 waits. */
	FLotGame MakeThreePavers()
	{
		FLotGame G;
		G.DebugGrantItem(ELotItem::AutoPaver);
		G.SetAutoPaverAuto(false);
		G.DebugFillTanks();
		G.AssignAutoPaverSector(0, 7);
		G.DebugGrantItem(ELotItem::AutoPaver2);
		G.AssignAutoPaverSector(1, 20); // an empty tank: it goes to a station first
		G.DebugGrantItem(ELotItem::AutoPaver3);
		// Every robot has its own levels.
		G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 2);
		G.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 1);
		G.DebugSetPaverUpgrade(1, ELotPaverUpgrade::Capacity, 2);
		G.DebugSetPaverUpgrade(2, ELotPaverUpgrade::Speed, 1);
		G.DebugSetPlantLevel(5);
		G.DebugSetPlantStock(G.GetPlantStorage());
		for (int32 I = 0; I < 1000 && G.GetAutoPaverStep(0) < 15; ++I)
		{
			G.Tick(0.25);
		}
		return G;
	}
}

bool FLotSaveThreePaversRoundTripTest::RunTest(const FString& Parameters)
{
	const FLotGame Source = MakeThreePavers();
	TestEqual(TEXT("three robots"), Source.GetNumAutoPavers(), 3);
	TestEqual(TEXT("robot 0 sector"), Source.GetAutoPaverSector(0), 7);
	TestEqual(TEXT("robot 1 sector"), Source.GetAutoPaverSector(1), 20);
	TestEqual(TEXT("robot 2 waits"), Source.GetAutoPaverSector(2), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("robot 0 has worked"), Source.GetAutoPaverStep(0) >= 15);
	TestTrue(TEXT("the phases differ"), Source.GetAutoPaverPhase(0) != Source.GetAutoPaverPhase(2) && Source.GetAutoPaverPhase(1) != Source.GetAutoPaverPhase(2));

	const TArray<uint8> Bytes = Source.SaveToBytes();
	FLotGame Loaded;
	if (!TestTrue(TEXT("loads"), Loaded.LoadFromBytes(Bytes)))
	{
		return false;
	}
	TestTrue(TEXT("save -> load -> save is byte-identical"), Loaded.SaveToBytes() == Bytes);
	TestEqual(TEXT("three robots after loading"), Loaded.GetNumAutoPavers(), 3);
	for (int32 P = 0; P < MaxAutoPavers; ++P)
	{
		TestEqual(*FString::Printf(TEXT("robot %d sector"), P), Loaded.GetAutoPaverSector(P), Source.GetAutoPaverSector(P));
		TestEqual(*FString::Printf(TEXT("robot %d step"), P), Loaded.GetAutoPaverStep(P), Source.GetAutoPaverStep(P));
		TestEqual(*FString::Printf(TEXT("robot %d position"), P), Loaded.GetAutoPaverPos(P), Source.GetAutoPaverPos(P));
		TestTrue(*FString::Printf(TEXT("robot %d phase"), P), Loaded.GetAutoPaverPhase(P) == Source.GetAutoPaverPhase(P));
		TestEqual(*FString::Printf(TEXT("robot %d tank"), P), Loaded.GetAutoPaverTank(P), Source.GetAutoPaverTank(P));
		TestEqual(*FString::Printf(TEXT("robot %d travelling"), P), Loaded.IsAutoPaverTravelling(P), Source.IsAutoPaverTravelling(P));
		for (const ELotPaverUpgrade Kind : {ELotPaverUpgrade::Speed, ELotPaverUpgrade::Capacity})
		{
			TestEqual(*FString::Printf(TEXT("robot %d upgrade %d"), P, static_cast<int32>(Kind)), Loaded.GetPaverUpgradeLevel(P, Kind), Source.GetPaverUpgradeLevel(P, Kind));
		}
		TestEqual(*FString::Printf(TEXT("robot %d rate"), P), Loaded.GetAutoPaverRateCellsPerMin(P), Source.GetAutoPaverRateCellsPerMin(P));
		TestEqual(*FString::Printf(TEXT("robot %d tank size"), P), Loaded.GetAutoPaverTankCapacity(P), Source.GetAutoPaverTankCapacity(P));
	}
	TestEqual(TEXT("the automatic flag"), Loaded.IsAutoPaverAuto(), Source.IsAutoPaverAuto());
	return true;
}

bool FLotSaveLoadsOlderPaverTest::RunTest(const FString& Parameters)
{
	// A fresh version 5 save has the size the version 5 game wrote: the helper builds the real old format.
	{
		FLotGame Fresh;
		const TArray<uint8> V5 = ToVersion5(Fresh.SaveToBytes());
		TestEqual(TEXT("a fresh version 5 save has the old size"), V5.Num(), OldOff::V5FreshSize);
		TestEqual(TEXT("it says version 5"), ReadI32(V5, Off::Version), 5);
		FLotGame Loaded;
		TestTrue(TEXT("it loads"), Loaded.LoadFromBytes(V5));
		TestEqual(TEXT("no robot"), Loaded.GetNumAutoPavers(), 0);
		TestTrue(TEXT("saved again it is the current save"), Loaded.SaveToBytes() == Fresh.SaveToBytes());
	}

	// The robot of a version 5 save is robot 0, whatever it was doing.
	for (const ELotPaverPhase Phase : {ELotPaverPhase::Working, ELotPaverPhase::Loading})
	{
		const TCHAR* const Name = Phase == ELotPaverPhase::Working ? TEXT("working") : TEXT("loading");
		FLotGame Source;
		Source.DebugGrantItem(ELotItem::AutoPaver);
		Source.SetAutoPaverAuto(false);
		Source.AssignAutoPaverSector(0, 12);
		TArray<uint8> Current = Source.SaveToBytes();
		const FVector2D Where(1234.5, -6789.25);
		PatchI32(Current, Off::PaverStep, 40);
		PatchDouble(Current, Off::PaverPos, Where.X);
		PatchDouble(Current, Off::PaverPos + 8, Where.Y);
		PatchI32(Current, Off::PaverPhase, static_cast<int32>(Phase));
		PatchDouble(Current, Off::PaverTank, 900.0);

		const TArray<uint8> V5 = ToVersion5(Current);
		// The robot is where a version 5 game kept it.
		TestEqual(*FString::Printf(TEXT("%s: v5 sector"), Name), ReadI32(V5, OldOff::V5PaverSector), 12);
		TestEqual(*FString::Printf(TEXT("%s: v5 step"), Name), ReadI32(V5, OldOff::V5PaverStep), 40);
		TestEqual(*FString::Printf(TEXT("%s: v5 phase"), Name), ReadI32(V5, OldOff::V5PaverPhase), static_cast<int32>(Phase));
		TestEqual(*FString::Printf(TEXT("%s: v5 size"), Name), V5.Num(), Current.Num() - (MaxAutoPavers * Off::PaverBlockBytes - OldOff::V6PaverBlockBytes));

		FLotGame Loaded;
		if (!TestTrue(*FString::Printf(TEXT("%s: version 5 loads"), Name), Loaded.LoadFromBytes(V5)))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s: one robot"), Name), Loaded.GetNumAutoPavers(), 1);
		TestEqual(*FString::Printf(TEXT("%s: sector"), Name), Loaded.GetAutoPaverSector(0), 12);
		TestEqual(*FString::Printf(TEXT("%s: step"), Name), Loaded.GetAutoPaverStep(0), 40);
		TestEqual(*FString::Printf(TEXT("%s: position"), Name), Loaded.GetAutoPaverPos(0), Where);
		TestTrue(*FString::Printf(TEXT("%s: phase"), Name), Loaded.GetAutoPaverPhase(0) == Phase);
		TestEqual(*FString::Printf(TEXT("%s: tank"), Name), Loaded.GetAutoPaverTank(0), 900.0);
		TestEqual(*FString::Printf(TEXT("%s: robot 1 is empty"), Name), Loaded.GetAutoPaverSector(1), static_cast<int32>(INDEX_NONE));
		TestEqual(*FString::Printf(TEXT("%s: robot 2 has no tank"), Name), Loaded.GetAutoPaverTank(2), 0.0);
		TestTrue(*FString::Printf(TEXT("%s: saved again it is the current save"), Name), Loaded.SaveToBytes() == Current);

		// The same robot out of a version 4 save.
		FLotGame FromV4;
		TestTrue(*FString::Printf(TEXT("%s: version 4 loads"), Name), FromV4.LoadFromBytes(ToVersion4(V5)));
		TestEqual(*FString::Printf(TEXT("%s: v4 sector"), Name), FromV4.GetAutoPaverSector(0), 12);
		TestEqual(*FString::Printf(TEXT("%s: v4 tank"), Name), FromV4.GetAutoPaverTank(0), 900.0);
		TestEqual(*FString::Printf(TEXT("%s: v4 one robot"), Name), FromV4.GetNumAutoPavers(), 1);
	}

	// The checks of a robot apply to a version 5 save too.
	{
		FLotGame Source;
		Source.DebugGrantItem(ELotItem::AutoPaver);
		Source.SetAutoPaverAuto(false);
		const TArray<uint8> V5 = ToVersion5(Source.SaveToBytes());
		FLotGame Target = MakeThreePavers();
		ExpectPatchRejected(*this, Target, V5, TEXT("v5: sector 75"), [](TArray<uint8>& B) { PatchI32(B, OldOff::V5PaverSector, 75); });
		ExpectPatchRejected(*this, Target, V5, TEXT("v5: a phase without a sector"), [](TArray<uint8>& B) { PatchI32(B, OldOff::V5PaverPhase, 1); });
		ExpectPatchRejected(*this, Target, V5, TEXT("v5: tank above the tank"), [](TArray<uint8>& B) { PatchDouble(B, OldOff::V5PaverTank, LotBalance::AutoPaverTankLByLevel[0] + 1.0); });
		ExpectPatchRejected(*this, Target, V5, TEXT("v5: budget without a sector"), [](TArray<uint8>& B) { PatchDouble(B, OldOff::V5PaverBudget, 1.0); });
		ExpectPatchRejected(*this, Target, V5, TEXT("v5: position NaN"), [](TArray<uint8>& B) { PatchDouble(B, OldOff::V5PaverPos, NaN); });
		ExpectPatchRejected(*this, Target, TArray<uint8>(V5.GetData(), V5.Num() - 1), TEXT("v5: truncated"), [](TArray<uint8>&) {});
	}
	return true;
}

bool FLotSaveRejectsSharedSectorTest::RunTest(const FString& Parameters)
{
	const FLotGame Source = MakeThreePavers();
	const TArray<uint8> Base = Source.SaveToBytes();
	{
		FLotGame Probe;
		if (!TestTrue(TEXT("the base blob is valid"), Probe.LoadFromBytes(Base)))
		{
			return false;
		}
	}
	FLotGame Target = MakeRichGame(true);

	// Two robots on one sector, in every pair of blocks.
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 1 on the sector of robot 0"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver1, ReadI32(B, Off::PaverSector)); });
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 0 on the sector of robot 1"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSector, ReadI32(B, Off::Paver1)); });
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 2 on the sector of robot 1 (working)"), [](TArray<uint8>& B)
	{
		PatchI32(B, Off::Paver2, ReadI32(B, Off::Paver1));
		PatchI32(B, Off::Paver2 + 24, 1); // a sector needs a phase
	});

	// State on a robot that is not bought.
	const TArray<uint8> OneRobot = MakeRichGame(false).SaveToBytes();
	{
		FLotGame Probe;
		TestTrue(TEXT("a one-robot blob is valid"), Probe.LoadFromBytes(OneRobot));
	}
	ExpectPatchRejected(*this, Target, OneRobot, TEXT("a sector on robot 1 that is not bought"), [](TArray<uint8>& B)
	{
		PatchI32(B, Off::Paver1, 3);
		PatchI32(B, Off::Paver1 + 24, 1);
	});
	ExpectPatchRejected(*this, Target, OneRobot, TEXT("a tank on robot 1 that is not bought"), [](TArray<uint8>& B) { PatchDouble(B, Off::Paver1 + 36, 5.0); });
	ExpectPatchRejected(*this, Target, OneRobot, TEXT("a tank on robot 2 that is not bought"), [](TArray<uint8>& B) { PatchDouble(B, Off::Paver2 + 36, 5.0); });
	ExpectPatchRejected(*this, Target, OneRobot, TEXT("a budget on robot 2 that is not bought"), [](TArray<uint8>& B) { PatchDouble(B, Off::Paver2 + 28, 1.0); });

	// The checks hold for every block.
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 1 sector 75"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver1, 75); });
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 2 step 400"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver2 + 4, 400); });
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 1 phase 5"), [](TArray<uint8>& B) { PatchI32(B, Off::Paver1 + 24, 5); });
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 2 tank above the tank"), [](TArray<uint8>& B) { PatchDouble(B, Off::Paver2 + 36, LotBalance::AutoPaverTankLByLevel[0] + 1.0); });
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 1 position NaN"), [](TArray<uint8>& B) { PatchDouble(B, Off::Paver1 + 8, NaN); });
	return true;
}

bool FLotSavePaverUpgradesRoundTripTest::RunTest(const FString& Parameters)
{
	// Different levels on every robot survive a save, byte for byte, and each robot keeps its own rate and tank.
	FLotGame Source = MakeThreePavers();
	const int32 Speed[3] = {2, 0, 1};
	const int32 Capacity[3] = {1, 2, 0};
	Source.DebugSetPaverUpgrade(2, ELotPaverUpgrade::Capacity, 0);
	const TArray<uint8> Bytes = Source.SaveToBytes();
	TestEqual(TEXT("the speed level of robot 0 is where the format says"), ReadI32(Bytes, Off::PaverSpeed), 2);
	TestEqual(TEXT("the capacity level of robot 0"), ReadI32(Bytes, Off::PaverCapacity), 1);
	TestEqual(TEXT("the speed level of robot 1"), ReadI32(Bytes, Off::Paver1 - 8), 0);
	TestEqual(TEXT("the capacity level of robot 1"), ReadI32(Bytes, Off::Paver1 - 4), 2);
	TestEqual(TEXT("the speed level of robot 2"), ReadI32(Bytes, Off::Paver2 - 8), 1);
	TestEqual(TEXT("the capacity level of robot 2"), ReadI32(Bytes, Off::Paver2 - 4), 0);
	FLotGame Loaded;
	if (!TestTrue(TEXT("loads"), Loaded.LoadFromBytes(Bytes)))
	{
		return false;
	}
	TestTrue(TEXT("byte-identical"), Loaded.SaveToBytes() == Bytes);
	for (int32 P = 0; P < MaxAutoPavers; ++P)
	{
		TestEqual(*FString::Printf(TEXT("robot %d speed"), P), Loaded.GetPaverUpgradeLevel(P, ELotPaverUpgrade::Speed), Speed[P]);
		TestEqual(*FString::Printf(TEXT("robot %d capacity"), P), Loaded.GetPaverUpgradeLevel(P, ELotPaverUpgrade::Capacity), Capacity[P]);
		TestEqual(*FString::Printf(TEXT("robot %d rate"), P), Loaded.GetAutoPaverRateCellsPerMin(P), LotBalance::AutoPaverRateCellsPerMin[Speed[P]]);
		TestEqual(*FString::Printf(TEXT("robot %d tank size"), P), Loaded.GetAutoPaverTankCapacity(P), LotBalance::AutoPaverTankLByLevel[Capacity[P]]);
	}

	// A tank filled to a big capacity is valid and comes back full.
	FLotGame Big;
	Big.DebugGrantItem(ELotItem::AutoPaver);
	Big.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Capacity, 2);
	Big.DebugFillTanks();
	FLotGame BigLoaded;
	TestTrue(TEXT("a full big tank loads"), BigLoaded.LoadFromBytes(Big.SaveToBytes()));
	TestEqual(TEXT("and is still full"), BigLoaded.GetAutoPaverTank(0), LotBalance::AutoPaverTankLByLevel[2]);
	return true;
}

bool FLotSaveRejectsPaverUpgradesTest::RunTest(const FString& Parameters)
{
	const FLotGame Source = MakeThreePavers();
	const TArray<uint8> Base = Source.SaveToBytes();
	{
		FLotGame Probe;
		if (!TestTrue(TEXT("the base blob is valid"), Probe.LoadFromBytes(Base)))
		{
			return false;
		}
	}
	FLotGame Target = MakeRichGame(true);

	// A level above the maximum or below 0, in every block and for both kinds.
	for (int32 P = 0; P < MaxAutoPavers; ++P)
	{
		const int32 SpeedAt = Off::PaverSpeed + P * Off::PaverBlockBytes;
		for (const int32 Bad : {MaxPaverUpgradeLevel + 1, -1, MAX_int32})
		{
			ExpectPatchRejected(*this, Target, Base, *FString::Printf(TEXT("robot %d speed level %d"), P, Bad), [SpeedAt, Bad](TArray<uint8>& B) { PatchI32(B, SpeedAt, Bad); });
			ExpectPatchRejected(*this, Target, Base, *FString::Printf(TEXT("robot %d capacity level %d"), P, Bad), [SpeedAt, Bad](TArray<uint8>& B) { PatchI32(B, SpeedAt + 4, Bad); });
		}
	}

	// A level on a robot that is not owned.
	FLotGame One;
	One.DebugGrantItem(ELotItem::AutoPaver);
	const TArray<uint8> OneRobot = One.SaveToBytes();
	{
		FLotGame Probe;
		TestTrue(TEXT("a one-robot blob is valid"), Probe.LoadFromBytes(OneRobot));
	}
	for (int32 P = 1; P < MaxAutoPavers; ++P)
	{
		const int32 SpeedAt = Off::PaverSpeed + P * Off::PaverBlockBytes;
		ExpectPatchRejected(*this, Target, OneRobot, *FString::Printf(TEXT("robot %d speed level without the robot"), P), [SpeedAt](TArray<uint8>& B) { PatchI32(B, SpeedAt, 1); });
		ExpectPatchRejected(*this, Target, OneRobot, *FString::Printf(TEXT("robot %d capacity level without the robot"), P), [SpeedAt](TArray<uint8>& B) { PatchI32(B, SpeedAt + 4, 1); });
	}
	ExpectPatchRejected(*this, Target, FreshBlob(), TEXT("robot 0 speed level without any robot"), [](TArray<uint8>& B) { PatchI32(B, Off::PaverSpeed, 1); });

	// The tank must fit the capacity of the robot's own level, not of another robot's.
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 0 tank above its level (robot 1 has the big tank)"), [](TArray<uint8>& B)
	{
		PatchDouble(B, Off::PaverTank, LotBalance::AutoPaverTankLByLevel[2]);
	});
	ExpectPatchRejected(*this, Target, Base, TEXT("robot 2 tank above its level 0"), [](TArray<uint8>& B)
	{
		PatchDouble(B, Off::Paver2 + 36, LotBalance::AutoPaverTankLByLevel[0] + 1.0);
	});
	{
		TArray<uint8> Blob = Base;
		PatchDouble(Blob, Off::Paver1 + 36, LotBalance::AutoPaverTankLByLevel[2]); // robot 1 has capacity level 2
		FLotGame Probe;
		TestTrue(TEXT("robot 1 may hold the tank of its level"), Probe.LoadFromBytes(Blob));
	}
	return true;
}

bool FLotSaveLoadsVersion6Test::RunTest(const FString& Parameters)
{
	// The speed items were global in version 6: the level they gave belongs to every robot that is owned, and the other items are where they were.
	for (int32 Level = 0; Level <= MaxPaverUpgradeLevel; ++Level)
	{
		FLotGame Source;
		Source.DebugGive(1234.5);
		for (const ELotItem Item : {ELotItem::Bucket, ELotItem::Trolley, ELotItem::AutoPaver, ELotItem::AutoPaver2, ELotItem::AutoPaver3, ELotItem::Car, ELotItem::CarT2, ELotItem::CarT3})
		{
			Source.DebugGrantItem(Item);
		}
		for (int32 P = 0; P < MaxAutoPavers; ++P)
		{
			Source.DebugSetPaverUpgrade(P, ELotPaverUpgrade::Speed, Level);
		}
		Source.SetAutoPaverAuto(false);
		Source.AssignAutoPaverSector(1, 9);
		const TArray<uint8> Current = Source.SaveToBytes();
		const TArray<uint8> V6 = ToVersion6(Current);
		const FString Name = FString::Printf(TEXT("speed %d"), Level);
		if (!TestEqual(*(Name + TEXT(": a version 6 save has 15 flags and smaller robot blocks")), V6.Num(), Current.Num() + 2 - MaxAutoPavers * 8))
		{
			return false;
		}
		TestEqual(*(Name + TEXT(": it says version 6")), ReadI32(V6, Off::Version), 6);
		TestEqual(*(Name + TEXT(": the Fast flag")), static_cast<int32>(V6[Off::Owned + OldOff::V6Fast]), Level >= 1 ? 1 : 0);
		TestEqual(*(Name + TEXT(": the Faster flag")), static_cast<int32>(V6[Off::Owned + OldOff::V6Faster]), Level >= 2 ? 1 : 0);
		TestEqual(*(Name + TEXT(": Car where it was")), static_cast<int32>(V6[Off::Owned + 9]), 1);

		FLotGame Loaded;
		if (!TestTrue(*(Name + TEXT(": loads")), Loaded.LoadFromBytes(V6)))
		{
			continue;
		}
		for (int32 P = 0; P < MaxAutoPavers; ++P)
		{
			TestEqual(*FString::Printf(TEXT("%s: robot %d speed level"), *Name, P), Loaded.GetPaverUpgradeLevel(P, ELotPaverUpgrade::Speed), Level);
			TestEqual(*FString::Printf(TEXT("%s: robot %d capacity level"), *Name, P), Loaded.GetPaverUpgradeLevel(P, ELotPaverUpgrade::Capacity), 0);
			TestEqual(*FString::Printf(TEXT("%s: robot %d rate"), *Name, P), Loaded.GetAutoPaverRateCellsPerMin(P), LotBalance::AutoPaverRateCellsPerMin[Level]);
		}
		for (int32 I = 0; I < static_cast<int32>(ELotItem::Count); ++I)
		{
			TestEqual(*FString::Printf(TEXT("%s: item %d"), *Name, I), Loaded.Owns(static_cast<ELotItem>(I)), Source.Owns(static_cast<ELotItem>(I)));
		}
		TestTrue(*(Name + TEXT(": CarT2 stayed CarT2")), Loaded.Owns(ELotItem::CarT2) && Loaded.Owns(ELotItem::CarT3) && !Loaded.Owns(ELotItem::CarT4));
		TestEqual(*(Name + TEXT(": robot 1 sector")), Loaded.GetAutoPaverSector(1), 9);
		TestEqual(*(Name + TEXT(": credits")), Loaded.GetCredits(), Source.GetCredits());
		TestTrue(*(Name + TEXT(": saved again it is the current save")), Loaded.SaveToBytes() == Current);
	}

	// A robot that was not owned keeps level 0 even when the old global item was.
	{
		FLotGame Source;
		Source.DebugGrantItem(ELotItem::AutoPaver);
		Source.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 2);
		FLotGame Loaded;
		TestTrue(TEXT("one robot with both speed items loads"), Loaded.LoadFromBytes(ToVersion6(Source.SaveToBytes())));
		TestEqual(TEXT("one robot"), Loaded.GetNumAutoPavers(), 1);
		TestEqual(TEXT("it is at speed level 2"), Loaded.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 2);
		TestEqual(TEXT("robot 1 is not owned"), Loaded.GetPaverUpgradeLevel(1, ELotPaverUpgrade::Speed), -1);
	}

	// Version 5 and version 4 saves with the speed items give the robot its level too.
	{
		FLotGame Source;
		Source.DebugGrantItem(ELotItem::AutoPaver);
		Source.DebugSetPaverUpgrade(0, ELotPaverUpgrade::Speed, 2);
		const TArray<uint8> V5 = ToVersion5(Source.SaveToBytes());
		FLotGame FromV5;
		TestTrue(TEXT("version 5 loads"), FromV5.LoadFromBytes(V5));
		TestEqual(TEXT("version 5: speed level 2"), FromV5.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 2);
		FLotGame FromV4;
		TestTrue(TEXT("version 4 loads"), FromV4.LoadFromBytes(ToVersion4(V5)));
		TestEqual(TEXT("version 4: speed level 2"), FromV4.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Speed), 2);
		TestEqual(TEXT("version 4: capacity level 0"), FromV4.GetPaverUpgradeLevel(0, ELotPaverUpgrade::Capacity), 0);
	}

	// The checks of the old item flags: a speed item needs the one before it.
	{
		FLotGame Source;
		Source.DebugGrantItem(ELotItem::AutoPaver);
		const TArray<uint8> V6 = ToVersion6(Source.SaveToBytes());
		{
			FLotGame Probe;
			TestTrue(TEXT("the base version 6 blob is valid"), Probe.LoadFromBytes(V6));
		}
		FLotGame Target = MakeRichGame(true);
		ExpectPatchRejected(*this, Target, V6, TEXT("v6: Faster without Fast"), [](TArray<uint8>& B) { PatchU8(B, Off::Owned + OldOff::V6Faster, 1); });
		ExpectPatchRejected(*this, Target, ToVersion6(FreshBlob()), TEXT("v6: Fast without the robot"), [](TArray<uint8>& B) { PatchU8(B, Off::Owned + OldOff::V6Fast, 1); });
		ExpectPatchRejected(*this, Target, V6, TEXT("v6: robot 1 without robot 0 (item AutoPaver2 at its old place)"), [](TArray<uint8>& B) { PatchU8(B, Off::Owned + 13, 1); PatchU8(B, Off::Owned + 6, 0); PatchU8(B, Off::Owned + OldOff::V6Fast, 0); });
		ExpectPatchRejected(*this, Target, TArray<uint8>(V6.GetData(), V6.Num() - 1), TEXT("v6: truncated"), [](TArray<uint8>&) {});
		ExpectPatchRejected(*this, Target, V6, TEXT("v6: one byte too many"), [](TArray<uint8>& B) { B.Add(0); });
		ExpectPatchRejected(*this, Target, V6, TEXT("v6: tank above the tank of level 0"), [](TArray<uint8>& B) { PatchDouble(B, OldOff::V6PaverSector + 36, LotBalance::AutoPaverTankLByLevel[0] + 1.0); });
		ExpectPatchRejected(*this, Target, V6, TEXT("v6: sector 75"), [](TArray<uint8>& B) { PatchI32(B, OldOff::V6PaverSector, 75); });
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
