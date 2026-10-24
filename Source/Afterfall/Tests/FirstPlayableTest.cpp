// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Misc/AutomationTest.h"
#include "World/ResourceNode.h"
#include "AI/ZombiePrototype.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstPlayableYieldsTest, "Afterfall.Slice.Yields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFirstPlayableYieldsTest::RunTest(const FString& P)
{
	TestEqual(TEXT("wood 3-5"), AResourceNode::YieldFor(EResourceKind::Wood, 0.999f), 5);
	TestEqual(TEXT("stone 4-8"), AResourceNode::YieldFor(EResourceKind::Stone, 0.999f), 8);
	TestEqual(TEXT("hemp 2-4"), AResourceNode::YieldFor(EResourceKind::Hemp, 0.999f), 4);
	TestTrue(TEXT("min bounds"), AResourceNode::YieldFor(EResourceKind::Wood, 0.f) >= 3);
	return true;
}

#endif
