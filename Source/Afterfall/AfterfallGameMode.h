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

	// Server-only: canonical seed for deterministic region generation.
	// Replicated to clients via GameState in later milestone (Phase 4).
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Region")
	int32 RegionSeed = 1337;
};
