// Copyright Do it Asphalt. All Rights Reserved.

// Tests are not part of a Shipping build.
#if WITH_DEV_AUTOMATION_TESTS

#include "AsphaltTest.h"

DIA_TEST(FCoreSmokeTest, "DoItAsphalt.Core.Smoke")

bool FCoreSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("module loads"), true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
