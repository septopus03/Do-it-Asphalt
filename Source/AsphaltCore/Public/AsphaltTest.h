// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

/** Declares a simple automation test that runs in every application context and is part of the product filter. */
#define DIA_TEST(ClassName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
