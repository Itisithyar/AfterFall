// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallGameMode.h"
#include "Afterfall.h"
#include "AfterfallCharacter.h"
#include "World/PhysicalItem.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Building/ProceduralBuilding.h"

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

void AAfterfallGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnAuthorizedTestBox();
	if (bSpawnTestBuilding)
	{
		UWorld* W = GetWorld();
		if (W)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AProceduralBuilding* B = W->SpawnActor<AProceduralBuilding>(AProceduralBuilding::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
			if (B) UE_LOG(LogAfterfall, Log, TEXT("Spawned test procedural building"));
		}
	}
}

void AAfterfallGameMode::SpawnAuthorizedTestBox()
{
	// Server only — a client GameMode never exists, but guard anyway.
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client)
	{
		return;
	}
	// Exactly once per session.
	if (bSpawnedTestBox)
	{
		return;
	}
	// Authorized scope: ONE map only. Every other map is untouched, so this
	// cannot leak a stray prop into future levels.
	const FString MapName = W->GetMapName();
	if (!MapName.Contains(TEXT("Lvl_FirstPerson")))
	{
		return;
	}
	bSpawnedTestBox = true;

	// Beside the player start so it is visible and walkable-to immediately;
	// 50 cm above the floor, then the spawn adjust finds the surface.
	const APlayerStart* Start = Cast<APlayerStart>(
		UGameplayStatics::GetActorOfClass(W, APlayerStart::StaticClass()));
	const FVector Anchor = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	const FVector SpawnAt = Anchor + FVector(120.f, 80.f, 50.f);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APhysicalItem* Box = W->SpawnActor<APhysicalItem>(APhysicalItem::StaticClass(),
		SpawnAt, FRotator::ZeroRotator, P);
	if (!Box)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("kilo-box-001: test box spawn FAILED at %s"),
			*SpawnAt.ToString());
		return;
	}

	// Light by spec (~2-5 kg): full throw speed, no heavy-carry cap, so the
	// load-to-movement path is exercised without tripping the heavy threshold.
	Box->ItemType = TEXT("AF_R_Scrap");
	Box->MassKg = 3.f;
	Box->ItemId = FGuid::NewGuid();
	// Keep the simulated body in step with the gameplay mass: the ctor already
	// applied the DEFAULT MassKg, so re-assert it after the assignment above.
	// (Load accounting reads MassKg directly; physics must agree with it.)
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Box->GetRootComponent()))
	{
		Body->SetMassOverrideInKg(NAME_None, Box->MassKg, true);
	}

	UE_LOG(LogAfterfall, Log, TEXT("kilo-box-001: one test box at %s (%.1f kg, item %s)"),
		*Box->GetActorLocation().ToString(), Box->MassKg, *Box->ItemId.ToString());
}