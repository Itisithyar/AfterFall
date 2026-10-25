# Migration manifest — retired `Real Game/` tree → canonical root (2026-10-24, BEAM)

Canonical project: `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject` (sole source tree
`Source/`, 141 files). The nested `Real Game/` directory is a retired scaffold: it contains
NO `Source/`, NO `Config/`, NO `Content/` at its own root (verified by listing 2026-10-24
~21:00). CORRECTION 21:35: nested `Real Game/AfterFall/` DOES hold AfterFall.uproject +
Config + Content (130 MB) + Saved (no Source). Full census in `Docs/AFTERFALL_MERGE_
REPORT.md`: 174/239 nested content files already in root; the rest is unsafe to merge
(ExternalActors), owner-disabled (.bak anims), or superseded (template configs) — zero
copies required, nothing portable remains.

## Preserved (copied, no-clobber, docs-only — zero build impact)
- `Real Game/README-PORT.md` → `Docs/RETIRED_TREE_README_PORT_2026-10-24.md`
- `Real Game/BUILD_RECOVERY_HANDOFF.md` → `Docs/RETIRED_TREE_BUILD_RECOVERY_HANDOFF_2026-10-24.md`
- Prior related note: root `Binaries/Win64` editor DLL + `AfterfallEditor.target` rebuilt 20:39
  from current source (BUILD_EXIT:0); game/shipping binaries and both `Builds/` packages are
  older and stale.

## Referenced, NOT copied (per README-PORT's own evidence rule: only `RealGame*.log` and
`realgame_*.out` count; merging would pollute root evidence)
- `Real Game/Saved/RoomTest/*.out`, `Saved/Pipeline/*`, `Saved/AutomationReports/index.*`,
  `Saved/build-*.log`, `Build/Windows/FileOpenOrder/*.log` (cooker open-order, useful for packaging)
- `Real Game/Saved/Config/ConsoleHistory.ini` (trivial, skipped)

## Deliberately NOT migrated
- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Builds/` (machine-generated; the 16:28
  `Builds/Windows` package is proven broken at launch — uncooked `WorldGridMaterial` shader map,
  fatal in `MaterialShared.cpp:3162`; see `_pkg_launch.log`)
- `Afterfall.uproject`, `*.sln` (stale paths; root `.uproject`/`.sln` are supersets)
- `BuildAndLaunchAfterfall.bat` (already retargeted at root; no action)
- `Saved/Crashes`, `Autosaves`, `Cooked`, `StagedBuilds` (transient)

## Blocked at manifest time (unchanged, needs owner session)
- Interactive `UnrealEditor.exe` live during manifest (3 GB); no builds, tests, imports, or
  package launches were started. Manny Physics Asset, automation suites, `-afBoxWall` rendered
  checks all still require that editor window.
- Headless `UnrealEditor-Cmd -ExecCmds="Automation RunTests …"` exits 0 after platform
  validation without launching any test (reproduced via bash and cmd.exe) — the established
  script's step [2/3] needs repair before gates can pass.

## Update 21:50 — runner repair identified (BEAM, zero launches)
- Repair: drop `-TestExit` from the headless invocation. Evidence: `Docs/AGENT-GAME-CONNECT-
  TEST-REPORT.md` shows the same `-Cmd` + `-ExecCmds="Automation RunTests …"` pattern reaching
  TEST COMPLETE EXIT CODE 0 without `-TestExit`; all three BEAM failures carried
  `-TestExit="Automation Test Queue Empty"` (quit-on-empty fires before ExecCmds enqueues).
  House pattern for full runs: `UnrealEditor.exe <uproject> /Engine/Maps/Entry -server …
  -abslog=<abs path> -stdout -FullStdOutLogOutput` + `-af<SelfTest>` flags.
- Observed third-party results (not BEAM-run, logs cited): 21:27 automation 46 Success /
  0 Fail (`Afterfall.BoxBuilding.*` incl. FIX-A assertions, Medical, BodyPhysics), MECHTEST
  14/0 (melee/glass/grapple), WORLDGEN determinism + spacing + radius passes.
- Not launched by BEAM: another agent's pipeline fires every few minutes; any BEAM launch
  would contend. Awaiting quiet window for the single controlled verification run.

## Update 21:21 (BEAM, user-authorized build window)
- FULL-SEND §3 mobility fix: `ABoxBlockDebris::ArmFrom` sets Movable before SetStaticMesh.
- Editor build `AfterfallEditor Win64 Development RealGame.uproject` → BUILD_EXIT:0, 4 actions
  incl. `BoxWallStructure.cpp`, 0 warnings/errors (`_build_mobility_fix.log`). Full tree green
  with all agents' latest source (ArticulatedBody, inventory fix, wall runtime).
- RAM at build: 31.9 GB total / 17.0 GB free — 8 GB budget met with headroom.
- My treatment/medical files verified intact and coherent (bandage→consume→ServerHeal chain);
  ArticulatedBody is server-authoritative, no conflicts with medical/body-physics files.
