# RUNTIME-PIPELINE.md — destruction/state/collision/delta proven in the game runtime

Status: **implemented, built, and verified** on 2026-10-24 (Win64 Editor target,
headless game world). No commit, no push, no packaging, no new persistence format.

Scope of the claim (keep these separate):

| Layer | State |
|---|---|
| Module compiled | yes (`Result: Succeeded`, exit 0) |
| Module loaded | yes (`InternalLoadLibrary: 'Afterfall'`) |
| Subsystem registered | yes (`SubsystemRegistry: registered Voxel (1 total)`, `afPipelineSelfTest` step 1) |
| Production event path connected | yes (`OnBuildPressed` → `ServerRequestPlacement`; `ServerAttack`/`ServerFireHitscan` → `ServerApplyImpact`) |
| Automated runtime integration test | yes (`-afPipelineSelfTest`, 17/17 in a live game world) |
| Human interactive play | **no** — no display in this environment (see §7) |
| Packaged `.exe` | **no** — blocked, see §7 |

## 1. Root cause (evidence)

Three separate gaps kept the systems from being *proven* to work together. They were
found by tracing callers, not assumed.

**1a. Nothing in play ever created a destructible structure.**
`AAfterfallCharacter::ServerRequestPlacement` is the only production creator of
`AStructuralActor`, and before this repair it had **no caller**: no input action, no
UI, no timer. `grep` over the tree found the declaration, the definition and the
`_Validate` body — and zero call sites. A played session therefore had no structural
geometry, so cell mapping, durability, destruction commits, collision sync, rebuild,
delta export and reload all had nothing to operate on.

ROOMTEST hid this: `UDestructionRoomSubsystem::SpawnRoomPart` spawns its own five
parts and the fixture owns the geometry, so the nine assertions pass without any
production placement.

**1b. ROOMTEST damages structures through a test-only entry point.**
`UDestructionRoomSubsystem::ApplyLocalHit` builds an `FDestructionEvent` from a
*local* origin/direction and calls `AStructuralActor::ServerApplyImpact` directly.
The character's real melee/hitscan handlers (`ServerAttack`, `ServerFireHitscan` —
which trace from the server-side camera, resolve the actor, and only then call
`ServerApplyImpact`) were never executed by any test. They were implemented and
unverified: a bug in aim, range gating, ammo gating or actor resolution would have
been invisible while all nine ROOMTEST checks stayed green.

**1c. The build helper built the wrong project (false-pass hazard).**
`Scripts/build_editor_tmp.bat` invoked UBT with `..\Afterfall.uproject` (the *source*
project) while every run loaded `Real Game\Binaries\Win64\UnrealEditor-Afterfall.dll`.
Observed directly during this work: a source edit was present in the file, UBT
printed `Result: Succeeded`, and the running binary still contained the old code —
i.e. a verification loop that can pass against stale code. Fixed in this change.

## 2. Changes

| File | Change | Why |
|---|---|---|
| `Source/Afterfall/AfterfallCharacter.h/.cpp` | `OnBuildPressed` + `IA_Build` (V) → `ServerRequestPlacement(BuildPreviewRequest(...))` | Part A: gives the only production structural spawn path a real caller |
| `Source/Afterfall/Slice/SliceSubsystem.h/.cpp` | Runtime structural seed in `DoSliceSetup` (foundation + wall via `SpawnStructuralPart`, i.e. `SpawnActor` + `ServerInitialize`); `TraceThroughCell` helper; `RunPipelineSelfTest`; `RequestPipelineSelfTest`; `-afPipelineSelfTest` flag + `afPipelineSelfTest` console command | Makes the pipeline reachable in play and gives it a runtime acceptance test that exits 0/1 |
| `Scripts/build_editor_tmp.bat` | Targets `RealGame.uproject` | Removes the stale-binary false pass (§1c) |
| `Scripts/run_realgame_tests.bat` | New stage `[4/4] afPipelineSelfTest`, each stage's exit code asserted independently | One documented entry point that cannot be satisfied by an earlier stage's success |

No second implementation of any existing subsystem was added: the test drives the
character RPCs, `ServerApplyImpact`, `ExportDelta`/`ImportDelta` and
`RefreshDestructionVisual` exactly as gameplay does.

## 3. Call graph

Before:

```
Player input ──▶ (nothing)                 ServerRequestPlacement  ← dead code
Player LMB/F ─▶ ServerAttack / ServerFireHitscan ─▶ ServerApplyImpact   (implemented, never run)
AStructuralActor ◀── only UDestructionRoomSubsystem::SpawnRoomPart (test fixture)
ROOMTEST: ApplyLocalHit ─▶ ServerApplyImpact        (bypasses the character)
```

After:

```
Player input V ─▶ OnBuildPressed ─▶ ServerRequestPlacement ─▶ SpawnActor<AStructuralActor>
                                                            + ServerInitialize (authority)
World start   ─▶ UAfterfallSliceSubsystem::DoSliceSetup ─▶ SpawnStructuralPart ×2 (runtime seed)

Player LMB ─▶ OnAttackPressed ─▶ ServerAttack ─▶ server view trace ─▶ AStructuralActor::
Player F   ─▶ OnFirePressed   ─▶ ServerFireHitscan ─▶ server view trace ─▶   ServerApplyImpact
                                                                              │
                            DestructionPropagation (energy/resistance, rotation-aware cells)
                                                                              ▼
                                                       DetachedCells (authoritative) + Durability
                                                                              ▼
                            RefreshDestructionVisual ─▶ ISMC rebuild + per-cell collision sync
                                                                              ▼
                            ExportDelta ─▶ (save) ─▶ ImportDelta ─▶ RefreshDestructionVisual (load)
```

`-afPipelineSelfTest` walks that graph in a live world and asserts each hop; it does
not inject damage behind the character's back.

## 4. Verification (actual commands and output)

Runner: `Real Game\Scripts\run_realgame_tests.bat` → `RUNNER_EXIT:0`
(log: `Saved/Pipeline/full_runner.out`).

| Stage | Command | Result |
|---|---|---|
| 1 Build | `Build.bat AfterfallEditor Win64 Development RealGame.uproject` | `Result: Succeeded`, exit 0 |
| 2 Automation | `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests Afterfall; Quit"` | `AUTOMATION_EXIT:0`, 36 tests `Result={Success}`, 0 `Result={Fail}` |
| 3 Room baseline | `UnrealEditor.exe ... -server -ExecCmds="afRoomSelfTest"` | `ROOMTEST SUMMARY pass=9 fail=0 ROOMTEST PASS` |
| 4 Runtime pipeline | `UnrealEditor.exe ... -game -nullrhi -afPipelineSelfTest` | `PIPELINE SUMMARY pass=17 fail=0 PIPELINE PASS`, process exit 0 |

All nine original ROOMTEST scenarios still pass unmodified; no assertion was
weakened or removed.

`afPipelineSelfTest` (game world, listen server, `-game -nullrhi`), from
`Saved/Pipeline/pipeline_run.out`:

```
PASS registry available
PASS voxel provider registered + ready (state=2)
PASS repeated initialize does not duplicate (before=1 after=1)
PASS destructible structure in world (StructuralActor_1)
PASS production spawn path creates a target part
PASS possessed character available (AfterfallCharacter_0)
view: cam=X=-645.344 Y=579.013 Z=558.000 rot=P=0 Y=90  forward-hit=StructuralActor_2 target=StructuralActor_2
PASS melee -> structural cells detached (0 -> 1)
PASS hitscan -> more structural cells detached (1 -> 3)
PASS durability debited by impacts (60.0 / 100.0)
PASS detached cell no longer blocks (cell 10 -> nothing)
PASS intact cell still blocks (cell 0 -> StructuralActor_2)
PASS delta matches authoritative state (cells=3/3)
PASS reload spawns replacement
PASS reload restores destroyed cells (3/3)
PASS reload keeps stable identity
PASS reloaded hole stays non-colliding (cell 10 -> nothing)
PASS reload does not duplicate structures (before=3 after=3)
PIPELINE SUMMARY pass=17 fail=0 PIPELINE PASS
```

The first three assertions are registration/lifecycle (module loaded → provider
present and `Ready`; `Initialize()` twice does not duplicate the index). The rest
are the gameplay pipeline: melee → cells, hitscan → more cells, durability debit,
collision removal for the hole, collision retention for an intact cell, delta equals
authority, reload restores the hole and keeps it non-colliding, and reload does not
duplicate actors.

## 5. Test-harness notes (so the test is not mistaken for production code)

* The pawn is a real `AAfterfallCharacter`. If no possessed one exists the fixture
  spawns one and possesses it via a real `APlayerController` — that is scaffolding;
  the damage itself always goes through the character's server RPCs.
* Aim is produced through the real view transform: the fixture measures the camera
  offset the pawn produces, stands so the camera looks at the target centre, and
  calls `PlayerCameraManager::UpdateCamera(0)` because the view is otherwise only
  refreshed on tick. Two earlier red runs (0 cells, no durability change) were
  caused by a grazing/stale camera ray, not by damage logic — logging the view and
  the forward hit is what localised it.
* The damage target is a second part spawned through `SpawnStructuralPart`, in clear
  space. The slice seed wall sits on its own foundation (interpenetrating boxes), so
  a camera ray reaches the foundation first.
* Room/geometry assertions use `ECC_Visibility` traces through the specific cell, the
  same technique ROOMTEST uses, and the traced actor is logged with every check.

## 6. Instrumentation

* `-afPipelineSelfTest` — command-line flag, runs 4 s after slice setup, exits with
  code 0/1 and logs `PIPELINE ...` / `PIPELINE SUMMARY`.
* `afPipelineSelfTest` — console command for manual use; it defers until slice setup
  has produced the seed (and refuses on dedicated servers with an explicit message
  rather than a misleading pass).

## 7. Limitations / not verified

1. **No interactive play.** No display is available here: the runtime test is a
   headless `-game -nullrhi` world. A human session (looking, aiming, pressing V/LMB/F)
   is still the remaining manual acceptance step.
2. **No packaged executable.** The project cannot cook locally (no reachable
   Zen/local cook for this content, as recorded in the runner header), so the
   standalone `Afterfall.exe` path remains unproven; `-game`/`-server` in-editor
   remain the supported runtime paths. Packaging was **not** attempted, so nothing
   is claimed about it.
3. **Keystroke → input action binding** is compiled and bound (`IA_Build` on V, bound
   in `SetupPlayerInputComponent`) but is not synthesised by the test; the test calls
   the same server RPCs the input handlers call.
4. **Multi-client replication** of the runtime-spawned seed was not re-run in this
   pass (`Scripts/run_ue_replication_test.bat` from the earlier room work remains the
   reference for replication; it is unaffected by these changes).
5. `afRoomSelfTest`'s own damage steps still use `ApplyLocalHit`; they were left
   untouched on purpose so the nine baseline assertions stay exactly as they were.

## 8. Reproduce

```bat
cd "Real Game"
Scripts\run_realgame_tests.bat
```

Single stage:

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" ^
  "C:\Users\mckee\Desktop\Come Up Game\Real Game\RealGame.uproject" ^
  /Engine/Maps/Entry -game -unattended -nopause -nosplash -nullrhi -nosound ^
  -afPipelineSelfTest -log -NoVerifyGC -stdout -FullStdOutLogOutput
```

Exit code 0 and a `PIPELINE PASS` line are both required; the runner asserts each.

## 9. Untouched

Everything else in the tree, including the source-project tree (`Source/`, `Config/`,
`Docs/` at the repository root), `AfterFall 5.8/`, and the other agents' untracked
scripts and logs. `Real Game/` itself is untracked in the repository; it was left as
found apart from the four files listed in §2.
