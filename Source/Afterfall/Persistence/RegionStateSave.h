// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Persistence) — delta-only saves. Never store static regen content.
// Versioned: RegionSaveVersion + GeneratorVersion + LootTableVersion.
#pragma once

#include "CoreMinimal.h"
#include "Loot/LootProfiles.h"
#include "RegionStateSave.generated.h"

USTRUCT(BlueprintType)
struct FStructureDelta
{
	GENERATED_BODY()
	UPROPERTY() FGuid Guid;
	UPROPERTY() FName PartId;
	UPROPERTY() FVector_NetQuantize Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float Durability = 100.f;
	UPROPERTY() int32 Tier = 0;
	UPROPERTY() int32 OwnerPlayerId = -1;
	UPROPERTY() bool bClaimed = false;
	UPROPERTY() bool bDestroyed = false;
};

USTRUCT(BlueprintType)
struct FContainerDelta
{
	GENERATED_BODY()
	UPROPERTY() FGuid ContainerGuid;
	UPROPERTY() FName LocationId;
	UPROPERTY() int32 TableVersion = 1;
	UPROPERTY() TArray<FName> Items;
	UPROPERTY() TArray<int32> Quantities;
	UPROPERTY() TArray<bool> Taken;
	UPROPERTY() FString LootGenId;
	UPROPERTY() bool bTombstoned = false;
};

USTRUCT(BlueprintType)
struct FGatherNodeDelta
{
	GENERATED_BODY()
	UPROPERTY() FGuid NodeId;
	// Server time (seconds) when node becomes harvestable again. 0 = available.
	UPROPERTY() int64 DepletedUntilUnix = 0;
};

USTRUCT(BlueprintType)
struct FVehicleDelta
{
	GENERATED_BODY()
	UPROPERTY() FGuid VehicleId;
	UPROPERTY() FVector_NetQuantize Location = FVector::ZeroVector;
	UPROPERTY() float FuelLiters = 0.f;
	UPROPERTY() int32 EnginePct = 100;
	UPROPERTY() int32 TireCount = 0;
};

USTRUCT(BlueprintType)
struct FClaimDelta
{
	GENERATED_BODY()
	UPROPERTY() FGuid BuildingId;
	UPROPERTY() int32 OwnerPlayerId = -1;
	UPROPERTY() int64 ClaimedAtUnix = 0;
};

USTRUCT(BlueprintType)
struct FRegionStateSave
{
	GENERATED_BODY()
	UPROPERTY() int32 RegionSaveVersion = 1;
	UPROPERTY() int32 GeneratorVersion = 1;
	UPROPERTY() int32 LootTableVersion = 1;
	UPROPERTY() int32 RegionSeed = 1337;
	UPROPERTY() FString RegionId;
	UPROPERTY() TArray<FStructureDelta> Structures;
	UPROPERTY() TArray<FContainerDelta> Containers;
	UPROPERTY() TArray<FGatherNodeDelta> Nodes;
	UPROPERTY() TArray<FVehicleDelta> Vehicles;
	UPROPERTY() TArray<FClaimDelta> Claims;
};
