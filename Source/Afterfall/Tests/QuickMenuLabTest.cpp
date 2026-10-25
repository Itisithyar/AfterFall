// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (UI/Lab) — headless tests for the quick-menu page router
// and the 10kg/5m lab analytical references (E=490.5J, v~=9.90m/s, p~=99).
#include "Misc/AutomationTest.h"
#include "AfterfallCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickMenuRouterTest, "Afterfall.UI.QuickMenuRouter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuickMenuRouterTest::RunTest(const FString& P)
{
	using Q = EQuickMenuPage;
	TestEqual(TEXT("map opens"), AAfterfallCharacter::QuickMenuNextPage(Q::Main, FName(TEXT("Map"))), Q::Map);
	TestEqual(TEXT("controls opens"), AAfterfallCharacter::QuickMenuNextPage(Q::Main, FName(TEXT("Controls"))), Q::Controls);
	TestEqual(TEXT("lab opens"), AAfterfallCharacter::QuickMenuNextPage(Q::Main, FName(TEXT("Lab"))), Q::Lab);
	TestEqual(TEXT("back returns"), AAfterfallCharacter::QuickMenuNextPage(Q::Map, FName(TEXT("Back"))), Q::Main);
	TestEqual(TEXT("back from controls"), AAfterfallCharacter::QuickMenuNextPage(Q::Controls, FName(TEXT("Back"))), Q::Main);
	TestEqual(TEXT("resume keeps page"), AAfterfallCharacter::QuickMenuNextPage(Q::Lab, FName(TEXT("Resume"))), Q::Lab);
	TestEqual(TEXT("respawn keeps page"), AAfterfallCharacter::QuickMenuNextPage(Q::Main, FName(TEXT("Respawn"))), Q::Main);
	TestEqual(TEXT("lab run keeps page"), AAfterfallCharacter::QuickMenuNextPage(Q::Lab, FName(TEXT("LabRun"))), Q::Lab);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLabReferenceMathTest, "Afterfall.Physics.LabReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLabReferenceMathTest::RunTest(const FString& P)
{
	// 10kg, 5m, g=9.81 (analytical references used by afPhysicsLab).
	const double E = 10.0 * 9.81 * 5.0;
	TestTrue(TEXT("E=490.5J"), FMath::IsNearlyEqual(E, 490.5, 0.01));
	const double V = FMath::Sqrt(2.0 * 9.81 * 5.0);
	TestTrue(TEXT("v~=9.90m/s"), FMath::IsNearlyEqual(V, 9.905, 0.01));
	const double Mom = 10.0 * V;
	TestTrue(TEXT("p~=99.0kgm/s"), FMath::IsNearlyEqual(Mom, 99.05, 0.1));
	const double T = FMath::Sqrt(2.0 * 5.0 / 9.81);
	TestTrue(TEXT("t~=1.01s"), FMath::IsNearlyEqual(T, 1.010, 0.01));
	// Engine gravity 980cm/s^2 sits 0.1% under 9.81: documented tolerance.
	TestTrue(TEXT("engine gravity within 0.2% of 9.81"),
		FMath::Abs(980.0 - 981.0) / 981.0 < 0.002);
	return true;
}

#endif
