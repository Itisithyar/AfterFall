// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Loot team — mirrors Content/Data/Loot/DA_Location_*.json (TableVersion 1).
// JSON is source of truth for tuning; these structs are the runtime view.
#pragma once

#include "CoreMinimal.h"
#include "LootProfiles.generated.h"

USTRUCT(BlueprintType)
struct FLootEntryPoolItem
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) FName ItemId;
	UPROPERTY(EditDefaultsOnly) int32 Weight = 1;
	UPROPERTY(EditDefaultsOnly) int32 QtyMin = 1;
	UPROPERTY(EditDefaultsOnly) int32 QtyMax = 1;
	UPROPERTY(EditDefaultsOnly) bool bRequiresLock = false;
};

USTRUCT(BlueprintType)
struct FLootContainerProfile
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) FName Type;
	UPROPERTY(EditDefaultsOnly) int32 Count = 1;
	UPROPERTY(EditDefaultsOnly) int32 EntriesMin = 1;
	UPROPERTY(EditDefaultsOnly) int32 EntriesMax = 2;
	UPROPERTY(EditDefaultsOnly) int32 EmptyPct = 25;
	// Allowed category codes (FOOD/MED/TOOL/BLD/MAT/WPN/CLO/VEH/ELC/FRM/TRD/SPC).
	UPROPERTY(EditDefaultsOnly) TArray<FName> Allow;
};

USTRUCT(BlueprintType)
struct FLootLocationProfile
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) FName LocationId;
	UPROPERTY(EditDefaultsOnly) int32 TableVersion = 1;
	// Grade weights: Common/Uncommon/Rare/VeryRare, sum 100.
	UPROPERTY(EditDefaultsOnly) TMap<FName, float> Grade;
	// Category mix, sum 100. Zero = never spawns.
	UPROPERTY(EditDefaultsOnly) TMap<FName, float> CategoryMix;
	UPROPERTY(EditDefaultsOnly) TArray<FLootContainerProfile> Containers;
};

// One sealed stack roll. Entries = rolls, not units.
USTRUCT(BlueprintType)
struct FLootSealedRoll
{
	GENERATED_BODY()
	UPROPERTY() FGuid ContainerGuid;
	UPROPERTY() FName LocationId;
	UPROPERTY() int32 TableVersion = 1;
	UPROPERTY() TArray<FName> Items;
	UPROPERTY() TArray<int32> Quantities;
	// Parallel to Items: true = already taken (atomic).
	UPROPERTY() TArray<bool> Taken;
	UPROPERTY() FString LootGenId;
	UPROPERTY() bool bTombstoned = false;
};
