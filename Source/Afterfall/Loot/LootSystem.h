// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Loot - POI-based loot system
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LootSystem.generated.h"

USTRUCT(BlueprintType)
struct FLootItem
{
	GENERATED_BODY()
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 MinCount = 1;
	UPROPERTY() int32 MaxCount = 1;
	UPROPERTY() float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct FLootProfile
{
	GENERATED_BODY()
	UPROPERTY() FName ProfileId;
	UPROPERTY() TArray<FLootItem> Items;
	UPROPERTY() int32 MinItems = 1;
	UPROPERTY() int32 MaxItems = 3;
};

USTRUCT(BlueprintType)
struct FLootInstance
{
	GENERATED_BODY()
	UPROPERTY() FGuid InstanceId;
	UPROPERTY() FGuid ContainerId;
	UPROPERTY() TArray<FName> GeneratedItems;
	UPROPERTY() TArray<int32> ItemCounts;
	UPROPERTY() bool bHasBeenGenerated = false;
};

UCLASS()
class AFTERFALL_API ULootSystem : public UObject
{
	GENERATED_BODY()

public:
	static ULootSystem* Get(UWorld* World);
	
	void RegisterLootProfile(const FLootProfile& Profile);
	FLootInstance GenerateLoot(const FGuid& ContainerId, const FName& ProfileId, int32 Seed);
	FLootInstance GetLootInstance(const FGuid& ContainerId) const;
	
	// Never regenerate existing loot
	bool HasLootInstance(const FGuid& ContainerId) const;

protected:
	UPROPERTY() TMap<FName, FLootProfile> LootProfiles;
	UPROPERTY() TMap<FGuid, FLootInstance> LootInstances;
};
