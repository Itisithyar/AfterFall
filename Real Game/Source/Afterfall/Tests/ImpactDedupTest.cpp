#include "Misc/AutomationTest.h"
#include "Impact/ImpactDedup.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAfterfallImpactDedupTest, "Afterfall.Impact.Dedup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAfterfallImpactDedupTest::RunTest(const FString& Parameters)
{
    using Afterfall::FImpactDedup;
    {
        FImpactDedup D(0.25);
        int Applied = 0;
        for (int Block = 0; Block < 5; ++Block) Applied += D.TryCommit(7, 100, 1.000 + Block * 0.001);
        TestEqual(TEXT("5 callbacks, 1 applied"), Applied, 1);
    }
    {
        FImpactDedup D(0.25);
        int Applied = D.TryCommit(7, 100, 0.099) + D.TryCommit(7, 100, 0.101);
        TestEqual(TEXT("straddling boundary, 1 applied"), Applied, 1);
    }
    {
        FImpactDedup D(0.25);
        TestTrue(TEXT("body A applies"), D.TryCommit(7, 100, 1.0));
        TestTrue(TEXT("body B applies"), D.TryCommit(8, 100, 1.0));
    }
    {
        FImpactDedup D(0.25);
        TestTrue(TEXT("wall A applies"), D.TryCommit(7, 100, 1.0));
        TestTrue(TEXT("wall B applies"), D.TryCommit(7, 101, 1.0));
    }
    {
        FImpactDedup D(0.25);
        TestTrue(TEXT("first"), D.TryCommit(7, 100, 1.0));
        TestFalse(TEXT("within cooldown"), D.TryCommit(7, 100, 1.1));
        TestTrue(TEXT("after cooldown"), D.TryCommit(7, 100, 1.3));
    }
    {
        FImpactDedup D(0.25, 8);
        for (uint32_t i = 0; i < 100; ++i) D.TryCommit(i, 100, 1.0);
        TestTrue(TEXT("size capped"), D.Size() <= 8);
    }
    return true;
}
#endif