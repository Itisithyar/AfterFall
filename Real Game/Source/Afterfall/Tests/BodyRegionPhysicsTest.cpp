// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Mannequin foundation tests: profile table integrity + pure math.
#include "Misc/AutomationTest.h"
#include "Combat/BodyRegionPhysics.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyRegionTableTest, "Afterfall.BodyPhysics.Table",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBodyRegionTableTest::RunTest(const FString& P)
{
	// Six regions, mass shares sum to 1.0, all positive glass thresholds.
	const TArray<FBodyRegionPhysicsProfile>& Tbl = UBodyRegionPhysicsLib::GetProfiles();
	TestEqual(TEXT("six regions"), Tbl.Num(), 6);
	float Sum = 0.f;
	for (const FBodyRegionPhysicsProfile& R : Tbl)
	{
		Sum += R.MassShare;
		TestTrue(TEXT("positive glass threshold"), R.GlassBreakEnergy > 0.f);
	}
	TestTrue(TEXT("mass shares sum to 1"), FMath::IsNearlyEqual(Sum, 1.f, 0.001f));
	TestFalse(TEXT("torso resolvable"), UBodyRegionPhysicsLib::FindProfile(EBodyPart::Torso) == nullptr);
	// 80 kg reference: torso ~40 kg, arm ~4 kg.
	TestTrue(TEXT("torso mass"), FMath::IsNearlyEqual(
		UBodyRegionPhysicsLib::RegionMassKg(EBodyPart::Torso, 80.f), 40.f, 0.05f));
	TestTrue(TEXT("arm mass"), FMath::IsNearlyEqual(
		UBodyRegionPhysicsLib::RegionMassKg(EBodyPart::ArmL, 80.f), 4.f, 0.05f));
	TestEqual(TEXT("zero body mass"), UBodyRegionPhysicsLib::RegionMassKg(EBodyPart::Head, 0.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyRegionMathTest, "Afterfall.BodyPhysics.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBodyRegionMathTest::RunTest(const FString& P)
{
	// Lighter region gains more velocity from the same impulse (no teleporting).
	const float ArmV = UBodyRegionPhysicsLib::ImpulseVelocity(100.f, EBodyPart::ArmL, 80.f);
	const float TorsoV = UBodyRegionPhysicsLib::ImpulseVelocity(100.f, EBodyPart::Torso, 80.f);
	TestTrue(TEXT("arm moves more than torso"), ArmV > TorsoV);
	TestEqual(TEXT("zero impulse"), UBodyRegionPhysicsLib::ImpulseVelocity(0.f, EBodyPart::Torso, 80.f), 0.f);
	// Heavy fast torso breaks glass; slow brush does not.
	TestTrue(TEXT("torso at 5 m/s breaks"),
		UBodyRegionPhysicsLib::CanBreakGlass(EBodyPart::Torso, 80.f, 500.f));
	TestFalse(TEXT("torso at 0.5 m/s safe"),
		UBodyRegionPhysicsLib::CanBreakGlass(EBodyPart::Torso, 80.f, 50.f));
	TestFalse(TEXT("zero speed safe"),
		UBodyRegionPhysicsLib::CanBreakGlass(EBodyPart::Head, 80.f, 0.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
