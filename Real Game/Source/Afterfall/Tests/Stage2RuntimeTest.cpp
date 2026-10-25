// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Tests) — mirrors Scripts/validate_stage2.py.
#include "Misc/AutomationTest.h"
#include "Movement/AfterfallMovementComponent.h"
#include "World/PhysicalItem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedStepTest, "Afterfall.Stage2.FixedStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFixedStepTest::RunTest(const FString& P)
{
	auto TotalStepsFor = [](double Dt, int32 Frames)
	{
		double Acc = 0.0;
		int32 Total = 0;
		for (int32 i = 0; i < Frames; ++i)
		{
			Total += UAfterfallMovementComponent::ConsumeFixedSteps(Acc, Dt, 1.0 / 120.0, 64);
		}
		return Total;
	};
	TestEqual(TEXT("30fps -> 600 quanta/5s"), TotalStepsFor(1.0 / 30.0, 150), 600);
	TestEqual(TEXT("60fps -> 600 quanta/5s"), TotalStepsFor(1.0 / 60.0, 300), 600);
	TestEqual(TEXT("240fps -> 600 quanta/5s"), TotalStepsFor(1.0 / 240.0, 1200), 600);

	double Acc = 0.0;
	TestEqual(TEXT("zero max steps -> 0"), UAfterfallMovementComponent::ConsumeFixedSteps(Acc, 1.0, 1.0 / 120.0, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSprintTimingTest, "Afterfall.Stage2.SprintTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSprintTimingTest::RunTest(const FString& P)
{
	auto BurnSprint = [](double Dt)
	{
		EAfterfallSprintState St = EAfterfallSprintState::Ready;
		float TL = 0.f, RL = 0.f;
		UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, 0.f);
		int32 Frames = 0;
		while (St == EAfterfallSprintState::Sprinting && Frames < 100000)
		{
			UAfterfallMovementComponent::AdvanceSprint(St, TL, RL, true, 5.f, 3.f, (float)Dt);
			++Frames;
		}
		return TPair<int32, EAfterfallSprintState>(Frames, St);
	};
	const TPair<int32, EAfterfallSprintState> R30 = BurnSprint(1.0 / 30.0);
	const TPair<int32, EAfterfallSprintState> R240 = BurnSprint(1.0 / 240.0);
	TestTrue(TEXT("30fps spends sprint"), R30.Value == EAfterfallSprintState::Recovering);
	TestTrue(TEXT("240fps spends sprint"), R240.Value == EAfterfallSprintState::Recovering);
	TestTrue(TEXT("30fps ~150 frames"), R30.Key >= 149 && R30.Key <= 152);
	TestTrue(TEXT("240fps ~1200 frames"), R240.Key >= 1195 && R240.Key <= 1205);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThrowModelTest, "Afterfall.Stage2.ThrowModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThrowModelTest::RunTest(const FString& P)
{
	const FVector V = APhysicalItem::ComputeReleaseVelocity(
		FVector(300.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 900.f, 1.f);
	TestTrue(TEXT("inherits player velocity"), V.Equals(FVector(1200.f, 0.f, 0.f), 0.01f));
	const FVector V0 = APhysicalItem::ComputeReleaseVelocity(
		FVector(300.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), 900.f, 0.f);
	TestTrue(TEXT("zero inherit drops player term"), V0.Equals(FVector(0.f, 900.f, 0.f), 0.01f));
	return true;
}

#endif
