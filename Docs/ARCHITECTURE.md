# AFTERFALL — Architecture Report (Phase 1, UE 5.8)

Date: 2026-10-23. Workspace was EMPTY (no .uproject, no Source, no git). Scaffolded per brief §§1–2.

## 1. Inspection result
- Folder `C:/Users/mckee/Desktop/Come Up Game/` contained zero files, not a git repo (now `git init` + LFS 3.8.0 done).
- `C:/Program Files/Epic Games/UE_5.8/` holds only `.egstore` stub — engine NOT installed.
- VS Community present but WITHOUT C++ workload (`VC/Tools/MSVC/` missing).
- Disk: 133 GB free — enough for UE 5.8 (~60 GB) + project.
- Targets locked: UE 5.8, C++ First-Person template, Windows client + Windows dedicated server.

## 2. Dependency map
```
Afterfall.uproject (EngineAssociation 5.8)
 ├─ Source/Afterfall.Target.cs        (Game, Windows)
 ├─ Source/AfterfallServer.Target.cs (Server, authoritative)
 ├─ Source/AfterfallEditor.Target.cs (Editor)
 └─ Source/Afterfall/Afterfall.Build.cs → Core, CoreUObject, Engine, InputCore, EnhancedInput, NetCore
     ├─ Afterfall.h/.cpp             (module, LogAfterfall) — Owner: Tech
     ├─ AfterfallGameMode.h/.cpp     (RegionSeed=1337) — Owner: Gameplay
     ├─ AfterfallCharacter.h/.cpp    (BuildPreviewRequest client-only + ServerRequestPlacement) — Owner: Gameplay
     └─ Structural/
         ├─ StructuralTypes.h        (EStructuralPart, FStructuralSpec 50cm grid, FPlacementRequest) — Owner: Structural ★SHARED★
         └─ StructuralActor.h/.cpp   (replicated Spec/Durability/OwnerPlayerId, ServerApplyDamage/Repair) — Owner: Structural
Config/DefaultEngine.ini → 30Hz server tick, World Partition default, TestNeighborhood maps
Config/DefaultGame.ini   → Shipping packaging, server target, TestNeighborhood cook
Scripts/ → check_prereqs.sh, build.sh, run_tests.sh, launch_2client_server.sh
```

## 3. Key decisions (brief compliance)
- **Shared structural system first (§§4–5 of brief):** `FStructuralSpec`/`AStructuralActor` are the SINGLE definitions for proc-gen buildings (Phase 4) and player builds (Phase 3). No fork.
- **Server authority (§§8–9):** only `ServerRequestPlacement`, `ServerApplyDamage`, `ServerRepair` mutate state; client sends grid+rotation only. `ServerInitialize` rejects non-authority callers. Grep-guarded in `run_tests.sh`.
- **Determinism:** `RegionSeed` lives on GameMode (GameState replication + PCG seed in Phase 4).
- **Streaming:** World Partition enabled by default; `TestNeighborhood` (10–20 buildings) is the first partitioned map.
- **Perf provisional:** 60 FPS client / 30 Hz server (`NetServerMaxTickRate=30`); profiling hooks deferred to Phase 7.

## 4. Naming / ownership (agent workflow)
- `A-` actors, `F-` structs, `E-` enums, `LogAfterfall` log category.
- Exclusive owners: Tech = Config/Scripts/Targets; Gameplay = Character/GameMode; Structural = Structural/*. Parallel work must publish header changes first and re-run `run_tests.sh`.

## 5. Build / test / launch (reproducible)
- `bash Scripts/check_prereqs.sh` — currently FAILS on 2 items (engine + MSVC) with fix commands.
- `bash Scripts/run_tests.sh` — functional checks pass without engine; UE automation skipped until install.
- `bash Scripts/build.sh all` — editor + server (blocked until prereqs pass).
- `bash Scripts/launch_2client_server.sh` — 1 server + 2 clients, ports 7777, acceptance steps in script echo.

## 6. Phase 3b — server validation wiring (2026-10-23)
- `Structural/ConstructionValidator` (pure, unit-tested): grid 50cm, `RequiresSupport` (all but Foundation),
  cost table Foundation 4 / Wall-Floor 2 / Door-Window-Roof 3 / Barricade-Lock 1,
  durability Foundation 300 / Wall 100 / Door 80 / Window 60 / Barricade 50.
  `ValidatePlacement` order: off-grid → lot-claimed → no-support → insufficient-resources.
- `Inventory/InventoryComponent` (replicated Wood=10 start): `ServerAddResource` validates 1..100,
  `ServerTryConsume` authority-only, single debit path for placement/repair/upgrade.
- `AAfterfallCharacter`: placement does overlap support query (30cm box at -25cm Z),
  ownership query (claimed-by-other reject), `ValidatePlacement`, debit-then-spawn with refund on
  collision failure. New `ServerRequestRepair` (1 wood/50 missing, ceil) and
  `ServerRequestUpgrade` (tier 0->1, 2 wood, 1.5x durability).
- `AStructuralActor`: +`UpgradeTier` replicated, `ServerUpgrade`, `ServerClaim` (first-claimer wins).
- Tests: `Tests/ConstructionValidatorTest.cpp` (Afterfall.Construction.*) mirrored by
  `Scripts/validate_phase3.py` — 9/9 mirror cases pass without engine (see run_tests.sh).
- No client-authoritative spawn (grep-guarded). Client still sends only part+grid+rotation.

## 7. Blockers & next step
1. ~~UE 5.8 install~~ DONE (UnrealEditor.exe present) — VS C++ workload STILL MISSING
   (`VC/Tools/MSVC/` absent). Fix: VS Installer → Modify Community → “Desktop development with C++” + Win11 SDK.
2. Then: `build.sh editor` → create `/Game/Maps/TestNeighborhood` (World Partition, 10–20 lots)
   → FP Input Mapping Context → run Automation `Afterfall.Construction.*` in-editor.
3. Phase 4 next: deterministic lots + room connectivity reusing `FStructuralSpec`/`AStructuralActor`.
