// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Persistence/PersistenceService.h"
#include "Misc/Guid.h"
#include "Afterfall.h"

FGuid UPersistenceService::MakeStableGuid(int32 RegionSeed, int32 GeneratorVersion, const FString& TypeAndIndex)
{
	// Deterministic Guid via hashed string. Stable across restarts for same inputs.
	const FString Src = FString::Printf(TEXT("%d:%d:%s"), RegionSeed, GeneratorVersion, *TypeAndIndex);
	const uint32 H1 = GetTypeHash(Src);
	const uint32 H2 = GetTypeHash(Src + TEXT("#2"));
	const uint32 H3 = GetTypeHash(Src + TEXT("#3"));
	const uint32 H4 = GetTypeHash(Src + TEXT("#4"));
	return FGuid(H1, H2, H3, H4);
}

bool UPersistenceService::IsSaveCompatible(int32 SaveVersion, int32 CodeVersion, bool& bOutNeedsMigration)
{
	bOutNeedsMigration = false;
	if (SaveVersion == CodeVersion)
	{
		return true;
	}
	if (SaveVersion < CodeVersion)
	{
		// Forward-only migration for same major line (slice: v1 only).
		bOutNeedsMigration = true;
		return true;
	}
	UE_LOG(LogAfterfall, Warning, TEXT("Save v%d newer than code v%d — rejected"), SaveVersion, CodeVersion);
	return false;
}

FString UPersistenceService::CellKeyForLocation(const FVector& Loc, float CellSize)
{
	const int32 CX = FMath::FloorToInt(Loc.X / CellSize);
	const int32 CY = FMath::FloorToInt(Loc.Y / CellSize);
	return FString::Printf(TEXT("%d,%d"), CX, CY);
}

bool UPersistenceService::ShouldTombstone(bool bTaken, bool bVersionStale, bool bUniqueConsumed)
{
	// Consumed or stale-version rolls are sealed history. Never resurrect.
	return bTaken || bVersionStale || bUniqueConsumed;
}

float UPersistenceService::CatchUpProgress(float ElapsedSec, float RatePerSec, float Cap)
{
	if (ElapsedSec <= 0.f || RatePerSec <= 0.f)
	{
		return 0.f;
	}
	return FMath::Min(ElapsedSec * RatePerSec, Cap);
}
