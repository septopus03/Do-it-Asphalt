// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltBalance.h"
#include "AsphaltGame.h"
#include "AsphaltLayout.h"
#include <limits>

DIA_TEST(FSaveRoundTripTest, "DoItAsphalt.Core.Save.RoundTrip")
DIA_TEST(FSaveWonRoundTripTest, "DoItAsphalt.Core.Save.WonRoundTrip")
DIA_TEST(FSaveContinuesIdenticallyTest, "DoItAsphalt.Core.Save.ContinuesIdentically")
DIA_TEST(FSaveRejectsCorruptDataTest, "DoItAsphalt.Core.Save.RejectsCorruptData")
DIA_TEST(FSaveRejectsInvalidFieldsTest, "DoItAsphalt.Core.Save.RejectsInvalidFields")

namespace
{
	constexpr int32 NumSpecials = static_cast<int32>(EAsphaltSpecial::Count);

	/** A game that has been played a little in every respect that the save has to keep. */
	void PlaySomeGame(FAsphaltGame& G)
	{
		// 25 lot cells plus the five cells of road 3.
		for (int32 Cell = 0; Cell < 25; ++Cell)
		{
			G.DebugCompleteCell(Cell);
		}
		for (const int32 Cell : G.GetMap().GetRoadCells(3))
		{
			G.DebugCompleteCell(Cell);
		}
		G.DebugGive(500.0, 5);
		G.Buy(EAsphaltUpgrade::Backpack);
		G.Buy(EAsphaltUpgrade::Tool);
		G.Buy(EAsphaltUpgrade::Plant);
		G.DebugGrantSpecial(EAsphaltSpecial::AutoPaver);
		G.DebugGrantSpecial(EAsphaltSpecial::PaverCar);
		G.Tick(90.0);
		G.RefillBackpack(1.0);
		G.RefillCar(1.0);
		G.PourFromBackpack(FVector2D(0.0, 0.0), FVector2D(300.0, 120.0));

		FAsphaltWorldState& World = G.World();
		World.PlayerPos = FVector2D(123.5, -456.25);
		World.PlayerYawDeg = 77.5;
		World.StationPos = FVector2D(-800.0, 90.0);
		World.bStationCarried = true;
		World.CarPos = FVector2D(1500.0, -300.0);
		World.CarYawDeg = -135.0;
		World.bPlayerInCar = true;
	}

	/** Every value that can be read from the game; the save has to reproduce all of them. */
	void ExpectSameState(FAutomationTestBase& T, const FAsphaltGame& A, const FAsphaltGame& B)
	{
		T.TestEqual(TEXT("money"), A.GetMoney(), B.GetMoney(), 0.0);
		T.TestEqual(TEXT("shards"), A.GetShards(), B.GetShards());
		T.TestEqual(TEXT("playtime"), A.GetPlaytimeSeconds(), B.GetPlaytimeSeconds(), 0.0);
		T.TestEqual(TEXT("total litres"), A.GetTotalLitresPoured(), B.GetTotalLitresPoured(), 0.0);
		for (const EAsphaltUpgrade U : {EAsphaltUpgrade::Backpack, EAsphaltUpgrade::Tool, EAsphaltUpgrade::Plant})
		{
			T.TestEqual(TEXT("level"), A.GetLevel(U), B.GetLevel(U));
			T.TestEqual(TEXT("next price"), A.GetNextPrice(U), B.GetNextPrice(U), 0.0);
			T.TestEqual(TEXT("can buy"), A.CanBuy(U), B.CanBuy(U));
		}
		T.TestEqual(TEXT("backpack capacity"), A.GetBackpackCapacity(), B.GetBackpackCapacity(), 0.0);
		T.TestEqual(TEXT("backpack litres"), A.GetBackpackLitres(), B.GetBackpackLitres(), 0.0);
		T.TestEqual(TEXT("tool width"), A.GetToolWidthCm(), B.GetToolWidthCm(), 0.0);
		T.TestEqual(TEXT("plant rate"), A.GetPlantRatePerMin(), B.GetPlantRatePerMin(), 0.0);
		T.TestEqual(TEXT("plant storage"), A.GetPlantStorage(), B.GetPlantStorage(), 0.0);
		T.TestEqual(TEXT("plant stock"), A.GetPlantStock(), B.GetPlantStock(), 0.0);
		T.TestEqual(TEXT("car litres"), A.GetCarLitres(), B.GetCarLitres(), 0.0);

		for (int32 I = 0; I < NumSpecials; ++I)
		{
			const EAsphaltSpecial S = static_cast<EAsphaltSpecial>(I);
			T.TestEqual(TEXT("has special"), A.HasSpecial(S), B.HasSpecial(S));
			T.TestEqual(TEXT("can buy special"), A.CanBuySpecial(S), B.CanBuySpecial(S));
		}
		for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
		{
			T.TestEqual(TEXT("road progress"), A.GetRoadProgress(Road), B.GetRoadProgress(Road));
			T.TestEqual(TEXT("road complete"), A.IsRoadComplete(Road), B.IsRoadComplete(Road));
		}
		T.TestEqual(TEXT("exit open"), A.IsExitOpen(), B.IsExitOpen());
		T.TestEqual(TEXT("won"), A.HasWon(), B.HasWon());

		T.TestTrue(TEXT("paver position"), A.GetAutoPaverPos().Equals(B.GetAutoPaverPos(), 0.0));
		T.TestEqual(TEXT("paver target"), A.GetAutoPaverTarget(), B.GetAutoPaverTarget());

		T.TestEqual(TEXT("complete cells"), A.GetGrid().NumCompleteCells(), B.GetGrid().NumCompleteCells());
		int32 DifferentFills = 0;
		int32 DifferentComplete = 0;
		for (int32 Cell = 0; Cell < A.GetMap().Num(); ++Cell)
		{
			DifferentFills += A.GetGrid().GetCellFill(Cell) != B.GetGrid().GetCellFill(Cell) ? 1 : 0;
			DifferentComplete += A.GetGrid().IsCellComplete(Cell) != B.GetGrid().IsCellComplete(Cell) ? 1 : 0;
		}
		T.TestEqual(TEXT("cells with a different fill"), DifferentFills, 0);
		T.TestEqual(TEXT("cells with a different completion"), DifferentComplete, 0);

		const FAsphaltWorldState& WA = A.World();
		const FAsphaltWorldState& WB = B.World();
		T.TestTrue(TEXT("player pos"), WA.PlayerPos.Equals(WB.PlayerPos, 0.0));
		T.TestEqual(TEXT("player yaw"), WA.PlayerYawDeg, WB.PlayerYawDeg, 0.0);
		T.TestTrue(TEXT("station pos"), WA.StationPos.Equals(WB.StationPos, 0.0));
		T.TestEqual(TEXT("station carried"), WA.bStationCarried, WB.bStationCarried);
		T.TestTrue(TEXT("car pos"), WA.CarPos.Equals(WB.CarPos, 0.0));
		T.TestEqual(TEXT("car yaw"), WA.CarYawDeg, WB.CarYawDeg, 0.0);
		T.TestEqual(TEXT("player in car"), WA.bPlayerInCar, WB.bPlayerInCar);
	}

	/** Offset of the only occurrence of the bytes of Value, INDEX_NONE if there is none or more than one. */
	template <typename T>
	int32 FindUniqueOffset(const TArray<uint8>& Bytes, T Value)
	{
		int32 Found = INDEX_NONE;
		for (int32 At = 0; At + static_cast<int32>(sizeof(T)) <= Bytes.Num(); ++At)
		{
			if (FMemory::Memcmp(Bytes.GetData() + At, &Value, sizeof(T)) == 0)
			{
				if (Found != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				Found = At;
			}
		}
		return Found;
	}

	template <typename T>
	TArray<uint8> Patched(const TArray<uint8>& Bytes, int32 Offset, T Value)
	{
		TArray<uint8> Copy = Bytes;
		FMemory::Memcpy(Copy.GetData() + Offset, &Value, sizeof(T));
		return Copy;
	}
}

bool FSaveRoundTripTest::RunTest(const FString& Parameters)
{
	FAsphaltGame Original;
	PlaySomeGame(Original);
	TestTrue(TEXT("setup completed cells"), Original.GetGrid().NumCompleteCells() >= 30);
	TestTrue(TEXT("setup has a backpack content"), Original.GetBackpackLitres() > 0.0 || Original.GetMoney() > 0.0);

	const TArray<uint8> Bytes = Original.SaveToBytes();
	TestTrue(TEXT("the save holds at least the coverage"), Bytes.Num() > 51200);

	FAsphaltGame Loaded;
	TestTrue(TEXT("load succeeds"), Loaded.LoadFromBytes(Bytes));
	ExpectSameState(*this, Original, Loaded);

	// Saving the loaded game gives the same bytes again.
	TestTrue(TEXT("save of the loaded game is identical"), Loaded.SaveToBytes() == Bytes);

	// Loading over an already played game replaces everything.
	FAsphaltGame Other;
	Other.DebugGive(99999.0, 99);
	Other.DebugCompleteCell(300);
	TestTrue(TEXT("load over another game"), Other.LoadFromBytes(Bytes));
	ExpectSameState(*this, Original, Other);

	// A new game round-trips as well.
	FAsphaltGame Fresh;
	FAsphaltGame FreshLoaded;
	FreshLoaded.DebugGive(5.0, 5);
	TestTrue(TEXT("fresh game loads"), FreshLoaded.LoadFromBytes(Fresh.SaveToBytes()));
	ExpectSameState(*this, Fresh, FreshLoaded);
	return true;
}

bool FSaveWonRoundTripTest::RunTest(const FString& Parameters)
{
	FAsphaltGame G;
	for (int32 Cell = 0; Cell < G.GetMap().Num(); ++Cell)
	{
		G.DebugCompleteCell(Cell);
	}
	G.SetWon();
	TestTrue(TEXT("setup: won"), G.HasWon());

	FAsphaltGame Loaded;
	TestTrue(TEXT("load succeeds"), Loaded.LoadFromBytes(G.SaveToBytes()));
	TestTrue(TEXT("won"), Loaded.HasWon());
	TestTrue(TEXT("exit open"), Loaded.IsExitOpen());
	TestEqual(TEXT("complete cells"), Loaded.GetGrid().NumCompleteCells(), 499);
	for (int32 Road = 0; Road < FHexMap::NumRoads; ++Road)
	{
		TestTrue(TEXT("road complete"), Loaded.IsRoadComplete(Road));
	}
	ExpectSameState(*this, G, Loaded);
	return true;
}

bool FSaveContinuesIdenticallyTest::RunTest(const FString& Parameters)
{
	// Cached state of the grid (completed-cell counter, per-cell bookkeeping) must be consistent after a load:
	// the loaded game has to behave exactly like the original from here on.
	FAsphaltGame Original;
	PlaySomeGame(Original);
	FAsphaltGame Loaded;
	TestTrue(TEXT("load succeeds"), Loaded.LoadFromBytes(Original.SaveToBytes()));

	FAsphaltEvents EventsOriginal;
	FAsphaltEvents EventsLoaded;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		EventsOriginal.Append(Original.Tick(30.0));
		EventsLoaded.Append(Loaded.Tick(30.0));
		Original.RefillBackpack(1.0);
		Loaded.RefillBackpack(1.0);
		EventsOriginal.Append(Original.PourFromBackpack(FVector2D(-200.0, Step * 40.0), FVector2D(200.0, Step * 40.0)));
		EventsLoaded.Append(Loaded.PourFromBackpack(FVector2D(-200.0, Step * 40.0), FVector2D(200.0, Step * 40.0)));
	}
	TestEqual(TEXT("litres"), EventsOriginal.LitresPoured, EventsLoaded.LitresPoured, 0.0);
	TestEqual(TEXT("cells"), EventsOriginal.CompletedCells.Num(), EventsLoaded.CompletedCells.Num());
	TestTrue(TEXT("same cells in the same order"), EventsOriginal.CompletedCells == EventsLoaded.CompletedCells);
	TestTrue(TEXT("something happened"), EventsOriginal.LitresPoured > 0.0);
	ExpectSameState(*this, Original, Loaded);
	return true;
}

bool FSaveRejectsCorruptDataTest::RunTest(const FString& Parameters)
{
	FAsphaltGame Source;
	PlaySomeGame(Source);
	const TArray<uint8> B = Source.SaveToBytes();

	// Level patch: two games that differ only in the Tool level show where that value is stored.
	FAsphaltGame LevelA;
	FAsphaltGame LevelB;
	LevelA.DebugSetLevel(EAsphaltUpgrade::Tool, 1);
	LevelB.DebugSetLevel(EAsphaltUpgrade::Tool, 2);
	const TArray<uint8> BytesA = LevelA.SaveToBytes();
	const TArray<uint8> BytesB = LevelB.SaveToBytes();
	int32 ToolOffset = INDEX_NONE;
	for (int32 At = 0; At < BytesA.Num() && At < BytesB.Num(); ++At)
	{
		if (BytesA[At] != BytesB[At])
		{
			ToolOffset = At;
			break;
		}
	}
	TestTrue(TEXT("found the Tool level"), ToolOffset != INDEX_NONE);

	struct FCase
	{
		FString Name;
		TArray<uint8> Bytes;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("empty"), TArray<uint8>()});
	Cases.Add({TEXT("first half"), TArray<uint8>(B.GetData(), B.Num() / 2)});
	Cases.Add({TEXT("all but the last byte"), TArray<uint8>(B.GetData(), B.Num() - 1)});
	TArray<uint8> Longer = B;
	Longer.Add(0);
	Cases.Add({TEXT("trailing byte"), Longer});
	TArray<uint8> BadMagic = B;
	BadMagic[0] ^= 0xFF;
	Cases.Add({TEXT("magic flipped"), BadMagic});
	TArray<uint8> BadVersion = B;
	BadVersion[4] = 2;
	Cases.Add({TEXT("version 2"), BadVersion});
	TArray<uint8> Noise;
	Noise.Init(0xAB, 100000);
	Cases.Add({TEXT("100000 bytes of 0xAB"), Noise});
	if (ToolOffset != INDEX_NONE)
	{
		TArray<uint8> BadTool = B;
		BadTool[ToolOffset] = 99;
		Cases.Add({TEXT("Tool level 99"), BadTool});
	}

	for (const FCase& Case : Cases)
	{
		FAsphaltGame Game;
		Game.DebugGive(123.0, 4);
		Game.DebugCompleteCell(7);
		const double MoneyBefore = Game.GetMoney();
		const TArray<uint8> Before = Game.SaveToBytes();
		TestFalse(*FString::Printf(TEXT("%s is rejected"), *Case.Name), Game.LoadFromBytes(Case.Bytes));
		TestEqual(*FString::Printf(TEXT("%s: money untouched"), *Case.Name), Game.GetMoney(), MoneyBefore, 0.0);
		TestTrue(*FString::Printf(TEXT("%s: nothing changed"), *Case.Name), Game.SaveToBytes() == Before);
	}

	// Control: the unmodified bytes load.
	FAsphaltGame Control;
	TestTrue(TEXT("control loads"), Control.LoadFromBytes(B));
	return true;
}

bool FSaveRejectsInvalidFieldsTest::RunTest(const FString& Parameters)
{
	// Each case patches one value of an otherwise valid save to something the game could never have produced.
	const auto ExpectRejected = [this](const TCHAR* What, const TArray<uint8>& Bytes)
	{
		FAsphaltGame Game;
		Game.DebugGive(321.0, 3);
		const TArray<uint8> Before = Game.SaveToBytes();
		TestFalse(*FString::Printf(TEXT("%s is rejected"), What), Game.LoadFromBytes(Bytes));
		TestTrue(*FString::Printf(TEXT("%s: nothing changed"), What), Game.SaveToBytes() == Before);
	};

	// Header: cell count and grid size. They follow magic and version, each a 32-bit integer.
	{
		FAsphaltGame G;
		const TArray<uint8> B = G.SaveToBytes();
		FAsphaltGame Control;
		TestTrue(TEXT("a fresh save loads"), Control.LoadFromBytes(B));
		ExpectRejected(TEXT("cell count 498"), Patched<int32>(B, 8, 498));
		ExpectRejected(TEXT("grid size 320"), Patched<int32>(B, 12, 320));
	}

	// Economy.
	{
		FAsphaltGame G;
		G.DebugGive(123.456, 7777);
		const TArray<uint8> B = G.SaveToBytes();
		const int32 MoneyAt = FindUniqueOffset<double>(B, 123.456);
		const int32 ShardsAt = FindUniqueOffset<int32>(B, 7777);
		TestTrue(TEXT("found money"), MoneyAt != INDEX_NONE);
		TestTrue(TEXT("found shards"), ShardsAt != INDEX_NONE);
		if (MoneyAt != INDEX_NONE)
		{
			ExpectRejected(TEXT("NaN money"), Patched<double>(B, MoneyAt, FMath::Sqrt(-1.0)));
			ExpectRejected(TEXT("infinite money"), Patched<double>(B, MoneyAt, std::numeric_limits<double>::infinity()));
			ExpectRejected(TEXT("negative money"), Patched<double>(B, MoneyAt, -1.0));
		}
		if (ShardsAt != INDEX_NONE)
		{
			ExpectRejected(TEXT("negative shards"), Patched<int32>(B, ShardsAt, -1));
		}
	}

	// Containers: litres beyond the capacities.
	{
		FAsphaltGame G;
		G.Tick(37.0);
		G.RefillBackpack(0.45);
		const double Stock = G.GetPlantStock();
		const double Backpack = G.GetBackpackLitres();
		TestTrue(TEXT("setup: stock and backpack differ"), Stock > 0.0 && Backpack > 0.0 && Stock != Backpack);
		const TArray<uint8> B = G.SaveToBytes();
		const int32 StockAt = FindUniqueOffset<double>(B, Stock);
		const int32 BackpackAt = FindUniqueOffset<double>(B, Backpack);
		TestTrue(TEXT("found stock"), StockAt != INDEX_NONE);
		TestTrue(TEXT("found backpack"), BackpackAt != INDEX_NONE);
		if (StockAt != INDEX_NONE)
		{
			ExpectRejected(TEXT("stock above the storage"), Patched<double>(B, StockAt, G.GetPlantStorage() + 1.0));
			ExpectRejected(TEXT("negative stock"), Patched<double>(B, StockAt, -0.5));
		}
		if (BackpackAt != INDEX_NONE)
		{
			ExpectRejected(TEXT("backpack above the capacity"), Patched<double>(B, BackpackAt, G.GetBackpackCapacity() + 1.0));
			ExpectRejected(TEXT("NaN backpack"), Patched<double>(B, BackpackAt, FMath::Sqrt(-1.0)));
		}
	}

	// Auto paver target: INDEX_NONE or a valid cell, nothing else.
	{
		FAsphaltGame G;
		G.DebugGrantSpecial(EAsphaltSpecial::AutoPaver);
		G.Tick(5.0);
		const int32 Target = G.GetAutoPaverTarget();
		TestTrue(TEXT("setup: paver has a target"), Target != INDEX_NONE);
		const TArray<uint8> B = G.SaveToBytes();
		const int32 TargetAt = FindUniqueOffset<int32>(B, Target);
		TestTrue(TEXT("found the paver target"), TargetAt != INDEX_NONE);
		if (TargetAt != INDEX_NONE)
		{
			FAsphaltGame Idle;
			TestTrue(TEXT("INDEX_NONE target loads"), Idle.LoadFromBytes(Patched<int32>(B, TargetAt, INDEX_NONE)));
			TestEqual(TEXT("target is idle"), Idle.GetAutoPaverTarget(), static_cast<int32>(INDEX_NONE));
			FAsphaltGame Last;
			TestTrue(TEXT("last cell as target loads"), Last.LoadFromBytes(Patched<int32>(B, TargetAt, 498)));
			TestEqual(TEXT("target is last cell"), Last.GetAutoPaverTarget(), 498);

			ExpectRejected(TEXT("target 499"), Patched<int32>(B, TargetAt, 499));
			ExpectRejected(TEXT("target -2"), Patched<int32>(B, TargetAt, -2));
			ExpectRejected(TEXT("target INT_MAX"), Patched<int32>(B, TargetAt, TNumericLimits<int32>::Max()));
		}
	}
	return true;
}
