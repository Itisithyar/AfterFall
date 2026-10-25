// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Combat/InjuryStateComponent.h"
#include "Combat/MedicalCapability.h"
#include "Combat/HealthComponent.h"
#include "Crafting/CraftingService.h"
#include "Crafting/ItemCombiner.h"
#include "Afterfall.h"

UInventoryComponent::UInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	Wood = 10;
	Items.Add({ TEXT("AF_R_Wood"), 10 });

	// Default slot grid: 24 slots, capacity 1 each (stackables share via stacking logic).
	for (int32 i = 0; i < 24; ++i)
	{
		FInventorySlot S;
		S.SlotIndex = i;
		S.Capacity = 1;
		Slots.Add(S);
	}
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

int32 UInventoryComponent::GetCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FInventoryEntry& E : Items)
	{
		if (E.ItemId == ItemId)
		{
			Total += E.Count;
		}
	}
	for (const FInventorySlot& S : Slots)
	{
		if (!S.IsEmpty() && S.Item.ItemId == ItemId)
		{
			Total += S.Item.Count;
		}
	}
	return Total;
}

float UInventoryComponent::GetWeightKg() const
{
	float Total = 0.f;
	for (const FInventoryEntry& E : Items)
	{
		Total += WeightForItem(E.ItemId) * E.Count;
	}
	for (const FInventorySlot& S : Slots)
	{
		if (!S.IsEmpty())
		{
			UItemDefinition* Def = LoadDefinition(S.Item.ItemId);
			Total += (Def ? Def->GetWeightKg() : WeightForItem(S.Item.ItemId)) * S.Item.Count;
		}
	}
	return Total;
}

float UInventoryComponent::WeightForItem(FName ItemId)
{
	static const TMap<FName, float> W = {
		{ TEXT("AF_R_Wood"), 0.5f }, { TEXT("res_wood_log"), 0.5f },
		{ TEXT("AF_R_Stone"), 0.8f }, { TEXT("AF_R_HempFiber"), 0.1f },
		{ TEXT("AF_R_Scrap"), 1.0f }, { TEXT("res_scrap_metal"), 1.0f },
		{ TEXT("AF_M_Plank"), 1.2f }, { TEXT("mat_plank"), 1.2f },
		{ TEXT("AF_M_Rope"), 0.4f }, { TEXT("mat_rope"), 0.4f },
		{ TEXT("AF_M_Cloth"), 0.3f }, { TEXT("AF_M_Fasteners"), 0.1f },
		{ TEXT("mat_nail"), 0.05f }, { TEXT("AF_F_Canned"), 0.4f },
		{ TEXT("AF_MED_Bandage"), 0.1f }, { TEXT("res_med_bandage"), 0.1f },
	};
	if (const float* F = W.Find(ItemId))
	{
		return *F;
	}
	return 1.0f;
}

float UInventoryComponent::WeightForItemDefinition(TSoftObjectPtr<UItemDefinition> Def)
{
	if (Def.IsValid() && Def.Get())
	{
		return Def.Get()->GetWeightKg();
	}
	return 1.0f;
}

int32* UInventoryComponent::FindEntry(FName ItemId)
{
	for (FInventoryEntry& E : Items)
	{
		if (E.ItemId == ItemId)
		{
			return &E.Count;
		}
	}
	return nullptr;
}

FInventorySlot* UInventoryComponent::FindSlotByInstance(FGuid InstanceId)
{
	for (FInventorySlot& S : Slots)
	{
		if (!S.IsEmpty() && S.Item.InstanceId == InstanceId)
		{
			return &S;
		}
	}
	return nullptr;
}

UItemDefinition* UInventoryComponent::LoadDefinition(FName ItemId) const
{
	if (TSoftObjectPtr<UItemDefinition>* Ptr = ItemDefinitions.Find(ItemId))
	{
		if (Ptr->IsValid() && Ptr->Get())
		{
			return Ptr->Get();
		}
	}
	// Soft-load by path convention: /Game/Data/Items/ItemId.ItemId
	FString SoftPath = FString::Printf(TEXT("/Game/Data/Items/%s.%s"), *ItemId.ToString(), *ItemId.ToString());
	TSoftObjectPtr<UItemDefinition> SoftRef{FSoftObjectPath(SoftPath)};
	if (UItemDefinition* Loaded = SoftRef.LoadSynchronous())
	{
		ItemDefinitions.Add(ItemId, SoftRef);
		return Loaded;
	}
	return nullptr;
}

UItemDefinition* UInventoryComponent::GetItemDefinition(FName ItemId) const
{
	return LoadDefinition(ItemId);
}

EItemCategory UInventoryComponent::CategoryForItem(FName ItemId) const
{
	UItemDefinition* Def = LoadDefinition(ItemId);
	return Def ? Def->Category : EItemCategory::Resources;
}

int32 UInventoryComponent::FirstFreeSlotIndex() const
{
	for (const FInventorySlot& S : Slots)
	{
		if (S.IsEmpty() && !S.bLocked)
		{
			return S.SlotIndex;
		}
	}
	return -1;
}

int32 UInventoryComponent::FindStackableSlot(FName ItemId) const
{
	UItemDefinition* Def = LoadDefinition(ItemId);
	if (!Def || !Def->bStackable)
	{
		return -1;
	}
	for (const FInventorySlot& S : Slots)
	{
		if (!S.IsEmpty() && S.Item.ItemId == ItemId && S.Item.Count < S.Capacity && !S.bLocked)
		{
			return S.SlotIndex;
		}
	}
	return -1;
}

bool UInventoryComponent::CanCarryWeight(float AdditionalKg) const
{
	return GetWeightKg() + AdditionalKg <= MaxWeightKg + 0.001f;
}

TArray<FInventorySlot> UInventoryComponent::GetItemsByCategory(EItemCategory Category) const
{
	TArray<FInventorySlot> Out;
	for (const FInventorySlot& S : Slots)
	{
		if (S.IsEmpty())
		{
			continue;
		}
		if (Category == EItemCategory::All || CategoryForItem(S.Item.ItemId) == Category)
		{
			Out.Add(S);
		}
	}
	return Out;
}

FGuid UInventoryComponent::GetEquippedItem(FName SlotName) const
{
	for (const FEquipmentSlotEntry& Entry : EquipmentSlots)
	{
		if (Entry.SlotName == SlotName)
		{
			return Entry.InstanceId;
		}
	}
	return FGuid();
}

void UInventoryComponent::OnRep_EquipmentSlots()
{
	BroadcastInventoryChanged();
}

void UInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::ConsumeInstance(FGuid InstanceId, int32 Count)
{
	if (FInventorySlot* S = FindSlotByInstance(InstanceId))
	{
		S->Item.Count -= Count;
		if (S->Item.Count <= 0)
		{
			S->Item = FItemInstance();
		}
		BroadcastInventoryChanged();
	}
}

void UInventoryComponent::MoveInstanceToSlot(FGuid InstanceId, int32 TargetSlotIndex)
{
	if (FInventorySlot* S = FindSlotByInstance(InstanceId))
	{
		if (TargetSlotIndex >= 0 && TargetSlotIndex < Slots.Num())
		{
			Slots[TargetSlotIndex].Item = S->Item;
			S->Item = FItemInstance();
			BroadcastInventoryChanged();
		}
	}
}

bool UInventoryComponent::ServerAddItemToSlots_Validate(FName ItemId, int32 Count)
{
	return !ItemId.IsNone() && Count > 0 && Count <= 100;
}

void UInventoryComponent::ServerAddItemToSlots_Implementation(FName ItemId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	float AddedWeight = 0.f;
	UItemDefinition* Def = LoadDefinition(ItemId);
	AddedWeight = (Def ? Def->GetWeightKg() : WeightForItem(ItemId)) * Count;
	if (!CanCarryWeight(AddedWeight))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("AddItemToSlots rejected: overweight"));
		return;
	}

	// Try to stack into existing stackable slot.
	int32 Remaining = Count;
	if (Def && Def->bStackable)
	{
		int32 StackSlot = FindStackableSlot(ItemId);
		if (StackSlot >= 0)
		{
			FInventorySlot& S = Slots[StackSlot];
			int32 Space = S.FreeSpace();
			int32 Add = FMath::Min(Remaining, Space);
			S.Item.Count += Add;
			Remaining -= Add;
		}
	}

	while (Remaining > 0)
	{
		int32 Free = FirstFreeSlotIndex();
		if (Free < 0)
		{
			UE_LOG(LogAfterfall, Warning, TEXT("AddItemToSlots: no free slots"));
			break;
		}
		int32 Add = Def && Def->bStackable ? FMath::Min(Remaining, Def->MaxStack) : 1;
		FInventorySlot& S = Slots[Free];
		S.Item = FItemInstance(ItemId, Add);
		S.Item.Count = Add;
		Remaining -= Add;
	}

	// Keep legacy Items map + Wood in sync during migration.
	if (int32* C = FindEntry(ItemId))
	{
		*C = FMath::Clamp(*C + Count, 0, 9999);
	}
	else
	{
		Items.Add({ ItemId, Count });
	}
	if (ItemId == TEXT("AF_R_Wood") || ItemId == TEXT("res_wood_log"))
	{
		Wood = GetCount(TEXT("AF_R_Wood")) + GetCount(TEXT("res_wood_log"));
	}

	BroadcastInventoryChanged();
}

bool UInventoryComponent::ServerEquipItem_Validate(FGuid InstanceId)
{
	return InstanceId.IsValid();
}

void UInventoryComponent::ServerEquipItem_Implementation(FGuid InstanceId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FInventorySlot* S = FindSlotByInstance(InstanceId);
	if (!S || S->IsEmpty())
	{
		return;
	}
	UItemDefinition* Def = LoadDefinition(S->Item.ItemId);
	if (!Def || !Def->bEquippable || Def->EquipmentSlot.IsNone())
	{
		UE_LOG(LogAfterfall, Verbose, TEXT("Equip rejected: not equippable %s"), *S->Item.ItemId.ToString());
		return;
	}
	FName SlotName = Def->EquipmentSlot;

	// Unequip anything currently in that slot.
	const FGuid Current = GetEquippedItem(SlotName);
	if (Current.IsValid())
	{
		if (Current != InstanceId)
		{
			ServerUnequipItem(SlotName);
		}
		else
		{
			// Already equipped; toggle off.
			EquipmentSlots.RemoveAll([SlotName](const FEquipmentSlotEntry& E)
			{
				return E.SlotName == SlotName;
			});
			S->Item.Count = 1; // return to inventory slot
			OnItemUnequipped.Broadcast(SlotName);
			BroadcastInventoryChanged();
			return;
		}
	}

	FEquipmentSlotEntry Entry;
	Entry.SlotName = SlotName;
	Entry.InstanceId = InstanceId;
	EquipmentSlots.Add(Entry);
	S->Item.Count = 1; // equipped items occupy their slot, count stays 1
	OnItemEquipped.Broadcast(SlotName);
	BroadcastInventoryChanged();
}

bool UInventoryComponent::ServerUnequipItem_Validate(FName SlotName)
{
	return !SlotName.IsNone();
}

void UInventoryComponent::ServerUnequipItem_Implementation(FName SlotName)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (GetEquippedItem(SlotName).IsValid())
	{
		EquipmentSlots.RemoveAll([SlotName](const FEquipmentSlotEntry& E)
		{
			return E.SlotName == SlotName;
		});
		OnItemUnequipped.Broadcast(SlotName);
		BroadcastInventoryChanged();
	}
}

bool UInventoryComponent::ServerDropItem_Validate(FGuid InstanceId, int32 Count)
{
	return InstanceId != FGuid() && Count > 0;
}

void UInventoryComponent::ServerDropItem_Implementation(FGuid InstanceId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FInventorySlot* S = FindSlotByInstance(InstanceId);
	if (!S || S->IsEmpty() || S->Item.Count < Count)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Spawn a physical world item at the owner's location.
	FVector SpawnLoc = Owner->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	FRotator SpawnRot = FRotator::ZeroRotator;
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	APhysicalItem* Pickup = GetWorld()->SpawnActor<APhysicalItem>(SpawnLoc, SpawnRot, SpawnParams);
	if (!Pickup)
	{
		// Destination failed -> source must stay intact. Never destroy
		// inventory items on a failed world spawn (recoverable state beats
		// silent deletion); the client can simply retry.
		UE_LOG(LogAfterfall, Warning, TEXT("Drop rejected: world spawn failed (items kept)"));
		return;
	}
	Pickup->ItemType = S->Item.ItemId;
	// Hand the full instance state to the world actor: identity, count,
	// condition and lineage cross the boundary with the item.
	Pickup->InstancePayload = S->Item;
	Pickup->InstancePayload.Count = Count;
	if (Count < S->Item.Count)
	{
		// Partial drop = stack split: the piece entering the world becomes
		// its own tracked instance (fresh id, provenance inherited); the
		// remaining stack keeps the original identity in the inventory.
		// A single identity can never live in two containers at once.
		Pickup->InstancePayload.InstanceId = FGuid::NewGuid();
	}
	Pickup->ItemId = Pickup->InstancePayload.InstanceId; // one canonical id

	ConsumeInstance(InstanceId, Count);
}

bool UInventoryComponent::ServerSplitItem_Validate(FGuid InstanceId, int32 Count)
{
	return InstanceId != FGuid() && Count > 0;
}

void UInventoryComponent::ServerSplitItem_Implementation(FGuid InstanceId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FInventorySlot* S = FindSlotByInstance(InstanceId);
	if (!S || S->IsEmpty() || S->Item.Count <= Count)
	{
		return;
	}
	UItemDefinition* Def = LoadDefinition(S->Item.ItemId);
	if (!Def || !Def->bStackable)
	{
		return;
	}
	int32 Free = FirstFreeSlotIndex();
	if (Free < 0)
	{
		return;
	}
	S->Item.Count -= Count;
	FInventorySlot& NewSlot = Slots[Free];
	NewSlot.Item = FItemInstance(S->Item.ItemId, Count);
	BroadcastInventoryChanged();
}

bool UInventoryComponent::ServerUseItem_Validate(FGuid InstanceId)
{
	return InstanceId != FGuid();
}

void UInventoryComponent::ServerUseItem_Implementation(FGuid InstanceId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FInventorySlot* S = FindSlotByInstance(InstanceId);
	if (!S || S->IsEmpty())
	{
		return;
	}
	UItemDefinition* Def = LoadDefinition(S->Item.ItemId);
	if (!Def || !Def->bUsable)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Medical path: route to canonical InjuryStateComponent treatment.
	if (!Def->MedicalItemId.IsNone())
	{
		if (UInjuryStateComponent* Injury = Owner->FindComponentByClass<UInjuryStateComponent>())
		{
			if (Injury->GetOwner() && Injury->GetOwner()->HasAuthority())
			{
				// Pick first wounded part that this capability matches.
				bool bStarted = false;
				for (int32 i = 0; i < 6 && !bStarted; ++i)
				{
					const FInjuryRecord& R = Injury->PartInjuries[i];
					if (!R.bWounded)
					{
						continue;
					}
					const FMedicalCapability* Cap = UMedicalCapabilityLib::FindCapability(Def->MedicalItemId);
					if (Cap && UMedicalCapabilityLib::CapabilityMatchesInjury(*Cap,
						R.bBleeding, R.bPain, R.Severity, R.bStabilized))
					{
						Injury->ServerBeginTreatment(static_cast<EBodyPart>(i), Def->MedicalItemId);
						bStarted = true;
					}
				}
				if (!bStarted)
				{
					UE_LOG(LogAfterfall, Verbose, TEXT("Medical item %s matches no wound"), *Def->MedicalItemId.ToString());
				}
			}
		}
	}

	// Consume one use.
	ConsumeInstance(InstanceId, 1);
	OnItemConsumed.Broadcast(S->Item.ItemId);
}

bool UInventoryComponent::ServerPickupItem_Validate(APhysicalItem* Item)
{
	return Item != nullptr;
}

void UInventoryComponent::ServerPickupItem_Implementation(APhysicalItem* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Item)
	{
		return;
	}
	if (!Item->IsFree())
	{
		return;
	}

	// Restore the instance that previously left an inventory (identity,
	// count, condition, lineage). Legacy spawns without a payload fall back
	// to a fresh instance of ItemType with count 1.
	FItemInstance Incoming = Item->InstancePayload;
	if (Incoming.ItemId.IsNone())
	{
		Incoming.ItemId = Item->ItemType;
	}
	if (Incoming.Count <= 0)
	{
		Incoming.Count = 1;
	}
	if (Item->ItemId.IsValid())
	{
		Incoming.InstanceId = Item->ItemId; // actor's canonical id wins
	}
	if (Incoming.ItemId.IsNone())
	{
		return; // nothing identifiable to pick up
	}
	const FName ItemId = Incoming.ItemId;
	const int32 Count = Incoming.Count;

	const UItemDefinition* Def = LoadDefinition(ItemId);
	const float AddedWeight = (Def ? Def->GetWeightKg() : WeightForItem(ItemId)) * Count;
	if (!CanCarryWeight(AddedWeight))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Pickup rejected: overweight"));
		return;
	}

	// Identity-preserving placement: the item lands as its OWN instance so
	// guid, condition and lineage survive the transition. Only when the
	// grid is completely full do we merge into a compatible stack — the
	// documented merge where the surviving stack's identity wins.
	const int32 Free = FirstFreeSlotIndex();
	if (Free >= 0)
	{
		Slots[Free].Item = Incoming;
		// Legacy wallet sync — mirrors ServerAddItemToSlots exactly so both
		// placement paths behave identically for existing consumers.
		if (int32* C = FindEntry(ItemId))
		{
			*C = FMath::Clamp(*C + Count, 0, 9999);
		}
		else
		{
			Items.Add({ ItemId, Count });
		}
		if (ItemId == TEXT("AF_R_Wood") || ItemId == TEXT("res_wood_log"))
		{
			Wood = GetCount(TEXT("AF_R_Wood")) + GetCount(TEXT("res_wood_log"));
		}
	}
	else
	{
		// Full grid: fall back to the existing stacking path (identity
		// merges into the surviving stack — logged, never silent).
		UE_LOG(LogAfterfall, Warning,
			TEXT("Pickup: grid full, merging %s into a stack (instance id not preserved)"),
			*ItemId.ToString());
		ServerAddItemToSlots(ItemId, Count);
	}

	// Keep the actor's canonical id in sync, then move it to the off-world
	// Stored state (hidden, collision off) so physics/visuals match the
	// authoritative inventory ownership instead of leaving a live ghost.
	Item->ItemId = Incoming.InstanceId;
	Item->ServerStore();
	BroadcastInventoryChanged();
}

void UInventoryComponent::OnRep_Wood() {}
void UInventoryComponent::OnRep_Items() {}
void UInventoryComponent::OnRep_Slots()
{
	BroadcastInventoryChanged();
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInventoryComponent, Wood);
	DOREPLIFETIME(UInventoryComponent, Items);
	DOREPLIFETIME(UInventoryComponent, Slots);
	DOREPLIFETIME(UInventoryComponent, EquipmentSlots);
}

bool UInventoryComponent::ServerAddResource_Validate(int32 Amount)
{
	return Amount > 0 && Amount <= 100;
}

void UInventoryComponent::ServerAddResource_Implementation(int32 Amount)
{
	Wood = FMath::Clamp(Wood + Amount, 0, 9999);
	if (int32* C = FindEntry(TEXT("AF_R_Wood")))
	{
		*C = FMath::Clamp(*C + Amount, 0, 9999);
	}
	else
	{
		Items.Add({ TEXT("AF_R_Wood"), Amount });
	}
	BroadcastInventoryChanged(); // resource grant must refresh derived load
}

bool UInventoryComponent::ServerAddItem_Validate(FName ItemId, int32 Count)
{
	return !ItemId.IsNone() && Count > 0 && Count <= 100;
}

void UInventoryComponent::ServerAddItem_Implementation(FName ItemId, int32 Count)
{
	const float NewWeight = GetWeightKg() + WeightForItem(ItemId) * Count;
	if (NewWeight > MaxWeightKg + 0.001f)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("AddItem rejected: overweight %.1f/%.1f"), NewWeight, MaxWeightKg);
		return;
	}
	if (int32* C = FindEntry(ItemId))
	{
		*C = FMath::Clamp(*C + Count, 0, 9999);
	}
	else
	{
		Items.Add({ ItemId, Count });
	}
	if (ItemId == TEXT("AF_R_Wood") || ItemId == TEXT("res_wood_log"))
	{
		Wood = GetCount(TEXT("AF_R_Wood")) + GetCount(TEXT("res_wood_log"));
	}
	BroadcastInventoryChanged(); // add must refresh derived load
}

bool UInventoryComponent::ServerTryConsume(int32 Cost)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (Cost <= 0)
	{
		return false;
	}
	if (int32* C = FindEntry(TEXT("AF_R_Wood")))
	{
		if (*C < Cost)
		{
			return false;
		}
		*C -= Cost;
		Wood = *C + GetCount(TEXT("res_wood_log"));
		BroadcastInventoryChanged(); // mass changed -> recompute carried load
		return true;
	}
	if (Wood < Cost)
	{
		return false;
	}
	Wood -= Cost;
	BroadcastInventoryChanged(); // legacy wallet path: same recompute rule
	return true;
}

bool UInventoryComponent::ServerTryConsumeItem(FName ItemId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ServerTryConsumeItem without authority — rejected"));
		return false;
	}
	if (ItemId.IsNone() || Count <= 0)
	{
		return false;
	}
	if (int32* C = FindEntry(ItemId))
	{
		if (*C < Count)
		{
			return false;
		}
		*C -= Count;
		if (*C <= 0)
		{
			Items.RemoveAll([&](const FInventoryEntry& E) { return E.ItemId == ItemId; });
		}
		BroadcastInventoryChanged(); // consume must refresh derived load
		return true;
	}
	return false;
}

// ---------- Phase 5: instance-level combination (lineage-aware) ----------

bool UInventoryComponent::TryCombine(const TArray<FGuid>& InputInstanceIds, FName RecipeId,
	const FGuid& TransactionId, FString& OutError)
{
	OutError.Reset();

	if (!TransactionId.IsValid())
	{
		OutError = TEXT("invalid transaction id");
		return false;
	}
	if (RecentCombineTransactions.Contains(TransactionId))
	{
		OutError = TEXT("duplicate transaction id — already committed");
		return false;
	}
	if (InputInstanceIds.Num() < 1 || InputInstanceIds.Num() > 8)
	{
		OutError = TEXT("combine needs between 1 and 8 input instances");
		return false;
	}
	const FCraftRecipe* Recipe = UCraftingService::FindRecipe(RecipeId);
	if (!Recipe)
	{
		OutError = FString::Printf(TEXT("unknown recipe %s"), *RecipeId.ToString());
		return false;
	}

	// Resolve caller-supplied ids to concrete live instances.
	TArray<FItemInstance> Provided;
	for (const FGuid& Id : InputInstanceIds)
	{
		const FInventorySlot* Found = nullptr;
		for (const FInventorySlot& Slot : Slots)
		{
			if (!Slot.IsEmpty() && Slot.Item.InstanceId == Id)
			{
				Found = &Slot;
				break;
			}
		}
		if (!Found)
		{
			OutError = FString::Printf(TEXT("input instance %s not in inventory"),
				*Id.ToString(EGuidFormats::Digits));
			return false;
		}
		Provided.Add(Found->Item);
	}

	// Pure validation: the engine plans consumption before anything mutates.
	TArray<FCombineConsumeStep> Plan;
	if (!UItemCombiner::PlanConsumption(*Recipe, Provided, Plan, OutError))
	{
		return false;
	}

	// --- Working copy: every check below runs against it; Slots is only
	// replaced at the commit point, so any failure leaves inventory intact.
	TArray<FInventorySlot> Working = Slots;

	// 1. Consume planned amounts. A partially consumed stack keeps its id.
	for (const FCombineConsumeStep& Step : Plan)
	{
		const FGuid& Id = Provided[Step.ProvidedIndex].InstanceId;
		for (FInventorySlot& S : Working)
		{
			if (!S.IsEmpty() && S.Item.InstanceId == Id)
			{
				S.Item.Count -= Step.Count;
				if (S.Item.Count <= 0)
				{
					S.Item = FItemInstance();
				}
				break;
			}
		}
	}

	// 2. Weight budget: consumed mass leaves, output mass arrives.
	float FreedKg = 0.f;
	for (const FCombineConsumeStep& Step : Plan)
	{
		const FItemInstance& P = Provided[Step.ProvidedIndex];
		const UItemDefinition* Def = LoadDefinition(P.ItemId);
		FreedKg += (Def ? Def->GetWeightKg() : WeightForItem(P.ItemId)) * Step.Count;
	}
	const UItemDefinition* OutDef = LoadDefinition(Recipe->OutputItem);
	const float OutKg = (OutDef ? OutDef->GetWeightKg() : WeightForItem(Recipe->OutputItem))
		* Recipe->OutputCount;
	if (GetWeightKg() - FreedKg + OutKg > MaxWeightKg + 0.001f)
	{
		OutError = TEXT("combine rejected: overweight");
		return false;
	}

	// 3. Mint the child (new identity + lineage) and place it as its OWN
	// slot: merging into an existing stack would erase the very identity
	// and provenance this operation creates. Full inventory = no commit.
	const FCombineOutcome Child = UItemCombiner::BuildChild(*Recipe, Provided, TransactionId);
	if (!Child.bSuccess)
	{
		OutError = Child.Error;
		return false;
	}
	int32 Dest = -1;
	for (int32 i = 0; i < Working.Num(); ++i)
	{
		if (Working[i].IsEmpty() && !Working[i].bLocked)
		{
			Dest = i;
			break;
		}
	}
	if (Dest < 0)
	{
		OutError = TEXT("combine rejected: no free slot for output");
		return false;
	}
	Working[Dest].Item = Child.Child;

	// --- Commit (single atomic replacement + bounded idempotency ledger).
	Slots = MoveTemp(Working);
	RecentCombineTransactions.Add(TransactionId);
	if (RecentCombineTransactions.Num() > MaxRecentCombineTransactions)
	{
		RecentCombineTransactions.RemoveAt(0,
			RecentCombineTransactions.Num() - MaxRecentCombineTransactions);
	}
	BroadcastInventoryChanged();
	UE_LOG(LogAfterfall, Log, TEXT("Combine %s committed (tx %s): child %s from %d parent(s)"),
		*RecipeId.ToString(), *TransactionId.ToString(EGuidFormats::Digits),
		*Child.Child.InstanceId.ToString(EGuidFormats::Digits),
		Child.Child.ParentInstanceIds.Num());
	return true;
}

bool UInventoryComponent::ServerCombineItems_Validate(const TArray<FGuid>& InputInstanceIds,
	FName RecipeId, FGuid TransactionId)
{
	return InputInstanceIds.Num() >= 1 && InputInstanceIds.Num() <= 8
		&& !RecipeId.IsNone() && TransactionId.IsValid();
}

void UInventoryComponent::ServerCombineItems_Implementation(const TArray<FGuid>& InputInstanceIds,
	FName RecipeId, FGuid TransactionId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FString Error;
	if (!TryCombine(InputInstanceIds, RecipeId, TransactionId, Error))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Combine %s rejected: %s"),
			*RecipeId.ToString(), *Error);
	}
}

bool UInventoryComponent::ServerCombineFirstAvailable(const FGuid& TransactionId, FString& OutError)
{
	OutError.Reset();
	if (RecentCombineTransactions.Contains(TransactionId))
	{
		OutError = TEXT("duplicate transaction id — already committed");
		return false;
	}
	for (const FCraftRecipe& Recipe : UCraftingService::GetSliceRecipes())
	{
		// Collect the concrete instances covering this recipe's inputs.
		TArray<FGuid> Ids;
		for (const FCraftIngredient& In : Recipe.Inputs)
		{
			int32 Need = In.Count;
			for (const FInventorySlot& S : Slots)
			{
				if (Need <= 0)
				{
					break;
				}
				if (S.IsEmpty() || S.Item.ItemId != In.ItemId)
				{
					continue;
				}
				Ids.AddUnique(S.Item.InstanceId);
				Need -= S.Item.Count;
			}
			if (Need > 0)
			{
				Ids.Reset();
				break;
			}
		}
		if (Ids.Num() == 0)
		{
			continue; // inputs not available — try the next rule
		}
		// First satisfiable rule in table order is the slice's deterministic
		// selection policy; environmental failures (weight, capacity) are not
		// recipe-specific, so they stop the search instead of masking.
		return TryCombine(Ids, Recipe.RecipeId, TransactionId, OutError);
	}
	OutError = TEXT("no recipe's inputs are available in this inventory");
	return false;
}
