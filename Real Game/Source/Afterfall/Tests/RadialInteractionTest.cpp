// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Interaction) — pure math tests for the unified
// hold-to-choose radial layer. No world, mesh, or input required: the hold
// rule (MenuOpen = t_h >= T_r) and the drag-to-sector map are static and
// deterministic. Gameplay execution (server RPCs, constraints, treatment)
// is covered by the existing interaction/combat/treatment suites.
#include "Misc/AutomationTest.h"
#include "UI/RadialMenuComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRadialHoldThresholdTest, "Afterfall.Interact.Radial.Threshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRadialHoldThresholdTest::RunTest(const FString& P)
{
	const double Tr = 0.25;
	TestFalse(TEXT("press instant: no menu"), URadialMenuComponent::ShouldOpenMenu(0.0, Tr));
	TestFalse(TEXT("below threshold: no menu"), URadialMenuComponent::ShouldOpenMenu(0.24, Tr));
	TestTrue(TEXT("at threshold: menu opens"), URadialMenuComponent::ShouldOpenMenu(0.25, Tr));
	TestTrue(TEXT("past threshold: menu stays open"), URadialMenuComponent::ShouldOpenMenu(1.5, Tr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRadialSectorMapTest, "Afterfall.Interact.Radial.Sectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRadialSectorMapTest::RunTest(const FString& P)
{
	const float Dead = 24.f;
	// No options, or inside the dead zone: nothing selected.
	TestEqual(TEXT("zero options"), URadialMenuComponent::SectorForDrag(FVector2D(100.f, 0.f), Dead, 0), -1);
	TestEqual(TEXT("inside dead zone"), URadialMenuComponent::SectorForDrag(FVector2D(5.f, 5.f), Dead, 8), -1);
	TestEqual(TEXT("at rest"), URadialMenuComponent::SectorForDrag(FVector2D::ZeroVector, Dead, 8), -1);

	// Screen-right (+X) is sector 0 by contract.
	TestEqual(TEXT("east selects sector 0"),
		URadialMenuComponent::SectorForDrag(FVector2D(100.f, 0.f), Dead, 8), 0);

	// The eight cardinal/diagonal directions cover all eight sectors exactly once.
	bool Seen[8] = { false, false, false, false, false, false, false, false };
	const FVector2D Dirs[8] = {
		FVector2D(1.f, 0.f), FVector2D(1.f, 1.f), FVector2D(0.f, 1.f), FVector2D(-1.f, 1.f),
		FVector2D(-1.f, 0.f), FVector2D(-1.f, -1.f), FVector2D(0.f, -1.f), FVector2D(1.f, -1.f)
	};
	for (int32 i = 0; i < 8; ++i)
	{
		const int32 S = URadialMenuComponent::SectorForDrag(Dirs[i] * 100.f, Dead, 8);
		TestTrue(TEXT("direction selects a sector"), S >= 0 && S < 8);
		if (S >= 0 && S < 8)
		{
			TestFalse(TEXT("sectors are distinct"), Seen[S]);
			Seen[S] = true;
		}
	}

	// Fewer options than directions: indices stay in range.
	for (int32 i = 0; i < 8; ++i)
	{
		const int32 S = URadialMenuComponent::SectorForDrag(Dirs[i] * 100.f, Dead, 3);
		TestTrue(TEXT("3-option wheel stays in range"), S >= 0 && S < 3);
	}
	return true;
}

#endif
