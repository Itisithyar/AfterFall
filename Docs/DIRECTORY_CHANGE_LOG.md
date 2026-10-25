# AFTERFALL — Directory Change Log

**Canonical project:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject`

## Date: 2026-10-25

## Change type: Additive directory creation only

No existing files were moved, renamed, overwritten, or deleted. All existing `Source/Afterfall/*`
and `Content/*` subdirectories remain in their original locations.

## Created directories (Source)

| Path | Purpose |
|---|---|
| `Source/Afterfall/Buildings/` | Building types, structural parts, destruction, doors, windows, loot containers |
| `Source/Afterfall/Players/` | Player character, movement, equipment, injury, inventory, interaction, animation, UI, input |
| `Source/Afterfall/NPCs/` | Survivors, civilians, factions, dialogue, schedules, trading, companions |
| `Source/Afterfall/Zombies/` | Zombie archetypes, perception, pursuit, attacks, injuries, ragdolls, hordes |
| `Source/Afterfall/Geography/` | Terrain, roads, water, vegetation, weather, landmarks, World Partition |
| `Source/Afterfall/Zones/` | Districts, safe zones, infected zones, loot regions, spawn rules |
| `Source/Afterfall/WorldSystems/` | Interaction, damage, physics, loot, crafting, time/weather, save/load, events, navigation, audio, replication |
| `Source/Afterfall/Shared/` | Reusable base classes, interfaces, data assets |

## Created directories (Content)

| Path | Purpose |
|---|---|
| `Content/Buildings/` | Building meshes, materials, blueprints |
| `Content/Players/` | Character, equipment, inventory UI |
| `Content/NPCs/` | Survivor/civilian assets |
| `Content/Zombies/` | Zombie meshes, anims, physics assets |
| `Content/Geography/` | Landscape, terrain, roads, vegetation |
| `Content/Zones/` | Zone data assets, streaming |
| `Content/WorldSystems/` | Loot tables, crafting recipes, audio, VFX |
| `Content/Shared/` | Reusable materials, textures, meshes, audio, VFX, UI |
| `Content/AF_External/Buildings/` | Third-party building packs (isolated) |
| `Content/AF_External/Characters/` | Third-party character packs (isolated) |
| `Content/AF_External/Animations/` | Third-party animation packs (isolated) |
| `Content/AF_External/Environments/` | Third-party environment packs (isolated) |
| `Content/AF_External/Props/` | Third-party prop packs (isolated) |

## Created documentation

| Path | Purpose |
|---|---|
| `Docs/WORLD_ARCHITECTURE.md` | Modular world architecture design |
| `Docs/WORLD_CONTENT_MANIFEST.md` | Content inventory and mapping |
| `Docs/ASSET_DEPENDENCY_MAP.md` | Asset dependencies and cook/packaging notes |

## Conflicts and unresolved dependencies

| Item | Status |
|---|---|
| C++ compile blocker: `InventoryComponent.cpp` TSoftObjectPtr / FGuid errors | BLOCKED — build not green; do not touch compile-blocked files per constraints |
| MSVC toolchain | Partially resolved — VS Community 2026 MSVC present (14.16–14.52); Build Tools 2022 lacks MSVC |
| Shader archive missing | `Builds/Windows/RealGame/Content/ShaderArchive-Global-PCD3D_SM5-PCD3D_SM5.ushaderbytecode` absent |
| No Physics Asset for Manny | Must be created in-editor before ragdoll |
| No `TestNeighborhood.umap` | World Partition playable map not built |
| Zero audio assets | Footsteps, UI, ambient, weapon audio missing |

## Next steps (after build is green)

1. Fix `InventoryComponent.cpp` compile errors (FGuid comparison, TSoftObjectPtr construction).
2. Re-run `Build.bat AfterfallEditor Win64 Development` until green.
3. Implement/modularize one vertical slice at a time into the new directories.
4. Import verified free assets into `Content/AF_External/<Category>/<PackName>/`.
5. Run PIE, automation tests, and packaged build verification.
