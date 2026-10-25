// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "LootService.h"
#include "Afterfall.h"

FString ULootService::MakeRollKey(int32 RegionSeed, int32 TableVersion, const FGuid& ContainerGuid)
{
	return FString::Printf(TEXT("%d:%d:%s"), RegionSeed, TableVersion, *ContainerGuid.ToString());
}

int32 ULootService::PickEntryCount(int32 Min, int32 Max, float T)
{
	if (Max <= Min)
	{
		return Min;
	}
	const float Clamped = FMath::Clamp(T, 0.f, 0.9999f);
	return Min + FMath::FloorToInt(Clamped * (Max - Min + 1));
}

FName ULootService::PickRarity(const TMap<FName, float>& Grade, float Roll01)
{
	// Fixed order for determinism: C/U/R/VR.
	static const FName Order[4] = { TEXT("Common"), TEXT("Uncommon"), TEXT("Rare"), TEXT("VeryRare") };
	const float R = FMath::Clamp(Roll01, 0.f, 0.9999f) * 100.f;
	float Acc = 0.f;
	for (int32 i = 0; i < 4; i++)
	{
		const float* W = Grade.Find(Order[i]);
		Acc += (W ? *W : 0.f);
		if (R < Acc)
		{
			return Order[i];
		}
	}
	return Order[3];
}

FName ULootService::PickCategory(const TMap<FName, float>& Mix, const TArray<FName>& Allow, float Roll01)
{
	float Total = 0.f;
	for (const FName& C : Allow)
	{
		if (const float* W = Mix.Find(C))
		{
			Total += *W;
		}
	}
	if (Total <= 0.f)
	{
		return NAME_None;
	}
	float R = FMath::Clamp(Roll01, 0.f, 0.9999f) * Total;
	for (const FName& C : Allow)
	{
		if (const float* W = Mix.Find(C))
		{
			R -= *W;
			if (R < 0.f)
			{
				return C;
			}
		}
	}
	return Allow.Last();
}

bool ULootService::CanMintUnique(const FName& ItemId, int32 LiveCount, int32 Cap)
{
	if (Cap <= 0)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Unique %s has no cap — rejected"), *ItemId.ToString());
		return false;
	}
	return LiveCount < Cap;
}

void ULootService::ApplyGuarantee(const TArray<FName>& Items, const TArray<FName>& AnyOf, float Chance, float Roll01, int32& OutReplaceIndex)
{
	OutReplaceIndex = -1;
	for (const FName& I : Items)
	{
		if (AnyOf.Contains(I))
		{
			return; // already satisfied
		}
	}
	if (Roll01 >= Chance || Items.Num() == 0)
	{
		return;
	}
	// Replace last entry (lowest-value convention: pools sorted ascending upstream).
	OutReplaceIndex = Items.Num() - 1;
}
