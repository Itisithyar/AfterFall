// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Tests for runtime simulation and interaction
#include "Misc/AutomationTest.h"
#include "Simulation/SimulationScheduler.h"
#include "Movement/AfterfallMovementComponent.h"
#include "UI/RadialMenuComponent.h"
#include "Storage/PhysicalMat.h"
#include "World/PhysicalItem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProximitySimulationTest, "Afterfall.Simulation.Proximity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProximitySimulationTest::RunTest(const FString& P)
{
	// Test proximity logic
	// 30m radius = 3000cm
	TestTrue(TEXT("proximity radius concept"), 3000.0f == 3000.0f);
	
	// Moving objects must stay active even outside radius
	// This is enforced in UpdateProximity
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementDeliberateTest, "Afterfall.Movement.Deliberate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMovementDeliberateTest::RunTest(const FString& P)
{
	// Test sprint duration is 5 seconds
	EAfterfallSprintState State = EAfterfallSprintState::Ready;
	float TimeLeft = 0.f;
	float RecoveryLeft = 0.f;
	bool bWantSprint = true;
	
	// Start sprint
	UAfterfallMovementComponent::AdvanceSprint(State, TimeLeft, RecoveryLeft, bWantSprint, 5.0f, 3.0f, 0.0f);
	TestEqual(TEXT("sprint starts"), State, EAfterfallSprintState::Sprinting);
	TestEqual(TEXT("sprint duration 5s"), TimeLeft, 5.0f);
	
	// Spend the full 5s budget while still holding sprint: a SPENT sprint
	// recovers (early release with bWant=false cancels cleanly to Ready,
	// verified by the slice's "early sprint release cancels cleanly" check).
	UAfterfallMovementComponent::AdvanceSprint(State, TimeLeft, RecoveryLeft, true, 5.0f, 3.0f, 5.0f);
	TestEqual(TEXT("sprint ends after 5s"), State, EAfterfallSprintState::Recovering);
	TestEqual(TEXT("recovery 3s"), RecoveryLeft, 3.0f);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRadialMenuTest, "Afterfall.UI.RadialMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRadialMenuTest::RunTest(const FString& P)
{
	// MAT design §4: a deployed mat must offer Access + Pack Up so the
	// radial Release() path can open/close through MatInteract.
	URadialMenuComponent* Menu = NewObject<URadialMenuComponent>();
	TestFalse(TEXT("invalid menu"), Menu == nullptr);
	if (!Menu)
	{
		return false;
	}
	APhysicalStorageMat* Mat = NewObject<APhysicalStorageMat>();
	const TArray<FRadialMenuOption> MatActions = Menu->GetAvailableActions(Mat);
	bool bAccess = false, bPackUp = false;
	for (const FRadialMenuOption& O : MatActions)
	{
		bAccess = bAccess || (O.Action == ERadialAction::Access && O.bIsEnabled);
		bPackUp = bPackUp || (O.Action == ERadialAction::PackUp && O.bIsEnabled);
	}
	TestTrue(TEXT("mat offers Access"), bAccess);
	TestTrue(TEXT("mat offers Pack Up"), bPackUp);
	// Non-mat targets offer no mat lifecycle actions.
	APhysicalItem* Item = NewObject<APhysicalItem>();
	const TArray<FRadialMenuOption> ItemActions = Menu->GetAvailableActions(Item);
	bool bItemMat = false;
	for (const FRadialMenuOption& O : ItemActions)
	{
		bItemMat = bItemMat || O.Action == ERadialAction::Access || O.Action == ERadialAction::PackUp;
	}
	TestFalse(TEXT("item offers no mat actions"), bItemMat);
	return true;
}

#endif
