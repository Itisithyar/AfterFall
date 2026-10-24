// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Loot team — server-only rolls + atomic take. No client RNG.
#pragma once

#include "CoreMinimal.h"
#include "LootProfiles.h"
#include "UObject/NoExportTypes.h"
#include "LootService.generated.h"

UCLASS()
class AFTERFALL_API ULootService : public UObject
{
	GENERATED_BODY()

public:
	// Deterministic key: seed:tableVersion:containerGuid. Stable across restarts.
	static FString MakeRollKey(int32 RegionSeed, int32 TableVersion, const FGuid& ContainerGuid);

	// Pure helpers (unit-tested, mirrored in validate_lootservice.py):
	// Pick entry count in [Min,Max] from uniform t in [0,1).
	static int32 PickEntryCount(int32 Min, int32 Max, float T);
	// Weighted rarity pick from grade map. Returns Common/Uncommon/Rare/VeryRare.
	static FName PickRarity(const TMap<FName, float>& Grade, float Roll01);
	// Category pick: weight x allow-mask, renormalized. Returns NAME_None when all masked out.
	static FName PickCategory(const TMap<FName, float>& Mix, const TArray<FName>& Allow, float Roll01);

	// Single-copy guard: VR/engine/key items must bind UniqueInstanceId at gen.
	// Returns false when cap already reached (no second copy minted).
	static bool CanMintUnique(const FName& ItemId, int32 LiveCount, int32 Cap);

	// Guarantee post-pass: when none of AnyOf present, replace lowest-value entry.
	// OutReplaceIndex = -1 when already satisfied or rollover chance fails.
	static void ApplyGuarantee(const TArray<FName>& Items, const TArray<FName>& AnyOf, float Chance, float Roll01, int32& OutReplaceIndex);
};
