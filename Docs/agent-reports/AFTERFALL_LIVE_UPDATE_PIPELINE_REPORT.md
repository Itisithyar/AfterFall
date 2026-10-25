# Afterfall Live Update Pipeline Report

## Implemented
- Watcher with health file.
- Adapter with health file.
- React component shows watcher health + events.

## Status: PARTIALLY VERIFIED (UI integration done, no E2E run).

Live game preserved.

---

## PIPELINE COMPLETION (2026-10-24, Muse) — real end-to-end delivery achieved

### Files changed
- `Tony/afterfall_watcher.py` — ROOT retargeted to `Real Game/Source/Afterfall`
  (was watching the stale root tree); startup prime (no whole-tree flood);
  fixed quiet-timer re-arm bug (pending stamp refreshed every poll, so a
  stable change NEVER emitted — the reason no real event ever delivered);
  `AF_WATCH_ROOT` override; heartbeat path made absolute.
- `Tony/tony_event_adapter.py` — offset+inode consumption (no full re-read);
  restart dedupe via TONY-tail preload; torn-line holdback (fixed
  double-read offset bug found by test); malformed counting/skip;
  rotation recovery; TONY cap 1000 -> keep 500 (atomic replace); `AF_WATCH_FILE`/
  `AF_TONY_FILE` overrides; output now `OpenJarvis/frontend/public/tony_events.jsonl`
  (vite serves `public/` at `/`, no custom `publicDir`; `*.jsonl` gitignored).
- `Tony/run_afterfall_events.py` (new) — explicit foreground launcher:
  single-instance lockfile guard, watcher child + in-process adapter loop,
  Ctrl+C terminates child and removes lock. No autostart, no background.
- `OpenJarvis/frontend/src/components/AfterfallEvents.tsx` (untracked, preserved)
  — `fetch(..., { cache: 'no-store' })`, render list capped at last 200 rows.
- `App.tsx` — untouched this session (prior 2-line import+render stands).
- Game source — untouched in final state (probe comment added then fully
  removed; EOF byte-identical to authored state, `grep probe` = 0).

### Actual producer input/output contract
- Watcher stdout (JSONL, `id`=sha12(path+detect-time), `status: detected`)
  appended to `Tony/watcher_events.jsonl` by the launcher.
- Adapter consumes new bytes only -> appends unseen ids to
  `frontend/public/tony_events.jsonl` with `status: OWNERSHIP_UNKNOWN`,
  one valid JSON object per line, stable ids preserved.
- Component polls `/tony_events.jsonl` every 2s, dedupes by `e.id`.

### Launch / shutdown
- Start (foreground): `cd C:\Users\mckee\Desktop\Tony && python run_afterfall_events.py`
- Stop: Ctrl+C (child terminated, lock removed, verified zero python left).
- Refuses duplicates while a live lockfile exists. Logs: `pipeline_run*.log`.

### Tests executed (results)
- 8/8 synthetic transport tests PASS (temp files, clearly labeled): convert,
  idle-noop, restart-dedupe, malformed-skip, missing-source-quiet,
  torn-line, rotation-recovery, trim-cap. Testing caught + fixed 2 real bugs
  (torn-line double-read, quiet-timer re-arm).
- Static tsc check of component + App.tsx: clean (stub-harness, prior session).
- REAL run: 3 genuine change events (`MechPlaygroundSubsystem.cpp` probe
  lifecycle) flowed watcher -> adapter -> served JSONL with stable unique ids
  (`3e11614e85d6`, `9be98ae05be1`, `aae02d456fc5`); 15 s idle -> zero growth
  (no duplicate rows); shutdown -> no python processes, lock removed.
- NOT executed (no node_modules; would need `npm install` + new server
  process): vite dev serving the file over HTTP, and pixel-level UI render.
  Serving path verified statically (default `public/` -> `/`); render path
  verified by code inspection (dedupe set + row map).

### Remaining blockers
1. `npm install` + `vite dev` (or `vite build`) then open the UI to SEE the
   3 real rows — needs a desktop session with node_modules; explicitly not
   done here to avoid heavy installs and new long-lived processes.
2. `status: OWNERSHIP_UNKNOWN` overwrite is the inherited contract — owner
   to confirm whether the UI should keep raw watcher status instead.

---

## BEAM — LIVE UI VERIFICATION (2026-10-24, Muse, user-authorized install)

With explicit user authorization: `npm install` (576 packages, exit 0) then
real `tsc -b` (exit 0 — full project typecheck, supersedes stub harness).
Vite dev started on :5197 (NOT :5173, which the user's own server holds).

- HTTP: `GET /tony_events.jsonl` -> 200, 699 bytes, all 3 genuine events
  (`3e11614e85d6`, `9be98ae05be1`, `aae02d456fc5`), stable across polls.
- DOM (headless Edge, 2 loads ~15 s apart): 3 event rows each
  (`...MechPlaygroundSubsystem.cpp modified OWNERSHIP_UNKNOWN` x3) —
  fetched, rendered, deduplicated, idle-stable. Ids are React keys only;
  visible row text is timestamp/path/type/status.
- Screenshot `Tony/logs/af-ui-20261024.png`: app loads clean; a first-run
  analytics opt-in modal overlays the view (fresh profile, no localStorage)
  with events rendered beneath. DOM dumps are the row evidence.
- Console/network: no JS errors, no failed resources in Edge logs
  (only a benign task-manager fallback notice).
- Shutdown: vite :5197 process terminated, port free; user's :5173 server
  (PID 11400) untouched; no python producers remain.
- Verification level achieved: HTTP serving + DOM render + idle/dedupe +
  screenshot, all against the 3 genuine events. No synthetics used.

### TREE RELOCATION (~18:39 2026-10-24, outside this pipeline)

`Come Up Game/Real Game/Source/` was removed; all session files verified
intact (content + timestamps + EOF) under the surviving tree
`Come Up Game/Source/Afterfall/` (grapple wiring x7/x20 hits, playground
markers x7, game file probe-free). `RealGame.uproject` lives at
`Come Up Game/RealGame.uproject` (plus `Real Game/Afterfall.uproject`).
Watcher `DEFAULT_ROOT` updated to the surviving path (121 files visible);
`AF_WATCH_ROOT` override unchanged. Prior report paths under
`Real Game/Source/` now resolve to `Source/`. No pipeline code changed
beyond the ROOT default; no rerun needed for the delivered evidence.