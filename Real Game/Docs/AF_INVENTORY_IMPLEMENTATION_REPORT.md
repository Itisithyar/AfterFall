# AFTERFALL — Inventory Implementation Report

**Canonical project:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject` (UE 5.8, C++, module `Afterfall`)
**Owner:** Muse (Gameplay/Inventory)

## Summary

Implemented the native C++ gameplay layer for inventory and item interaction, extended additively onto the existing Phase A wallet (no existing canonical classes replaced). The seven-tab inventory uses one authoritative item collection filtered by category via `UInventoryComponent::GetItemsByCategory(EItemCategory)`.

## Files changed

| File | Change |
|---|---|
| `Source/Afterfall/Inventory/ItemTypes.h` (new) | `EItemCategory` enum (7 tabs), `FItemInstance`, `FInventorySlot` structs |
| `Source/Afterfall/Inventory/ItemDefinition.h` (new) | `UItemDefinition : UPrimaryDataAsset` — data-driven item definitions |
| `Source/Afterfall/Inventory/ItemDefinition.cpp` (new) | Constructor implementation |
| `Source/Afterfall/Inventory/InventoryComponent.h` (extended) | Slots array, `EquipmentSlots`, change delegates, equip/unequip/drop/split/use/pickup RPCs, `GetItemsByCategory` |
| `Source/Afterfall/Inventory/InventoryComponent.cpp` (extended) | Full implementations of all new RPCs, stacking, weight checks, medical routing, pickup |
| `Source/Afterfall/AfterfallCharacter.h` (extended) | `IA_Inventory`, `bInventoryOpen`, `ToggleInventory`/`OpenInventory`/`CloseInventory`/`OnInventoryPressed`, `IsInventoryOpen` |
| `Source/Afterfall/AfterfallCharacter.cpp` (extended) | Tab + gamepad DPad-Left input mapping, action binding, toggle impl, movement/look gating |
| `Source/Afterfall/Slice/SliceSubsystem.cpp` (fixed) | `SetMobility(Movable)` before `SetStaticMesh` on runtime-spawned floor + lab blocks (StaticMobility warning fix) |
| `Docs/AF_CPP_CONVERSION_INVENTORY.md` (created) | Full audit + conversion plan + status matrix |
| `Docs/WORLD_ARCHITECTURE.md` (created) | Modular world directory architecture |
| `Docs/WORLD_CONTENT_MANIFEST.md` (created) | Content inventory + mapping |
| `Docs/ASSET_DEPENDENCY_MAP.md` (created) | Asset dependencies + cook/packaging notes |
| `Docs/DIRECTORY_CHANGE_LOG.md` (created) | Directory creation log |

## Native C++ foundation

### `EItemCategory` (ItemTypes.h)
BlueprintType enum matching the 7 tabs: `All`, `Weapons`, `Equipment`, `Medical`, `Resources`, `FoodDrink`, `Quest`.

### `UItemDefinition` (ItemDefinition.h/.cpp)
`UPrimaryDataAsset`. Designers create `.uasset` instances under `Content/Data/Items/`; no hardcoded item classes. Fields: `ItemId`, `DisplayName`, `Description`, `Icon`, `Category`, `WeightKg`, `bStackable`, `MaxStack`, `bUsable`, `bEquippable`, `EquipmentSlot`, `MedicalItemId`.

### `FItemInstance` / `FInventorySlot` (ItemTypes.h)
Runtime instance: `ItemId`, `InstanceId` (FGuid), `Count`, `Condition`, `CustomData`. Slot: `SlotIndex`, `Item`, `Capacity`, `bLocked`, `IsEmpty()`, `FreeSpace()`, `CanAccept()`.

### `UInventoryComponent` (extended, additive)
- `Slots` (replicated `TArray<FInventorySlot>`)
- `ItemDefinitions` (soft-loaded `TMap<FName, TSoftObjectPtr<UItemDefinition>>`)
- `EquipmentSlots` (replicated `TMap<FName, FGuid>`)
- Delegates: `OnInventoryChanged`, `OnItemEquipped`, `OnItemUnequipped`, `OnItemConsumed`
- RPCs: `ServerAddItemToSlots`, `ServerEquipItem`, `ServerUnequipItem`, `ServerDropItem`, `ServerSplitItem`, `ServerUseItem`, `ServerPickupItem`
- Query: `GetItemsByCategory(EItemCategory)`, `GetEquippedItem(FName)`, `GetWeightKg()`

Phase A wallet (`Wood`, `Items`, `ServerAddItem`, `ServerTryConsumeItem`, `ServerAddResource`) fully preserved.

## Seven-tab mapping (final)

| Tab | EItemCategory | Example item IDs |
|---|---|---|
| All | `All` | any |
| Weapons | `Weapons` | `WPN_*` |
| Equipment | `Equipment` | `EQP_*`, `bEquippable` items |
| Medical | `Medical` | `AF_MED_Bandage`, `res_med_kit`, `med_*` |
| Resources | `Resources` | `AF_R_Wood`, `AF_R_Stone`, `res_wood_log` |
| Food & Drink | `FoodDrink` | `AF_F_Canned` |
| Quest / Key Items | `Quest` | `QUEST_*`, `KEY_*` |

## Medical integration (canonical path)

Medical tab items use `UInjuryStateComponent` + `UMedicalCapabilityLib` only — no competing health system:
- `ServerUseItem` → if item has `MedicalItemId`, calls `Injury->ServerBeginTreatment(EBodyPart, MedicalItemId)` on the first wounded matching part
- `UMedicalCapabilityLib::FindCapability(ItemId)` + `CapabilityMatchesInjury` validates item vs wound (bleeding/pain/severity/stabilized)
- `UInjuryStateComponent::ServerAdvance` consumes item via `ServerTryConsumeItem`, applies bleeding/pain/stabilize/HP effects
- `UHealthComponent::ServerHeal` restores HP via `ConsumePendingHeal`

Existing tests `Tests/MedicalTreatmentTest.cpp` (3 tests) remain green.

## Weapons & equipment integration

Connected to existing combat architecture:
- `UItemDefinition.EquipmentSlot` / `bEquippable` drive `ServerEquipItem`/`ServerUnequipItem`
- Character equips → equipped weapon damage config used by existing `ServerFireHitscan`/`ServerAttack`
- If weapon actor classes don't exist yet, `ServerEquipItem` equips the definition reference and marks the slot; integration boundary clearly marked — no fake weapon system created.

## UI input (Tab toggle)

- `Tab` opens/closes the inventory; gamepad `DPad-Left` also toggles
- Movement and look are gated when inventory open
- Cursor shows on open, hides on close; input mode `GameAndUI` / `GameOnly`
- No pause on inventory open (single-player pause is an explicit configurable design choice, not assumed)

## Build & verification

| Command | Result |
|---|---|
| `Build.bat AfterfallEditor Win64 Development RealGame.uproject` | Succeeded |
| `Build.bat Afterfall Win64 Development RealGame.uproject` | Succeeded |
| PIE: AfterfallGameMode (Seed=1337) + AfterfallCharacter spawn | Verified (log) |
| PIE: no `SetStaticMesh` Mobility warning | Verified (log) |
| `Validate_phase3` … `validate_first_playable` (13 Python mirrors) | All pass |

## Known limitations / next steps

- Tabbed inventory UI widget (UMG) not yet implemented — input wiring done; widget + 7 tab buttons + item rows pending
- No `Content/Data/Items/*.uasset` item definitions authored yet — designers need to create them
- PIE Tab keypress not automated (headless) — manual verification needed
- Packaged build not yet run with new code

## Memory (8 GB budget)

Build used VS Community 2026 MSVC 14.50.35717 + Windows 10 SDK. System has 32 GB RAM; editor/build peak well under 8 GB. No concurrent builds or duplicate editors run.
