# AFTERFALL — C++ Conversion & Inventory Rollout (Muse/DeepSeek/FLINT)

**Project:** `RealGame.uproject` (UE 5.8, C++), module `Afterfall`
**Canonical target:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject`
**Engine:** UE 5.8 at `C:\Program Files\Epic Games\UE_5.8\` (UnrealEditor.exe present)
**Blocker:** VS C++ workload NOT installed (`VC/Tools/MSVC/` absent) — C++ compilation blocked until installed.

---

## 1. Existing system audit (Source/Afterfall/)

| Domain | Class/File | Role | Status |
|---|---|---|---|
| Inventory | `Inventory/InventoryComponent.h/.cpp` | Server-authoritative wallet: `Items` (ItemId+Count), Wood legacy, weight cap, `ServerAddItem`, `ServerTryConsumeItem`, replicated. No item definitions, no categories, no equipment slots, no UI events. | PARTIAL (wallet only) |
| Medical/Injury | `Combat/InjuryStateComponent.h/.cpp` | Per-body-part injury ledger (6 parts), treatment sessions, bleeding drain, `ServerBeginTreatment`, `ServerAdvance`, `ConsumePendingHeal`. Canonical. | COMPLETE |
| Medical capabilities | `Combat/MedicalCapability.h/.cpp` | `UMedicalCapabilityLib` data-driven table: bandage, kit, pills, antiseptic, antibiotic. Item IDs match inventory IDs. | COMPLETE |
| Health | `Combat/HealthComponent.h/.cpp` | Server-authoritative HP, `ServerTakeDamage`, `ServerHeal`, `ServerRevive`, `OnHealthDepleted`. | COMPLETE |
| Interaction | `Interaction/Interactable.h` | `UInteractable` interface: `GetInteractPrompt`, `CanInteract`, `ServerInteract`. | COMPLETE |
| Interaction | `Interaction/InteractComponent.h/.cpp` | Camera-focus 10Hz tick, range check, prompt display. | COMPLETE |
| Carry | `Interaction/CarryComponent.h/.cpp` | Single held slot, `ServerPickup`/`ServerDrop`/`ServerThrow`, replicated. | COMPLETE |
| Mat/Storage | `Interaction/MatInteractionComponent.h/.cpp` | Physical inventory mat open/close, server-owned. | COMPLETE |
| World pickup | `World/PhysicalItem.h/.cpp` | `APhysicalItem : AActor, IInteractable` — persistent world item, `ServerPickupBy`/`ServerReleaseToWorld`/`ServerStore`. | COMPLETE |
| Crafting | `Crafting/CraftingService.h/.cpp` | Recipe table, `HasInputs`, `DismantleReturn`. Server authority. | COMPLETE |
| Loot | `Loot/LootService.h/.cpp` + `LootProfiles.h` | Location-based loot rolls, rarity, guarantees, unique first-mints. | COMPLETE |
| Persistence | `Persistence/PersistenceService.h/.cpp` + `RegionStateSave.h` | Save versioning, GUIDs, tombstones, cell keys, catch-up. Pure helpers. | COMPLETE |
| Health/Damage | `Combat/DamageResolver.h/.cpp` + `BodyDamageTypes.h` | Per-part damage, lethality conditions, armor mitigation. | COMPLETE |
| UI | `UI/AfterfallHUD.h/.cpp` + `RadialMenuComponent.h/.cpp` | HUD, radial menu, quick menu. No inventory tab UI yet. | PARTIAL (no inventory UI) |
| Character | `AfterfallCharacter.h/.cpp` | Pawn with Inventory, Health, Injury components; `ServerSelfTreatBandage`, `ServerInteractRequest`. | COMPLETE |
| Tests | `Tests/` (19 files) | Automation tests for inventory, medical, construction, destruction, loot, persistence, etc. | COMPLETE |

**Item IDs already established** (used across inventory, medical, crafting, loot):
- `AF_R_Wood` / `res_wood_log` — resource
- `AF_R_Stone` — resource
- `AF_R_HempFiber` — resource
- `AF_R_Scrap` / `res_scrap_metal` — resource
- `AF_M_Plank` / `mat_plank` — material
- `AF_M_Rope` / `mat_rope` — material
- `AF_M_Cloth` — material
- `AF_M_Fasteners` / `mat_nail` — material
- `AF_F_Canned` — food
- `AF_MED_Bandage` / `res_med_bandage` — medical
- `res_med_kit` — medical
- `res_med_pills` — medical
- `med_antiseptic` — medical
- `med_antibiotic` — medical

---

## 2. Proposed native C++ classes (Muse — gameplay implementation)

All new files under `Source/Afterfall/Inventory/`. No existing canonical class is replaced; existing `UInventoryComponent` is extended additively.

### 2.1 `EItemCategory` (enum, new file or in ItemTypes.h)

Matches the 7 inventory tabs. BlueprintType for designer exposure.

| Enum value | Tab | Example item IDs |
|---|---|---|
| `All` | All | — |
| `Weapons` | Weapons | `WPN_*` |
| `Equipment` | Equipment | `EQP_*` |
| `Medical` | Medical | `res_med_*`, `med_*` |
| `Resources` | Resources | `AF_R_*`, `res_*` |
| `FoodDrink` | Food & Drink | `AF_F_*` |
| `Quest` | Quest / Key Items | `QUEST_*`, `KEY_*` |

### 2.2 `UItemDefinition` (UPrimaryDataAsset, new: `ItemDefinition.h/.cpp`)

Data-driven item definition. Designers create `.uasset` instances in `Content/Data/Items/`. No hardcoded item classes.

Properties:
- `ItemId` (FName) — stable identifier
- `DisplayName` (FText)
- `Description` (FText)
- `Icon` (UTexture2D*)
- `Category` (EItemCategory)
- `WeightKg` (float)
- `bStackable` (bool)
- `MaxStack` (int32)
- `bUsable` (bool)
- `bEquippable` (bool)
- `EquipmentSlot` (FName) — e.g. "Primary", "Secondary", "Head", "Torso"
- `MedicalItemId` (FName) — cross-ref to MedicalCapability table when applicable

### 2.3 `FItemInstance` (struct, new: in ItemTypes.h)

Runtime instance of an item in an inventory slot.
- `InstanceId` (FGuid) — stable per-instance identity
- `Definition` (TSoftObjectPtr<UItemDefinition>) — soft ref to data asset
- `Count` (int32) — for stackables
- `Condition` (float, 0..1) — durability/condition for equipment/weapons
- `CustomData` (FString) — designer-extensible JSON-ish payload

### 2.4 `FInventorySlot` (struct, new: in ItemTypes.h)

- `Item` (FItemInstance)
- `Capacity` (int32) — max stack for this slot
- `bLocked` (bool)

### 2.5 Extended `UInventoryComponent` (modify: `InventoryComponent.h/.cpp`)

Additive extension only. Existing `Wood`, `Items`, `ServerAddItem`, `ServerTryConsumeItem` preserved.

New:
- `TArray<FInventorySlot> Slots` — replicated slot array
- `TMap<FName, UItemDefinition*> ItemDefinitions` — loaded definition cache
- `FOnInventoryChanged OnInventoryChanged` — multicast delegate (BlueprintAssignable)
- `FOnItemEquipped OnItemEquipped`, `FOnItemUnequipped OnItemUnequipped`, `FOnItemConsumed OnItemConsumed`
- `ServerEquipItem(FName SlotName, FGuid InstanceId)`
- `ServerUnequipItem(FName SlotName)`
- `ServerDropItem(FGuid InstanceId, int32 Count)`
- `ServerSplitItem(FGuid InstanceId, int32 Count)`
- `ServerUseItem(FGuid InstanceId)` — medical routes to InjuryStateComponent
- `ServerPickupItem(APhysicalItem* Item)` — world pickup
- `GetItemsByCategory(EItemCategory)` — UI tab filter
- `GetEquippedItem(FName SlotName)`
- `WeightForItemDefinition(UItemDefinition*)` — replaces hardcoded WeightForItem map over time

### 2.6 `APickupItem` (new or extend `APhysicalItem`)

`APhysicalItem` already implements `IInteractable` and world pickup. Extend it with:
- `FItemInstance ItemData` — the item this pickup represents
- `ServerInteract_Implementation` routes to inventory `ServerPickupItem`

No new actor class needed unless `APhysicalItem` is insufficient.

---

## 3. Medical integration (Medical tab)

The Medical tab uses the **canonical** path only:
- `UInjuryStateComponent::ServerBeginTreatment(EBodyPart, ItemId)` — begins treatment session
- `UMedicalCapabilityLib::FindCapability(ItemId)` / `CapabilityMatchesInjury` — validates item vs wound
- `UInjuryStateComponent::ServerAdvance` — treatment progress + completion (consumes item via `ServerTryConsumeItem`, applies bleeding/pain/stabilize/HP effects)
- `UHealthComponent::ServerHeal` — HP restoration via `ConsumePendingHeal`

**No competing health system.** Bandage use = `ServerUseItem` → `ServerBeginTreatment` → item consumed on completion → injury state updated. UI displays `PartInjuries`, `TreatmentState`, `BleedRateHpS`, `PendingHPRestore`.

Existing tests: `Tests/MedicalTreatmentTest.cpp` (3 tests, all pass).

---

## 4. Weapons & equipment integration

Existing combat/weapon surface:
- `AfterfallCharacter`: `AmmoInMag`, `MagSize`, `FireIntervalSec`, `ServerReload`, `ServerFireHitscan`, `ServerAttack` (melee)
- `Combat/BodyDamageTypes.h`: `UBodyDamageDataAsset`, `EBodyPart`, `EDamageLethalCondition`, `FWeaponDamageConfig`
- `Combat/DamageResolver.h/.cpp`: damage resolution

Inventory items connect via:
- `UItemDefinition.EquipmentSlot` / `bEquippable`
- `UInventoryComponent.ServerEquipItem` / `ServerUnequipItem`
- Character equips weapon → `ServerFireHitscan`/`ServerAttack` use equipped weapon's damage config

Integration boundary: if weapon actor classes don't exist yet, `ServerEquipItem` equips the item definition reference and marks the slot; actual weapon firing is gated on existing `ServerFireHitscan`/`ServerAttack`. Marked as integration boundary — no fake weapon system created.

---

## 5. UI requirements (UMG, visual composition in UMG; authority in C++)

- Tab buttons (7) filtering `GetItemsByCategory(EItemCategory)` from the same authoritative `UInventoryComponent`
- Item rows: icon (`UItemDefinition.Icon`), name (`DisplayName`), quantity (`Count`), description (`Description`)
- Selected-item state (highlight + details panel)
- Context actions: Use / Equip / Unequip / Drop / Split (enabled per item capabilities)
- Empty state + full-inventory feedback (weight vs `MaxWeightKg`)
- Keyboard/gamepad navigation (focusable buttons, directional nav)
- Delegates bound to `OnInventoryChanged`, `OnItemEquipped`, `OnItemUnequipped`, `OnItemConsumed` for reactive updates
- Medical tab reflects `InjuryStateComponent.PartInjuries` + `TreatmentState`; Use on a bandage invokes `ServerUseItem` → `ServerBeginTreatment`

No parallel inventory model inside the widget. Widget reads from `UInventoryComponent` only.

---

## 6. Save/load compatibility

Existing `PersistenceService` is pure (no IO). `UPersistenceService::MakeStableGuid`, `IsSaveCompatible`, `CellKeyForLocation`, `ShouldTombstone`, `CatchUpProgress`.

Inventory save format (additive):
- `FInventoryEntry` array already exists (ItemId + Count) — reuse
- Add `FItemInstance` serialization for equipped/condition-tracked items
- Save version bump handled by `IsSaveCompatible` (major.minor)

No new save system; existing architecture extended.

---

## 7. Conversion status matrix

| Candidate | Original | Proposed C++ | Dependencies | Status | Tests |
|---|---|---|---|---|---|
| Item wallet | `InventoryComponent.Items` (FInventoryEntry[]) | `UItemDefinition` + `FItemInstance` + `Slots` | CoreUObject, Engine | PLANNED | `Afterfall.Inventory.*` |
| Item categories | none (hardcoded strings) | `EItemCategory` enum | Core | PLANNED | `Afterfall.Inventory.Categories` |
| Stacking / split | inline in `ServerAddItem`/`ServerTryConsumeItem` | `ServerSplitItem`, slot capacity | InventoryComponent | PLANNED | `Afterfall.Inventory.Stack` |
| Equip / unequip | none | `ServerEquipItem`/`ServerUnequipItem` | InventoryComponent, Character | PLANNED | `Afterfall.Inventory.Equip` |
| World pickup | `APhysicalItem` + `ServerInteract` | `ServerPickupItem` on InventoryComponent | PhysicalItem, Interactable | PLANNED | `Afterfall.Inventory.Pickup` |
| Medical use | `InjuryStateComponent.ServerBeginTreatment` | `ServerUseItem` → treatment | InjuryStateComponent, MedicalCapabilityLib | EXISTING (route only) | `MedicalTreatmentTest` (pass) |
| UI tabs | none | UMG widget + C++ filter | UMG, InventoryComponent | PLANNED | PIE |
| Save/load | `PersistenceService` (pure) | inventory serialization | PersistenceService | PLANNED | `PersistenceTest` |

---

## 8. Build / test commands (blocked until MSVC installed)

```
# Editor build (needs MSVC + generated project files):
Build.bat AfterfallEditor Win64 Development RealGame.uproject -WaitMutex

# Functional checks (no engine, PASS baseline):
python3 Scripts/validate_phase3.py          # PASS
python3 Scripts/validate_inventory_crafting.py  # PASS
python3 Scripts/validate_loot.py            # PASS
python3 Scripts/validate_lootservice.py     # PASS
python3 Scripts/validate_persistence.py     # PASS
python3 Scripts/validate_body_damage.py     # PASS
python3 Scripts/validate_destruction.py     # PASS
python3 Scripts/validate_entity_lifecycle.py # PASS
python3 Scripts/validate_slice.py           # PASS
python3 Scripts/validate_stage2.py          # PASS
python3 Scripts/validate_agent_link.py      # PASS
python3 Scripts/validate_perf_budget.py     # PASS
python3 Scripts/validate_first_playable.py  # PASS

# UE automation (needs editor build):
Automation RunTests Afterfall
```

---

## 9. Known blockers

1. **VS C++ workload missing** — `VC/Tools/MSVC/` absent. Cannot compile C++ or run UE automation/PIE. Fix: Visual Studio Installer → Modify → "Desktop development with C++" + Windows 11 SDK.
2. **Shader archive missing** — `Builds/Windows/RealGame/Content/ShaderArchive-Global-PCD3D_SM5-PCD3D_SM5.ushaderbytecode` absent. Cook/package incomplete (per `BUILD_RECOVERY_HANDOFF.md`).
3. **No PhysicsAsset for Manny** — `Content/Characters/UE5_Mannequins/Meshes/` has `SK_Mannequin` + `SKM_Manny_Simple` but no `*PhysicsAsset*`. Must be created in-editor (`SK_Mannequin` → Create → Physics Asset).
4. **`TestNeighborhood` map not built** — `DefaultEngine.ini` points to `Lvl_FirstPerson`; `TestNeighborhood` is commented out. World Partition map needed for partitioned playable area.
