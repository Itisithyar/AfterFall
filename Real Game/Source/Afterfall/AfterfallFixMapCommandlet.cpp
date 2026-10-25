#include "AfterfallFixMapCommandlet.h"

#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "AfterfallGameMode.h"

UAfterfallFixMapCommandlet::UAfterfallFixMapCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UAfterfallFixMapCommandlet::Main(const FString& Params)
{
    const FString MapPath = TEXT("/Game/FirstPerson/Lvl_FirstPerson");
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("FIXMAP: failed to load map %s"), *MapPath);
        return 1;
    }
    AWorldSettings* WS = World->GetWorldSettings();
    UClass* Before = WS->DefaultGameMode;
    WS->DefaultGameMode = AAfterfallGameMode::StaticClass();
    WS->MarkPackageDirty();
    const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(World, MapPath);
    UE_LOG(LogTemp, Log, TEXT("FIXMAP: before=%s after=%s saved=%s"),
        Before ? *Before->GetName() : TEXT("null"),
        WS->DefaultGameMode ? *WS->DefaultGameMode->GetName() : TEXT("null"),
        bSaved ? TEXT("true") : TEXT("false"));
    return bSaved ? 0 : 2;
}
#else
UAfterfallFixMapCommandlet::UAfterfallFixMapCommandlet() {}
int32 UAfterfallFixMapCommandlet::Main(const FString& Params) { return 0; }
#endif
