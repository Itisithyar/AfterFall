# AFTERFALL — Asset Dependency Map

**Canonical project:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject` (UE 5.8)

## Asset categories and dependencies

| Category | Dependencies | Cook/packaging notes |
|---|---|---|
| Static meshes | Materials, Physics Assets (if simulated) | Hard-reference from cooked map or AlwaysCook |
| Skeletal meshes | Skeleton, Physics Asset, Materials, Animations | Skeleton must cook with mesh |
| Animations | Skeleton, optionally Curve Tables | Retarget if skeleton differs |
| Physics Assets | Skeletal mesh, collision primitives | Create in-editor for ragdoll |
| Materials | Textures, Material Functions | Instance chains must cook together |
| Textures | None (source) | Compression settings per platform |
| Audio cues | Sound waves, Sound classes | Attenuation settings |
| Niagara | Materials, meshes (optional) | Module dependencies |
| Data assets | Referenced classes/structs | Must be referenced by cooked content |

## Known asset state

- `Content/Characters/UE5_Mannequins/Meshes/SK_Mannequin.uasset` — skeleton, no Physics Asset (must be created in-editor before ragdoll)
- `Content/Characters/UE5_Mannequins/Meshes/SKM_Manny_Simple.uasset` — mesh → SK_Mannequin
- `Content/Characters/Mannequins/Anims/` — Death(6), Pistol(29), Rifle(39), Unarmed(28) — all → SK_Mannequin
- `Content/Data/Loot/DA_Location_*.json` — 6 loot profiles, referenced by LootService
- `Content/FirstPerson/Lvl_FirstPerson.umap` — startup map, references GameMode + external actors
- `Content/LevelPrototyping/` — graybox meshes + materials, engine-bound, always cook-safe

## Missing dependencies (gaps)

- No Physics Asset for any skeletal mesh (blocks ragdoll)
- No audio assets (footsteps, UI, ambient, weapon)
- No zombie meshes/anims (blocks zombie AI slice)
- `TestNeighborhood.umap` not built (blocks World Partition playable area)

## Cook verification

Assets survive packaging only if:
1. Referenced by a cooked map, OR
2. Added to `AlwaysCook` lists in `DefaultEngine.ini`, OR
3. Hard-referenced from a cooked Blueprint/DataAsset

Staged assets under `Content/Characters/UE5_Mannequins/` load in editor/PIE but will NOT ship
unless referenced by a cooked map.
