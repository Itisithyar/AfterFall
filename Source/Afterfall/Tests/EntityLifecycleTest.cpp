// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Misc/AutomationTest.h"
#include "Entities/EntityLifecycleService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntityPromoteTest, "Afterfall.Entity.Promote",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FEntityPromoteTest::RunTest(const FString& P)
{
	TestTrue(TEXT("hit promotes"), UEntityLifecycleService::ShouldPromote(true, false, false, false));
	TestTrue(TEXT("chase promotes without hit"), UEntityLifecycleService::ShouldPromote(false, true, false, false));
	TestTrue(TEXT("encounter promotes"), UEntityLifecycleService::ShouldPromote(false, false, true, false));
	TestTrue(TEXT("server flag promotes"), UEntityLifecycleService::ShouldPromote(false, false, false, true));
	TestFalse(TEXT("idle stays ghost"), UEntityLifecycleService::ShouldPromote(false, false, false, false));
	TestTrue(TEXT("30m kept"), UEntityLifecycleService::MustKeepForPersistence(20.f));
	TestFalse(TEXT("31m not forced"), UEntityLifecycleService::MustKeepForPersistence(31.f));
	TestFalse(TEXT("pursuer blocks demote"), UEntityLifecycleService::CanDemote(true, 500.f, false, false));
	TestFalse(TEXT("near blocks demote"), UEntityLifecycleService::CanDemote(false, 50.f, false, false));
	TestTrue(TEXT("far+clean demotes"), UEntityLifecycleService::CanDemote(false, 150.f, false, false));
	TestTrue(TEXT("one death container"), UEntityLifecycleService::IsValidDeathContainerCount(1));
	TestFalse(TEXT("zero invalid"), UEntityLifecycleService::IsValidDeathContainerCount(0));
	TestFalse(TEXT("N invalid"), UEntityLifecycleService::IsValidDeathContainerCount(12));
	return true;
}

#endif
