// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Load|Tests) — shared-equation load-response curves.
// Regression guards: the FPortableEquation-backed factors evaluated in
// ComputeMovementFactors equal the documented 1 - k*L values exactly, and
// the same definitions export through Afterfall::Portable::ToJson.
#include "Misc/AutomationTest.h"
#include "Movement/AfterfallMovementComponent.h"
#include "Math/PortableEquation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadEquationEvalTest, "Afterfall.Load.Equations.Eval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoadEquationEvalTest::RunTest(const FString& P)
{
	// The shared definitions ARE the documented curves (A = 0, y = B*L + C).
	const FPortableEquation Accel = UAfterfallMovementComponent::AccelLoadEquation();
	const FPortableEquation Speed = UAfterfallMovementComponent::SpeedLoadEquation();
	const FPortableEquation Brake = UAfterfallMovementComponent::BrakeLoadEquation();
	TestTrue(TEXT("accel slope/intercept"), Accel.A == 0.f && Accel.B == -0.15f && Accel.C == 1.f);
	TestTrue(TEXT("speed slope/intercept"), Speed.A == 0.f && Speed.B == -0.08f && Speed.C == 1.f);
	TestTrue(TEXT("brake slope/intercept"), Brake.A == 0.f && Brake.B == -0.10f && Brake.C == 1.f);
	// Single evaluation point matches the old inline formulas exactly.
	for (float L : { 0.f, 0.5f, 1.f, 2.5f, 4.f })
	{
		TestTrue(*FString::Printf(TEXT("f_a(%g)"), L),
			FMath::IsNearlyEqual(Afterfall::Portable::Eval(Accel, L), 1.f - 0.15f * L));
		TestTrue(*FString::Printf(TEXT("f_v(%g)"), L),
			FMath::IsNearlyEqual(Afterfall::Portable::Eval(Speed, L), 1.f - 0.08f * L));
		TestTrue(*FString::Printf(TEXT("f_b(%g)"), L),
			FMath::IsNearlyEqual(Afterfall::Portable::Eval(Brake, L), 1.f - 0.10f * L));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadEquationFactorsTest, "Afterfall.Load.Equations.Factors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoadEquationFactorsTest::RunTest(const FString& P)
{
	// Movement factors consume the equations and keep the documented floors.
	float A, V, B;
	UAfterfallMovementComponent::ComputeMovementFactors(0.f, A, V, B);
	TestTrue(TEXT("unloaded = full"), A == 1.f && V == 1.f && B == 1.f);
	// NOTE: float IsNearlyEqual defaults to 1e-8 in this engine, but the
	// assertions below compare against rounded DECIMAL literals (0.40f is 1
	// ulp above the true float result of 1-0.15*4), so they carry the same
	// explicit 1e-4 tolerance the fracture tests use for literal compares.
	UAfterfallMovementComponent::ComputeMovementFactors(1.f, A, V, B);
	TestTrue(TEXT("L=1 accel 0.85"), FMath::IsNearlyEqual(A, 0.85f, 1.e-4f));
	TestTrue(TEXT("L=1 speed 0.92"), FMath::IsNearlyEqual(V, 0.92f, 1.e-4f));
	TestTrue(TEXT("L=1 brake 0.90"), FMath::IsNearlyEqual(B, 0.90f, 1.e-4f));
	UAfterfallMovementComponent::ComputeMovementFactors(4.f, A, V, B);
	TestTrue(TEXT("L=4 accel 0.40"), FMath::IsNearlyEqual(A, 0.40f, 1.e-4f));
	TestTrue(TEXT("L=4 speed 0.68"), FMath::IsNearlyEqual(V, 0.68f, 1.e-4f));
	TestTrue(TEXT("L=4 brake 0.60"), FMath::IsNearlyEqual(B, 0.60f, 1.e-4f));
	// Clamp: overload pins at L = 4, never below the floors.
	UAfterfallMovementComponent::ComputeMovementFactors(10.f, A, V, B);
	TestTrue(TEXT("overload clamps"), FMath::IsNearlyEqual(A, 0.40f, 1.e-4f) && FMath::IsNearlyEqual(V, 0.68f, 1.e-4f) && FMath::IsNearlyEqual(B, 0.60f, 1.e-4f));
	UAfterfallMovementComponent::ComputeMovementFactors(-2.f, A, V, B);
	TestTrue(TEXT("negative clamps to full"), A == 1.f && V == 1.f && B == 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadEquationExportTest, "Afterfall.Load.Equations.Export",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoadEquationExportTest::RunTest(const FString& P)
{
	// The gameplay curves export through the shared portable definition —
	// the same JSON the lab page and external adapters read.
	const FString Json = Afterfall::Portable::ToJson(UAfterfallMovementComponent::AccelLoadEquation());
	TestTrue(TEXT("export carries slope"), Json.Contains(TEXT("-0.15")));
	TestTrue(TEXT("export carries version"), Json.Contains(TEXT("version")));
	return true;
}

#endif
