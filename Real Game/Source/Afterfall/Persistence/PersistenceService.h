// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Persistence) — pure, unit-tested helpers. No IO here.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PersistenceService.generated.h"

UCLASS()
class AFTERFALL_API UPersistenceService : public UObject
{
	GENERATED_BODY()

public:
	// Stable entity Guid from seed chain. Must match LootService::MakeRollKey family.
	static FGuid MakeStableGuid(int32 RegionSeed, int32 GeneratorVersion, const FString& TypeAndIndex);

	// Save compat: major versions must match exactly; older minor migrates, newer rejects.
	static bool IsSaveCompatible(int32 SaveVersion, int32 CodeVersion, bool& bOutNeedsMigration);

	// Dirty-region tracking: mark cell dirty when a delta inside it changes.
	static FString CellKeyForLocation(const FVector& Loc, float CellSize);

	// Tombstone rule: stale-version or consumed unique rolls never resurrect.
	static bool ShouldTombstone(bool bTaken, bool bVersionStale, bool bUniqueConsumed);

	// Catch-up: progress made while dormant = elapsed * rate, clamped to cap.
	static float CatchUpProgress(float ElapsedSec, float RatePerSec, float Cap);
};
