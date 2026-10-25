#include "Misc/AutomationTest.h"
#include "Math/AfterfallEquations.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAfterfallEquationsTest, "Afterfall.Equations.Math",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAfterfallEquationsTest::RunTest(const FString& Parameters)
{
    using namespace Afterfall;
    const double Tol = 1e-9;
    TestEqual(TEXT("RhombusFaces"), Eval(RhombusFaces, 1), 1.0, Tol);
    TestEqual(TEXT("RhombusEdges"), Eval(RhombusEdges, 1), 4.0, Tol);
    TestEqual(TEXT("RhombusVerts"), Eval(RhombusVerts, 1), 4.0, Tol);
    TestEqual(TEXT("ImpactEnergy"), Eval(ImpactEnergy, 10), 50.0, Tol);
    TestEqual(TEXT("FragmentImpulse(0)"), Eval(FragmentImpulse, 0), 1.0, Tol);
    TestEqual(TEXT("FragmentImpulse(1)"), Eval(FragmentImpulse, 1), 0.0, Tol);
    return true;
}
#endif