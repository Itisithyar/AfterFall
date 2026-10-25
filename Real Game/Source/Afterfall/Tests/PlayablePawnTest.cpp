#include "Misc/AutomationTest.h"
#include "AfterfallGameMode.h"
#include "AfterfallCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayablePawnConfigTest, "Afterfall.Playable.ConfigGameMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayablePawnConfigTest::RunTest(const FString& P)
{
    FString Ini;
    FFileHelper::LoadFileToString(Ini, *(FPaths::ProjectConfigDir() + TEXT("DefaultEngine.ini")));
    TestTrue(TEXT("default map contains Lvl_FirstPerson"), Ini.Contains(TEXT("/Game/FirstPerson/Lvl_FirstPerson")));
    TestTrue(TEXT("default map contains game option"), Ini.Contains(TEXT("?game=/Script/Afterfall.AfterfallGameMode")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayablePawnResolveTest, "Afterfall.Playable.ResolveGameMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayablePawnResolveTest::RunTest(const FString& P)
{
    FString Ini;
    FFileHelper::LoadFileToString(Ini, *(FPaths::ProjectConfigDir() + TEXT("DefaultEngine.ini")));
    FString Opt = UGameplayStatics::ParseOption(Ini, TEXT("game"));
    // fallback: extract option manually
    int32 Idx = Ini.Find(TEXT("game="));
    if (Idx != INDEX_NONE) Opt = Ini.Mid(Idx + 5);
    int32 End = Opt.Find(TEXT("\n"));
    if (End != INDEX_NONE) Opt = Opt.Left(End);
    Opt.TrimStartAndEndInline();
    if (!Opt.StartsWith(TEXT("Class'")))
    {
        Opt = TEXT("Class'") + Opt + TEXT("'");
    }
    UClass* GM = StaticLoadClass(AAfterfallGameMode::StaticClass(), nullptr, *Opt);
    TestNotNull(TEXT("game option resolves to AfterfallGameMode"), GM);
    if (GM)
    {
        AAfterfallGameMode* CDO = Cast<AAfterfallGameMode>(GM->GetDefaultObject());
        TestNotNull(TEXT("game mode cdo"), CDO);
        if (CDO)
        {
            TestTrue(TEXT("default pawn is AfterfallCharacter"), CDO->DefaultPawnClass.Get() == AAfterfallCharacter::StaticClass());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayablePawnPlayerStartTest, "Afterfall.Playable.PlayerStartCreated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayablePawnPlayerStartTest::RunTest(const FString& P)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("test world created"), World);
    if (!World) return false;
    AAfterfallGameMode* GM = World->SpawnActor<AAfterfallGameMode>();
    TestNotNull(TEXT("game mode spawned"), GM);
    APlayerController* PC = World->SpawnActor<APlayerController>();
    AActor* Start = GM ? GM->ChoosePlayerStart(PC) : nullptr;
    TestNotNull(TEXT("player start created"), Start);
    World->DestroyWorld(false);
    return true;
}

#endif
// possession is verified only by the packaged smoke test, not by these tests.