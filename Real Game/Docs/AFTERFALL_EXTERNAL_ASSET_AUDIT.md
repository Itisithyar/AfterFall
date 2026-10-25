# AFTERFALL External Asset Audit
_EV: 2026-10-25 v1 (Flint / GLM-5.3-Flash-Free) — research pass, nothing downloaded yet._

## A. Local library / workspace scan
Scanned the workspace root and project (read-only). Reusable implementations found in the
existing engine/source tree (no imports needed, they are already in-repo):
- Destruction stack exists: `Source/Afterfall/Destruction/*` (DestructionTypes, DestructionPropagator,
  DestructionPropagation, FractureSolver) + `Slice/DestructionRoomSubsystem` + `Structural/*`
  (`StructuralActor::ServerApplyImpact` — server-validated impact with event GUID dedup) + Tests.
- Medical/injury: `Combat/InjuryStateComponent` + `MedicalCapability` — canonical wound ledger
  (adopted for FR-003; FLINT's duplicate `BleedingComponent` removed).
- Physics validation harness: `Physics/PhysicsValidationSubsystem`, `PhysicsTestSubsystem`.
- Engine stock destruction assets in `C:\Program Files\Epic Games\UE_5.8\` samples are engine-bound;
  no license issue but migration/editing cost — not brought in this pass.
No local Chaos geometry-collection demos were found in the workspace.

## B. Candidate free external projects (identified, NOT downloaded/integrated)
| Candidate | URL | Contains | UE | License (verified basis) | Decision |
|---|---|---|---|---|---|
| Epic "First Person" template | marketplace-free / engine template | FPS rig, inputs | 5.x | Epic EULA (template content reusable inside Unreal) | Already the base of Lvl_FirstPerson |
| Chaos Destruction learning sample (Epic education) | dev.epicgames.com/community | Geometry collections, fracture setups | 5.x | Epic EULA learning content | Research only; port FX assets selectively later |
| Fab free Chaos destruction/victim asset packs | fab.com | Debris, glass fracture GCs | varies | Per-asset Fab license — MUST be checked individually per asset | Not integrated; needs per-asset license check |

## C. Rejected / blocked
- Random unlicensed GitHub "UE destruction" repos: commonly missing explicit licenses;
  absent license = all-rights-reserved default. Not reusable commercially. Skipped.

## E. Physics-validation-module repair directive (2026-10-25) — NOT VERIFIABLE / FALSE ALARM?
Reported: "Physics validity module is the only broken .cpp". Evidence contradicts this:
- Latest AfterfallEditor builds: BUILD exit 0; Adaptive Build listed `PhysicsValidationSubsystem.cpp`
  excluded from unity (thus compiled as its own action) with no error reported; UBT log shows
  no errors referencing it.
- `Afterfall.BodyPhysics` automation (2026-10-25F 00:52 UTC): Math + Table tests, Result={Success}
  both, TEST COMPLETE EXIT CODE 0.
No repair applied: the module compiles and its tests PASS. Original report appears stale/unfounded.
1. Any Paid/owner-owned Fab library to query? (No marketplace library path found in-workspace.)

---
## F. BEAM asset pass (2026-10-25) — local inventory + integration pick list
Status vocabulary for the final report: FOUND / LICENSE-VERIFIED / IMPORTED / INTEGRATED / RENDER-VERIFIED.
Nothing below was downloaded, copied, or imported by BEAM in this pass — no editor migration
was run and no Content/ file was added, moved, or overwritten.

### F1. In-project reusable assets (FOUND, license-clear: already in repo / engine stock)
| Asset | Path | Use for |
|---|---|---|
| SM_Cube, SM_ChamferCube | `Content/LevelPrototyping/Meshes/` | Wall-block geometry (procedural fallback; note: prototype cubes are 100 cm — use 0.5 mesh scale or 100 cm profile for the 50 cm default in `FBoxBuildingProfile`) |
| SM_DoorFrame_Edge, SM_DoorFrame_Corner, SM_Door | `Content/LevelPrototyping/Interactable/Door/Meshes/` | Door/window opening frames matching `FBoxOpening` rects |
| MI_PrototypeGrid_Gray(_02), MI_PrototypeGrid_Gray_Round, MI_PrototypeGrid_TopDark, M_FlatCol, MF_ProcGrid | `Content/LevelPrototyping/Materials/` | Distinct per-material wall looks (fallback until real PBR sets arrive) |
| MI_FirstPersonColorway | `Content/FirstPerson/` | Accent/trim material |
| /Engine/BasicShapes/Cube | Engine stock (no import) | Already used by `PhysicsTestSubsystem`; zero-cost block/debris fallback |

### F2. VaultCache inventory (FOUND, inspected read-only)
- `VaultCache/FabLibrary/Game_Animation_Sample-880e319a/` — Fab **manifest only** (792 KB). No meshes/materials. NOT usable as asset source.
- `VaultCache/FabLibrary/Poor_man_Outfit-ebeea391/` — MetaHuman outfit `.mhpkg` (289 MB, UE_5.6). Character clothing; wrong domain for walls/debris. NOT used.
- `VaultCache/GameAnimationSample_5.8/data/` — full 7 GB sample copy, UE 5.8 (version-compatible). RELEVANT contents: `Content/Characters/UE5_Mannequins/Meshes/SKM_Manny(_Simple).uasset`, `SKM_Quinn(_Simple).uasset`, `SK_Mannequin.uasset`, `MI_Manny_01/02`, `MI_Quinn_01/02`, rigs/control-rigs. Physics asset expected as `SKM_Manny_PhysicsAsset` (name to confirm at migration; `PA_Mannequin` in Rigs is a Pose Asset, not physics). NO wall/building/debris meshes found in this sample (locomotion sample). License basis: Epic sample content, UE EULA — reusable inside Unreal projects; migrate ONLY the mannequin subset (mesh + skeleton + materials + physics asset), never the whole 7 GB tree.
- In-project `Content/` has NO character assets today (no mannequin, no PhysicsAsset, no AnimBP) — thrown-body visual tests will need the GAS migration above.

### F3. External candidates (FOUND, license NOT verified — per-asset check required at acquisition)
| Candidate | Source | Why | State |
|---|---|---|---|
| Modular Concrete Debris Pack (59 rubble/debris assets) | fab.com/listing `2b76cfcb-…` | Concrete fragments/rubble for detached-block debris | NOT downloaded; price/license must be confirmed on the listing; download needs the user's Fab entitlement |
| Broken Concrete Brick and Rubble – Modular Bundle | fab.com/listing `ed9aa2e9-…` | Ruined brick walls + debris piles + scatter | Same as above |
| Modular Ruined Walls (30 wall pieces + 11 debris) | fab.com/listing `b791e11b-…` | Ruined brick wall modules | Same as above |
| Chaos Destruction learning sample (Epic) | dev.epicgames.com/community | Geometry-collection fracture reference | Research only (per §B) |
Unlicensed GitHub UE-destruction repos: still rejected (§C stands).

### F4. Procedural fallback mapping (no new architecture; consumes `EBoxBlockMaterial`)
Drywall→prototype-gray grid; Wood→flat warm colorway; Brick→grid-round; Concrete→top-dark;
Glass→translucent colorway instance (to create); Metal→flat dark. Intact walls render instanced/merged
per material; detached blocks swap to per-block simulated meshes (SM_Cube/SM_ChamferCube) with the
material's look; debris budget + sleep/cleanup enforced by the runtime caller (FLINT's wiring step).
Stable block Ids and the destruction ledger are untouched by this mapping.

### F5. Gaps + who unblocks them
- GAP-1 No wall-module/debris meshes exist locally (IMPORTED: none). Unblock: user acquires a Fab
  candidate (§F3) via their Epic account, or approves staying on the §F4 fallback.
- GAP-2 No editor migration or rendered session was run (INTEGRATED/RENDER-VERIFIED: none).
  Unblock: (a) FLINT's runtime caller wiring `EvaluateImpact → ServerApplyImpact` (no caller exists yet),
  (b) an editor session to migrate the GAS mannequin subset + any Fab pack and render the box.
- Preserved: no Content/ file added/modified; regression tests untouched; FLINT's BoxBuilding files untouched.

### F6. User decisions (2026-10-25) + staged migration plan
- DECISION-1 Fab packs: **procedural fallback for now** — no external acquisition. §F3 stays
  research-only; §F4 (`SM_Cube`/`SM_ChamferCube` + prototype materials + door frames) is the visual
  path until the user provides a pack. (FOUND: yes, IMPORTED: no — by user choice.)
- DECISION-2 Mannequin: **migrate the GAS subset** for thrown-body tests (approved, editor session
  required — not executed by BEAM, no editor migration was run).
- Staged migration file list (copy ONLY these; dependencies auto-follow in-editor; destination
  `Content/Characters/UE5_Mannequins/` — path-mirrored below Content so /Game/ references hold):
  `Characters/UE5_Mannequins/Meshes/SK_Mannequin.uasset` (skeleton),
  `Characters/UE5_Mannequins/Meshes/SKM_Manny_Simple.uasset` (low-bone mesh, physics-friendly),
  `Characters/UE5_Mannequins/Materials/M_Mannequin.uasset` +
  `Characters/UE5_Mannequins/Materials/Instances/Manny/MI_Manny_01.uasset`
  (source root: `VaultCache/GameAnimationSample_5.8/data/Content/`).
- EXECUTED 2026-10-25 (BEAM, file copy only — no editor run yet): Manny subset now staged at
  `Content/Characters/UE5_Mannequins/` (skeleton, SKM_Manny_Simple, M_Mannequin + functions +
  Manny instances + Manny/Shared textures, 146 MB; Quinn trimmed, nothing overwritten;
  all 10 `BoxBuildingFallbackAssets.h` asset paths verified present on disk 2026-10-25).
- RUNTIME LANDED (FLINT, uncommitted): `ABoxWallStructure::ServerApplyBoxImpact` (authority +
  GUID dedup) + `OnBlockHit` thrown-body route + `ABoxBlockDebris` (bounded impulse, 12 s life,
  settle-destroy) + `UBoxWallRuntimeSubsystem` (`-afBoxWall` demo box: 4-wide profile, front door).
  WIRING HANDOFF (not applied — FLINT's files, their call): the actor hardcodes
  `/Engine/BasicShapes/Cube.Cube` in `BoxWallRuntime::CubeMesh()`. Suggested one-spot change:
  try `BoxFallbackAssets::DetachedBlockMesh()/IntactBlockMesh()` first, keep the engine cube as
  the null fallback. CAUTION: /Game/LevelPrototyping paths load in editor/PIE but are NOT cooked
  into packaged builds unless hard-referenced — engine cube stays the packaged-safe default.
  Open gaps (theirs to close): collapsed ids are hidden+spawned but their `State` is never set to
  `Detached` in the committed ledger, so `NearestBlock` can retarget physically-gone blocks;
  debris is server-only (`SetReplicates(false)` — clients never see shards); `FBoxWallRuntime`
  struct is unused; Glass has no fallback material (empty path by design).
- FINALIZE (user-authorized, BEAM executing 2026-10-25 ~20:35 UTC-5): canonical target confirmed
  as root `RealGame.uproject` ("authoritative integration target", UE 5.8, ProceduralMeshComponent
  on); `Real Game/Afterfall.uproject` is the older vertical-slice project. Build command:
  `Build.bat AfterfallEditor Win64 Development RealGame.uproject -WaitMutex`. Applied fixes:
  (A) collapsed blocks committed `Detached` in `UpdatedBlocks` + `NearestBlock` skips Destroyed;
  collapsed ids appended to replicated `DetachedBlockIds`; (B) `NetMulticast` debris echo so
  clients spawn visual-only shards (server physics never assumed on clients); (C) one-location
  fallback patch — `CubeMesh(Primary)` tries `BoxFallbackAssets` mesh first, engine cube on
  null/failure + per-material `ApplyFallbackMaterial`; (D) glass explicitly mapped to `M_FlatCol`
  stand-in (all six materials now distinct); (E) thrown-body `OnBlockHit` route + bounded
  splash/collapse verified by review — no full-structure erase. Strengthened `BoxBuildingTest`
  with FIX-A ledger-commit assertions (additive only). COOKING NOTE: `DefaultGame.ini` cooks only
  `/Game/FirstPerson/Lvl_FirstPerson`; staged Manny + fallback assets load in editor/PIE but will
  NOT ship unless referenced by a cooked map or added to AlwaysCook — flagged for the shipping pass,
  deliberately not added now (would force +146 MB into every package).
  New file: `Source/Afterfall/Structural/BoxBuildingFallbackAssets.h` — header-only, UHT-free
  fallback mesh/material/budget mapping for the runtime caller (consumes FBoxBuildingProfile
  material ids; duplicates no logic; FLINT's files untouched).
- IMPORTANT source finding: the Game Animation Sample ships **no PhysicsAsset** for Manny
  (no `*phys*`/`SKM_*PhysicsAsset*` exists; `PA_Mannequin` is a Pose Asset). Post-migration step:
  in-editor right-click `SK_Mannequin` → Create → Physics Asset (default ragdoll) — required
  before any thrown-body simulation. Quinn files are out of scope unless a second body is needed.
2. Whether to adopt Chaos geometry collections vs the existing voxel/structural destruction
   path (canon per `Slice/DestructionRoomSubsystem` + voxel subsystem) — lean reuse-first.
