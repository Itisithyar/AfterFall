# AFTERFALL — Template Compatibility Matrix

**Canonical project:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject` (UE 5.8, C++, module `Afterfall`)
**Owner:** DeepSeek (Asset/Template analysis & import)

## Template/BP audit — existing Blueprints & their parent classes

| Blueprint | Path | Parent C++ class | Dependencies | Convert to C++? | Status | Notes |
|---|---|---|---|---|---|---|
| BP_FirstPersonCharacter | Content/FirstPerson/Blueprints | ACharacter (Engine) | Camera, mesh, input | No | Keep | Template reference; C++ AfterfallCharacter replaces it in play |
| BP_FirstPersonPlayerController | Content/FirstPerson/Blueprints | APlayerController | Input | No | Keep | Swapped for AfterfallCharacter at runtime by SliceSubsystem |

## Existing C++ systems audit (Source/Afterfall/)

| Domain | Class | Role | C++ status | Convert needed? | Tests |
|---|---|---|---|---|---|
| Inventory | UInventoryComponent | Wallet + slots | C++ (Phase A + B extended) | No | InventoryCraftingTest |
| Item definitions | UItemDefinition | Data asset | C++ (new) | N/A | — |
| Medical injury | UInjuryStateComponent | Per-part injury ledger | C++ | No | MedicalTreatmentTest (pass) |
| Medical capability | UMedicalCapabilityLib | Data-driven treatment table | C++ | No | MedicalTreatmentTest (pass) |
| Health | UHealthComponent | HP, damage, heal, revive | C++ | No | — |
| Interaction | UInteractable (interface) | Interact prompt/can-interact | C++ | No | — |
| Interaction | UInteractComponent | Camera-focus tick, range | C++ | No | — |
| Carry | UCarryComponent | Held-slot pickup/drop/throw | C++ | No | — |
| World pickup | APhysicalItem | World item actor, IInteractable | C++ | No | — |
| Crafting | UCraftingService | Recipe table, HasInputs | C++ | No | — |
| Loot | ULootService | Location-based loot rolls | C++ | No | — |
| Persistence | UPersistenceService | Save versioning, GUIDs, cell keys | C++ (pure helpers) | No | PersistenceTest |
| Damage | UDamageResolver | Per-part damage, lethality | C++ | No | BodyDamageTest |
| Body types | UBodyDamageDataAsset | Damage config data | C++ | No | — |
| Destruction | UBoxBuildingLib / ABoxWallStructure | BoxWall construction + damage | C++ | No | DestructionTest |
| Structural | AStructuralActor | Structural parts | C++ | No | — |
| HUD | AAfterfallHUD | Canvas HUD, quick menu | C++ | No | — |
| Movement | UAfterfallMovementComponent | Movement tuning | C++ | No | — |
| Zombie | AZombiePrototype | Zombie AI prototype | C++ | No | — |

## Candidates for native C++ conversion

| Candidate | Original | Proposed C++ | Dependencies | Recommended? | Reason |
|---|---|---|---|---|---|
| Inventory data layer | Blueprint/wallet hybrid | UItemDefinition (UPrimaryDataAsset) + FItemInstance | CoreUObject, Engine | YES | Data-driven, no hardcoded item classes |
| Inventory slots/actions | Inline in InventoryComponent | ServerEquip/Unequip/Drop/Split/Use/Pickup RPCs | InventoryComponent | YES | Server authority, replication |
| Tabbed inventory UI | None (planned UMG) | UMG widget + C++ filter delegate | UMG, InventoryComponent | YES (widget visual in UMG) | UI separation of concerns |
| Medical use flow | Inventory → injury treatment | ServerUseItem → ServerBeginTreatment | InjuryStateComponent, MedicalCapabilityLib | PARTIAL | Routed in C++; UI widget pending |
| Equipment slots | None | EquipmentSlots TMap + equip RPCs | InventoryComponent | YES | Replicated equipment state |
| World pickup | APhysicalItem + ServerInteract | ServerPickupItem on InventoryComponent | PhysicalItem, IInteractable | YES | Routes world item into slots |

## Logic that must remain Blueprint-driven

| Logic | Reason |
|---|---|
| Animation Blueprints / Animation Graphs | Visual composition, state machines — stay in UMG/AnimBP |
| Material instances, textures, meshes | Visual assets — remain assets, not C++ |
| Level/map composition (Lvl_FirstPerson) | World Partition placement — editor-authored |
| Designer-tunable configuration (damage values, loot tables) | Data assets / JSON — stay data-driven |
| Widget visual layout | UMG visual composition — stays in UMG |

## Third-party plugin dependencies

| Plugin | Enabled | Required for Afterfall? | Notes |
|---|---|---|---|
| EnhancedInput | Yes | Yes | Input architecture |
| ProceduralMeshComponent | Yes | No (not currently used) | Available if needed |
| ModelingToolsEditorMode | No | No | Disabled |

No unavailable third-party plugins block C++ conversion. No asset model can be mechanically converted into C++; meshes/skeletons/anims/textures/materials remain assets.

## Skeleton compatibility (for future imports)

| Mesh | Skeleton | Compatible anims source | Retarget needed? |
|---|---|---|---|
| SK_Mannequin | SK_Mannequin | Mannequins/Anims/* | No |
| SKM_Manny_Simple | SK_Mannequin | Mannequins/Anims/* | No |
| SKM_Quinn_Simple | SK_Mannequin | Mannequins/Anims/* | No |
| Future zombie mesh | TBD | TBD | Likely yes (different skeleton) |

## Build verification

| Check | Result |
|---|---|
| `Build.bat AfterfallEditor Win64 Development` | Succeeded |
| `Build.bat Afterfall Win64 Development` | Succeeded |
| 13 Python validation mirrors | All pass |
| PIE: GameMode + character spawn | Verified |
| StaticMobility warning | Fixed (no warning) |
