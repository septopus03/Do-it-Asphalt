// Copyright Do it Asphalt. All Rights Reserved.

#include "AsphaltTest.h"

DIA_TEST(FCoreSmokeTest, "DoItAsphalt.Core.Smoke")

bool FCoreSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("module loads"), true);
	return true;
}
