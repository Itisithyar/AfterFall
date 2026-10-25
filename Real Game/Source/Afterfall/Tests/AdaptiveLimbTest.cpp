// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Animation (Limb|Tests) — pure math + config tests for the adaptive
// limb system. No world or mesh required.
#include "Misc/AutomationTest.h"
#include "Animation/AdaptiveLimbTypes.h"
#include "Animation/AdaptiveLimbComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdaptiveLimbConfigDefaultsTest, "Afterfall.Limb.Config.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdaptiveLimbConfigDefaultsTest::RunTest(const FString& P)
{
	FAdaptiveLimbConfig C;
	TestTrue(TEXT("max reach 120cm"), FMath::IsNearlyEqual(C.MaxReachCm, 120.f));
	TestTrue(TEXT("shoulder offset X 20cm"), FMath::IsNearlyEqual(C.ShoulderOffsetCm.X, 20.f));
	TestTrue(TEXT("shoulder offset Y 0cm"), FMath::IsNearlyEqual(C.ShoulderOffsetCm.Y, 0.f));
	TestTrue(TEXT("shoulder offset Z 10cm"), FMath::IsNearlyEqual(C.ShoulderOffsetCm.Z, 10.f));
	TestTrue(TEXT("elbow preferred bend 45deg"), FMath::IsNearlyEqual(C.ElbowPreferredBendDeg, 45.f));
	TestTrue(TEXT("reach speed 600cm/s"), FMath::IsNearlyEqual(C.ReachSpeedCmS, 600.f));
	TestTrue(TEXT("brace max force 15kg"), FMath::IsNearlyEqual(C.BraceMaxForceKg, 15.f));
	TestEqual(TEXT("right limb side"), C.LimbSide, EAdaptiveLimbSide::Right);
	TestTrue(TEXT("debug draw off by default"), !C.bDebugDraw);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdaptiveLimbTargetValidityTest, "Afterfall.Limb.Target.Validity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdaptiveLimbTargetValidityTest::RunTest(const FString& P)
{
	FAdaptiveLimbTarget Empty;
	TestFalse(TEXT("empty target invalid"), Empty.IsValid());

	FAdaptiveLimbTarget WithDesired;
	WithDesired.Desired = FVector(100.f, 0.f, 0.f);
	TestTrue(TEXT("desired-only target valid"), WithDesired.IsValid());

	FAdaptiveLimbTarget WithReachable;
	WithReachable.Reachable = FVector(0.f, 100.f, 0.f);
	TestTrue(TEXT("reachable-only target valid"), WithReachable.IsValid());

	FAdaptiveLimbTarget Contacting;
	Contacting.bContacting = true;
	Contacting.ContactPoint = FVector(50.f, 0.f, 50.f);
	Contacting.ContactNormal = FVector::UpVector;
	TestTrue(TEXT("contacting target valid"), Contacting.IsValid());
	TestTrue(TEXT("contacting flag"), Contacting.bContacting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdaptiveLimbElbowTest, "Afterfall.Limb.Elbow.Position",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdaptiveLimbElbowTest::RunTest(const FString& P)
{
	FAdaptiveLimbConfig C;
	C.ElbowPreferredBendDeg = 45.f;
	C.ElbowMaxBendDeg = 150.f;
	C.LimbSide = EAdaptiveLimbSide::Right;

	const FVector Shoulder(0.f, 0.f, 0.f);
	const FVector Wrist(100.f, 0.f, 0.f);

	FVector Elbow = UAdaptiveLimbComponent::ComputeElbowPosition(Shoulder, Wrist, C);

	// Elbow should be between shoulder and wrist (X in [0,100]).
	TestTrue(TEXT("elbow X >= 0"), Elbow.X >= 0.f);
	TestTrue(TEXT("elbow X <= 100"), Elbow.X <= 100.f);
	// Elbow should be bent outward (nonzero Y offset from straight line).
	TestTrue(TEXT("elbow is bent (not straight line)"), (Elbow - FVector(50.f, 0.f, 0.f)).Size() > 1.f);
	// Elbow distance from shoulder should be <= wrist distance.
	TestTrue(TEXT("elbow not beyond wrist"), (Elbow - Shoulder).Size() <= (Wrist - Shoulder).Size() + 1.f);

	// Left arm mirrors: outward bend should flip sign.
	FAdaptiveLimbConfig CLeft = C;
	CLeft.LimbSide = EAdaptiveLimbSide::Left;
	FVector ElbowLeft = UAdaptiveLimbComponent::ComputeElbowPosition(Shoulder, Wrist, CLeft);
	TestTrue(TEXT("left elbow outward sign flipped"), FMath::Abs(ElbowLeft.Y) > 0.f &&
		FMath::Abs(Elbow.Y) > 0.f &&
		((Elbow.Y > 0.f) != (ElbowLeft.Y > 0.f)));

	// Zero-length arm returns midpoint.
	FVector Degenerate = UAdaptiveLimbComponent::ComputeElbowPosition(Shoulder, Shoulder, C);
	TestEqual(TEXT("degenerate arm returns midpoint"), Degenerate, Shoulder);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdaptiveLimbStateEnumTest, "Afterfall.Limb.State.Enum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdaptiveLimbStateEnumTest::RunTest(const FString& P)
{
	TestEqual(TEXT("animated state"), (uint8)EAdaptiveLimbState::Animated, (uint8)0);
	TestEqual(TEXT("reach state"), (uint8)EAdaptiveLimbState::Reach, (uint8)1);
	TestEqual(TEXT("contact state"), (uint8)EAdaptiveLimbState::Contact, (uint8)2);
	TestEqual(TEXT("brace state"), (uint8)EAdaptiveLimbState::Brace, (uint8)3);
	TestEqual(TEXT("yield state"), (uint8)EAdaptiveLimbState::Yield, (uint8)6);
	TestEqual(TEXT("ragdoll state"), (uint8)EAdaptiveLimbState::Ragdoll, (uint8)8);
	return true;
}

#endif
