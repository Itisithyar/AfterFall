// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Load|Tests) — pure regression tests for the mass-accounting
// fixes: (1) total carried mass = wallet + held slot, each item counted once;
// (2) load ratio lambda guarded against invalid reference; (3) heavy-haul flag
// keyed to the HELD item only, so wallet weight can neither trigger nor clear
// it (the flag-clobbering defect). No world, mesh, or authority required —
// matches the house pure-helper test pattern (Stage2RuntimeTest, InventoryCraftingTest).
#include "Misc/AutomationTest.h"
#include "Movement/AfterfallMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadTotalMassTest, "Afterfall.Load.Accounting.Total",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoadTotalMassTest::RunTest(const FString& P)
{
	// Wallet and held slot are disjoint ownerships: totals simply sum.
	TestTrue(TEXT("wallet only"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeTotalMassKg(10.f, 0.f), 10.f));
	TestTrue(TEXT("held only"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeTotalMassKg(0.f, 7.f), 7.f));
	TestTrue(TEXT("both contribute exactly once"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeTotalMassKg(8.f, 7.f), 15.f));
	TestTrue(TEXT("zero load"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeTotalMassKg(0.f, 0.f), 0.f));
	TestTrue(TEXT("negative inputs clamp, never reduce load"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeTotalMassKg(-5.f, -3.f), 0.f));
	TestTrue(TEXT("negative one side clamps only that side"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeTotalMassKg(-5.f, 7.f), 7.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadRatioTest, "Afterfall.Load.Accounting.Ratio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoadRatioTest::RunTest(const FString& P)
{
	// lambda = Total / Reference (dimensionless), guarded.
	TestTrue(TEXT("unity at reference"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeLoadRatio(15.f, 15.f), 1.f));
	TestTrue(TEXT("half load"),
		FMath::IsNearlyEqual(UAfterfallMovementComponent::ComputeLoadRatio(7.5f, 15.f), 0.5f));
	TestTrue(TEXT("combined total feeds ratio"),
		FMath::IsNearlyEqual(
			UAfterfallMovementComponent::ComputeLoadRatio(
				UAfterfallMovementComponent::ComputeTotalMassKg(8.f, 7.f), 15.f), 1.f));
	TestTrue(TEXT("zero total -> 0"), UAfterfallMovementComponent::ComputeLoadRatio(0.f, 15.f) == 0.f);
	TestTrue(TEXT("zero reference fails safe -> 0"), UAfterfallMovementComponent::ComputeLoadRatio(10.f, 0.f) == 0.f);
	TestTrue(TEXT("negative reference fails safe -> 0"), UAfterfallMovementComponent::ComputeLoadRatio(10.f, -1.f) == 0.f);
	TestTrue(TEXT("negative total fails safe -> 0"), UAfterfallMovementComponent::ComputeLoadRatio(-2.f, 15.f) == 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadHeavyHaulTest, "Afterfall.Load.Accounting.HeavyHaul",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoadHeavyHaulTest::RunTest(const FString& P)
{
	const float HeavyKg = 5.f; // Tuning.HeavyMassKg default

	// Boundary: at threshold = heavy; below = not.
	TestTrue(TEXT("exactly at threshold is heavy"),
		UAfterfallMovementComponent::ComputeHeavyHaul(5.f, HeavyKg));
	TestTrue(TEXT("above threshold is heavy"),
		UAfterfallMovementComponent::ComputeHeavyHaul(7.f, HeavyKg));
	TestFalse(TEXT("below threshold not heavy"),
		UAfterfallMovementComponent::ComputeHeavyHaul(4.9f, HeavyKg));

	// THE REGRESSION (flag-clobbering defect): a heavy WALLET alone must
	// never set the held-object flag. Before the fix, SetCarriedMass derived
	// the flag from the total, so 20 kg of inventory both triggered the
	// 0.894 m/s haul cap and could clear the flag after a real pickup.
	TestFalse(TEXT("wallet-only load never triggers heavy haul"),
		UAfterfallMovementComponent::ComputeHeavyHaul(0.f, HeavyKg));
	const float WalletHeavyRatio =
		UAfterfallMovementComponent::ComputeLoadRatio(
			UAfterfallMovementComponent::ComputeTotalMassKg(20.f, 0.f), 15.f);
	TestTrue(TEXT("wallet can still drive lambda (>1) without heavy flag"),
		WalletHeavyRatio > 1.f &&
		!UAfterfallMovementComponent::ComputeHeavyHaul(0.f, HeavyKg));

	// Invalid threshold: rule is disabled rather than always-true.
	TestFalse(TEXT("zero threshold disables the rule"),
		UAfterfallMovementComponent::ComputeHeavyHaul(100.f, 0.f));
	return true;
}

#endif