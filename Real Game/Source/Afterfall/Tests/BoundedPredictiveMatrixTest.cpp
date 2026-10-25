#include "Misc/AutomationTest.h"
#include "Prediction/BoundedPredictiveMatrix.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoundedCandidateCountTest, "Afterfall.Prediction.BoundedCandidateCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBoundedCandidateCountTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("clamps 5 to 3"), Afterfall::ClampCandidateCount(5), 3);
	TestEqual(TEXT("keeps 2"), Afterfall::ClampCandidateCount(2), 2);
	TestEqual(TEXT("clamps negative"), Afterfall::ClampCandidateCount(-1), 0);
	return true;
}
