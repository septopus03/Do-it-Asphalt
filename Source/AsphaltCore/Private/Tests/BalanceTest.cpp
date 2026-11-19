// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"
#include "AsphaltBalance.h"
#include "HexMap.h"

DIA_TEST(FBalancePricesTest, "DoItAsphalt.Core.Balance.Prices")
DIA_TEST(FBalanceTierTest, "DoItAsphalt.Core.Balance.BackpackTier")
DIA_TEST(FBalanceIncreasingTest, "DoItAsphalt.Core.Balance.StrictlyIncreasing")

namespace
{
	template <typename T, int32 N>
	T SumOf(const T (&Values)[N])
	{
		T Sum = T(0);
		for (const T& Value : Values)
		{
			Sum += Value;
		}
		return Sum;
	}

	template <typename T, int32 N>
	bool IsStrictlyIncreasing(const T (&Values)[N])
	{
		for (int32 I = 1; I < N; ++I)
		{
			if (!(Values[I] > Values[I - 1]))
			{
				return false;
			}
		}
		return true;
	}
}

bool FBalancePricesTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltBalance;

	TestEqual(TEXT("backpack shards"), SumOf(BackpackPriceShards), 253);
	const double TotalMoney = SumOf(ToolPriceMoney) + SumOf(PlantPriceMoney) + SumOf(SpecialPriceMoney);
	TestEqual(TEXT("money total"), TotalMoney, 5795.0, 1e-9);
	TestTrue(TEXT("money fits 60 % of all paint"), TotalMoney <= 0.6 * FHexMap::NumCells * LitresPerCell * MoneyPerLitre);
	TestTrue(TEXT("shards fit the cells"), SumOf(BackpackPriceShards) <= FHexMap::NumCells * ShardsPerCell);
	return true;
}

bool FBalanceTierTest::RunTest(const FString& Parameters)
{
	using AsphaltBalance::BackpackTier;

	TestEqual(TEXT("2"), BackpackTier(2), 0);
	TestEqual(TEXT("3"), BackpackTier(3), 1);
	TestEqual(TEXT("6"), BackpackTier(6), 1);
	TestEqual(TEXT("7"), BackpackTier(7), 2);
	TestEqual(TEXT("9"), BackpackTier(9), 2);
	TestEqual(TEXT("10"), BackpackTier(10), 3);
	TestEqual(TEXT("12"), BackpackTier(12), 3);
	return true;
}

bool FBalanceIncreasingTest::RunTest(const FString& Parameters)
{
	using namespace AsphaltBalance;

	TestTrue(TEXT("BackpackCapacityL"), IsStrictlyIncreasing(BackpackCapacityL));
	TestTrue(TEXT("BackpackPriceShards"), IsStrictlyIncreasing(BackpackPriceShards));
	TestTrue(TEXT("ToolWidthM"), IsStrictlyIncreasing(ToolWidthM));
	TestTrue(TEXT("ToolPriceMoney"), IsStrictlyIncreasing(ToolPriceMoney));
	TestTrue(TEXT("PlantRateLPerMin"), IsStrictlyIncreasing(PlantRateLPerMin));
	TestTrue(TEXT("PlantPriceMoney"), IsStrictlyIncreasing(PlantPriceMoney));
	TestTrue(TEXT("SpecialPriceMoney"), IsStrictlyIncreasing(SpecialPriceMoney));
	return true;
}
