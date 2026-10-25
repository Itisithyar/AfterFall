// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Building) — pure math tests for the FragmentBuilding blast
// rule. No world, mesh, or physics needed: FragmentImpulseFor is a static,
// deterministic function, so direction, falloff, seeding and the degenerate
// epicenter case are all checkable headlessly. The physics response itself
// (sections actually flying) needs PIE and is on the playtest checklist.
#include "Misc/AutomationTest.h"
#include "Building/ProceduralBuilding.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFragmentBlastDirectionTest, "Afterfall.Building.Fragment.BlastDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFragmentBlastDirectionTest::RunTest(const FString& P)
{
	// Zero jitter: impulse must be exactly radial, away from the epicenter.
	FRandomStream Stream(1);
	const FVector Epi(0.f, 0.f, 0.f);
	const FVector Sec(1000.f, 0.f, 0.f);
	const FVector Imp = AProceduralBuilding::FragmentImpulseFor(Sec, Epi, 1000.f, 1600.f, 0.f, Stream);
	TestTrue(TEXT("points along +X"), FVector::DotProduct(Imp, FVector::ForwardVector) > 0.f);
	TestNearlyEqual(TEXT("radial axis only"), Imp.Y, 0.0, 1e-4);
	TestNearlyEqual(TEXT("no vertical kick"), Imp.Z, 0.0, 1e-4);
	// Falloff: 1 - 1000/1600 = 0.375 of strength.
	TestNearlyEqual(TEXT("falloff-scaled magnitude"), Imp.X, 375.0, 1e-4);

	// Strength scales linearly.
	FRandomStream Stream2(1);
	const FVector Imp2 = AProceduralBuilding::FragmentImpulseFor(Sec, Epi, 2000.f, 1600.f, 0.f, Stream2);
	TestNearlyEqual(TEXT("2x strength = 2x impulse"), Imp2.X, 750.0, 1e-4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFragmentBlastFalloffTest, "Afterfall.Building.Fragment.Falloff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFragmentBlastFalloffTest::RunTest(const FString& P)
{
	FRandomStream Stream(7);
	const FVector Epi(0.f, 0.f, 0.f);
	// Beyond the falloff radius nothing moves (zero jitter isolates the falloff term).
	const FVector Far = AProceduralBuilding::FragmentImpulseFor(FVector(5000.f, 0.f, 0.f), Epi,
		1000.f, 1600.f, 0.f, Stream);
	TestTrue(TEXT("outside radius is zero"), Far.IsNearlyZero());
	// Exactly at the epicenter: straight up, full strength (up instead of a
	// zero-length direction that would leave the piece in place).
	const FVector Here = AProceduralBuilding::FragmentImpulseFor(Epi, Epi, 1000.f, 1600.f, 0.f, Stream);
	TestNearlyEqual(TEXT("degenerate blasts up"), Here.Z, 1000.0, 1e-4);
	TestTrue(TEXT("degenerate is pure vertical"), FVector::DotProduct(Here, FVector::UpVector) > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFragmentBlastSeedTest, "Afterfall.Building.Fragment.SeededJitter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFragmentBlastSeedTest::RunTest(const FString& P)
{
	const FVector Epi(0.f, 0.f, 0.f);
	const FVector Sec(-400.f, 300.f, 500.f);
	// Same seed + same inputs => byte-identical impulse (reproducible blasts).
	FRandomStream A(12345), B(12345);
	const FVector IA = AProceduralBuilding::FragmentImpulseFor(Sec, Epi, 900.f, 1600.f, 0.25f, A);
	const FVector IB = AProceduralBuilding::FragmentImpulseFor(Sec, Epi, 900.f, 1600.f, 0.25f, B);
	TestTrue(TEXT("same seed reproducible"), IA.Equals(IB));

	// A different seed jitters differently but stays within the jitter budget
	// around the pure radial term: |radial| + |jitter| * sqrt(3) + slack.
	FRandomStream C(999);
	const FVector IC = AProceduralBuilding::FragmentImpulseFor(Sec, Epi, 900.f, 1600.f, 0.25f, C);
	TestFalse(TEXT("different seed differs"), IA.Equals(IC));
	FRandomStream D(0);
	const FVector Radial = AProceduralBuilding::FragmentImpulseFor(Sec, Epi, 900.f, 1600.f, 0.f, D);
	const float Budget = Radial.Size() + 900.f * 0.25f * FMath::Sqrt(3.f) + 1.f;
	TestTrue(TEXT("jitter within budget"), IC.Size() <= Budget);
	return true;
}

#endif
