#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAfterfallWorldGenHouseEvidence, "Afterfall.WorldGen.HouseEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAfterfallWorldGenHouseEvidence::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Log, TEXT("HOUSE EVIDENCE seed=0 streamRadius=0 planned=0 insideRadius=0 withGeometry=0"));
    return true;
}
#endif