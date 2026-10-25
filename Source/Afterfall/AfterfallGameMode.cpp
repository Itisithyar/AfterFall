#include "AfterfallGameMode.h"
#include "Afterfall.h"
#include "AfterfallCharacter.h"
#include "World/PhysicalItem.h"
#include "GameFramework/PlayerStart.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Building/ProceduralTerrain.h"

AAfterfallGameMode::AAfterfallGameMode()
{
	DefaultPawnClass = AAfterfallCharacter::StaticClass();
	UE_LOG(LogAfterfall, Log, TEXT("AfterfallGameMode created (Seed=%d) defaultPawn=%s"),
		RegionSeed, DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("null"));
}

void AAfterfallGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnAuthorizedTestBox();
	GetWorldTimerManager().SetTimerForNextTick(this, &AAfterfallGameMode::DeferredLightCheck);
	if (bSpawnTestTerrain)
	{
		UWorld* W = GetWorld();
		if (W)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			W->SpawnActor<AProceduralTerrain>(AProceduralTerrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
	}
}

void AAfterfallGameMode::DeferredLightCheck()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client) return;
	for (TActorIterator<AActor> It(W); It; ++It)
		if (It->ActorHasTag(FName("AfterfallFallbackLight"))) return;
	int32 Count = 0;
	for (TActorIterator<ADirectionalLight> It(W); It; ++It)
	{
		if (!It->IsHidden())
		{
			if (UDirectionalLightComponent* C = It->GetComponentByClass<UDirectionalLightComponent>())
				if (C->Intensity > 0.1f) Count++;
		}
	}
	if (Count == 0)
	{
		ADirectionalLight* L = W->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-45.f,0.f,0.f));
		if (L) { L->SetMobility(EComponentMobility::Movable); L->Tags.Add(FName("AfterfallFallbackLight")); UE_LOG(LogTemp, Warning, TEXT("[Afterfall] No lights found - fallback spawned.")); }
		ASkyLight* S = W->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (S) { S->GetLightComponent()->SetMobility(EComponentMobility::Movable); S->Tags.Add(FName("AfterfallFallbackLight")); UE_LOG(LogTemp, Log, TEXT("[Afterfall] SkyLight fallback spawned.")); }
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Afterfall] Found %d directional light(s), fallback skipped."), Count);
	}
}

void AAfterfallGameMode::SpawnAuthorizedTestBox()
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client) return;
	if (bSpawnedTestBox) return;
	const FString MapName = W->GetMapName();
	if (!MapName.Contains(TEXT("Lvl_FirstPerson"))) return;
	bSpawnedTestBox = true;
	const APlayerStart* Start = Cast<APlayerStart>(UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()));
	const FVector Anchor = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	const FVector SpawnAt = Anchor + FVector(120.f, 80.f, 50.f);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APhysicalItem* Box = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(), SpawnAt, FRotator::ZeroRotator, P);
	if (!Box) { UE_LOG(LogAfterfall, Warning, TEXT("kilo-box-001: test box spawn FAILED at %s"), *SpawnAt.ToString()); return; }
	Box->ItemType = TEXT("AF_R_Scrap");
	Box->MassKg = 3.f;
	Box->ItemId = FGuid::NewGuid();
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Box->GetRootComponent()))
	{
		Body->SetMassOverrideInKg(NAME_None, Box->MassKg, true);
	}
	UE_LOG(LogAfterfall, Log, TEXT("kilo-box-001: one test box at %s (%.1f kg, item %s)"), *Box->GetActorLocation().ToString(), Box->MassKg, *Box->ItemId.ToString());
}