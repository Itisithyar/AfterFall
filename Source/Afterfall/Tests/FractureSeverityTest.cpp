// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Destruction (Fracture|Tests) — quadratic severity response
// D(x) = K * x^2 as a DESIGNED curve (not a physical fracture law).
// Pure math + deterministic solver checks: no world, mesh, or physics needed.
// Reference calibration (80 kg throw table): E_ref = 1000 J, K = 100,
// D_thr = 64  ->  threshold solve 100x^2 = 64  ->  x = 0.8  ->  800 J
// (v ~= 4.47 m/s). 4 m/s = 640 J dents (D = 40.96); 6 m/s = 1440 J
// fractures (D = 207.36).
#include "Misc/AutomationTest.h"
#include "Destruction/FractureSolver.h"
#include "Structural/BoxBuilding.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFractureQuadraticThresholdTest,
	"Afterfall.Destruction.Quadratic.Threshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFractureQuadraticThresholdTest::RunTest(const FString& P)
{
	const AfterfallFracture::FFractureSeverityResponse R;

	// Documented threshold solve: D(x) = 64 with K = 100 -> x = 0.8.
	TestTrue(TEXT("x* = sqrt(64/100) = 0.8"),
		FMath::IsNearlyEqual(AfterfallFracture::InitiationSeverityX(R), 0.8f, 1.e-4f));

	// D at the threshold severity equals the initiation threshold exactly.
	TestTrue(TEXT("D(0.8 * E_ref) = 64"),
		FMath::IsNearlyEqual(AfterfallFracture::SeverityD(R, 0.8f * R.ReferenceEnergyJ),
			R.InitiationThreshold, 1.e-3f));

	// Gate at and below/above the threshold energy (x* * E_ref = 800 J).
	TestFalse(TEXT("sub-threshold: no initiation"),
		AfterfallFracture::InitiatesFracture(R, 799.f));
	TestTrue(TEXT("at threshold: initiation"),
		AfterfallFracture::InitiatesFracture(R, 0.8f * R.ReferenceEnergyJ));
	TestTrue(TEXT("super-threshold: initiation"),
		AfterfallFracture::InitiatesFracture(R, 1440.f)); // 6 m/s, 80 kg

	// Example numbers from the design table (80 kg effective mass).
	TestTrue(TEXT("D(2 m/s = 160 J) = 100 * 0.16^2 = 2.56"),
		FMath::IsNearlyEqual(AfterfallFracture::SeverityD(R, 160.f), 2.56f, 1.e-3f));
	TestTrue(TEXT("D(4 m/s = 640 J) = 100 * 0.64^2 = 40.96"),
		FMath::IsNearlyEqual(AfterfallFracture::SeverityD(R, 640.f), 40.96f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFractureQuadraticScalingTest,
	"Afterfall.Destruction.Quadratic.Scaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFractureQuadraticScalingTest::RunTest(const FString& P)
{
	const AfterfallFracture::FFractureSeverityResponse R;

	// Quadratic law: doubling energy quadruples D (2^2 = 4).
	const float D1 = AfterfallFracture::SeverityD(R, 500.f);
	const float D2 = AfterfallFracture::SeverityD(R, 1000.f);
	const float D4 = AfterfallFracture::SeverityD(R, 2000.f);
	TestTrue(TEXT("D(2E) = 4 * D(E)"), FMath::IsNearlyEqual(D2, 4.f * D1, 1.e-3f));
	TestTrue(TEXT("D(4E) = 16 * D(E)"), FMath::IsNearlyEqual(D4, 16.f * D1, 1.e-3f));
	TestTrue(TEXT("monotone increasing"), D1 < D2 && D2 < D4);

	// Propagation budget: E_eff = E^2 / E_ref, identity at the reference.
	TestTrue(TEXT("identity at E_ref"),
		FMath::IsNearlyEqual(AfterfallFracture::EffectivePropagationEnergy(R, R.ReferenceEnergyJ),
			R.ReferenceEnergyJ, 1.e-3f));
	TestTrue(TEXT("quadratic below reference"),
		FMath::IsNearlyEqual(AfterfallFracture::EffectivePropagationEnergy(R, 500.f),
			250.f, 1.e-3f));
	TestTrue(TEXT("quadratic above reference"),
		FMath::IsNearlyEqual(AfterfallFracture::EffectivePropagationEnergy(R, 2000.f),
			4000.f, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFractureQuadraticGateTest,
	"Afterfall.Destruction.Quadratic.Gate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFractureQuadraticGateTest::RunTest(const FString& P)
{
	const AfterfallFracture::FFractureSeverityResponse R;
	const FIntVector Start(2, 2, 2);
	const FVector Dir = FVector::ForwardVector;

	// Sub-threshold impact: the gate refuses initiation outright — no path,
	// no consumed energy, so the caller removes no voxel (dent only).
	AfterfallFracture::FFractureResult Weak =
		AfterfallFracture::Solve(Start, 1337u, EMaterialType::Wood, 100.f, 8, Dir);
	TestFalse(TEXT("sub-threshold solve does not initiate"), Weak.bInitiated);
	TestEqual(TEXT("no fracture path"), Weak.Path.Num(), 0);
	TestEqual(TEXT("no energy consumed"), Weak.EnergyConsumed, 0.f);

	// Super-threshold impact initiates and propagates deterministically.
	AfterfallFracture::FFractureResult Strong =
		AfterfallFracture::Solve(Start, 1337u, EMaterialType::Wood, 1440.f, 8, Dir);
	TestTrue(TEXT("super-threshold solve initiates"), Strong.bInitiated);
	TestTrue(TEXT("propagation path non-empty"), Strong.Path.Num() > 0);
	TestTrue(TEXT("energy consumed by the walk"), Strong.EnergyConsumed > 0.f);

	// Determinism (E41): same seed + same energy -> same path length.
	AfterfallFracture::FFractureResult Repeat =
		AfterfallFracture::Solve(Start, 1337u, EMaterialType::Wood, 1440.f, 8, Dir);
	TestEqual(TEXT("deterministic path length"), Repeat.Path.Num(), Strong.Path.Num());
	TestTrue(TEXT("deterministic consumed energy"),
		FMath::IsNearlyEqual(Repeat.EnergyConsumed, Strong.EnergyConsumed, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFractureUtilizationTest,
	"Afterfall.Destruction.Quadratic.Utilization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFractureUtilizationTest::RunTest(const FString& P)
{
	// Demand-to-capacity ratio (energy units on both sides).
	TestTrue(TEXT("U = 0.5"), FMath::IsNearlyEqual(AfterfallFracture::FailureUtilization(500.f, 1000.f), 0.5f));
	TestTrue(TEXT("U = 1 at capacity"), FMath::IsNearlyEqual(AfterfallFracture::FailureUtilization(1000.f, 1000.f), 1.f));
	TestTrue(TEXT("U > 1 over capacity"), FMath::IsNearlyEqual(AfterfallFracture::FailureUtilization(2500.f, 1000.f), 2.5f));
	TestTrue(TEXT("zero demand -> 0"), AfterfallFracture::FailureUtilization(0.f, 1000.f) == 0.f);

	// Numerical safety (contract: explicit handling, no NaN/Inf escapes).
	TestTrue(TEXT("negative demand -> 0"), AfterfallFracture::FailureUtilization(-5.f, 1000.f) == 0.f);
	TestTrue(TEXT("zero capacity fails safe -> 0"), AfterfallFracture::FailureUtilization(1000.f, 0.f) == 0.f);
	TestTrue(TEXT("negative capacity fails safe -> 0"), AfterfallFracture::FailureUtilization(1000.f, -5.f) == 0.f);
	TestTrue(TEXT("NaN demand -> 0"), AfterfallFracture::FailureUtilization(NAN, 1000.f) == 0.f);
	TestTrue(TEXT("Inf demand -> 0"), AfterfallFracture::FailureUtilization(INFINITY, 1000.f) == 0.f);
	const float Huge = AfterfallFracture::FailureUtilization(1.e30f, 1.e-20f);
	TestTrue(TEXT("overflow clamped and finite"), FMath::IsFinite(Huge) && Huge <= 1.e6f);

	// Severity math stays safe on invalid energy too (regression guard).
	const AfterfallFracture::FFractureSeverityResponse R;
	TestTrue(TEXT("negative energy -> D = 0"), AfterfallFracture::SeverityD(R, -100.f) == 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFractureMaterialChainTest,
	"Afterfall.Destruction.Quadratic.MaterialChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFractureMaterialChainTest::RunTest(const FString& P)
{
	// Capacity adapter reuses the EXISTING box-path table (no invented
	// values): cross-map contract, not literal pinning of FLINT's numbers.
	TestTrue(TEXT("Wood -> Wood"), FMath::IsNearlyEqual(
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Wood),
		UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Wood)));
	TestTrue(TEXT("Stone -> Brick"), FMath::IsNearlyEqual(
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Stone),
		UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Brick)));
	TestTrue(TEXT("Plaster -> Drywall"), FMath::IsNearlyEqual(
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Plaster),
		UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Drywall)));
	TestTrue(TEXT("Concrete -> Concrete"), FMath::IsNearlyEqual(
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Concrete),
		UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Concrete)));
	TestTrue(TEXT("Metal -> Metal"), FMath::IsNearlyEqual(
		AfterfallFracture::MaterialStrengthJ(EMaterialType::Metal),
		UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Metal)));

	const FIntVector Start(2, 2, 2);
	const FVector Dir = FVector::ForwardVector;

	// Full chain: 1440 J (6 m/s, 80 kg) passes the severity gate for
	// CONCRETE (D = 207.36 >= 64) but utilization is 1440/2500 = 0.576
	// < 1 -> material capacity NOT reached -> initiation refused, and the
	// utilization is still reported on the result.
	AfterfallFracture::FFractureResult Concrete =
		AfterfallFracture::Solve(Start, 1337u, EMaterialType::Concrete, 1440.f, 8, Dir);
	TestFalse(TEXT("concrete refuses below capacity"), Concrete.bInitiated);
	TestEqual(TEXT("concrete path empty"), Concrete.Path.Num(), 0);
	TestTrue(TEXT("utilization reported on refusal"),
		FMath::IsNearlyEqual(Concrete.Utilization, 1440.f / 2500.f, 1.e-3f));

	// Same energy on WOOD (capacity 250 J, U = 5.76) -> initiates.
	AfterfallFracture::FFractureResult Wood =
		AfterfallFracture::Solve(Start, 1337u, EMaterialType::Wood, 1440.f, 8, Dir);
	TestTrue(TEXT("wood initiates above capacity"), Wood.bInitiated);
	TestTrue(TEXT("wood utilization >= 1"), Wood.Utilization >= 1.f);

	// Raising demand above concrete capacity -> chain opens.
	AfterfallFracture::FFractureResult Strong =
		AfterfallFracture::Solve(Start, 1337u, EMaterialType::Concrete, 3000.f, 8, Dir);
	TestTrue(TEXT("concrete initiates at capacity"), Strong.bInitiated);
	TestTrue(TEXT("concrete utilization >= 1"), Strong.Utilization >= 1.f);
	TestTrue(TEXT("propagation ran"), Strong.Path.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFractureTuningGuardTest,
	"Afterfall.Destruction.Quadratic.TuningGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFractureTuningGuardTest::RunTest(const FString& P)
{
	// Invalid tuning must fall back to the documented defaults so the lab
	// page / console can never wedge the solver.
	AfterfallFracture::FFractureSeverityResponse Bad;
	Bad.ReferenceEnergyJ = NAN;
	Bad.K = -1.f;
	Bad.InitiationThreshold = INFINITY;
	AfterfallFracture::SetSeverityResponse(Bad);
	const AfterfallFracture::FFractureSeverityResponse& Got = AfterfallFracture::GetSeverityResponse();
	TestTrue(TEXT("E_ref falls back"), FMath::IsNearlyEqual(Got.ReferenceEnergyJ, 1000.f));
	TestTrue(TEXT("K falls back"), FMath::IsNearlyEqual(Got.K, 100.f));
	TestTrue(TEXT("threshold falls back"), FMath::IsNearlyEqual(Got.InitiationThreshold, 64.f));
	TestTrue(TEXT("x* still 0.8 after garbage tuning"),
		FMath::IsNearlyEqual(AfterfallFracture::InitiationSeverityX(Got), 0.8f, 1.e-4f));

	// Restore defaults for any test running after this one.
	AfterfallFracture::SetSeverityResponse(AfterfallFracture::FFractureSeverityResponse{});
	return true;
}

#endif