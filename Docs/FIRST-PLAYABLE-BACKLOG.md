# FIRST-PLAYABLE-BACKLOG.md — prioritized, smallest-safe-first

## P0 (blocks Play on slice)
1. Install MSVC C++ workload (user action, 20GB). Unblocks compile + all PIE on canonical project.
2. `Generate project files` + `build.sh editor` green. Owner: Tech. Accept: no errors.
3. `Content/Maps/TestNeighborhood.umap` (WP, 6 POIs, lightmass/sky). Owner: Art. Accept: opens + PIE boots.

## P1 (first game loop: move → gather → zombie → HUD → perf)
4. FP character wire-up: `BP_FirstPersonCharacter` → `AAfterfallCharacter` (or child BP), `IMC_Default` → `ServerRequestPlacement` preview path. Accept: move/look/collide + ghost preview visible.
5. 3 placeholder resource nodes (static meshes + `ServerAddItem` on interact). Reuse loot JSON weights later.
6. 1 zombie spawner (pooled mesh, ghost→active on 30m/noise, melee + die + tombstone). Reuse `EntityLifecycleService` rules.
7. Minimal HUD (HP + Wood count + interact prompt + `stat fps` readout). Blueprint-only.
8. `perf_baseline.sh` capture on interim + slice maps (avg/1%/0.1%/hitches). No 165 claim until data.

## P2 (streaming-ready, not overbuilt)
9. 200m cell grid + `PredictedCells` helper + dirty-cell save hook. Keep WP; no custom engine fork.
10. Wire `RegionStateSave` to TestNeighborhood (claims/structures/containers/nodes). Restart-restore test.
11. Re-point `DefaultEngine.ini` to TestNeighborhood (uncomment block), keep `Lvl_FirstPerson` as fallback.

## Explicitly deferred
- Full 3km world, day/night cycle, vehicles drivable, guns/ammo spawn, vendors, farming growth, code locks, NVG. Tables exist as data; do not spawn until loop + perf proven.
