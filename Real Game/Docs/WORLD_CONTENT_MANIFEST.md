# AFTERFALL — World Content Manifest

**Canonical project:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject`

## Current Content inventory (pre-restructure)

| Path | Contents | Status |
|---|---|---|
| `Content/FirstPerson/` | Template FP blueprints, map, colorway material | READY (template) |
| `Content/LevelPrototyping/` | Graybox meshes, materials, door/jumppad interactables | READY (graybox) |
| `Content/Data/Loot/` | 6 location JSON loot profiles | READY |
| `Content/Characters/Mannequins/` | SK_Mannequin, SKM_Manny_Simple, SKM_Quinn_Simple, anims, materials, textures | READY (Epic template) |
| `Content/Characters/UE5_Mannequins/` | SK_Mannequin, SKM_Manny_Simple, M_Mannequin + instances + textures | STAGED (GAS subset) |
| `Content/AF_External/EngineTemplates/` | Variant_Combat, Variant_Horror, Variant_Shooter, Weapons (GrenadeLauncher, Pistol, Rifle) | STAGED (templates) |
| `Content/Input/` | IMC_Default, IMC_MouseLook, IA_* actions | READY |
| `Content/Collections/` | Empty | EMPTY |
| `Content/Developers/mckee/` | Developer scratch | DEV |
| `Content/__ExternalActors__/FirstPerson/` | External actor refs | EXTERNAL |

## Target Content directories (created)

| Path | Purpose | Populated from |
|---|---|---|
| `Content/Buildings/` | Building meshes, materials, blueprints | New / existing LevelPrototyping |
| `Content/Players/` | Character, equipment, inventory UI | Existing Characters + new UI |
| `Content/NPCs/` | Survivor/civilian assets | New |
| `Content/Zombies/` | Zombie meshes, anims, physics assets | New / imports |
| `Content/Geography/` | Landscape, terrain, roads, vegetation | New |
| `Content/Zones/` | Zone data assets, streaming | New |
| `Content/WorldSystems/` | Loot tables, crafting recipes, audio, VFX | Existing Data/Loot + new |
| `Content/Shared/` | Reusable materials, textures, meshes, audio, VFX, UI | Existing + new |
| `Content/AF_External/Buildings/` | Third-party building packs | Future imports |
| `Content/AF_External/Characters/` | Third-party character packs | Future imports |
| `Content/AF_External/Animations/` | Third-party animation packs | Future imports |
| `Content/AF_External/Environments/` | Third-party environment packs | Future imports |
| `Content/AF_External/Props/` | Third-party prop packs | Future imports |

## Current-to-target mapping (no files moved)

Existing content remains in place. New directories are additive. When assets are eventually
migrated into category folders, references will be preserved via redirectors and soft paths.

## Notes

- No existing files moved or overwritten.
- `TestNeighborhood` map not yet built (commented out in `DefaultEngine.ini`).
- Zero audio assets currently exist in `Content/`.
