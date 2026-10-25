// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "LootSystem.h"

static TWeakObjectPtr<ULootSystem> GLootSystemInstance;

ULootSystem* ULootSystem::Get(UWorld* World)
{
	if (!GLootSystemInstance.IsValid())
	{
		GLootSystemInstance = NewObject<ULootSystem>(GetTransientPackage());
	}
	return GLootSystemInstance.Get();
}

void ULootSystem::RegisterLootProfile(const FLootProfile& Profile)
{
	LootProfiles.Add(Profile.ProfileId, Profile);
}

FLootInstance ULootSystem::GenerateLoot(const FGuid& ContainerId, const FName& ProfileId, int32 Seed)
{
	// Never regenerate if already exists
	if (HasLootInstance(ContainerId))
	{
		return GetLootInstance(ContainerId);
	}
	
	FLootInstance Instance;
	Instance.InstanceId = FGuid::NewGuid();
	Instance.ContainerId = ContainerId;
	Instance.bHasBeenGenerated = true;
	
	if (const FLootProfile* Profile = LootProfiles.Find(ProfileId))
	{
		FRandomStream Stream(Seed);
		int32 ItemCount = Stream.RandRange(Profile->MinItems, Profile->MaxItems);
		
		for (int32 i = 0; i < ItemCount && i < Profile->Items.Num(); ++i)
		{
			const FLootItem& Item = Profile->Items[i];
			int32 Count = Stream.RandRange(Item.MinCount, Item.MaxCount);
			Instance.GeneratedItems.Add(Item.ItemId);
			Instance.ItemCounts.Add(Count);
		}
	}
	
	LootInstances.Add(ContainerId, Instance);
	return Instance;
}

FLootInstance ULootSystem::GetLootInstance(const FGuid& ContainerId) const
{
	if (const FLootInstance* Instance = LootInstances.Find(ContainerId))
	{
		return *Instance;
	}
	return FLootInstance();
}

bool ULootSystem::HasLootInstance(const FGuid& ContainerId) const
{
	return LootInstances.Contains(ContainerId);
}
