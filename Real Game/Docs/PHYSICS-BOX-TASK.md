# SMALL BOX TASK — coordination record (Muse, 2026-10-24)

## Baseline (recorded before any implementation)
- Map: `Content/FirstPerson/Lvl_FirstPerson.umap` (template FP map; no
  `Content/Maps/` exists). Size 14257 bytes,
  md5 `9f9988334c60c6a3c0473c685028b2f6`, dated 2026-10-23 (untouched).
- `Source/` in Real Game is byte-identical to source tree (`diff -r` clean).
- No Kilo diff is present in the tree: nothing in-progress to preserve.
- Last verified results: build exit 0, automation 28/28, room 9/9
  (`Saved/RoomTest/realgame_20261024_121017.out`). 9/9 NOT yet reverified
  after any box change (no change exists yet).
- Stray scratch `Scripts/launch_realgame_tmp.bat` removed (own tmp file).

## Existing mechanics the box can reuse (no new framework needed)
- `APhysicalItem`: replicated, `PhysicsActor` profile, mass in kg,
  Free/Held/Stored states, server-validated pickup (E) / throw / drop via
  `UCarryComponent`. A box IS already a supported object type.
- `UCarryComponent`: range-checked pickup (300 cm), throw model
  `v_player + dir * speed`, drop at zero velocity, dupe/spam guards.
- Collision: stock `PhysicsActor` / `BlockAll` profiles; no custom channels.
- Input: code-built `SliceIMC` already binds Interact/Throw/Drop.

## Approved narrow task for Kilo (P1–P3 NOT required for this)
P1 (mass-scaled throw), P2 (fixture reconcile), P3 (carry-cap test) are
independent improvements and are EXPLICITLY OUT of this task. Implement:

- ONE small box in `Lvl_FirstPerson`, near the player start, accessible,
  clearly visible, modest size (suggest ~50 cm cube, ~2–5 kg so it is
  "light": full throw speed, no heavy-carry cap).
- Reuse `APhysicalItem` (or engine Cube primitive with the same
  `PhysicsActor` + mass setup). Map size, bounds, floor, player start:
  UNCHANGED.
- Minimal diff; report every changed file. No new plugins, assets,
  dependencies, packaging or boundary changes.

## AUTHORIZED — delegation order (owner approval 2026-10-24)
Owner authorizes Kilo to implement. HANDOFF.txt boundary is lifted FOR
THIS TASK ONLY; all other locks/claims stand (muse-physics-001,
muse-mat-001 files are off-limits).

### Finding: no project-supported map-edit workflow exists
`Scripts/` references `Lvl_FirstPerson` only for launch/test
(`open_demo.bat`, `launch_headless_gametest.cmd`, validators); no
script edits `.umap`, and the template map is World-Partitioned (per
`run_ue_replication_test.bat` comment), so headless `.umap` surgery
would be an invented workflow. A GUI map edit is banned for headless
agents. Approved path is therefore:

### Implementation path: runtime spawn, map asset byte-identical
- Spawn ONE `APhysicalItem` (existing class: 35 cm graybox cube,
  `PhysicsActor` profile, 2 kg default = "light", full throw speed,
  no heavy-carry cap) at server `BeginPlay`, ONLY when the loaded map
  is `Lvl_FirstPerson`, placed near the `APlayerStart`
  (`UGameplayStatics::GetActorOfClass` pattern already used in
  `Slice/SliceSubsystem.cpp:54`) with forward/ground offset and
  spawn-collision handling so it never embeds in the floor.
- Log the spawn (class, ItemId, location) via `LogAfterfall` so the box
  is verifiable headless in the server log.
- Keep it strictly cosmetic-to-systems: pickup/throw/drop/collision all
  come free from `APhysicalItem` + `UCarryComponent`; NO formula or
  tuning changes (P1 explicitly excluded).

### File boundaries (Kilo: touch NOTHING else)
- ALLOWED: `Source/Afterfall/AfterfallGameMode.h`,
  `Source/Afterfall/AfterfallGameMode.cpp` (BeginPlay spawn only).
- FORBIDDEN: `.umap`/any `Content/` asset, `Config/`, packaging settings,
  `Physics/*`, `Interaction/*`, `Storage/*`, `Tests/*` (unless adding one
  spawn-presence test — optional), `Docs/` boundary files, `*.uproject`,
  `Scripts/run_realgame_tests.bat`.
- If the spawn needs anything beyond these two files, STOP and report
  before proceeding.

### Kilo's acceptance checklist
- [ ] Exactly one box spawns on `Lvl_FirstPerson` server start; other
  maps unaffected (map-name guard).
- [ ] `Lvl_FirstPerson.umap` byte-identical (md5 recorded above).
- [ ] Diff limited to the two allowed files; every changed line explained.
- [ ] No new plugins, assets, dependencies, network calls, or warnings
  in touched TUs.
- [ ] Dev-AI/runtime boundary intact (no agent-infra references).

## Validation (Muse, after Kilo's diff lands)
1. `diff` review: only necessary files; one box; no unrelated changes.
2. Map size check: `Lvl_FirstPerson.umap` bounds/floor/start unchanged
   (size/hash recorded above; dimension comparison via editor map check).
3. Build `RealGame.uproject` editor target → exit 0.
4. `Automation RunTests Afterfall` → all green; `afRoomSelfTest` → 9/9.
5. Box presence + collision: owner-session visual/push check (P4 item).
6. Report: changed files, box spec, results, 9/9 status, gate eligibility.

Final gate stays CLOSED until all of the above pass with evidence.

## Combined-tree reverification (Muse, 2026-10-24 ~12:26, pre-box baseline)
No Kilo diff had landed; verification covers MAT + physics fixtures as
integrated in this tree:

- Build `RealGame.uproject` editor target: exit 0 ("Result: Succeeded").
- Automation: 28/28 `Result={Success}`, 0 fail, TEST COMPLETE exit 0
  (`Saved/Logs/RealGame_2.log`; command line proves dest `.uproject`),
  incl. `Afterfall.MatInteraction.Transitions` + `.RepeatedInput`.
- Room `afRoomSelfTest`: 9/9 ROOMTEST PASS, SUMMARY pass=9 fail=0, exit 0
  (`Saved/RoomTest/realgame_20261024_122616.out`).
- This closes MAT-INTERACTION.md's pending "full link + automation"
  item. Remaining MAT items are owner-session runtime (montage/UMG/PIE).
- `kilo-box-001` still unpicked; box validation runs after its diff lands.
