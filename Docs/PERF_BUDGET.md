# AFTERFALL — 165 FPS Performance Budget (Stage A baseline, UNMEASURED)

> All numbers are **provisional budgets, not results**. No claim of 165 FPS compliance until captured on reference hardware.

## Reference (to lock before acceptance)
- HW: TBD by user (propose: RTX 4070 / Ryzen 7 7800X3D / 32GB / NVMe, Win11).
- Resolution/preset: 1080p competitive (no Lumen GI?/medium shadows, TSR balanced) vs 1440p quality — record both.
- Scenarios (§8 of brief): empty field, dense forest, village+interiors, sprint chunk crossing, fortified base, 24-zombie combat+gunfire, 2-player same cell, unload/reload, restart restore.

## Frame budget: 6.06ms @165Hz (critical path, CPU/GPU overlap — not split-half)
- Game thread gameplay (AI reduced to budgets, no per-frame full sim): ≤2.2ms
- Render thread (draw calls, shadow, base pass): ≤2.4ms
- GPU (shading, overdraw, post): ≤5.2ms overlapped; critical path ≤6.06ms
- Hitch rule: >12ms frame = hitch; 1%/0.1% lows reported, not average only.
- Server tick (30Hz, 33.3ms): gameplay ≤12ms, replication ≤8ms, persistence async off-tick (save <200ms/region background).

## Existing systems (do not rewrite)
- World Partition 200m cells + HLOD + ISM foliage; `AStructuralActor` damage-scalar (no actor swap); ghost/active/dormant lifecycle; sealed loot rolls + delta saves; relevancy per chunk. Stage A changes nothing here.

## Stage A instrumentation (this commit)
- `Scripts/perf_baseline.sh` — captures `stat unit/fps/game/gpu/net` + Insights trace + scenario checklist into `Saved/Perf/<date>/`.
- `Scripts/validate_perf_budget.py` — checks budget math (frame sum, server tick sum, overlap note) without engine.
- Next stages B-F per brief only after baseline captured.
