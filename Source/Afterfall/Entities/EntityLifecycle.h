// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: AI (Entities) — shared ghost/active/dormant + death records.
// Ghost = logical record (pooled visual ok). No AI/physics/replication tick.
#pragma once

#include "CoreMinimal.h"
#include "EntityLifecycle.generated.h"

UENUM(BlueprintType)
enum class EEntitySimState : uint8
{
	Ghost UMETA(DisplayName = "Ghost (cheap record)"),
	ActivePersistent UMETA(DisplayName = "Active persistent encounter"),
	Dormant UMETA(DisplayName = "Dormant (timestamped outcome)"),
	Resolved UMETA(DisplayName = "Resolved/tombstoned")
};

USTRUCT(BlueprintType)
struct FZombieGhost
{
	GENERATED_BODY()
	UPROPERTY() FGuid StableId;
	UPROPERTY() FVector Home = FVector::ZeroVector;
	UPROPERTY() int32 Archetype = 0; // 0 civilian, 1 worker, 2 police, 3 soldier
	UPROPERTY() int32 HP = 100;
	UPROPERTY() EEntitySimState State = EEntitySimState::Ghost;
};

USTRUCT(BlueprintType)
struct FZombieActive
{
	GENERATED_BODY()
	UPROPERTY() FGuid StableId;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() int32 HP = 100;
	UPROPERTY() FName TargetPlayerId;
	UPROPERTY() int64 LastSeenUnix = 0;
	UPROPERTY() bool bInEncounter = false;
};

USTRUCT(BlueprintType)
struct FDeathRecord
{
	GENERATED_BODY()
	UPROPERTY() FGuid DeathId;
	UPROPERTY() int32 PlayerId = -1;
	UPROPERTY() FVector_NetQuantize Position = FVector::ZeroVector;
	UPROPERTY() int64 TimestampUnix = 0;
	UPROPERTY() FName Cause;
	// Single grouped loot container (never N ticking items).
	UPROPERTY() FGuid ContainerId;
	UPROPERTY() int64 ExpiresUnix = 0;
};
