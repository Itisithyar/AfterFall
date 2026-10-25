# TESTNEIGHBORHOOD-SPEC.md — build in editor (no .umap in git yet)

Target: `Content/Maps/TestNeighborhood.umap`, World Partition ON, 500×500m, origin (0,0,0).

## Layout (local coords, meters; deterministic seed 1337 for gen parity)
- Village lane: 6 house shells along X axis, spacing 40m: (-100,0),(-60,0),(-20,0),(20,0),(60,0),(100,0). Reuse `AStructuralActor` wall/door specs + template masonry mats.
- Farm: (-80,-120), seed bins ×2 + hemp nodes ×4 (see nodes below).
- Garage: (90,-110), benches (loot GAR table), 1 wreck at (110,-130).
- River: line Y=-40, width 8m, bridge at X=0. Water fill node (dirty).
- Forest edge: X<-140, 20 trees (wood nodes) + 8 rocks.
- Hill tower: (0,160) water tower landmark, visible from everywhere.

## Actors to place (exact counts for slice)
- `AResourceNode`: 6 Wood (Yield 3-5), 4 Stone (4-8), 4 Hemp (2-4). Set Kind/YieldMin/Max per class defaults.
- `AZombiePrototype`: 5 spawns (village 2, farm 1, garage 1, forest 1). StableIds assigned at spawn via `MakeStableGuid`.
- PlayerStart: (0,-20,100) facing north. No PvP for slice.
- Lighting: Day preset (sun 45°) + overcast/night variants as Level Variants. Sky + fog + 1 dir light + skylight (template defaults OK).

## WP + persistence wiring
- WP grid 200m (9 cells over 500m). HLOD per 100m. Data Layers: Abandoned (overgrowth ×2), Contaminated OFF for slice.
- After placing: run `perf_baseline.sh TestNeighborhood`, save `RegionStateSave` checkpoint, verify restart-restore (claims/structures/containers/nodes).

## Acceptance
Open map → Play → move/collide → harvest 1 node (+Wood) → zombie approaches ≤30m persists → kill 1 (dies, corpse lootable) → HUD shows HP/Wood → `stat unit` recorded. Then uncomment slice maps in `DefaultEngine.ini` (already staged).
