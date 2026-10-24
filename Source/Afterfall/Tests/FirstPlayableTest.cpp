// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Misc/AutomationTest.h"
#include "World/ResourceNode.h"
#include "AI/ZombiePrototype.h"
#include "AfterfallGameMode.h"
#include "AfterfallCharacter.h"

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

// The baseline the rest of the game assumes: the session's pawn is the object
// that owns movement, input, health, inventory and grapple. A compile cannot
// prove this — only the class default can.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaselinePawnTest, "Afterfall.Baseline.PawnIsMechanicsCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBaselinePawnTest::RunTest(const FString& P)
{
	const AAfterfallGameMode* GameModeCDO = GetDefault<AAfterfallGameMode>();
	if (!TestNotNull(TEXT("game mode CDO"), GameModeCDO))
	{
		return false;
	}
	TestNotNull(TEXT("default pawn class is set"), GameModeCDO->DefaultPawnClass.Get());
	TestEqual(TEXT("default pawn is the mechanics character"),
		GameModeCDO->DefaultPawnClass.Get(), AAfterfallCharacter::StaticClass());
	// Guard against the template pawn silently being reintroduced via config.
	TestTrue(TEXT("mechanics character derives from APawn"),
		AAfterfallCharacter::StaticClass()->IsChildOf(APawn::StaticClass()));
	return true;
}

#endif
