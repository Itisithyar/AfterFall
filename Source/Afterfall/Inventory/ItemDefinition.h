// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Muse (Gameplay/Inventory) — data-driven item definition.
// Designers create UItemDefinition assets in Content/Data/Items/; no hardcoded item classes.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Inventory/ItemTypes.h"
#include "ItemDefinition.generated.h"

/**
 * One item definition. Drives display, stacking, weight, category, and
 * capability flags. Referenced by FItemInstance via TSoftObjectPtr so
 * definitions can be cooked on demand.
 */
UCLASS(BlueprintType, Blueprintable)
class AFTERFALL_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UItemDefinition(const FObjectInitializer& ObjectInitializer);

	/** Stable identifier. Must match inventory/medical/crafting/loot IDs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item", Meta = (DisplayName = "Item ID"))
	FName ItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	FText Description;

	/** Icon shown in the inventory UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Tab filter category. Must match EItemCategory. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	EItemCategory Category = EItemCategory::Resources;

	/** Weight per unit in kg. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item", Meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WeightKg = 1.f;

	/** Whether multiple units can share one slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	bool bStackable = true;

	/** Max units per stack (ignored when bStackable == false). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item", Meta = (ClampMin = "1", UIMin = "1", EditCondition = "bStackable"))
	int32 MaxStack = 99;

	/** Can be consumed/used via ServerUseItem. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	bool bUsable = false;

	/** Can be equipped into an equipment slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item")
	bool bEquippable = false;

	/** Equipment slot name (e.g. "Primary", "Secondary", "Head", "Torso"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Item", Meta = (EditCondition = "bEquippable"))
	FName EquipmentSlot = NAME_None;

	/** Cross-reference into UMedicalCapabilityLib when this is a medical item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Medical")
	FName MedicalItemId = NAME_None;

	/** Pure: weight of one unit. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Item")
	float GetWeightKg() const { return WeightKg; }

	/** Pure: true when the item has a medical capability entry. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Medical")
	bool IsMedicalItem() const { return !MedicalItemId.IsNone(); }
};
