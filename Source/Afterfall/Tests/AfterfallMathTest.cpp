#include "Misc/AutomationTest.h"
#include "Math/AfterfallMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAfterfallMathQuadratic, "Afterfall.Math.Quadratic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAfterfallMathQuadratic::RunTest(const FString& P)
{
	using namespace Afterfall::Math;
	TestEqual(TEXT("roots 2"), SolveQuadratic(1, -3, 2).Count, 2);
	TestTrue(TEXT("root values"), FMath::IsNearlyEqual(SolveQuadratic(1,-3,2).Roots[0],1.f) && FMath::IsNearlyEqual(SolveQuadratic(1,-3,2).Roots[1],2.f));
	TestEqual(TEXT("degenerate count 1"), SolveQuadratic(0,2,4).Count, 1);
	TestEqual(TEXT("no real roots"), SolveQuadratic(1,0,1).Count, 0);
	TestEqual(TEXT("nonfinite"), SolveQuadratic(NAN,1,1).Count, 0);
	TestTrue(TEXT("eval"), FMath::IsNearlyEqual(EvalQuadratic(1,2,3,2),11.f));
	TestTrue(TEXT("deriv"), FMath::IsNearlyEqual(Derivative(1,2,3),8.f));
	TestTrue(TEXT("vertex"), FMath::IsNearlyEqual(VertexX(1,4),-2.f));
	TestTrue(TEXT("unity roundtrip"), Afterfall::Math::ToUnreal(Afterfall::Math::ToUnity(FVector(1,2,3))) == FVector(1,2,3));
	return true;
}
