# FallAfter destruction mathematical model (E19–E26)

Status labels: **IMPLEMENTED** (code + test/log evidence) · **PARTIAL** (code exists,
gaps named) · **DESIGNED** (spec only) · **UNVERIFIED** (claimed, not confirmed).
Units: cm for geometry, seconds, "energy" in documented gameplay proxy units
(not joules unless a file converts to SI). Owner of committed destruction:
`UDestructionPropagationService` + `AStructuralActor` + `AVoxelChunkActor`.

## 1. Two destruction layers (do not merge them)

| Layer | State | Granularity | Commit path |
|---|---|---|---|
| Structural parts | `AStructuralActor::DetachedCells` (0..26) + `Durability` | 3×3×3 breakpoint grid per actor, cell extent 100 cm | `ServerApplyImpact` → `Propagate` → `ServerApplyDestruction` (idempotent, replicated) |
| Voxel terrain | `FVoxelChunkData::Voxels` (32³ bytes) | 3.06 m rhombohedral cell | `ServerApplyVoxelEdit` / `ServerApplyFractureImpact` → `MulticastApplyEdits` |

`UDestructionPropagator` exists but is **not** the gameplay path (its own header says so):
nothing outside its test calls it, it uses random jitter and a log-only damage stub.
`Docs/Destruction/DestructionPropagator.h` ownership note is authoritative.

## 2. Structural cell grid (E19, E22)

Cells are the 27 lattice points of the part's 3×3×3 grid:

```
GridPoint(C, h, ix, iy, iz) = C + ((clamp(ix,0,2)-1)·h, (clamp(iy,0,2)-1)·h, (clamp(iz,0,2)-1)·h)
GridIndex(ix, iy, iz)       = ix + 3·iy + 9·iz          // 0..26
```

Impact evaluation, per propagation step:

```
E_{k+1} = E_k - C_material · d_k - B_initiator
C_material = MaterialResistancePerCm(MaterialId)        // energy per cm
B_initiator = InitiatorBreakpointCost(Initiator)        // energy per breakpoint
d_k         = distance from current point to chosen breakpoint (cm)
```

Values in code today (`Destruction/DestructionPropagation.cpp`):

| Material | resistance / cm |
|---|---|
| Glass | 0.15 |
| Wood | 0.40 |
| Concrete | 1.20 |
| Metal | 1.60 |
| unknown | 0.60 |

| Initiator | breakpoint cost | reach (cm) |
|---|---|---|
| Bullet | 8 | 300 |
| Attack (melee) | 18 | 120 |
| Material failure | 12 | 150 |

Walk bounds: `MaxSteps = 8`, stop when `E ≤ MinEnergy = 1`, no reachable
unvisited breakpoint, or the chosen material cost exceeds the budget. Next
breakpoint selection is **nearest-first with a positive projection along the
impact direction** — a directional maze walk, not a radial/sphere query.

Documented energy/damage proxies per initiator (`AttackEnergy()` 60,
`BulletEnergy()` 150, `FailureEnergy()` 40; damage per cell 10 / 15 / 8). These
are gameplay tunables, not measured physical energies.

The whole evaluation runs in one frame: `Center/Origin/Direction` must share a
frame; `AStructuralActor` converts world impacts to actor-local on entry so a
rotated part still maps to its visual grid.

**Status: E19 IMPLEMENTED** (cells + exact grid math, `Afterfall.Destruction.Grid`,
`Afterfall.Destruction.LocalFrame`) · **E22 IMPLEMENTED** (directional cost/budget,
`Afterfall.Destruction.Propagate`, `…Propagation.Regression`). Material table is a
documented placeholder, so E20 stays **PARTIAL** (no separate compressive/tensile
strength or fracture energy).

## 3. Durability, support and collapse (E23–E24)

`AStructuralActor::ServerApplyDamage` clamps `Durability` to
`[0, Spec.MaxDurability]` and triggers `CheckMaterialFailure()` at zero;
`ServerApplyDestruction` charges `DamagePerCell` per **new** cell.

Support is a flag, not a load model: `UConstructionValidator::RequiresSupport(Part)`
gates placement and proc-gen. There is no load redistribution solver in the
shipping structural path, so the support graph and cascading-collapse equations
are **not** implemented there:

```
Σ_{j∈N_S(i)} w_{ij}·s_j < T_i  ⇒  unsupported        // DESIGNED for structural actors
L_j^{k+1} = L_j^k + ΔL_j,  Σ ΔL_j ≈ L_transferred    // DESIGNED
```

Two experimental structures exist under `Structural/` (`BoxBuilding`,
`BoxWallRuntimeSubsystem`) with their own tests (`Afterfall.Box…`) and support-loss
collapse behaviour; they are not the world-building path and their load numbers
are not modelled.

**Status: E23–E24 PARTIAL** (support-loss collapse exists only in the experimental
box structures; no load values anywhere).

## 4. Voxel fracture (E21, E25–E26)

`AfterfallFracture::Solve` (6-neighbour BFS on lattice coords):

```
r       = hash(x,y,z,seed) & 0xFFFF / 65535                      // weakness 0..1
R_mat   = {Wood 0.3, Plaster 0.2, Stone 0.7, Concrete 0.85, Metal 0.95}
Resist  = R_mat · (1 - 0.3·r)
Cost    = Resist · R_mat · 10 · (1 - 0.3·max(0, dir·loadDir))
```

Accepted neighbours debit `Cost` from the remaining budget (`MaxDepth = 8` at the
call site); `r > 0.65` marks a weak point that branches laterally when at least
`2·Cost` energy remains. `AVoxelChunkActor::ServerApplyFractureImpact` then writes
state `0` (empty) for the start cell and every path cell through the normal edit
path, so clients get the result as ordinary sequenced edits.

This is a **directional budget walk over a deterministic weakness field**, not
`η·E_abs/E_crit` per cell (E21) and not a stress solver. Debris is not spawned by
the voxel path (removed cells simply disappear); `ABoxBlockDebris` exists only for
the experimental box structures, with a bounded velocity and a 12 s settle budget.

**Status: E21 PARTIAL** (budget/threshold model instead of normalized damage) ·
**E25 PARTIAL** (debris exists for box structures only, none from voxels) ·
**E26 PARTIAL** (secondary impacts from debris do not feed the voxel path today).

## 5. Damage persistence and streaming (new)

Voxel damage survives a stream-out and a server restart:

* in memory: `UnloadChunk` snapshots chunks with `ServerSeq > 0` into a bounded
  cache (64 × 32 KB); `SpawnChunk` restores through
  `AVoxelChunkActor::ServerAdoptSnapshot` (mesh + collision rebuilt synchronously,
  snapshot pushed to clients);
* on disk: `Saved/WorldGen/Chunks_<seed>.afw`, layout
  `int32[4] {magic 'AFGW', version 1, seed, count}` then per chunk
  `int32[3] {X, Y, Seq}` + `TArray<uint8>` voxels (length-prefixed). Written once
  per streaming update that unloaded an edited chunk, and on `Deinitialize`;
  loaded in `GenerateWorld` before the first chunk spawns.

Structural damage persists separately via `FRegionStateSave` /
`AStructuralActor::ExportDelta|ImportDelta`.

**Evidence:** `WORLDGEN SUMMARY pass=23 fail=0` — includes
`edited chunks are written to the damage file`,
`previous session's edited chunks load from disk`,
`restart restores voxel damage from disk`.

## 6. Open gaps

1. Load redistribution / buckling / cascading collapse for **structural parts**.
2. Separate material strength parameters (Sc, St, Gf) instead of one resistance number.
3. Per-cell accumulated voxel damage (`d_i`) — damage is binary here.
4. Debris and secondary impacts from voxel fracture.
5. Fracture Lab control → solver parameter mapping is unverified.
