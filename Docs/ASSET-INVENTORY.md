# ASSET-INVENTORY.md — Afterfall asset discovery (2026-10-24, no engine)

Engine: UE 5.8 (`EngineAssociation: 5.8`). Plugins enabled: EnhancedInput only
(`ModelingToolsEditorMode` explicitly disabled). No MetaHuman, audio, or
asset-import plugins. Method: filesystem inventory + binary `/Game/` reference
scan of committed assets (see `Scripts/validate_assets.py`). No assets were
moved, edited, cooked, or opened in-editor for this report.

## Where things live

- `Content/` (committed, canonical): 43 `.uasset` + 6 loot JSONs +
  `Lvl_FirstPerson.umap`. NO skeletal meshes, skeletons, animation sequences,
  character models, or audio in the committed tree.
- `AfterFall/` (CONFIRMED, gitignored duplicate template project, local only):
  full `Content/Characters/Mannequins/` tree, 126 MB. Not committed; the only
  copy is on this machine.
- `AfterFall 5.8/` (untracked second template copy, local only): its
  `Content/Characters/` tree is byte-identical to `AfterFall/`'s (verified by
  recursive diff) — a pure 126 MB duplicate. Both copies are gitignored and
  uncommitted; either one serves as the merge source.
- `Content/FirstPerson/Anims.bak/` (quarantined, never cooked): `ABP_FP_Copy`,
  `CtrlRig_FPWarp`. Broken skeleton links (see below); restore only after an
  in-editor retarget fix (per HEAD commit 7f2bc2d).

## Character / animation stock (all in `AfterFall/Content/Characters/Mannequins/`)

| Group | Count | Contents |
|---|---|---|
| `Meshes/` | 3 | `SK_Mannequin` (skeleton), `SKM_Manny_Simple`, `SKM_Quinn_Simple` |
| `Anims/Unarmed/` | 28 | `ABP_Unarmed`, `BS_Idle_Walk_Run`, idle, 8-dir walk/jog, jump start/fall-loop/land, attack set |
| `Anims/Rifle/` | 39 | Aim (AO + poses), 8-dir jog/walk, jump set, fire/equip/reload montages, 8 hit-reacts |
| `Anims/Pistol/` | 29 | Same structure as Rifle (aim/jog/walk/jump/fire/reload) |
| `Anims/Death/` | 6 | Back/front/left/right death anims |
| `Rigs/` | 4 | `CR_Mannequin_Body`, `CR_Mannequin_FootIK`, `CR_Mannequin_Procedural` (Control Rigs), `PA_Mannequin` (PoseAsset) |
| `Materials/` | 5 | `M_Mannequin` (parent) + `MI_Manny_01/02_New`, `MI_Quinn_01/02` |
| `Textures/` | 14 | Manny/Quinn/Shared skin/cloth textures |

Internal links are self-consistent under `/Game/Characters/Mannequins/...`
(verified: `ABP_Unarmed` → `SK_Mannequin` + FootIK rig + its anims;
`SKM_Manny_Simple` → `SK_Mannequin` + Manny MIs + body/pose rigs).

## Committed `Content/` groups (ready)

- `Content/FirstPerson/Blueprints/`: 4 template BPs (character, controller,
  game mode, camera manager). `BP_FirstPersonCharacter` has BROKEN mesh/anim
  links (below) — gameplay uses C++ `AAfterfallCharacter` instead.
- `Content/Input/`: `IMC_Default`, `IMC_MouseLook`, 4 `IA_*` actions, touch UI
  (`BPI_TouchInterface`, thumbstick). All present; referenced by the FP BP.
- `Content/LevelPrototyping/`: 7 graybox meshes, 8 materials/MIs, grid texture,
  door/jumppad/target interactables. Ready for graybox levels.
- `Content/Data/Loot/`: 6 location JSONs (`DA_Location_*.json`). Ready.
- `Lvl_FirstPerson.umap`: loads (references only its GameMode + external actors).

## Verified broken references (nothing assumed)

1. `BP_FirstPersonCharacter` → `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`
   — MISSING from committed `Content/` (exists in `AfterFall/` template copy).
2. `BP_FirstPersonCharacter` → `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`
   — MISSING (same).
3. `BP_FirstPersonCharacter` → `/Game/FirstPerson/Anims/ABP_FP_Copy`
   — QUARANTINED (`.bak`, deliberately uncooked).
4. Quarantined `ABP_FP_Copy` + `CtrlRig_FPWarp` → `/Game/Characters/Mannequins/Meshes/SK_Mannequin`
   — MISSING (same root cause as 1–2). Restoring the `.bak` files WITHOUT the
   skeleton re-creates the cook/boot failure from HEAD.

C++ hard asset refs (`FObjectFinder`/`LoadObject` in `Source/`) point only at
`/Engine/BasicShapes/*` (engine content, always present). No code depends on
missing `/Game/` assets.

## Gaps (confirmed by search, not by assumption)

- Audio: zero `.wav`/`.mp3`/`.ogg` in `Content/` AND `AfterFall/Content/`.
  No footsteps, UI, ambient, or weapon audio for the slice.
- MetaHumans: none anywhere. (Not required for the slice; listed only because
  the mission asks.)
- Character source licensing: all character/anim content is Epic template
  content under the engine/template license; no third-party or paid assets, no
  separate license files found. `AfterFall/` itself is gitignored and
  uncommitted — a single local copy (loss/backup risk, not a license risk).
- "Unused" labels deliberately avoided: absence of a text reference is not
  proof of non-use (e.g. assets loadable at runtime via soft paths).

## Integration readiness

READY now: template BPs (minus character mesh/anim links), full Enhanced Input
set, graybox meshes/materials, loot JSONs, `Lvl_FirstPerson` map, C++ movement
(`UAfterfallMovementComponent`) + input (`AAfterfallCharacter`, code-built IMC).

SHORTEST SAFE CHARACTER PATH (no new imports, no downloads):
1. Copy `AfterFall/Content/Characters/` → `Content/Characters/` (same relative
   paths; all internal links resolve with zero retargeting; `.uasset` is
   LFS-tracked per `.gitattributes`).
2. In-editor: verify `SKM_Manny_Simple` + `ABP_Unarmed` open against
   `SK_Mannequin`; restore the 2 `.bak` assets only after the skeleton link
   is verified (else the quarantine failure returns).
3. First playable: `SKM_Manny_Simple` + `SK_Mannequin` + `ABP_Unarmed` Unarmed
   set (idle/walk/run blend, jump start/fall/land) — covers move/look/jump/land;
   Rifle set (39) for hitscan combat; Death set (6) for zombies; HitReact (in
   Rifle set) for damage feedback; Quinn mesh as the second-body variant.
4. Still missing for a full slice: ANY audio (record/reuse needed), and the
   `TestNeighborhood` map (separate backlog item).

BLOCKED until merge: FP character mesh/anim links (items 1–4 above), anim BP
restore, any skeletal-mesh zombie work (check what `ZombiePrototype` expects
in-editor before merging — C++ only references engine cubes today).

## Next three tasks (ranked by dependency × risk)

1. Merge `Characters/` tree (126 MB, path-preserving, LFS) + in-editor open
   verify of SKM/skeleton/ABP. Unblocks every character/anim item. Low risk.
2. Restore + retarget the 2 quarantined anim assets against the merged
   skeleton; re-run cook-guard. Unblocks template FP arms. Low risk after 1.
3. Source or record a minimal audio set (footstep/land/UI/interact loops).
   Currently zero coverage; independent of 1–2. Medium effort, no code risk.
