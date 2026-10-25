// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Input) — pure arbitration tests for Mouse-1.
// Drives the three-way race (click attacks, double-click rotates, 0.33 s hold
// opens the wheel) as a sequence of elapsed-time events: no world, no device.
#include "Misc/AutomationTest.h"
#include "Interaction/ClickArbitration.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClickWheelThresholdTest,
	"Afterfall.Interact.Mouse.WheelThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClickWheelThresholdTest::RunTest(const FString& P)
{
	const double Tr = 0.33; // spec: 330 ms
	TestFalse(TEXT("0.00s: no wheel"), AFClick::ShouldOpenWheel(0.00, Tr));
	TestFalse(TEXT("0.32s: no wheel"), AFClick::ShouldOpenWheel(0.32, Tr));
	TestTrue(TEXT("0.33s: wheel opens"), AFClick::ShouldOpenWheel(0.33, Tr));
	TestTrue(TEXT("1.00s: wheel stays open"), AFClick::ShouldOpenWheel(1.00, Tr));

	// A wheel release must never fall through to an attack.
	TestFalse(TEXT("long hold does not attack"), AFClick::ReleaseIsClick(0.50, Tr, true));
	TestTrue(TEXT("short hold is attack-eligible"), AFClick::ReleaseIsClick(0.10, Tr, false));
	TestFalse(TEXT("release at threshold is not a click"), AFClick::ReleaseIsClick(0.33, Tr, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClickDoublePressTest,
	"Afterfall.Interact.Mouse.DoublePress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClickDoublePressTest::RunTest(const FString& P)
{
	const double Window = 0.30;
	const double Resolve = 0.28;

	TestFalse(TEXT("no previous release: never a double click"),
		AFClick::IsDoubleClick(10.0, 0.0, Window, Resolve));

	// Release at 1.00 s, second press 0.10 s later: rotation takes over.
	TestTrue(TEXT("press 0.10s after release is the second click"),
		AFClick::IsDoubleClick(1.10, 1.00, Window, Resolve));

	// Past both windows: a fresh first click, not a double.
	TestFalse(TEXT("press 0.60s later is a new first click"),
		AFClick::IsDoubleClick(1.60, 1.00, Window, Resolve));
	TestTrue(TEXT("fresh press resolves to an attack"),
		AFClick::ResolvesToAttack(true, false, false));

	// State gates: rotation or an open wheel suppress the deferred attack.
	TestFalse(TEXT("rotating suppresses the attack"),
		AFClick::ResolvesToAttack(true, false, true));
	TestFalse(TEXT("open wheel suppresses the attack"),
		AFClick::ResolvesToAttack(true, true, false));
	TestFalse(TEXT("no pending click: nothing fires"),
		AFClick::ResolvesToAttack(false, false, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClickGrabThresholdTest,
	"Afterfall.Interact.Mouse.GrabThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClickGrabThresholdTest::RunTest(const FString& P)
{
	// RMB grab threshold is 230 ms, expressed in elapsed time like the wheel.
	const double Tg = 0.23;
	TestFalse(TEXT("0.10s hold does not grab"), AFClick::ShouldOpenWheel(0.10, Tg));
	TestTrue(TEXT("0.23s hold reaches the grab threshold"), AFClick::ShouldOpenWheel(0.23, Tg));
	return true;
}

#endif
