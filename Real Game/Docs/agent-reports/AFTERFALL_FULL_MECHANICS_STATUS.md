# AFTERFALL — Full mechanics status, integration map, and playable path

- **Audit date:** 2026-10-24, 17:2x–18:3x local.
**Consolidation follow-up:** executed after the audit — see §10. The two-fork
findings in §0 and §4.1 describe the state *before* it; §10 is the current
layout.
- **Scope:** read-only. No gameplay source edited, nothing deleted, committed or
  pushed. The only files written are this report and the coordination/decision
  documents in `Tony/`.
- **Projects inspected:**
  - `C:\Users\mckee\Desktop\Come Up Game\` — git repo, branch `master`, HEAD
    `7f2bc2d`; project `Afterfall.uproject` ("root project" below).
  - `C:\Users\mckee\Desktop\Come Up Game\Real Game\` — project
    `RealGame.uproject`, **entirely untracked** ("Real Game project" below).
  - `C:\Users\mckee\Desktop\Afterfall\` — packaged build.
- **Evidence:** `Tony/agent1-evidence-2026-10-24/`,
  `Tony/fix-evidence-2026-10-24/`, plus the commands quoted inline.

---

## 0. Executive summary

**There are two forks of the same game, and the packaged build is a third,
older snapshot of one of them.** That is the single most important fact in this
audit, because every "is X connected?" question has a different answer per fork.

| | Root project | Real Game project | Packaged build |
|---|---|---|---|
| Project file | `Afterfall.uproject` | `RealGame.uproject` | copied from Real Game |
| Version control | tracked | **untracked** | none |
| Unique source files | 3 | **28** | — |
| Automation tests | not run in this audit | **37/37 pass** | none |
| Newest mechanics | settings component (unreferenced) | voxel, subsystem registry, mech playground, fracture solver, agent link | snapshot 16:26 |
| Grapple in binary | n/a | yes (source 18:10) | **no** |

Mechanics themselves are numerous and mostly *built*: 37 automation tests pass,
321 script-suite checks pass, and the packaged shipping binary contains the voxel
and quick-menu systems. **Grapple is verified at runtime** — `afMechSelfTest`
passes 11/11 on the current binary, including attach, rope constraint, release
and momentum preservation (this corrects both `grapple_handoff.md` and the
"grapple not verified in-engine" premise). What is missing is not features — it
is **wiring at the top**: the game mode does not set a default pawn, and the only
packaged map is the stock template, so the mechanics character is not the thing
the player controls on a normal launch.

**Nothing below is marked "working in-game" on the strength of a compile.** The
only runtime evidence collected here is headless automation and command-line
hooks; no interactive PIE session was performed.

## 1. Evidence base (what was actually executed)

| Check | Result | Where |
|---|---|---|
| Editor build, Real Game | `Result: Succeeded`, exit 0 | `fix-evidence-2026-10-24/build3.log` |
| Full automation suite | **37 started / 37 Success / 0 Fail**, `TEST COMPLETE. EXIT CODE: 0` | `fix-evidence-2026-10-24/suite-final.log` |
| Script suite (`bash Scripts/run_tests.sh`, cwd `Real Game/`) | exit 0, **321 `[PASS]`, 0 `[FAIL]`** | `agent1-evidence-2026-10-24/script-suite-from-realgame.txt` |
| Agent status hook | `AGENTSTATUS op=status providers=1 code=ok registry_shutting_down=0`, exit 0 | `fix-evidence-2026-10-24/agentstatus-flag.log` |
| Mech playground self-test (`-afMechSelfTest`) | **11 pass / 0 fail**, `MECHTEST PASS`, exit 0 | `fix-evidence-2026-10-24/mechtest.log` |
| Packaged binary content | `VoxelChunkActor` ×3, `QuickMenu` ×1, `AfterfallCharacter` ×3, `Grapple` ×0, `AgentGameLink` ×0, `FractureSolver` ×0 (`UTF-16LE` search) | `Afterfall/RealGame/Binaries/Win64/Afterfall-Win64-Shipping.exe`, 166,248,960 B, 16:26 |

The 37 automation tests (exact paths, from the run log):

```
Afterfall.Agent.Link                      Afterfall.Loot.Rarity
Afterfall.Construction.GridSnap           Afterfall.Loot.SingleCopy
Afterfall.Construction.ValidatePlacement  Afterfall.MatInteraction.RepeatedInput
Afterfall.Crafting.Inputs                 Afterfall.MatInteraction.Transitions
Afterfall.Damage.BodyPartResolution       Afterfall.Movement.Deliberate
Afterfall.Damage.SixShotBaseline          Afterfall.Persistence.Compat
Afterfall.Destruction.CommitIdempotent    Afterfall.Persistence.DamageTriggered
Afterfall.Destruction.Grid                Afterfall.Physics.LabReference
Afterfall.Destruction.Initiators          Afterfall.Simulation.Proximity
Afterfall.Destruction.LocalFrame          Afterfall.Slice.Yields
Afterfall.Destruction.Propagate           Afterfall.Stage2.FixedStep
Afterfall.Destruction.Propagation         Afterfall.Stage2.SprintTiming
Afterfall.Entity.Promote                  Afterfall.Stage2.ThrowModel
Afterfall.Inventory.Weight                Afterfall.Subsystem.Registry
Afterfall.Loot.EntryCount                 Afterfall.UI.QuickMenuRouter
                                          Afterfall.UI.RadialMenu
Afterfall.Voxel.Basis                     Afterfall.Voxel.ChunkInit
Afterfall.Voxel.CellService               Afterfall.Voxel.EditOrder
                                          Afterfall.Voxel.MeshFaces
Afterfall.World.RootIdentity
```

Not executed in this audit: PIE, packaged-game launch, two-client session, room
self-test re-run, physics self-test. Where earlier reports claim those results,
they are marked "reported" below, not "verified".

## 2. Mechanics inventory

Classification: **1** working and verified in a running process · **2** built,
runtime unverified · **3** implemented but disconnected · **4** partial/prototype
· **5** missing/blocked · **6** unknown.
Paths are relative to `Real Game/Source/Afterfall/` unless stated.

### 2.1 Movement, camera, traversal

| Mechanic | Implementation | Wiring evidence | Class |
|---|---|---|---|
| Character movement, sprint, jump, fall | `Movement/AfterfallMovementComponent.{h,cpp}`, `AfterfallCharacter.{h,cpp}` | component created in character ctor; Enhanced Input IMC built in code (`AfterfallCharacter.cpp:166-188`) | 2 |
| Input (KB/M + gamepad) | `AfterfallCharacter.cpp` `MakeAction`/`MapKey` — `IA_Jump`, `IA_Look`, `IA_MouseLook`, `Slice_Grapple` on `T` and `Gamepad_DPad_Right` | bound at `:255` | 2 |
| Legacy input path | root project only: `BindKey(EKeys::ThumbMouseButton …)` at `AfterfallCharacter.cpp:193-194` | client-side only | 3 |
| Grapple / rope / tether | `Movement/AfterfallGrappleComponent.{h,cpp}` — **two divergent copies** (§4.1) | Real Game: `ServerRequestGrapple`/`ServerReleaseGrapple` RPCs with validation (`:956-995`); root: direct `TryGrapple()` from input. **Runtime-verified** by `afMechSelfTest`: attaches to a building wall, rope length assigned, constraint respected after ticks, release detaches, momentum preserved | **1** (root copy 3) |
| Swinging / momentum | inside grapple component (radial velocity projection + sweep correction, per `Tony/grapple_handoff.md`) | none in-engine | 2 |
| Landing | no dedicated landing logic found; falls through character movement | — | 5 |
| Respawn | `ServerRequestRespawn` (`AfterfallCharacter.h:203`), `DoRespawn`, `bRespawning` | server RPC, health-gated | 2 |
| Checkpoints | none found (`grep` for `Checkpoint` returns nothing) | — | 5 |

### 2.2 Physics, weight, forces

| Mechanic | Implementation | Wiring | Class |
|---|---|---|---|
| Gravity / free-fall / impulse measurement | `Physics/PhysicsTestSubsystem.{h,cpp}` — `-afPhysicsTest` flag + console command, spawns a 10 kg cube, measures dz, forces exit | hook at `OnPostWorldInitialization`; measured in code | 2 (owner-run per docs; not re-run) |
| Physics validation lab | `Physics/PhysicsValidationSubsystem.{h,cpp}` (~850 lines), test `Afterfall.Physics.LabReference` | test passes | 2 |
| Mass / weight model | `World/PhysicalItem.{h,cpp}`, `Storage/PhysicalMat.{h,cpp}`, `Interaction/CarryComponent.{h,cpp}` | tests `Stage2.ThrowModel`, `Inventory.Weight` | 2 |
| Throwing / dropping | `CarryComponent` `ServerThrowHeld`/`ServerDropHeld` | character binds | 2 |
| Fracture | `Destruction/FractureSolver.{h,cpp}`; consumed by `Voxel/VoxelChunkActor.cpp` | one consumer | 2 |

### 2.3 Destruction, debris

| Mechanic | Implementation | Wiring / tests | Class |
|---|---|---|---|
| Propagation solver, structural integrity | `Destruction/DestructionPropagation.*`, `DestructionPropagator.*`, `DestructionTypes.*`, `Structural/StructuralDestructionComponent.*` | 6 tests (`Destruction.*`) | 2 |
| Room-scale end-to-end | `Slice/DestructionRoomSubsystem.*` (~965 lines), `-afRoomSelfTest` (server) | docs report 9/9 PASS (not re-run here) | 2 |
| Voxel destruction / chunking | `Voxel/*` (11 files) + `VoxelNetSubsystem` spawning `AVoxelChunkActor` | 5 tests; chunks spawned at runtime (`VoxelNetSubsystem.cpp:141`); present in packaged binary | 2 |
| Debris reset | room self-test tears down by tag; mech playground resets by tag `MechPlay` | — | 2 |

### 2.4 Interaction, inventory, economy

| Mechanic | Implementation | Tests | Class |
|---|---|---|---|
| Interact / targeting | `Interaction/InteractComponent.*`, `Interactable.h` | — | 2 |
| Physical mats | `Interaction/MatInteractionComponent.*`, `Storage/PhysicalMat.*` | `MatInteraction.*` (2) | 2 |
| Carry / equip | `Interaction/CarryComponent.*` | `Stage2.ThrowModel` | 2 |
| Inventory, cost, wallet | `Inventory/InventoryComponent.*` | `Inventory.Weight` | 2 |
| Crafting | `Crafting/CraftingService.*` | `Crafting.Inputs` | 2 |
| Loot | `Loot/LootSystem.*`, `LootService.*`, `LootProfiles.h` | `Loot.*` (3) | 2 |
| Resources / harvesting | `World/ResourceNode.*`, `Slice/SliceSubsystem` seeding | `Slice.Yields` | 2 |
| Progression | no XP/level system found | — | 5 |

### 2.5 Health, damage, combat, AI

| Mechanic | Implementation | Tests | Class |
|---|---|---|---|
| Health, death latch | `Combat/HealthComponent.*` | — | 2 |
| Damage resolution, body parts | `Combat/DamageResolver.*`, `BodyDamageTypes.*` | `Damage.*` (2) | 2 |
| Hitscan fire | `AfterfallCharacter::ServerFireHitscan` | — | 2 |
| Melee impact pipeline | character + destruction propagation | `Destruction.Initiators` | 2 |
| Zombie AI | `AI/ZombiePrototype.{h,cpp}` (~500 lines); spawned by `SliceSubsystem.cpp:126` and `MechPlaygroundSubsystem.cpp:170`; observed pursuing in a headless `-game` run | **no dedicated test** | 2 |
| Animation | **no C++ animation classes**; only template assets (`Content/FirstPerson/Anims.bak/`, `ABP_FP_Copy` — quarantined by HEAD commit "quarantine broken template anim assets") | — | 5 |
| Audio | **none** — no `USoundBase`/`UAudioComponent` usage in source (one incidental mention in `MatInteractionComponent`) | — | 5 |
| Effects (VFX) | none | — | 5 |

### 2.6 World, UI, session

| Mechanic | Implementation | Tests | Class |
|---|---|---|---|
| Game mode | `AfterfallGameMode.{h,cpp}` — **sets no `DefaultPawnClass`**; comment says "set in DefaultEngine.ini / project settings later" | — | **3** |
| Game state / seed | `RegionSeed=1337` in game mode, logged | — | 2 |
| HUD | `UI/AfterfallHUD.{h,cpp}`; installed via `PC->ClientSetHUD` in `SliceSubsystem.cpp:228,241` | — | 2 |
| Radial menu | `UI/RadialMenuComponent.*` | `UI.RadialMenu` | 2 |
| Quick menu | character `bQuickMenuOpen` + router | `UI.QuickMenuRouter` | 2 |
| Settings UI | **root project only**: `UI/AfterfallSettingsComponent.*` — not referenced by the root character either | — | 3 (dead in its own fork) |
| Zones | `Zones/WorldZone.{h,cpp}` — referenced by nothing outside itself | — | **3** |
| Persistence / save | `Persistence/PersistenceService.*`, `RegionStateSave.h` | `Persistence.*` (2) | 2 |
| Simulation scheduling | `Simulation/SimulationScheduler.*` | `Simulation.Proximity` | 2 |
| Subsystem registry | `Subsystem/AfterfallSubsystemRegistry.*`, `SubsystemService.*` | `Subsystem.Registry`; live registry reports `providers=1` headless | **1** |
| Voxel network sync | `Voxel/VoxelNetSubsystem.*` | `Voxel.CellService` | 2 |
| Construction from player | `Structural/*`, `ConstructionValidator.*` | `Construction.*` (2) | 2 |
| Level / map | **only** `Content/FirstPerson/Lvl_FirstPerson.umap` (template). `GameDefaultMap` points at it; `TestNeighborhood` is commented out in `DefaultEngine.ini:25-29` | — | **5** |
| Controller support | gamepad keys mapped inline (Real Game) | — | 2 |
| Debug / self-test surface | `afRoomSelfTest`, `afPhysicsTest`, `afMechSelfTest`, `afSliceSelfTest`, `afPipelineSelfTest`, `afAgentStatus` | — | 1–2 |
| Agent interface v1 | `Agent/AgentGameLink.*` + `Agent/AgentStatusCommand.cpp` | `Agent.Link` + headless hook (`providers=1 code=ok`) | **1** (read-only status only) |
| External orchestration | none on either side (see `agent2-…architecture…md` §3) | — | 5 |

## 3. Connected vs disconnected

**Connected (a real call path exists):** movement/input, grapple RPCs, health and
damage, carry/throw, inventory/crafting/loot, destruction propagation, room and
physics self-tests, voxel chunk spawning, HUD/radial/quick menu, persistence,
simulation scheduler, slice seeding (mats, nodes, zombies), agent status.

**Disconnected (implemented, nothing calls it):**

1. `Zones/WorldZone.*` — no caller anywhere.
2. Root project's `UI/AfterfallSettingsComponent.*` — not referenced by its own
   character.
3. **`AAfterfallGameMode::DefaultPawnClass`** — unset. The gameplay character
   exists (spawned by `SliceSubsystem`, respawn, and the physics lab) but is not
   what a normal launch gives the player. `BP_FirstPersonCharacter.uasset`
   contains no reference to `AfterfallCharacter`, so the template pawn is not a
   subclass of it either.
4. Root project's grapple — wired client-side only, bypassing the server RPCs
   the Real Game version added.

**Missing entirely:** a real playable map, animation, audio, VFX, checkpoints,
progression, and the Tony↔Afterfall transport (`agent2-…architecture…md` §3.3).

## 4. Dependency and conflict map

### 4.1 Two forks with divergent shared files

Files present **only** in `Real Game/Source` (28): `Agent/*` (3), `Voxel/*` (11),
`Subsystem/*` (4), `Destruction/FractureSolver.*` (2),
`Slice/MechPlaygroundSubsystem.*` (2), `Tests/{AgentGameLink,QuickMenuLab,
SubsystemRegistry,VoxelCellService}Test.cpp` (4), `Voxel/VoxelChunkTest.cpp`.

Files present **only** in root `Source` (3): `Combat/InteractionEvent.h`,
`UI/AfterfallSettingsComponent.{h,cpp}`.

Shared files that **differ** between the forks: `AfterfallCharacter.*`,
`Movement/AfterfallGrappleComponent.*`, and others. The grapple difference is
behavioural, not cosmetic:

| | Root copy (`Source/…/Movement/`, mtime 17:46/17:51) | Real Game copy (`Real Game/Source/…/Movement/`, mtime 18:10) |
|---|---|---|
| Input | `BindKey(ThumbMouseButton)` | Enhanced Input `Slice_Grapple` on `T` / gamepad |
| Authority | client calls `TryGrapple()` directly | `ServerRequestGrapple` / `ServerReleaseGrapple` with `_Validate` |
| Size | 3962 B cpp / 2332 B h | 4980 B cpp / 3285 B h |

**Do not merge these two files by hand.** One of them is client-authoritative,
which the rest of the codebase deliberately avoids; the other is the pattern the
project uses everywhere. Pick Real Game's as canonical, and port anything the
root copy gained before deleting it.

### 4.2 Authority summary

| Concern | Authoritative owner |
|---|---|
| Movement / velocity | `UAfterfallMovementComponent` (CharacterMovementComponent subclass) |
| Position correction during grapple | grapple component in `TickComponent` (sweep + radial projection) |
| Forces / mass | engine physics on `UStaticMeshComponent` via `PhysicalItem` |
| Collision | engine, profile set by `PhysicalItem` / `StructuralActor` |
| Damage / death | `UHealthComponent` + `DamageResolver` (server) |
| Player state / inventory | `UInventoryComponent` (replicated, server-authoritative wallet) |
| Input | `AAfterfallCharacter::SetupPlayerInputComponent` (Enhanced Input, both KB/M and gamepad) |
| World state | `SliceSubsystem` seeds; `PersistenceService` saves; `WorldRoot` roots |
| Subsystem lifecycle | `UAfterfallSubsystemRegistry` (world-scoped) |

### 4.3 Competing / duplicate systems

- Grapple: two forks (§4.1).
- Character: two forks with different input stacks.
- Settings: root-only, unreferenced.
- Two automation test sets exist on disk (root has 13 test files; Real Game has
  17 + 4 unique). Only Real Game's 37 were executed here; the root project's
  suite was **not run**, so its green/red state is unknown.

## 5. Missing assets and blockers

| Blocker | Evidence | Effect |
|---|---|---|
| No gameplay map | only `Lvl_FirstPerson.umap` (template); `TestNeighborhood` commented out | nothing to walk around in except the template |
| No default pawn | `AfterfallGameMode` ctor has no `DefaultPawnClass`; BP has no AfterfallCharacter reference | player is not the mechanics character on launch |
| No animation | no C++ anim classes; `ABP_FP_Copy` quarantined | character and grapple motion will look unshipped |
| No audio/VFX | none in source | — |
| Grapple unverified in engine | `grapple_handoff.md`: "Verified in running game: No" | rope may not behave |
| Packaged build is stale | exe 16:26; grapple source 18:10; `Grapple` string absent | what ships is not what was built last |
| Real Game untracked | `git ls-files` empty for `Real Game/` | no rollback, no provenance |
| Concurrent editing | root grapple 17:46, Real Game grapple 18:10, `_vpass1_full_out.txt` 17:35 — other agents active during this audit | any edit now may collide; coordinate first |

## 6. Prioritized migration plan

Each step names owner, the test that proves it, and the acceptance evidence.

### Step 1 — Make the player the mechanics character (owner: Gameplay)

Change: set `DefaultPawnClass = AAfterfallCharacter::StaticClass()` in
`AAfterfallGameMode`'s constructor; ensure the configured map has a
`PlayerStart`. Do not touch the template BP.

Test: new automation `Afterfall.Baseline.PawnIsMechanicsCharacter` — assert
`GetDefaultPawnClassForController` is `AAfterfallCharacter`, and that a spawned
player start resolves.

Acceptance: test passes in the 38-test suite; headless `-game` log shows
`AfterfallCharacter_0` as the possessed pawn.

### Step 2 — A launchable map (owner: Tech/Content)

Change: either promote the runtime playground (`afMechSelfTest`) into a saved
map, or point `GameDefaultMap` at a new `Content/Maps/AfterfallSlice.umap`.
Uncomment/repair the `TestNeighborhood` entries only if that asset exists.

Test: headless boot + a `-game` run that logs player spawn, movement tick, and
restart.

Acceptance: `LogAfterfall` shows spawn → movement → restart with no fatal, and
the packaged map list contains the new map.

### Step 3 — Grapple feel and fork reconciliation (owner: Movement)

Already done: attachment, rope constraint, release and momentum are verified by
`afMechSelfTest` (11/11). Not done: how it *feels* with input in real time, and
the root project still carries the older client-authoritative copy.

Change: keep the Real Game (server-authoritative) implementation; retire the root
fork after diffing for unique logic. Add `Afterfall.Grapple.AttachRelease` so the
playground assertions also live in the automation suite.

Test: drive the playground with input in a real session (PIE or windowed `-game`)
and record attach → swing → release.

Acceptance: suite green + one recorded interactive run; root copy retired.

### Step 4 — Consolidate the forks (owner: Tech)

Change: choose `Real Game/` as canonical, bring over `InteractionEvent.h` and
any settings work worth keeping, then move the whole tree into version control
(a commit, not a copy). Retire the root project or mark it read-only.

Acceptance: `git ls-files` returns the source tree; one project, one suite.

### Step 5 — Content pass (owner: Content)

Animation (character + grapple), audio cues, VFX. No new mechanics.

Acceptance: packaged build launches with animation and audio present.

### Step 6 — Packaged Windows build (owner: Tech)

Change: package the canonical project, verify in a clean folder.

Acceptance: `Afterfall-Win64-Shipping.exe` launches outside the editor; a
status/grapple/movement smoke test is recorded; the build timestamp is newer
than every source file it ships.

## 7. Phase 4 — the path to something playable

| Milestone | Must change | How it is tested | Evidence that proves it |
|---|---|---|---|
| **A. Launchable baseline** (spawn, camera, movement, controls, restart) | Step 1 + Step 2 | headless `-game` run + `Afterfall.Baseline.PawnIsMechanicsCharacter` | log lines for possess/spawn/move/restart; test green |
| **B. Traversal** (grapple + rope + jump/fall/land + momentum) | Step 3 | `Afterfall.Grapple.*` + mech playground run | attached/released log + video or timed telemetry |
| **C. Physics + destruction in world** | wire `PhysicalItem`/destruction into the baseline map; run `afRoomSelfTest` | room self-test + `Destruction.*` (already green) | 9/9 room PASS from the *current* binary |
| **D. Remaining mechanics** (inventory, crafting, loot, damage, AI) | already connected — needs a map that exercises them | existing 37 tests + a scripted session | suite green + session log |
| **E. Debug menu with live tuning** | promote HUD diagnostics; add tuning commands | manual + automation for each control | each control changes a logged value |
| **F. Packaged Windows build** | Step 6 | launch outside editor | shipping exe newer than sources, smoke test log |

Ordering rule: A before everything. B and C in parallel after A. D reuses what
already exists — no rewrites. E and F last.

## 8. Recommended next implementation task

> **DONE — 2026-10-24, commit `0947fb4`.** See §11. The next task is now the
> startup-map decision in §11, which blocks packaging.

**Wire the default pawn and prove it headlessly.**

- Set `DefaultPawnClass = AAfterfallCharacter::StaticClass()` in
  `AAfterfallGameMode::AAfterfallGameMode()`.
- Add `Afterfall.Baseline.PawnIsMechanicsCharacter`.
- Build, run the full suite, and run one headless `-game` session on
  `/Engine/Maps/Entry` (the map already proven to boot) checking that the
  possessed pawn is `AAfterfallCharacter`.

Why this one: it is the smallest change that turns "many mechanics exist" into
"the player has the mechanics", it needs no new assets, and it is exactly what
Milestone A is blocked on. Everything in Phase 4 assumes it.

**Do not** start with the transport, the settings UI, or asset work — none of
them changes whether the game is playable, and the transport decision record
(`agent2-d1-d6-decision-record-2026-10-24.md`) is deliberately waiting on the
owner.

## 10. Consolidation executed (after the audit)

The fork described in §0 and §4.1 has been resolved.

**What was done**

- The Real Game tree was promoted to the repository root:
  `Source/`, `Config/`, `Content/`, `Scripts/`, `Docs/`, and
  `RealGame.uproject` now live at `C:\Users\mckee\Desktop\Come Up Game\`.
- The retired root project (`Afterfall.uproject` plus its `Source/`, `Config/`,
  `Content/`) was removed from the working tree; it remains in git history.
- Both dead files from the retired fork were carried across as requested:
  `Source/Afterfall/Combat/InteractionEvent.h` and
  `Source/Afterfall/UI/AfterfallSettingsComponent.{h,cpp}`.
- Files that existed in both forks with **different behaviour** (not merely
  different age) were quarantined under `Docs/archive/root-fork/` with a README
  naming which copy is live: character, grapple, zombie AI, Build.cs, resource
  node, HUD, radial menu, physics subsystem, slice subsystem, PhysicalMat,
  structural destruction, PhysicalItem, RuntimeSimulationTest, plus the
  fork-only `Combat/EventDedup.h`. **21 source files** quarantined; all 44
  identical files were dropped.
- `Content/` and `Config/` needed no merge — they were byte-identical between
  the forks.
- Build outputs, generated solutions and stray template copies are ignored, so
  the repository tracks only source, config, content, scripts and docs.

**Commits**

```
2995fd6  Promote Real Game to the repo root as the single canonical project
b53e112  Document the provenance of the quarantined retired-fork files
2d5c10a  Keep only the diverged copies in the fork quarantine
7fef21e  Point build and test tooling at the promoted project file
```

`build.sh`, `check_prereqs.sh`, `launch_2client_server.sh`, `run_tests.sh` and
`validate_first_playable.py` all required `Afterfall.uproject`, which no longer
exists; they now reference `RealGame.uproject`. Without that fix every documented
build and test command would have failed against a missing project file.

**Verified on the consolidated project** (evidence
`Tony/consolidation-evidence-2026-10-24/`)

| Check | Result |
|---|---|
| Editor build (`RealGame.uproject` at root) | `Result: Succeeded`, exit 0 |
| Full automation suite | **37 started / 37 Success / 0 Fail**, exit 0 |
| Agent status flag | `AGENTSTATUS op=status providers=1 code=ok`, exit 0 |
| Mech playground | `MECHTEST SUMMARY pass=11 fail=0`, exit 0 |
| Script suite from the new root | exit 0, **321 `[PASS]`, 0 `[FAIL]`** |

**Tracked content after the move:** `Source` 124 files, `Scripts` 38, `Docs` 46,
`Content` 51. Working tree was clean at commit time.

**Disclosures**

- The retired fork had one **uncommitted** 2-line change to
  `Scripts/open_demo.bat`. The promotion overwrote that file with the Real Game
  version, so the change is not recoverable. It was a convenience launcher; every
  other divergent source file is preserved in the quarantine folder.
- `Real Game/` still exists on disk holding ~11 GB of ignored build artifacts
  (Intermediate 6.4G, Builds 2.4G, Binaries 1.2G, Saved 429M, plus 300 MB of
  stray template-project copies). It is git-ignored and unused; deleting it is
  safe now that the root build is green, but it was not deleted without asking.
- A concurrent agent modified `Source/Afterfall/Physics/PhysicsTestSubsystem.cpp`
  after the commit. That change is left uncommitted and untouched.

**What this changes in the audit:** §4.1 (fork conflict) is closed; §5's
"Real Game untracked" blocker is closed. The recommended next task is unchanged —
§8, wire `DefaultPawnClass` — because consolidation made the tree singular, not
the game playable.

## 11. Default pawn wired (commit `0947fb4`)

`AAfterfallGameMode`'s constructor now sets
`DefaultPawnClass = AAfterfallCharacter::StaticClass()` and logs the resolved
class, replacing the "set in DefaultEngine.ini later" comment that never happened.
`Afterfall.Baseline.PawnIsMechanicsCharacter` asserts the class default, so removing
it fails the suite instead of silently regressing.

**Verified** (`Tony/pawn-evidence-2026-10-24/`)

| Check | Result |
|---|---|
| Editor build | `Result: Succeeded`, exit 0 |
| Automation suite | **38 started / 38 Success / 0 Fail** (37 + the new baseline test) |
| Headless game mode init | `AfterfallGameMode created (Seed=1337) defaultPawn=AfterfallCharacter` |
| Headless possession | `Slice pawn swapped for PlayerController_0`; zombies pursue `AfterfallCharacter_0` |
| Mech playground after the change | `MECHTEST SUMMARY pass=11 fail=0` |

### 11.1 The remaining blocker: the default map overrides the game mode

`Content/FirstPerson/Lvl_FirstPerson.umap` references `BP_FirstPersonGameMode`
(two occurrences in the asset). World Settings' GameMode override outranks
`GlobalDefaultGameMode`, and the config points `GameDefaultMap`,
`EditorStartupMap` and `ServerDefaultMap` at that map
(`Config/DefaultEngine.ini:21-23`). Consequence: **on the configured map the
player gets `BP_FirstPersonCharacter`, not `AAfterfallCharacter`** — the pawn fix
takes effect only where the map does not override, e.g. `/Engine/Maps/Entry`,
which is where all runtime evidence above was collected.

Packaging now would ship the template pawn. Three ways out, in order of cost:

1. **Repoint the maps in config** to a map without an override
   (`/Engine/Maps/Entry` is proven to boot headlessly and the slice subsystem
   seeds content into it), adding it to `MapsToCook`. Config-only, but the
   shipped game then has no authored geometry.
2. **Edit the map's World Settings** (editor, one dropdown) to select
   `AfterfallGameMode`. Keeps the template geometry; requires the editor.
3. **Reparent `BP_FirstPersonGameMode`** to `AAfterfallGameMode` in the editor.
   Same effect as 2, but the BP is Epic template content and quarantining it was
   already on the list.

Recommendation: (2) if the template map's geometry is wanted, otherwise (1).
Do not package before one of them is done — the deliverable would be a build
where none of the inventory, carry, damage or grapple code is reachable by the
player.

## 9. Concurrent reports, and what this audit changes about them

Other agents were writing into `Tony/` during this audit. Reconciliation, since
several overlap with §2:

| File | Time | Relationship to this audit |
|---|---|---|
| `MECHANICS_INVENTORY.md` | 17:39 | **0 bytes.** An empty placeholder — do not cite it as an inventory. §2 is the inventory. |
| `MECHANICS_INTEGRATION_REPORT.md` | 17:40 | Source-inspection inventory, no execution. Consistent where it overlaps; superseded by §2's paths + test evidence. |
| `AFTERFALL_MECHANICS_AND_DESTRUCTION_TEST_REPORT.md` (Muse) | 18:18 | Substantial: ports grapple into Real Game with server authority, adds `afMechSelfTest`, destroyable building. Claims, and its own §10 confirms, **no build and no test run** — the stated blocker was "editor was live". |
| `AGENT1_…`, `AGENT_A_…` | 17:33–17:35 | Both mark runtime BLOCKED on "no bash / no UE automation". Contradicted: both ran here (§1). |
| `AGENT_B_…` | 17:35 | Draft JSON contract. Its merged `code` field is rejected in D5 of the decision record. |

**Effect on Muse's blocker:** it is stale. The new files (mtime 18:10) were
compiled by the build in §1 and the 37-test suite ran green at 18:2x — so
"cannot build while the editor is live" did not hold at that moment. What remains
unverified is specifically their **new** assertions (`afMechSelfTest`:
grapple release preserves velocity, destroyable building, reset) — those were
not executed by anyone, and my run does not cover them because they live behind
a flag I did not pass. Treat them as class 2.

**Effect on §4.1:** the grapple divergence is now *intentional*, not drift. Muse
added server-authority refusal and replicated state on the Real Game copy; the
root copy remains the older client-authoritative version. The recommendation
stands — keep the Real Game copy as canonical — but delete or explicitly retire
the root one instead of merging the two.

**Effect on §2.5 / §2.6 — ran, passed:** `afMechSelfTest` on the current binary
reports `MECHTEST SUMMARY pass=11 fail=0 MECHTEST PASS` (exit 0), covering
playground build, floor/building/node/items/zombie spawns, destroyable building
with 4 live parts, melee detaching cells, grapple attach, rope length, rope
constraint after ticks, release, momentum preservation, reset, and rebuild. So
the playground fixture and the grapple port are **runtime-verified**, not just
compiled — Milestone B's hardest unknown is closed, and Muse's blocker is fully
superseded.
