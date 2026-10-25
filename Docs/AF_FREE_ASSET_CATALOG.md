# AFTERFALL — Free Asset Catalog

**Canonical project:** `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject` (UE 5.8)
**Owner:** DeepSeek (Asset/Template analysis & import)

## Catalog conventions

All third-party free packs are isolated under `Content/AF_External/<Category>/<PackName>/` with provenance + license recorded. Existing project assets are never overwritten.

## Already-staged assets (existing in project, not new imports)

| Path | Description | License | Dependencies | Status |
|---|---|---|---|---|
| `Content/Characters/UE5_Mannequins/Meshes/SK_Mannequin.uasset` | Epic starter mannequin skeletal mesh | Epic (UE EULA) | SK_Mannequin skeleton | Renders, no PhysicsAsset (blocks ragdoll) |
| `Content/Characters/UE5_Mannequins/Meshes/SKM_Manny_Simple.uasset` | Manny simple mesh | Epic (UE EULA) | SK_Mannequin | Staged |
| `Content/Characters/UE5_Mannequins/Meshes/SKM_Quinn_Simple.uasset` | Quinn simple mesh | Epic (UE EULA) | SK_Mannequin | Staged |
| `Content/Characters/UE5_Mannequins/Materials/M_Mannequin*` | Mannequin materials + instances | Epic (UE EULA) | Textures | Staged |
| `Content/Characters/UE5_Mannequins/Textures/*` | Mannequin textures | Epic (UE EULA) | — | Staged |
| `Content/Characters/Mannequins/Anims/Death/*` | Death anims (6) | Epic (UE EULA) | SK_Mannequin skeleton | Staged |
| `Content/Characters/Mannequins/Anims/Pistol/*` | Pistol anims (29) | Epic (UE EULA) | SK_Mannequin skeleton | Staged |
| `Content/Characters/Mannequins/Anims/Rifle/*` | Rifle anims (39) | Epic (UE EULA) | SK_Mannequin skeleton | Staged |
| `Content/Characters/Mannequins/Anims/Unarmed/*` | Unarmed anims (28) | Epic (UE EULA) | SK_Mannequin skeleton | Staged |
| `Content/FirstPerson/Lvl_FirstPerson.umap` | Startup map, graybox | Epic (UE EULA) | AfterfallGameMode, AfterfallCharacter | Playable in PIE |
| `Content/LevelPrototyping/*` | Graybox meshes, materials, doors, jumppads | Epic (UE EULA) | — | Staged |
| `Content/Data/Loot/DA_Location_*.json` | 6 loot location profiles | Internal (Afterfall) | LootService | Referenced |
| `Content/Input/IMC_Default.uasset`, `IMC_MouseLook.uasset` | Input mapping contexts | Internal | EnhancedInput | Used |
| `Content/Input/Actions/*`, `Content/Input/Touch/*` | Input actions | Internal | EnhancedInput | Used |

## Identified gaps (assets needed, not yet imported)

| Category | Needed | Why | Priority |
|---|---|---|---|
| Physics Asset | For SK_Mannequin / Manny / Quinn | Ragdoll, body physics | High |
| Zombie skeletal mesh | Infected/zombie character | Zombie AI slice | High |
| Zombie animations | Locomotion, attack, hit-reaction, death, stagger | Zombie AI slice | High |
| Zombie Physics Asset | Ragdoll for zombie | Zombie AI slice | High |
| Weapon meshes | Firearms, melee | Combat slice | Medium |
| Medical props | Bandage, kit, loot containers | Inventory/medical | Medium |
| Environment kits | Modular walls, doors, windows, rubble | World building | Medium |
| Horror materials/decals | Atmosphere | Presentation | Medium |
| Ambient audio | Footsteps, UI, weapon, impact | Presentation | Low |

## Imported assets (new)

None yet — no new packs downloaded/imported in this session. Next step: acquire verified free zombie + character packs from Epic/Fab (explicit free license), place under `Content/AF_External/Characters/<PackName>/`, verify skeleton compatibility with SK_Mannequin, retarget if needed, create Physics Assets.

## License obligations

- Epic/UE template content: covered by UE EULA; usable within UE projects only.
- Any Fab/Epic free listing: verify per-pack license before import; record source URL + license text.
- Do not import incompatible complete projects wholesale; do not overwrite existing assets.

## Cook/packaging verification

Assets ship only if referenced by a cooked map, hard-referenced from a cooked Blueprint/DataAsset, or listed in `AlwaysCook`. Staged `Content/Characters/UE5_Mannequins/` loads in editor/PIE but will NOT ship unless referenced by a cooked map.
