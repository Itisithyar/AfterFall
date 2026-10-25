# REPORT 2 — Afterfall / Muse / BEAM Filesystem Separation Audit (Muse, 2026-10-24 ~19:10)

Read-only audit. Nothing moved, renamed, deleted, merged, or reconfigured.
Authority unchanged: `C:\Users\mckee\Desktop\Come Up Game\RealGame.uproject`.

## Component table

| Component | Current path | Actual role | Reads / writes | Issues |
|---|---|---|---|---|
| Afterfall (authoritative) | `Come Up Game\RealGame.uproject` + `Come Up Game\Source\` + `Content\` + `Config\` | Game project (module `Afterfall`, UE 5.8) | Build/package/read-write via UBT+UAT; verified by §§4–6 of Report 1 | None — builds clean |
| Afterfall (stale twin) | `Come Up Game\Real Game\Afterfall.uproject` ("stylized multiplayer survival vertical slice", module also named `Afterfall`) | Diverged legacy tree; `Real Game\Source\` no longer exists (removed ~18:39 this session) | Nothing references it for builds now | Module-name collision risk if ever co-built; otherwise dormant |
| Template copies | `AfterFall\`, `AfterFall 5.8\`, `Real Game\AfterFall\`, `Real Game\AfterFall 5.8\` (.uproject, no modules) | Content-only template snapshots (135 MB–852 MB, differ from each other per `diff -rq`) | Not built, not referenced | Disk weight only; do not delete without owner approval |
| Muse workspace | `Come Up Game\Source\` edits are the *output*, not the workspace; no `Muse*` dir inside game tree (verified absent) | Implementation agent (this session) | Wrote: playground+grapple sources (prior task); this task: zero source edits | Clean |
| BEAM Free monitoring | `C:\Users\mckee\Desktop\Tony\` (`afterfall_watcher.py`, `tony_event_adapter.py`, `run_afterfall_events.py`, `watcher_events.jsonl`, reports, `OpenJarvis\frontend\`) | Watcher + event pipeline + verification reports | Reads game `Source\`; writes only inside `Tony\` (+ served copy under `OpenJarvis\frontend\public\`, gitignored) | Watcher DEFAULT_ROOT updated to surviving path (see below) — correct |
| Communication | `Tony\AFTERFALL_*.md` reports; `tony_events.jsonl`; `watcher/adapter_health.json`; `pipeline.lock` (single-instance) | Agent handoff + live event pipeline | Producer→adapter→UI; lockfile prevents duplicates | Intact; no producers running now |
| Archives/backups | `Come Up Game\Docs\archive\root-fork` (read-only fork note); `AfterfallSafetyBackups\2026-10-24_19-04-09\` (verified hashes) | Recovery/history | Read-only + verified backup | Intact |
| Build outputs | `Come Up Game\AfterfallStaging\` (928 MB, 18:53 user package, preserved); `Desktop\AfterfallStaging_2026-10-24_pkg\` (19:06 verified package); `Binaries\`, `Intermediate\`, `DerivedDataCache\`, `Saved\` | Generated artifacts | UBT/UAT outputs only | See finding 3 (staging lives in two places) |

## 1. Confirmed mixing / cross-wired references

1. **`Scripts\build_editor_tmp.bat` points at a nonexistent project** (confirmed:
   `ls "Real Game/RealGame.uproject"` → no such file). Its own comment claims it
   was fixed to build "the Real Game target" off `..\Afterfall.uproject`, but the
   coded path `...\Come Up Game\Real Game\RealGame.uproject` matches neither the
   authoritative (`...\Come Up Game\RealGame.uproject`) nor any existing file —
   invoking it fails (or worse, a future file at that path could false-pass).
   Several sibling scripts (`aftestrun.bat`, `launch_headless_gametest.cmd`,
   `live1_*`, `build_menu_lab.bat`, `build.sh`, `launch_2client_server.sh`) also
   name `Real Game`-rooted paths — each needs its target verified before use.
2. **Watcher root relocation (handled, not broken):** `Real Game\Source\` was
   removed ~18:39 outside this pipeline; `afterfall_watcher.py DEFAULT_ROOT` now
   points at the surviving `Come Up Game\Source\Afterfall` (121 files visible,
   session files verified intact). Old reports citing `Real Game\Source\` paths
   are stale in letter but correct in content.
3. **Staging ambiguity:** the task's stated destination
   `Desktop\AfterfallStaging` was never the real dir — actual outputs are
   `Come Up Game\AfterfallStaging\` (user's 18:53 package) and
   `Desktop\AfterfallStaging_2026-10-24_pkg\` (verified 19:06 package).
   No overwrite occurred (new timestamped dir used).

## 2. Duplicate / stale trees (evidence-distinguished)

- 6 `.uproject` files; only `RealGame.uproject` is authoritative (description
  field says so; it is the only one that compiles per Report 1).
- `Real Game\Afterfall.uproject` ≠ authoritative (different description, same
  module name — collision risk only if co-built; currently dormant).
- 4 module-less template `.uproject`s — inert snapshots.
- No BEAM/Muse/Tony config inside `Source\` (find: empty); no `.uproject`/`.umap`
  inside `Tony\` (find: empty). Agent↔game separation is real, not assumed.

## 3. Paths where an agent could edit the wrong files

- `Scripts\build_editor_tmp.bat` (nonexistent target — fix path before any use).
- Any script hard-coding `Real Game\Source\` (removed tree) — audit `Scripts\`
  before running (flagged files listed above).
- `AF_WATCH_ROOT`/`AF_WATCH_FILE`/`AF_TONY_FILE` overrides could redirect the
  watcher/adapter anywhere — currently unset (defaults verified correct).

## 4. Recommended separation changes (risk-ranked, NOT executed)

1. **P1 — Fix or quarantine `build_editor_tmp.bat`** (wrong target path; blocks
   false builds). Minimal fix: point at authoritative `.uproject` or delete with
   owner approval + backup.
2. **P2 — Audit remaining `Scripts\` for `Real Game\`-rooted paths** before any
   agent runs them (same false-target class of bug).
3. **P3 — Canonicalize staging locations** (one documented dir + timestamped
   subdirs; `Desktop\AfterfallStaging` referenced by task brief does not exist).
4. **P4 — Archive or document the stale twin** (`Real Game\Afterfall.uproject`;
   consider renaming module or adding a `STALE — do not build` marker file).
5. **P5 (defer) — Template copies + 852 MB `AfterFall\`** — leave alone; disk
   cleanup only with explicit owner approval.

## 5. Safe migration plan (proposed, NOT executed)

1. Verified backup of affected files (same `AfterfallSafetyBackups\` pattern,
   SHA-256 verified) before any path edit.
2. One-file-at-a-time script path corrections; `git diff` review per file.
3. Re-run `build_editor_tmp.bat` equivalent against authoritative project to
   prove the fixed path; keep `build_exit.txt` evidence.
4. Rollback: restore file(s) from timestamped backup; re-verify hash; confirm
   no newer user edits overwritten via `git status` before restore.
5. No directory moves/merges/deletes without separate owner approval per item.

## 6. P1 REPAIR EXECUTED 2026-10-24 ~19:15 (BEAM, authorized)
- File: `Come Up Game\Scripts\build_editor_tmp.bat` — one-line path fix
  (`Real Game\RealGame.uproject` -> `RealGame.uproject`). Diff: exactly 1 line;
  LF endings preserved; no other change.
- Backup: `Desktop\AfterfallSafetyBackups\2026-10-24_19-15-04\Scripts\build_editor_tmp.bat`,
  SHA-256 matched before edit; post-edit hash recorded in backup dir.
- Safety: no build/cook/game processes at edit time; physics diff preserved; no gameplay source touched.
- Execution test: repaired script ran -> `BUILD_EXIT:0`, `Result: Succeeded` (3.04 s, target up to date), log `test_build_editor_tmp2.log` in the same backup dir.
- Sibling audit (read-only): `aftestrun.bat`, `live1_*.bat`, `build_menu_lab.bat`, `build.sh`, `open_demo.bat`, `launch_headless_gametest.cmd` all resolve to the authoritative project — CORRECT, unchanged. `launch_2client_server.sh` default map `TestNeighborhood` is a missing-asset gap (arg-overridable), not a wrong path.
- Muse notified via existing pipeline: status event queued in `Tony\watcher_events.jsonl` (flows to UI on next launcher run; existing 5-key schema, no process spawned).
