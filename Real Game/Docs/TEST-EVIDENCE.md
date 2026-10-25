# TEST-EVIDENCE.md — headless verification (2026-10-24, no display)

All runs: `UnrealEditor-Cmd.exe Afterfall.uproject` (UE 5.8, MSVC 14.50 + WinSDK 22621).

## PROVEN

1. **Editor build SUCCEEDS** — `UBT AfterfallEditor Win64 Development` → `Result: Succeeded`, exit 0.
   All game C++ (validator, inventory, loot, persistence, crafting, entities, resource, zombie,
   7 automation suites) compiles under strict V7 / Cpp20 / Unreal5_8 include order.
2. **Game boots headless and runs** — `-game -nullrhi`:
   `Running engine for game: Afterfall` → module `Afterfall` loaded →
   `LogAfterfall: AfterfallGameMode created (Seed=1337)` → module startup →
   OpenWorld up for play → **stable 4+ min, no fatal, no real errors**
   (only benign aqProf/Vtune missing-DLL + SDK-range notices).
3. **Logic mirrors 8/8 PASS** (`run_tests.sh`): phase3, loot, lootservice, persistence,
   inventory/crafting, entity lifecycle, first playable, perf budget.
4. **ProjectID config bug FOUND + FIXED** — hyphenated `ProjectID=AFTERFALL-UE58-SLICE`
   broke config import (`LogObj ... import failed for ProjectID`); removed, engine generates ID.

## KNOWN ISSUES (for in-editor follow-up, not blockers on our code)

- **Lvl_FirstPerson doesn't load in Cmd-mode**: `open` needs `GEditor` (assert in
  `ActorEditorContextSubsystem`), and direct map arg gets msys-mangled to
  `C:/Program Files/Git/...`. In the real editor (GEditor present) use PIE directly.
  `Scripts/open_demo.bat` double-click path is unaffected.
- **Template anim warning**: `ABP_FP_Copy` reports a broken skeleton link
  (`LogBlueprint Error ... skeleton asset`). It's Epic-template content in
  `Content/FirstPerson/Anims/` (a `_Copy` duplicate), not AFTERFALL code.
  In-editor fix: open the ABP, reassign skeleton or delete the duplicate.
- **In-engine automation idle**: `-ExecTests=Afterfall -testexit="Automation Test Queue"`
  booted fine (automation modules loaded) but the queue never fired headless.
  Run instead in-editor: Session Frontend → Automation → `Afterfall.*` → Run.

## 2026-10-24 (later) — runtime pipeline in a real game world

`Scripts\run_realgame_tests.bat` now runs four stages and asserts each exit code
separately (`RUNNER_EXIT:0`):

1. build `RealGame.uproject` → `Result: Succeeded`,
2. `Afterfall.*` automation → exit 0, 36/36 success, 0 fail,
3. `afRoomSelfTest` → `ROOMTEST SUMMARY pass=9 fail=0 ROOMTEST PASS`,
4. `-game -afPipelineSelfTest` → `PIPELINE SUMMARY pass=17 fail=0 PIPELINE PASS`
   (melee + hitscan through the character RPCs → cells, durability, collision,
   delta, reload; see `RUNTIME-PIPELINE.md` for the root cause and the log).

The earlier headless-only caveat about automation no longer applies to this tree:
the `Afterfall.*` suite runs headless in the same runner. Still **not** proven:
packaged `.exe` (no local cook available) and human interactive play (no display).

## NOT RUN (needs display / your hands)

- PIE on Lvl_FirstPerson, TestNeighborhood creation, `stat unit` FPS capture,
  2-client server session. Toolchain is green now — see FIRST-PLAYABLE-TEST-CHECKLIST.
