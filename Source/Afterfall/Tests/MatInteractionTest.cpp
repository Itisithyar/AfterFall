// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Tests for the physical inventory mat interaction (transition table and
// repeated-input safety). Pure logic: no world, actors, or assets needed.
#include "Misc/AutomationTest.h"
#include "Interaction/MatInteractionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatTransitionTableTest, "Afterfall.MatInteraction.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatTransitionTableTest::RunTest(const FString& P)
{
	using S = EMatInteractionState;
	// Legal edges of the lifecycle.
	TestTrue(TEXT("closed->opening"), UMatInteractionComponent::CanTransition(S::Closed, S::Opening));
	TestTrue(TEXT("opening->open"), UMatInteractionComponent::CanTransition(S::Opening, S::Open));
	TestTrue(TEXT("opening->closed interrupt"), UMatInteractionComponent::CanTransition(S::Opening, S::Closed));
	TestTrue(TEXT("open->closing"), UMatInteractionComponent::CanTransition(S::Open, S::Closing));
	TestTrue(TEXT("open->closed force"), UMatInteractionComponent::CanTransition(S::Open, S::Closed));
	TestTrue(TEXT("closing->closed"), UMatInteractionComponent::CanTransition(S::Closing, S::Closed));
	// Everything else is rejected (no-ops at runtime).
	TestFalse(TEXT("closed->open skips unroll"), UMatInteractionComponent::CanTransition(S::Closed, S::Open));
	TestFalse(TEXT("closed->closing"), UMatInteractionComponent::CanTransition(S::Closed, S::Closing));
	TestFalse(TEXT("closing->opening needs close first"),
		UMatInteractionComponent::CanTransition(S::Closing, S::Opening));
	TestFalse(TEXT("closing->open"), UMatInteractionComponent::CanTransition(S::Closing, S::Open));
	TestFalse(TEXT("open->opening"), UMatInteractionComponent::CanTransition(S::Open, S::Opening));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatRepeatedInputTest, "Afterfall.MatInteraction.RepeatedInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatRepeatedInputTest::RunTest(const FString& P)
{
	using S = EMatInteractionState;
	// Double-open: B pressed while Opening/Open/Closing never restarts.
	TestFalse(TEXT("double open while opening"), UMatInteractionComponent::CanTransition(S::Opening, S::Opening));
	TestFalse(TEXT("open-while-open"), UMatInteractionComponent::CanTransition(S::Open, S::Opening));
	TestFalse(TEXT("open-while-closing"), UMatInteractionComponent::CanTransition(S::Closing, S::Opening));
	// Double-close: B pressed while Closing/Closed never double-packs.
	TestFalse(TEXT("double close while closing"), UMatInteractionComponent::CanTransition(S::Closing, S::Closing));
	TestFalse(TEXT("close-while-closed"), UMatInteractionComponent::CanTransition(S::Closed, S::Closing));
	// Blocking + UI predicates follow the same state (no world required).
	UMatInteractionComponent* C = NewObject<UMatInteractionComponent>();
	TestFalse(TEXT("invalid component"), C == nullptr);
	if (!C)
	{
		return false;
	}
	C->InteractionState = S::Closed;
	TestFalse(TEXT("closed does not block"), C->IsInteractionBlocking());
	TestFalse(TEXT("ui hidden while closed"), C->IsInventoryUiVisible());
	C->InteractionState = S::Opening;
	TestTrue(TEXT("opening blocks"), C->IsInteractionBlocking());
	TestFalse(TEXT("ui hidden while unrolling"), C->IsInventoryUiVisible());
	C->InteractionState = S::Open;
	TestTrue(TEXT("open blocks"), C->IsInteractionBlocking());
	TestTrue(TEXT("ui visible while open"), C->IsInventoryUiVisible());
	C->InteractionState = S::Closing;
	TestTrue(TEXT("closing blocks"), C->IsInteractionBlocking());
	TestFalse(TEXT("ui hidden while packing"), C->IsInventoryUiVisible());
	return true;
}

#endif
