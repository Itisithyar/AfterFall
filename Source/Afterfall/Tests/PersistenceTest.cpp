// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Misc/AutomationTest.h"
#include "Persistence/PersistenceService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistenceCompatTest, "Afterfall.Persistence.Compat",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPersistenceCompatTest::RunTest(const FString& P)
{
	bool bMig = false;
	TestTrue(TEXT("same version ok"), UPersistenceService::IsSaveCompatible(1, 1, bMig) && !bMig);
	TestTrue(TEXT("older migrates"), UPersistenceService::IsSaveCompatible(0, 1, bMig) && bMig);
	TestFalse(TEXT("newer rejected"), UPersistenceService::IsSaveCompatible(2, 1, bMig));
	TestEqual(TEXT("stable guid deterministic"),
		UPersistenceService::MakeStableGuid(1337, 1, TEXT("house:0")),
		UPersistenceService::MakeStableGuid(1337, 1, TEXT("house:0")));
	TestTrue(TEXT("tombstone on taken"), UPersistenceService::ShouldTombstone(true, false, false));
	TestTrue(TEXT("tombstone on stale"), UPersistenceService::ShouldTombstone(false, true, false));
	TestFalse(TEXT("live stays"), UPersistenceService::ShouldTombstone(false, false, false));
	TestEqual(TEXT("catchup clamped"), UPersistenceService::CatchUpProgress(1000.f, 1.f, 50.f), 50.f);
	TestEqual(TEXT("cell key"), UPersistenceService::CellKeyForLocation(FVector(250.f, -50.f, 0.f), 200.f), FString(TEXT("1,-1")));
	return true;
}

#endif
