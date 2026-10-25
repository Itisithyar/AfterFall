// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Muse (Gameplay/Inventory) — server-authoritative inventory.
// Phase B: extends Phase A wallet (Wood + Items map) with item definitions,
// slots, stacking, equipment, use/drop/split, and UI change events.
// All Phase A members preserved for backward compatibility.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/ItemTypes.h"
#include "Inventory/ItemDefinition.h"
#include "World/PhysicalItem.h"
#include "InventoryComponent.generated.h"

class UInjuryStateComponent;
class UMedicalCapabilityLib;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemEquipped, FName, SlotName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUnequipped, FName, SlotName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemConsumed, FName, ItemId);

USTRUCT(BlueprintType)
struct FInventoryEntry
{
	GENERATED_BODY()
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Count = 0;
};

/** One equipped instance: reflected entry because UHT rejects replicated
 * TMaps. Order is not meaningful; lookups are by SlotName. */
USTRUCT(BlueprintType)
struct FEquipmentSlotEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Inventory")
	FName SlotName = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Inventory")
	FGuid InstanceId;
};

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// ---------- Phase A: legacy wallet (preserved) ----------
	UPROPERTY(ReplicatedUsing = OnRep_Wood, VisibleAnywhere, Category = "Afterfall|Inventory")
	int32 Wood = 10;

	UPROPERTY(ReplicatedUsing = OnRep_Items, VisibleAnywhere, Category = "Afterfall|Inventory")
	TArray<FInventoryEntry> Items;

	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Inventory")
	float MaxWeightKg = 30.f;

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	int32 GetWood() const { return Wood; }

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	int32 GetCount(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	float GetWeightKg() const;

	static float WeightForItem(FName ItemId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddResource(int32 Amount);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddItem(FName ItemId, int32 Count);

	bool ServerTryConsume(int32 Cost);
	bool ServerTryConsumeItem(FName ItemId, int32 Count);

	// ---------- Phase B: item definitions & slots ----------
	/** Replicated slot grid. UI reads this directly for tab rendering. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots, VisibleAnywhere, Category = "Afterfall|Inventory")
	TArray<FInventorySlot> Slots;

	/** Loaded item definitions (soft-loaded on demand, not replicated). */
	UPROPERTY(VisibleAnywhere, Category = "Afterfall|Inventory")
	// Soft-load cache: mutable so const queries can resolve definitions.
	mutable TMap<FName, TSoftObjectPtr<UItemDefinition>> ItemDefinitions;

	/** One equipped instance. Reflected array entry: UHT rejects replicated
	 * TMaps, and the array keeps slot->instance semantics with replication. */
	UPROPERTY(ReplicatedUsing = OnRep_EquipmentSlots, BlueprintReadOnly, Category = "Afterfall|Inventory")
	TArray<FEquipmentSlotEntry> EquipmentSlots;

	// Change events (BlueprintAssignable for UI binding).
	UPROPERTY(BlueprintAssignable, Category = "Afterfall|Inventory")
	FOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Afterfall|Inventory")
	FOnItemEquipped OnItemEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Afterfall|Inventory")
	FOnItemUnequipped OnItemUnequipped;

	UPROPERTY(BlueprintAssignable, Category = "Afterfall|Inventory")
	FOnItemConsumed OnItemConsumed;

	// ---------- Phase B: queries ----------
	/** All items matching a category (EItemCategory::All returns everything). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	TArray<FInventorySlot> GetItemsByCategory(EItemCategory Category) const;

	/** Instance currently equipped in a slot (None if empty). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Inventory")
	FGuid GetEquippedItem(FName SlotName) const;

	// GetWeightKg() is declared once in the Phase A wallet section above;
	// UHT cannot expose the same name twice on one class.

	/** Weight of one unit of an item, using definition if loaded. */
	static float WeightForItemDefinition(TSoftObjectPtr<UItemDefinition> Def);

	/** Item definition for an id (soft-loads on demand, read-only). */
	UItemDefinition* GetItemDefinition(FName ItemId) const;

	// ---------- Phase B: mutations (server-authoritative) ----------
	/** Grant items into the first fitting slot, respecting stack + weight cap. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAddItemToSlots(FName ItemId, int32 Count);

	/** Equip an instance into its EquipmentSlot, unequipping anything already there. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerEquipItem(FGuid InstanceId);

	/** Unequip the item in a named slot, moving it back to the first free slot. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUnequipItem(FName SlotName);

	/** Drop Count units of an instance into the world at the owner's feet. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDropItem(FGuid InstanceId, int32 Count);

	/** Split Count units off an instance into a new slot/stack. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSplitItem(FGuid InstanceId, int32 Count);

	/** Use an item: medical routes to InjuryStateComponent treatment; others apply definition effects. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUseItem(FGuid InstanceId);

	/** Pick up a world item into the inventory (routes to first fitting slot). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPickupItem(APhysicalItem* Item);

	// ---------- Phase 5: instance-level combination (lineage-aware) ----------
	/**
	 * Combine concrete instances into a crafted child. Transactional: every
	 * rule (inputs, weight, capacity) is checked on a working copy and the
	 * inventory is only replaced when the whole operation can commit, so a
	 * failure never consumes or duplicates anything. Idempotent per
	 * TransactionId. Server-only; public so headless tests can drive it.
	 */
	bool TryCombine(const TArray<FGuid>& InputInstanceIds, FName RecipeId,
		const FGuid& TransactionId, FString& OutError);

	/** Server RPC entry: same rules, authority-gated. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCombineItems(const TArray<FGuid>& InputInstanceIds, FName RecipeId, FGuid TransactionId);

	/**
	 * Station "Craft" helper: first recipe in table order whose inputs are
	 * all satisfiable from concrete slots (deterministic priority), combined
	 * through the same transactional path.
	 */
	bool ServerCombineFirstAvailable(const FGuid& TransactionId, FString& OutError);

protected:
	UFUNCTION()
	void OnRep_Wood();
	UFUNCTION()
	void OnRep_Items();
	UFUNCTION()
	void OnRep_Slots();

	/** Clients rebuild equipment UI from the replicated entry array. */
	UFUNCTION()
	void OnRep_EquipmentSlots();

protected:
	int32* FindEntry(FName ItemId);
	FInventorySlot* FindSlotByInstance(FGuid InstanceId);
private:
	UItemDefinition* LoadDefinition(FName ItemId) const;
	EItemCategory CategoryForItem(FName ItemId) const;
	int32 FirstFreeSlotIndex() const;
	int32 FindStackableSlot(FName ItemId) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

private:
	void BroadcastInventoryChanged();
	void ConsumeInstance(FGuid InstanceId, int32 Count);
	void MoveInstanceToSlot(FGuid InstanceId, int32 TargetSlotIndex);
	bool CanCarryWeight(float AdditionalKg) const;

	/** Bounded idempotency ledger of committed combine transactions (FIFO). */
	TArray<FGuid> RecentCombineTransactions;
	static constexpr int32 MaxRecentCombineTransactions = 64;
};
