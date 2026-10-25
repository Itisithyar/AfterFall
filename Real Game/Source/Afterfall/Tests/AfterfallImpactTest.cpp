#include "Misc/AutomationTest.h"
#include "Impact/AfterfallImpact.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAfterfallImpactCommit, "Afterfall.Impact.Commit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAfterfallImpactCommit::RunTest(const FString& Parameters)
{
    using namespace Afterfall;
    std::set<long long> Committed;
    auto R1 = CommitImpact(Committed, 1, 10, 1, 0.5, 25);
    TestTrue(TEXT("first applies"), R1.bApplied);
    TestEqual(TEXT("damage"), R1.Damage, 1.0, 1e-9);
    auto R2 = CommitImpact(Committed, 1, 10, 1, 0.5, 25);
    TestFalse(TEXT("second skipped"), R2.bApplied);
    TestEqual(TEXT("second damage 0"), R2.Damage, 0.0, 1e-9);
    auto R3 = CommitImpact(Committed, 2, 10, 1, 0.5, 0);
    TestFalse(TEXT("resistance 0 rejected"), R3.bApplied);
    return true;
}
#endif