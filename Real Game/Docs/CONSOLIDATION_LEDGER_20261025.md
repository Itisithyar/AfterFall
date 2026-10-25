# AFTERFALL consolidation ledger — 2026-10-25 (`config/boot-fix`, HEAD `e8e64c2`)

Canonical project: `Real Game/RealGame.uproject` (UE 5.8, module `Afterfall`).
Checkpoint of pre-work state: `.build_monitor/checkpoint_20261025_1100/`
(git-state, full tracked diff, `RealGame.uproject`, launcher backup).
Removal manifest (prior session): `.build_monitor/consolidation_removal_manifest_2026-10-25.md`.

## 1. Branch survey — nothing left to merge

`HEAD` (`config/boot-fix` = layout + fork scripts + GameModeOverride fix)
already contains every other branch. `git log HEAD..BRANCH` is empty for:
`main`, `fix/playable-pawn`, `integ/fb-main`, `wip/freebuff2`,
`wip/kilo-building` (merge-only delta), `feature/load-physics-and-predictive-matrix`,
`wip/procedural-building`, `wip/fragment-building`, `tidy/module-layout`.
Only `wip/kilo-evidence` has a unique commit (11-line stub probe) that is
superseded by the richer 55-line `WorldGenHouseEvidenceTest.cpp` already in
the tree. Named features verified present: `Combat/HitPrediction.h` (FR-004),
`Structural/BoxBuilding.h` + `Tests/BoxBuildingTest.cpp` (FR-013),
`UI/RadialMenuComponent.h`. The `Diag/AfterfallPhaseWatchdog` was intentionally
temporary (owner removed it) — correctly absent.

## 2. Module / source consolidation (this session)

- Single module `Afterfall` (`IMPLEMENT_PRIMARY_GAME_MODULE` present): 1×
  `.Build.cs`, 3× `.Target.cs` (Game/Editor/Server) all consistent.
- No duplicate class names, no duplicate basenames in `Source/`.
- Removed 7 verified-empty scaffold dirs: `Source/Afterfall/{Buildings,
  Geography, NPCs, Players, Shared, WorldSystems, Zombies}/` (0 files each).
- 2 TODO markers kept as documented future work (Chaos fracture; damage-state
  visuals) — not defects.
- Config: `DefaultEngine.ini` (maps + `GlobalDefaultGameMode=/Script/Afterfall.AfterfallGameMode`,
  `?game=` URL on GameDefaultMap), `DefaultGame.ini` (targets Afterfall/AfterfallServer,
  cooks `Lvl_FirstPerson`), `DefaultInput.ini` (Enhanced Input) — coherent, no merge needed.
- Not touched (other agents' active work): `AfterfallCharacter.cpp` +1 line,
  `Lvl_FirstPerson.umap`, `AfterfallFixMapCommandlet.*`, `clear_gamemode_override.py`,
  `_inventory/`, `_CONFIG_AUDIT.md`, `Saved/`.

## 3. Repairs made (verified, with before/after evidence)

- **P0 — PIE ensure on every pawn spawn (regression-grade):**
  `AAfterfallCharacter::BeginPlay` bound `OnInventoryChangedForLoad` via
  `AddDynamic`, but the handler lacked `UFUNCTION()` → `Ensure condition
  failed: this->IsBound()` at every `BeginPlay` (log `RealGame.log`
  11:02:47 run, `DelegateSignatureImpl.inl:1220`). Fix: added `UFUNCTION()`
  to the declaration in `AfterfallCharacter.h` (header had no uncommitted
  edits; 1-line additive change matching the author's stated intent).
  After: UBT editor rebuild exit 0 (42.7 s, actually compiled), headless
  suite re-run shows **0 occurrences** of the ensure, 94/94 Success.
- No other repairs; no demonstrated consolidation regressions exist.

## 4. Feature matrix (evidence-backed)

| Feature | State | Evidence |
|---|---|---|
| Editor target compile | Implemented + verified | UBT `AfterfallEditor Win64 Development` exit 0 (×3 runs 2026-10-25) |
| Game target compile | Implemented + verified | UBT `Afterfall Win64 Development` exit 0, 96 s XGE, `Binaries/Win64/Afterfall.exe` (333 MB) |
| `Afterfall.*` automation (39 test files) | Implemented + verified | Headless `UnrealEditor-Cmd` run: **94 Success / 0 Fail, TEST COMPLETE exit 0** (`Saved/Logs/RealGame_2.log`, `RealGame.log` re-run) |
| Python mirror validators (13) | Implemented + verified | `validate_first_playable/body_damage/loot/persistence` all ALL PASS exit 0 |
| Pawn spawn + PlayerStart + GameMode | Implemented + verified | `ChoosePlayerStart` override + authored PlayerStart + `?game=` URL + umap override cleared (e8e64c2); PIE reaches `Match InProgress`, pawn possessed |
| Inventory/load, loot, persistence, destruction, BoxBuilding, HitPrediction, radial UI, slice, voxel | Implemented, suite-green (headless) | Covered by the 94 passing automation tests; runtime visuals not re-proven here |
| Chaos fracture / damage-state visuals | Partially implemented | 2 TODO markers (ProceduralBuilding.cpp:120, StructuralActor.cpp:299) |
| TestNeighborhood map | Missing (intentional interim) | Config points at template `Lvl_FirstPerson`; slice target commented in ini |
| Packaged game + play copy + smoke test | Blocked (deferred, not failed) | Tree has uncommitted changes (umap, character) + concurrent agent builds; packaging requires a stable revision and exclusive `Saved/` access (§8) |
| Second `.uproject`, fork/duplicate dirs, stale worktrees | Intentionally retired | Prior session manifest; `Afterfall.uproject` backup in `Docs/_fork_originals/` |

## 5. Build / test evidence index (2026-10-25)

- `/tmp/ubt_editor.log`, `/tmp/ubt_editor2.log`, `/tmp/ubt_rebuild.log` — editor UBT exit 0.
- `/tmp/ubt_game.log` — game UBT exit 0, 96 s XGE compile.
- `/tmp/ubt_fix.log` — post-`UFUNCTION` rebuild exit 0, 42.7 s.
- `/tmp/afterfall_tests.log` + `Saved/Logs/RealGame_2.log` — 94/94, exit 0 (ensure present).
- `/tmp/afterfall_tests2.log` + `Saved/Logs/RealGame.log` — 94/94, exit 0 (ensure gone).
- `/tmp/validate_*.log` — 4/4 mirror validators pass.

## 6. Exact next actions (owner)

1. Commit when ready (left uncommitted): root deletions + `Real Game/` twins + umap fix + commandlet + `AfterfallCharacter.{h,cpp}` + these docs. Coordinate the character/umap hunks with their author first.
2. Package from a clean, committed revision: close editors, confirm no `UnrealEditor-Cmd`, run `Real Game\BuildAndLaunchAfterfall.bat` in a normal Windows terminal.
3. Play-copy smoke test: `Scripts\Play.bat` after packaging; then `run_realgame_tests.bat` stages 3–4 (room + pipeline fixtures).
