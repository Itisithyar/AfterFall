// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Muse (Gameplay/Inventory) — item categories, instance, and slot types.
// Native C++ foundation for the tabbed inventory. No engine/world dependencies.
#pragma once

#include "CoreMinimal.h"
#include "ItemTypes.generated.h"

/** Inventory tab categories. Keep in one-to-one sync with the UI tab order. */
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	All        UMETA(DisplayName = "All"),
	Weapons    UMETA(DisplayName = "Weapons"),
	Equipment  UMETA(DisplayName = "Equipment"),
	Medical    UMETA(DisplayName = "Medical"),
	Resources  UMETA(DisplayName = "Resources"),
	FoodDrink  UMETA(DisplayName = "Food & Drink"),
	Quest      UMETA(DisplayName = "Quest / Key Items"),
	MAX        UMETA(Hidden)
};

/** One runtime instance of an item sitting in an inventory slot. */
USTRUCT(BlueprintType)
struct FItemInstance
{
	GENERATED_BODY()

	FItemInstance() = default;
	FItemInstance(FName InItemId, int32 InCount = 1)
		: ItemId(InItemId), Count(InCount) {}

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Item")
	FName ItemId = NAME_None;

	/**
	 * Stable per-instance identity (survives drop/store/transfer).
	 *
	 * The in-class default is deliberately NON-deterministic (every instance
	 * needs its own id), so the engine's struct-initialization check sees a
	 * default-constructed instance differ from a zeroed one and reports it as
	 * "not initialized properly". The value IS initialized; the check is told
	 * to skip this member instead of the id being made deterministic (which
	 * would give every item the same empty id).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Item",
		meta = (IgnoreForMemberInitializationTest))
	FGuid InstanceId = FGuid::NewGuid();

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Item")
	int32 Count = 1;

	/** 0..1 condition/durability for equipment & weapons. 1.f = pristine. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Item")
	float Condition = 1.f;

	/** Designer free-form payload (e.g. ammo type, attached mods). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Item")
	FString CustomData;

	// ---------- Lineage / provenance (combination engine, Phase 5) ----------
	/** Instance ids this item was produced from. Empty for world/loot spawns.
	 * Immediate parents only — not a full ancestor graph. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Lineage")
	TArray<FGuid> ParentInstanceIds;

	/** Combination rule that produced this instance (None if not crafted). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Lineage")
	FName ProducedByRecipe = NAME_None;

	/** Version of the rule at creation time (0 = not crafted). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Lineage")
	int32 RecipeVersion = 0;

	/** Idempotency/transaction id of the combine that minted this instance. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Lineage")
	FGuid CreationTransactionId;

	/** Instance schema version for save migration (current = 1). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Lineage")
	int32 SchemaVersion = 1;

	bool operator==(const FItemInstance& Other) const { return InstanceId == Other.InstanceId; }
	bool operator!=(const FItemInstance& Other) const { return !(*this == Other); }
};

/** One slot in the inventory grid. Capacity is max stack for stackables, 1 otherwise. */
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	FInventorySlot() = default;

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Inventory")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Inventory")
	FItemInstance Item;

	/** Max stack this slot can hold (1 for non-stackables/equipment). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Inventory")
	int32 Capacity = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Inventory")
	bool bLocked = false;

		bool IsEmpty() const { return Item.ItemId.IsNone() || Item.Count <= 0; }
	int32 FreeSpace() const { return Capacity - Item.Count; }
	bool CanAccept(const FItemInstance& Incoming) const
	{
		if (bLocked || Item.ItemId.IsNone())
		{
			return true; // empty or unlocked slot accepts anything
		}
		if (Item.ItemId != Incoming.ItemId)
		{
			return false; // different item type
		}
		return Incoming.Count <= FreeSpace();
	}
};
