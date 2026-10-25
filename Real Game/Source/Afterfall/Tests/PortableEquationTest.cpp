#include "Misc/AutomationTest.h"
#include "Math/PortableEquation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortableEqRoundTrip, "Afterfall.Math.PortableEquation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPortableEqRoundTrip::RunTest(const FString& P)
{
FPortableEquation E; E.A=1; E.B=2; E.C=3;
	FString J = Afterfall::Portable::ToJson(E);
	TestTrue(TEXT("json contains version"), J.Contains(TEXT("\"version\":1")));
	return true;
}
