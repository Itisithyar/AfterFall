// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Tests) — acceptance for Phase 3b shared validation.
// Run in editor: Session Frontend > Automation > Afterfall.Construction.
// Mirrored by Scripts/validate_phase3.py for no-engine CI evidence.
#include "Misc/AutomationTest.h"
#include "Structural/ConstructionValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConstructionValidatorGridTest,
	"Afterfall.Construction.GridSnap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConstructionValidatorGridTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("origin is on-grid"), UConstructionValidator::IsOnGrid(FVector::ZeroVector));
	TestTrue(TEXT("50cm point is on-grid"), UConstructionValidator::IsOnGrid(FVector(50.f, -100.f, 150.f)));
	TestFalse(TEXT("27cm point is off-grid"), UConstructionValidator::IsOnGrid(FVector(27.f, 0.f, 0.f)));
	TestEqual(TEXT("snap rounds 27->50"), UConstructionValidator::SnapToGrid(FVector(27.f, 0.f, 0.f)), FVector(50.f, 0.f, 0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConstructionValidatorPlacementTest,
	"Afterfall.Construction.ValidatePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConstructionValidatorPlacementTest::RunTest(const FString& Parameters)
{
	FString Reason;
	// Foundation bootstraps without support.
	TestTrue(TEXT("foundation passes with no support + funds"), UConstructionValidator::ValidatePlacement(
		EStructuralPart::Foundation, FVector(0.f, 0.f, 0.f), 0, false, 10, Reason));
	// Wall needs support.
	TestFalse(TEXT("wall without support rejected"), UConstructionValidator::ValidatePlacement(
		EStructuralPart::Wall, FVector(0.f, 0.f, 0.f), 0, false, 10, Reason));
	TestEqual(TEXT("reason is no-support"), Reason, FString(TEXT("no-support")));
	// Wall with support passes.
	TestTrue(TEXT("wall with support passes"), UConstructionValidator::ValidatePlacement(
		EStructuralPart::Wall, FVector(50.f, 0.f, 0.f), 1, false, 10, Reason));
	// Ownership beats funds.
	TestFalse(TEXT("claimed lot rejected"), UConstructionValidator::ValidatePlacement(
		EStructuralPart::Wall, FVector(50.f, 0.f, 0.f), 1, true, 10, Reason));
	// Poverty rejected with exact cost math (wall = 2 wood).
	TestFalse(TEXT("poor player rejected"), UConstructionValidator::ValidatePlacement(
		EStructuralPart::Wall, FVector(50.f, 0.f, 0.f), 1, false, 1, Reason));
	TestTrue(TEXT("cost table wall=2"), UConstructionValidator::GetMaterialCost(EStructuralPart::Wall) == 2);
	TestTrue(TEXT("cost table foundation=4"), UConstructionValidator::GetMaterialCost(EStructuralPart::Foundation) == 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
