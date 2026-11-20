// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// DIA_TEST exists only where the engine compiles development automation tests, so a Shipping build contains none of them.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

/** Declares a simple automation test that runs in every application context and is part of the product filter. */
#define DIA_TEST(ClassName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

#endif // WITH_DEV_AUTOMATION_TESTS
