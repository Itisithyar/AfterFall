#include "Misc/AutomationTest.h"
#include "Math/AfterfallMath.h"
using namespace Afterfall::Math;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeomMerge, "Afterfall.Math.Geom", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGeomMerge::RunTest(const FString& P)
{
	FVector a0(0,0,0), a1(4,0,0), a2(2,1.5f,0);
	FVector b0(0,0,0), b1(4,0,0), b2(2,-1.5f,0);
	TestTrue(TEXT("merge rhombus"), CanMergeTriangles(a0,a1,a2,b0,b1,b2));
	FQuad Q; TestTrue(TEXT("mergeout"), MergeTrianglesToQuad(a0,a1,a2,b0,b1,b2,Q));
	TestTrue(TEXT("rhombus"), IsRhombus(Q));
	// apex moved
	FVector a2b(2,1.8f,0);
	TestTrue(TEXT("merge2"), CanMergeTriangles(a0,a1,a2b,b0,b1,b2));
	FQuad Q2; MergeTrianglesToQuad(a0,a1,a2b,b0,b1,b2,Q2);
	TestFalse(TEXT("not rhombus"), IsRhombus(Q2));
	// concave
	TestFalse(TEXT("concave"), CanMergeTriangles(FVector(0,0,0),FVector(4,0,0),FVector(-3,1,0), FVector(0,0,0),FVector(4,0,0),FVector(-3,-1,0)));
	// same side
	TestFalse(TEXT("same side"), CanMergeTriangles(FVector(0,0,0),FVector(4,0,0),FVector(2,1,0), FVector(0,0,0),FVector(4,0,0),FVector(2,2,0)));
	// non-coplanar
	TestFalse(TEXT("noncoplanar"), CanMergeTriangles(FVector(0,0,0),FVector(4,0,0),FVector(2,1,0), FVector(0,0,0),FVector(4,0,0),FVector(2,-1,0.5f)));
	// no shared edge
	TestFalse(TEXT("no shared"), CanMergeTriangles(FVector(0,0,0),FVector(1,0,0),FVector(0,1,0), FVector(5,5,0),FVector(6,5,0),FVector(5,6,0)));
	return true;
}
