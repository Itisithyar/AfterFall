// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Playable pawn) — guards the packaged-game no-pawn
// regression. Lvl_FirstPerson must resolve its GameMode to
// AAfterfallGameMode (whose DefaultPawnClass is AAfterfallCharacter) and must
// contain a PlayerStart, or the engine has nowhere to spawn the pawn (smoke
// log: "Couldn't spawn Pawn of type BP_FirstPersonCharacter_C" +
// "NO PLAYERSTART").
//
// NOTE: engine possession itself (SpawnDefaultPawnAtTransform + Possess) only
// happens in a live session, so this test asserts every precondition the
// engine needs: the map loads, the GameMode resolves to AfterfallGameMode
// (explicit override, else the project default), at least one PlayerStart
// exists in the map's external-actor packages, and the resolved pawn class
// for a controller is the mechanics character.
#include "Misc/AutomationTest.h"
#include "AfterfallGameMode.h"
#include "AfterfallCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayablePawnMapTest, "Afterfall.Playable.PawnSpawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayablePawnMapTest::RunTest(const FString& P)
{
	// 1. Map loads.
	const FSoftObjectPath MapPath(TEXT("/Game/FirstPerson/Lvl_FirstPerson.Lvl_FirstPerson"));
	UWorld* World = Cast<UWorld>(MapPath.TryLoad());
	if (!TestNotNull(TEXT("map loads"), World))
	{
		return false;
	}

	// 2. GameMode resolves to AfterfallGameMode.
	const AWorldSettings* WS = World->GetWorldSettings();
	if (!TestNotNull(TEXT("world settings"), WS))
	{
		return false;
	}
	UClass* Override = WS->DefaultGameMode.Get();
	if (Override)
	{
		TestEqual(TEXT("map override is AfterfallGameMode"),
			Override, AAfterfallGameMode::StaticClass());
	}
	else
	{
		// No override: the project default must be AfterfallGameMode.
		FString ProjGM;
		GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"),
			TEXT("GlobalDefaultGameMode"), ProjGM, GGameIni);
		TestTrue(TEXT("project default is AfterfallGameMode"),
			ProjGM.Contains(TEXT("AfterfallGameMode")));
	}

	// 3. The resolved pawn class is the mechanics character (this is what the
	// engine spawns at the PlayerStart and possesses).
	const AAfterfallGameMode* GMCDO = GetDefault<AAfterfallGameMode>();
	if (!TestNotNull(TEXT("game mode CDO"), GMCDO))
	{
		return false;
	}
	TestTrue(TEXT("default pawn is AfterfallCharacter"),
		GMCDO->DefaultPawnClass.Get() == AAfterfallCharacter::StaticClass());
	TestTrue(TEXT("resolved pawn for controller is AfterfallCharacter"),
		const_cast<AAfterfallGameMode*>(GMCDO)->GetDefaultPawnClassForController(nullptr) == AAfterfallCharacter::StaticClass());

	// 4. A PlayerStart exists. World-Partitioned actors stream, so scan the
	// map's external-actor packages on disk (deterministic, no registry).
	TArray<AActor*> InMem;
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), InMem);
	int32 StartCount = InMem.Num();
	FString StartLoc;
	if (StartCount > 0)
	{
		StartLoc = InMem[0]->GetActorLocation().ToString();
	}
	else
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files,
			*FPaths::ProjectContentDir(), TEXT("*.uasset"), true, false);
		for (const FString& File : Files)
		{
			if (!File.Contains(TEXT("__ExternalActors__/FirstPerson/Lvl_FirstPerson")))
			{
				continue;
			}
			FString Pkg = File;
			FPaths::MakePathRelativeTo(Pkg, *FPaths::ProjectContentDir());
			Pkg.RemoveFromEnd(TEXT(".uasset"));
			const FString PkgName = TEXT("/Game/") + Pkg.Replace(TEXT("\\"), TEXT("/"));
			if (UPackage* Pk = LoadPackage(nullptr, *PkgName, LOAD_None))
			{
				TArray<UObject*> Objs;
				GetObjectsWithOuter(Pk, Objs);
				for (UObject* O : Objs)
				{
					if (APlayerStart* PS = Cast<APlayerStart>(O))
					{
						++StartCount;
						StartLoc = PS->GetActorLocation().ToString();
					}
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("PLAYABLEPAWN map=%s gamemode=%s playerStarts=%d loc=%s"),
		*MapPath.ToString(), Override ? *Override->GetName() : TEXT("<project-default>"),
		StartCount, *StartLoc);
	TestTrue(TEXT("map has a PlayerStart"), StartCount > 0);
	return true;
}

#endif
