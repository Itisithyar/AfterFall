// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Phase 1 medical tests: transition table, capability matching, bleed math,
// part mapping, and server-authority guards. Pure logic: no world, actors
// (beyond NewObject), or assets needed.
#include "Misc/AutomationTest.h"
#include "Combat/InjuryStateComponent.h"
#include "Combat/MedicalCapability.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMedTransitionTableTest, "Afterfall.Medical.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMedTransitionTableTest::RunTest(const FString& P)
{
	using S = ETreatmentState;
	TestTrue(TEXT("idle->treating"), UInjuryStateComponent::CanTransition(S::Idle, S::Treating));
	TestTrue(TEXT("treating->complete"), UInjuryStateComponent::CanTransition(S::Treating, S::Complete));
	TestTrue(TEXT("treating->interrupted"), UInjuryStateComponent::CanTransition(S::Treating, S::Interrupted));
	TestTrue(TEXT("complete->idle"), UInjuryStateComponent::CanTransition(S::Complete, S::Idle));
	TestTrue(TEXT("interrupted->treating (re-treat)"), UInjuryStateComponent::CanTransition(S::Interrupted, S::Treating));
	// Everything else is rejected (duplicate protection lives here).
	TestFalse(TEXT("idle->complete skips session"), UInjuryStateComponent::CanTransition(S::Idle, S::Complete));
	TestFalse(TEXT("treating->treating (dupe)"), UInjuryStateComponent::CanTransition(S::Treating, S::Treating));
	TestFalse(TEXT("idle->idle"), UInjuryStateComponent::CanTransition(S::Idle, S::Idle));
	TestTrue(TEXT("complete->treating re-treat"), UInjuryStateComponent::CanTransition(S::Complete, S::Treating));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMedCapabilityTableTest, "Afterfall.Medical.Capabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMedCapabilityTableTest::RunTest(const FString& P)
{
	// Established item IDs resolve; unknown IDs do not.
	TestFalse(TEXT("bandage known"), UMedicalCapabilityLib::FindCapability(TEXT("res_med_bandage")) == nullptr);
	TestFalse(TEXT("alias known"), UMedicalCapabilityLib::FindCapability(TEXT("AF_MED_Bandage")) == nullptr);
	TestFalse(TEXT("kit known"), UMedicalCapabilityLib::FindCapability(TEXT("res_med_kit")) == nullptr);
	TestTrue(TEXT("unknown null"), UMedicalCapabilityLib::FindCapability(TEXT("no_such_item")) == nullptr);
	// Dressing helps bleeding, not pain; pills help pain, not bleeding.
	const FMedicalCapability* Bandage = UMedicalCapabilityLib::FindCapability(TEXT("res_med_bandage"));
	TestTrue(TEXT("bandage stops bleed"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Bandage, true, false, 1, false));
	TestFalse(TEXT("bandage useless on pain-only"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Bandage, false, true, 1, false));
	const FMedicalCapability* Pills = UMedicalCapabilityLib::FindCapability(TEXT("res_med_pills"));
	TestTrue(TEXT("pills help pain"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Pills, false, true, 1, false));
	TestFalse(TEXT("pills useless on bleed"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Pills, true, false, 1, false));
	// Kit always a valid offer on a wound (HP restore); stabilizer needs severity>=2.
	const FMedicalCapability* Kit = UMedicalCapabilityLib::FindCapability(TEXT("res_med_kit"));
	TestTrue(TEXT("kit valid on wound"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Kit, false, false, 1, true));
	const FMedicalCapability* Sep = UMedicalCapabilityLib::FindCapability(TEXT("med_antiseptic"));
	TestTrue(TEXT("stabilizer on severe"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Sep, false, false, 2, false));
	TestFalse(TEXT("stabilizer on light"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Sep, false, false, 1, false));
	TestFalse(TEXT("stabilizer when stabilized"),
		UMedicalCapabilityLib::CapabilityMatchesInjury(*Sep, false, false, 2, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMedMathAndGuardsTest, "Afterfall.Medical.MathGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMedMathAndGuardsTest::RunTest(const FString& P)
{
	// Part mapping follows EBodyPart declaration order.
	TestEqual(TEXT("head=0"), UInjuryStateComponent::PartToIndex(EBodyPart::Head), 0);
	TestEqual(TEXT("torso=1"), UInjuryStateComponent::PartToIndex(EBodyPart::Torso), 1);
	TestEqual(TEXT("armL=2"), UInjuryStateComponent::PartToIndex(EBodyPart::ArmL), 2);
	TestEqual(TEXT("legR=5"), UInjuryStateComponent::PartToIndex(EBodyPart::LegR), 5);
	// Bleed math.
	TestEqual(TEXT("bleed 1.5hp/s*2s"), UInjuryStateComponent::BleedTick(1.5f, 2.f), 3.f);
	TestEqual(TEXT("bleed zero rate"), UInjuryStateComponent::BleedTick(0.f, 2.f), 0.f);
	TestEqual(TEXT("bleed zero dt"), UInjuryStateComponent::BleedTick(1.5f, 0.f), 0.f);
	// Authority guards: ownerless component never writes state, never crashes.
	UInjuryStateComponent* C = NewObject<UInjuryStateComponent>();
	TestFalse(TEXT("invalid component"), C == nullptr);
	if (!C)
	{
		return false;
	}
	C->ServerApplyWound(EBodyPart::Torso, 2); // no owner: rejected
	TestFalse(TEXT("no wound without authority"), C->PartInjuries[1].bWounded);
	C->ServerBeginTreatment(EBodyPart::Torso, TEXT("res_med_bandage")); // rejected
	TestTrue(TEXT("still idle without authority"), C->TreatmentState == ETreatmentState::Idle);
	C->ServerInterruptTreatment(); // safe no-op
	TestEqual(TEXT("no pending heal without authority"), C->ConsumePendingHeal(), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
