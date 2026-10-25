// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (GameMode) — server-authoritative session entry point.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "AfterfallGameMode.generated.h"

UCLASS()
class AFTERFALL_API AAfterfallGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AAfterfallGameMode();
	virtual void BeginPlay() override;

	// Server-only: canonical seed for deterministic region generation.
	// Replicated to clients via GameState in later milestone (Phase 4).
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Region")
	int32 RegionSeed = 1337;

protected:
	UPROPERTY(EditAnywhere, Category="Afterfall|Test")
	bool bSpawnTestBuilding = false;

private:
	/**
	 * kilo-box-001 (authorized 2026-10-24): exactly ONE small, light,
	 * pickable box near the player start — and only on Lvl_FirstPerson.
	 * Purely runtime: the map asset, config and every other map are
	 * untouched. Guarded so a server session can never spawn a second one.
	 */
	void SpawnAuthorizedTestBox();
	void DeferredLightCheck();

	bool bSpawnedTestBox = false;
};