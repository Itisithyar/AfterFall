// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Persistence) â€” delta-only saves. Never store static regen content.
// Versioned: RegionSaveVersion + GeneratorVersion + LootTableVersion.
#pragma once

#include "CoreMinimal.h"
#include "Loot/LootProfiles.h"
#include "AI/ZombiePrototype.h"
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
	/** Detached 3x3x3 destruction cells (0..26). Empty = intact. Additive field. */
	UPROPERTY() TArray<int32> DetachedCells;
	/** Material-failure propagation already fired (else reload would re-trigger). */
	UPROPERTY() bool bFailed = false;
	/** Bounded recent destruction event ids (cap 8) for replay protection. */
	UPROPERTY() TArray<FGuid> RecentEventIds;
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
struct FZombieDamageDelta
{
	GENERATED_BODY()
	UPROPERTY() FGuid StableId;
	UPROPERTY() FVector_NetQuantize Home = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize Position = FVector::ZeroVector;
	UPROPERTY() int32 HP = 100;
	// Indexed by EBodyPart (0-5). TArrays: raw C arrays are not UPROPERTY-safe.
	UPROPERTY() TArray<float> PerPartDamage;
	UPROPERTY() TArray<float> PerPartHealth;
	UPROPERTY() TArray<bool> bPartDamaged;
	UPROPERTY() float TotalDealt = 0.f;
	UPROPERTY() TArray<FName> StatusEffects;
	UPROPERTY() bool bDead = false;
	UPROPERTY() int64 TimestampUnix = 0;

	/** Snapshot a live server-side record. Only call when it HasRecord. */
	static FZombieDamageDelta FromRecord(const FZombieDamageRecord& R, FVector HomeLoc, FVector Pos, int32 HPNow, bool bIsDead)
	{
		FZombieDamageDelta D;
		D.StableId = R.StableId;
		D.Home = HomeLoc;
		D.Position = Pos;
		D.HP = HPNow;
		D.PerPartDamage.SetNum(6);
		D.PerPartHealth.SetNum(6);
		D.bPartDamaged.SetNum(6);
		for (int32 i = 0; i < 6; ++i)
		{
			D.PerPartDamage[i] = R.PerPart[i];
			D.PerPartHealth[i] = R.PerPartHealth[i];
			D.bPartDamaged[i] = R.bPartDamaged[i];
		}
		D.TotalDealt = R.TotalDealt;
		D.StatusEffects = R.StatusEffects;
		D.bDead = bIsDead;
		D.TimestampUnix = FDateTime::UtcNow().ToUnixTimestamp();
		return D;
	}
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
	UPROPERTY() TArray<FZombieDamageDelta> Zombies; // Only damaged zombies get individual records
};
