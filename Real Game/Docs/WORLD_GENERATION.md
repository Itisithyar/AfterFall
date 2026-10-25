# Procedural World Generation

Owner: Integration (World). Implemented on top of the existing voxel and
destruction systems — no second terrain, physics or destruction implementation.

| Piece | File | Reuses |
|---|---|---|
| Placement plan | `Source/Afterfall/World/WorldGen.cpp` (`PlanSites`) | `FRandomStream`, meters→cm convention |
| Terrain chunks | `AVoxelChunkActor` spawned by the generator | `FVoxelChunkData`, `VoxelMesher`, replication |
| Building shells | `AStructuralActor` floors / walls / windows / roofs | `ServerInitialize`, `ServerApplyImpact`, `UDestructionPropagationService` |
| Registry | `Docs/build_monitor/mechanics.registry.json` (`world-generation`) | `Scripts/mechanic_registry.py` |

## Domain vs. geometry (scope honesty)

`FAfterfallWorldConfig` separates the two scales on purpose:

* **Placement domain** — `RadiusM = 1000` (1 km radius, ≈3.14 km²). The site
  plan covers the whole disc; every site is a registry entry with a stable id
  (`StableIdFor(Seed, Index)`).
* **Streamed geometry** — `StreamRadiusM = 220`. Only sites inside it get real
  shells (`MaxGeometryBuildings = 8`), and only `MaxTerrainChunks = 16` chunk
  actors exist at once. Chunk size is `VoxelSizeM * ChunkCells` = 3.06 m × 32 =
  **97.92 m**.

Nothing pretends the whole 1 km is spawned. The limits are config fields, so the
world can be scaled by raising them and re-measuring, not by rewriting.

## Placement

1. Jittered grid of `BuildingMinSpacingM = 45` m over a square that
   circumscribes the disc.
2. Exact circular test `dx*dx + dz*dz <= R*R` rejects everything outside.
3. Jitter is ±0.15 × spacing, so the closest possible pair is 0.7 × spacing.
4. Candidate list is sorted by distance from `OriginM` and cut to
   `BuildingTarget = 160` — the cap keeps the *nearest* sites, which are the
   ones that stream in first, never an arbitrary edge slice.
5. `Archetype` 0/1/2 selects a 12 m / 18 m / 24 m footprint.

Generation is deterministic from `Seed`: same seed → identical ids and positions
(asserted), different seed → different layout (asserted).

## Terrain and buildings

* Terrain **streams around every player**, not a fixed ring. Every
  `StreamIntervalS = 0.5` s the server collects one focus per connected player
  (`Config.OriginM` while nobody is logged in), builds the union of chunk
  coordinates whose centre is inside `StreamRadiusM` + half a chunk, ranks each
  chunk by its distance to the **nearest** focus, keeps the nearest
  `MaxTerrainChunks`, unloads the rest and spawns what is missing. A listen or
  dedicated server therefore streams for all players: nobody's ring can starve
  another player's. Each chunk's `BeginPlay` builds its deterministic voxel
  content and mesh.
* Unload order matters: chunks leave first, so the live actor count never
  exceeds the cap mid-update.
* **Damage survives streaming.** A chunk with `ServerSeq > 0` is snapshotted on
  unload (`FVoxelChunkData` + sequence, 32 KB) into a bounded cache of 64
  entries. When it streams back in, `AVoxelChunkActor::ServerAdoptSnapshot`
  restores that data, rebuilds mesh + collision immediately and pushes the
  state to clients — walking away and back does not heal broken terrain.
  Pristine chunks are never cached; they regenerate identically.
* **Damage survives a restart.** The cache is flushed to
  `Saved/WorldGen/Chunks_<seed>.afw` (binary: magic `AFGW`, version, seed, count,
  then per chunk X/Y/Seq + the 32 KB voxel array) once per streaming update that
  unloaded an edited chunk, and again on subsystem shutdown. `GenerateWorld`
  loads the file before the first chunk spawns, so a server restart shows the
  same holes. Seeds are separate files, so worlds never mix. The file is capped
  by the 64-entry cache (≈2 MB); edits evicted from the cache are lost.
* Buildings are shells of `AStructuralActor` parts on a 3 m span grid
  (`CellExtentCm = 100`): foundation and roof tiles per bay, perimeter walls,
  glass on the outer bays, one open bay as the doorway. Because every piece is
  a structural part, the **existing** `ServerApplyImpact` → cell detach path
  applies unchanged.
* Melee reach is 120 cm, which is why cells are 100 cm — at 150 cm, melee could
  not reach a part's cells (found earlier in the mechanics playground).
* Everything the generator spawns carries the `AFWorldGen` tag, and
  `ClearGenerated()` destroys it. Regeneration replaces, it never stacks.

## Runtime hook

`UAfterfallWorldGenSubsystem::OnWorldBeginPlay` generates on the server when
`bGenerateOnStart` is true (default), so no authored `.umap` is needed for the
world to exist. Clients never generate (`IsNetMode(NM_Client)` returns early);
they receive chunk and structural actors through normal replication.

Console: `afWorldGen` regenerates on demand. Subsystem exists for Game and PIE
worlds only, so automation tests in the editor world are untouched.

## Verification (headless, this build)

```
WORLDGEN SUMMARY pass=23 fail=0 sites=160 radius=1000m parts=680 chunks=16
                 plan=0.2ms gen=596.7ms memUsed=1787MB peak=1787MB WORLDGEN PASS
```

Assertions: config validates · same seed reproduces · different seed differs ·
all sites inside the radius · spacing respected · world generates on server ·
chunk count within cap · building shells exist in the stream radius · no
duplicate chunks · voxel edit commits · chunks stream in at a new focus · cap
obeyed · every streamed chunk lies around the new focus · out-of-radius chunk
unloaded · stream-out observed · chunk reloads on return · **committed voxel
edits survive the stream-out** · a generated wall breaks through
`ServerApplyImpact`.

Streaming evidence from that run (focus jumped 660 m):

```
stream: focus=X=0     chunks=16 (in=16 out=0  restored=0 cached=0)
stream: focus=X=66000 chunks=16 (in=48 out=16 restored=0 cached=1)
stream: focus=X=0     chunks=16 (in=64 out=32 restored=1 cached=0)
```

Map-start generation (no test flag) measured ~350 ms for 16 chunks + 8 shells.

Command:

```bash
UnrealEditor.exe RealGame.uproject /Engine/Maps/Entry -server -unattended \
  -nullrhi -afWorldGenSelfTest -log
```

Related suites in the same build: `MECHTEST SUMMARY pass=14 fail=0` (now
including real grapple attach/release) and `Afterfall.*` automation 46/46 with
`TEST COMPLETE. EXIT CODE: 0`.

## Known limits

1. **Cost is linear in actors.** 8 shells = 680 structural actors ≈ 0.35 s and
   ≈0.3 GB at map start. Raising `MaxGeometryBuildings` needs instancing or
   merging, not just a bigger number.
2. **Streaming is server-side and radius-based.** Focuses are player pawns
   (one per controller), the update runs at 2 Hz, and the edit cache holds 64
   chunks: a chunk edited, unloaded, then evicted comes back pristine even
   though its file entry was overwritten. Chunks stream as whole actors (no
   LOD), and clients receive them through normal replication.
3. **Buildings are blockout shells.** No interiors, doors are breakable parts,
   no LODs.
4. **No authored map.** The world is built at runtime; an authored
   `Lvl_Afterfall_Playground` still needs editor time.
5. **Sites outside the stream radius are data only** — deliberately, so the
   full-scale plan can be validated without spawning 1 km of actors.
