// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallGameMode.h"
#include "Afterfall.h"
#include "AfterfallCharacter.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"

static TAutoConsoleVariable<int32> CVarAutoLight(TEXT("af.AutoLight"), 1, TEXT("Auto light fallback"), ECVF_Default);

void AAfterfallGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (CVarAutoLight.GetValueOnGameThread() == 0) return;
	UWorld* W = GetWorld();
	if (!W) return;
	bool bHasDir=false, bHasSky=false, bHasAtm=false, bHasPP=false;
	for (TActorIterator<ADirectionalLight> It(W); It; ++It) { bHasDir=true; break; }
	for (TActorIterator<ASkyLight> It(W); It; ++It) { bHasSky=true; break; }
	for (TActorIterator<AActor> It(W); It; ++It) { if (It->FindComponentByClass<USkyAtmosphereComponent>()) { bHasAtm=true; break; } }
	for (TActorIterator<APostProcessVolume> It(W); It; ++It) { if (It->bUnbound) { bHasPP=true; break; } }
	if (!bHasDir)
	{
		ADirectionalLight* L = W->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-50.f,0.f,0.f));
		L->SetMobility(EComponentMobility::Movable);
		UE_LOG(LogAfterfall, Log, TEXT("AutoLight: spawned DirectionalLight"));
	}
	else UE_LOG(LogAfterfall, Log, TEXT("AutoLight: DirectionalLight already present"));
	if (!bHasSky)
	{
		ASkyLight* S = W->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
		S->GetLightComponent()->RecaptureSky();
		UE_LOG(LogAfterfall, Log, TEXT("AutoLight: spawned SkyLight"));
	}
	else UE_LOG(LogAfterfall, Log, TEXT("AutoLight: SkyLight already present"));
	if (!bHasAtm)
	{
		AActor* A = W->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		A->AddComponentByClass(USkyAtmosphereComponent::StaticClass(), false, FTransform::Identity, false);
		UE_LOG(LogAfterfall, Log, TEXT("AutoLight: spawned SkyAtmosphere"));
	}
	else UE_LOG(LogAfterfall, Log, TEXT("AutoLight: SkyAtmosphere already present"));
	if (!bHasPP)
	{
		APostProcessVolume* P = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator);
		P->bUnbound = true;
		P->Settings.bOverride_AutoExposureMinBrightness = true;
		P->Settings.bOverride_AutoExposureMaxBrightness = true;
		P->Settings.AutoExposureMinBrightness = 1.f;
		P->Settings.AutoExposureMaxBrightness = 1.f;
		UE_LOG(LogAfterfall, Log, TEXT("AutoLight: spawned PostProcessVolume"));
	}
	else UE_LOG(LogAfterfall, Log, TEXT("AutoLight: PostProcessVolume already present"));
}

AAfterfallGameMode::AAfterfallGameMode()
{
	// The player is the mechanics character. Movement, Enhanced Input, health,
	// inventory, carry, mat interaction, grapple and the menus all live on
	// AAfterfallCharacter; without this the map's template pawn is used and every
	// one of those systems is bypassed (the spawn path only ran from the slice
	// subsystem, respawn and the physics lab).
	DefaultPawnClass = AAfterfallCharacter::StaticClass();

	// Dedicated server is authoritative: bUseSeamlessTravel left default for now.
	UE_LOG(LogAfterfall, Log, TEXT("AfterfallGameMode created (Seed=%d) defaultPawn=%s"),
		RegionSeed, DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("null"));
}
