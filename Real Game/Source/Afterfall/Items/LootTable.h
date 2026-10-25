#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/ItemStack.h"
#include "Items/AfterfallItemDefinition.h"
#include "LootTable.generated.h"

USTRUCT(BlueprintType)
struct FLootEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName ItemID = NAME_None;
	UPROPERTY(EditAnywhere) float Weight = 1.f;
	UPROPERTY(EditAnywhere) int32 MinQty = 1;
	UPROPERTY(EditAnywhere) int32 MaxQty = 1;
};

UCLASS(BlueprintType)
class AFTERFALL_API UAfterfallLootTable : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) TArray<FLootEntry> Entries;
	TArray<FItemStack> RollLoot(int32 NumRolls, int32 Seed) const;
	void Validate() const;
};
