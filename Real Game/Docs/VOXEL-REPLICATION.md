# Voxel Replication — single-chunk proof (2026-10-24)

Scope: **one** 32³ chunk, server-authoritative edits, compact replication,
client-side mesh + collision rebuild, late-join sync, save-free (no persistence
in this slice). No world streaming, no region generation, no multi-chunk, no
voxel world existence claim.

## 1. Audit (what already existed)

| Thing | Path | State found |
|---|---|---|
| Voxel lattice math | `Source/Afterfall/Voxel/VoxelBasis.h` | `S=3.06`, rhombohedral basis A/B/C (60°), `WorldToVoxel`, `VoxelToWorld`, `VoxelToChunk`/`ChunkLocal` with **32** edge. No gameplay caller, no storage, no actor. |
| Voxel basis test | `Voxel/VoxelBasisTest.cpp` | `Afterfall.Voxel.Basis` (`EditorContext|EngineFilter`) |
| Structural destruction | `Structural/StructuralActor.*`, `StructuralDestructionComponent` | Separate 3×3×3 per-actor cell lattice (`DetachedCells`, replicated). Preserved untouched. |
| Networking conventions | `AfterfallCharacter.*`, `StructuralActor.*` | All client→server gameplay intents are `Server, Reliable, WithValidation` RPCs **on the pawn**; actors replicate small state (`DetachedCells`, `Durability`, `StableGuid`). |
| Module deps | `Afterfall.Build.cs` | `Core, CoreUObject, Engine, InputCore, EnhancedInput, NetCore, PhysicsCore` — no mesh-generation module. |

Conclusion: the lattice existed, the voxel **world** did not. The new code sits
on top of `VoxelBasis` and does not touch the structural lattice.

## 2. Authoritative data model

`Source/Afterfall/Voxel/VoxelChunkTypes.h|.cpp`

* `FVoxelChunkData` — `TArray<uint8> Voxels`, 32 768 entries, index
  `X + 32*(Y + 32*Z)`, values `0` = empty, `1..4` = stone/wood/metal/scrap.
  Deterministic init: bottom 8 layers solid stone (8192 voxels). No randomness.
* `FVoxelEdit` — `{ Coord, State, Seq }`. `Seq` is assigned **only** by the
  server.
* `AfterfallVoxel::ApplyEdit(Data, Edit, InOutLastSeq)` — applies only strictly
  newer edits; duplicates/stale are no-ops; malformed coords advance the
  sequence without writing. This is the whole duplicate/replay policy, and it is
  unit-tested without a world.
* `AfterfallVoxel::IsGap(Seq, LastSeq)` — detects a hole so the client can ask
  for a resync.
* `FVoxelSnapshot` — `{ FVoxelChunkData, int32 Seq }`, the full-state payload.

## 3. Replication mechanism

`Source/Afterfall/Voxel/VoxelChunkActor.h|.cpp`

* `AVoxelChunkActor` — `bReplicates = true`, `bAlwaysRelevant = true`, root =
  `UProceduralMeshComponent`.
* **Incremental**: `ServerApplyVoxelEdit` → append to `PendingEdits`; one
  `NetMulticast, Reliable` `MulticastApplyEdits(TArray<FVoxelEdit>)` per frame
  (N edits in a frame cost one RPC). Client applies through `ApplyEdit` (dedupe
  by Seq) and rebuilds once per frame via a dirty flag.
* **Initial/late join/resync**: `MulticastResetChunk(FVoxelSnapshot)` with the
  full array. A new client connection (`NetDriver::ClientConnections.Num()`
  change, detected in `Tick` on the authority) triggers a **bounded burst of 3**
  snapshots 2.5 s apart — a burst, not a stream, because PostLogin happens
  before the client holds the actor channel.
* **Property replication**: only `ServerSeq` (a single int32) is a replicated
  property. The 32 KB voxel array is never a property delta, and mesh buffers
  are never replicated — clients rebuild from voxel data.
* **Client→server**: `AAfterfallCharacter::ServerRequestVoxelEdit(Chunk, Coord,
  State)` (the project's established pawn-relay pattern) and
  `ServerRequestVoxelResync(Chunk)`. Both re-validate on the server; the client
  never supplies `Seq`, damage, cells or mesh data.

### Why multicast, not `Client` RPCs (measured, not assumed)

First implementation used `UFUNCTION(Client, Reliable)`. In a real two-process
run the chunk actor replicated and `ServerSeq` arrived, but the client received
**0 of 8** snapshots. `Engine/Private/NetDriver.cpp`
(`UNetDriver::ProcessRemoteFunction`) drops `Client` RPCs on actors with no
owning connection:

> `UNetDriver::ProcessRemoteFunction: No owning connection for actor %ls. Function %ls will not be processed.`

The chunk actor is world-owned, so `NetMulticast` is the correct spec. Both
bodies early-out on `HasAuthority()` because a multicast also executes locally
on the server.

## 4. ProceduralMeshComponent

* Engine plugin `ProceduralMeshComponent` is present and `EnabledByDefault: true`
  (no `.uproject` edit needed).
* `Afterfall.Build.cs` adds `"ProceduralMeshComponent"` with a comment; nothing
  else in the module uses it.
* `VoxelChunkMesher.cpp` — for each solid voxel, the 6 faces are the
  parallelograms spanned by the two *other* lattice vectors (rhombohedral cells,
  **not** cubes). A face is emitted only when the neighbour along that lattice
  direction is empty; chunk borders emit (single chunk has no neighbours).
  Winding is fixed against the outward normal; UVs and a per-material vertex
  colour are written per quad.
* One mesh section, `bUseAsyncCooking = false` and `bUseComplexAsSimpleCollision
  = true`, so collision is valid the same frame the mesh is rebuilt. Rebuild is
  dirty-flagged and executed once per frame in `Tick` (game thread).

## 5. Tests run and exact results

### Automated (headless, no world) — `Voxel/VoxelChunkTest.cpp`

`UnrealEditor-Cmd … -ExecCmds="Automation RunTests Afterfall; Quit"`:

| Test | Covers | Result |
|---|---|---|
| `Afterfall.Voxel.ChunkInit` | deterministic init (8192 solid), coord validation (neg/overflow rejected), material validation, write/read-back | PASS |
| `Afterfall.Voxel.MeshFaces` | 2-voxel pair emits 10 quads (no internal face), slab emits 3072 quads / 6144 tris, byte-identical rebuild, unit normals, top/bottom orientation, 0 degenerate tris, empty chunk = no geometry, carving one voxel adds exactly 6 faces | PASS |
| `Afterfall.Voxel.EditOrder` | apply / duplicate-reject / stale-reject / no-op-seq-advance / out-of-range-advances-seq / gap detection | PASS |
| `Afterfall.Voxel.Basis` (pre-existing) | lattice math unchanged | PASS |

Suite total: **35/35 Success, 0 Fail**, `**** TEST COMPLETE. EXIT CODE: 0 ****`
(`Saved/Logs/VoxelAuto.log`, run 20:27).

### Room baseline (unchanged code path)

`afRoomSelfTest`: **pass=9 fail=0 ROOMTEST PASS**, exit 0
(`Saved/RoomTest/menulab_room.out`, 20:27:23).

### Real two-process network run — `Scripts/run_voxel_net.bat`

Dedicated server (`-server … -afVoxelServer`) + a **separate real client**
(`UnrealEditor.exe 127.0.0.1 -game -nullrhi … -afVoxelClientVerify`), i.e. actual
sockets + Unreal replication, not one-process simulation.

Server: chunk spawned, initialised (8192 solid), 3 authoritative edits applied
**before any client connected** → `VOXELSERVER READY seq=3 solid=8191 quads=3084`.

Client (joined late):

```
VOXELCLIENT snapshot applied seq=3 solid=8191
VOXELCLIENT PASS late join snapshot (seq=3 solid=8191 (expect 8191))
VOXELCLIENT PASS removed voxel empty (state=0)
VOXELCLIENT PASS placed voxel wood (state=2)
VOXELCLIENT PASS client mesh rebuilt (quads=3084 builds=3)
VOXELCLIENT PASS client collision blocks (hit VoxelChunkActor_0 at Z=20.0)
VOXELCLIENT PASS invalid edit sent (coord=(99,0,0) expected rejected)
VOXELCLIENT PASS invalid edit rejected (client seq=3 (expect 3, unchanged))
VOXELCLIENT edits=2 applied=2 solid=8190 seq=5
VOXELCLIENT PASS client edit round trip (seq=5 state=3 solid=8190)
VOXELCLIENT PASS authority seq advanced (seq=5)
VOXELCLIENT SUMMARY pass=9 fail=0 VOXELCLIENT PASS
```

Server side of the same run:

```
VOXELCHUNK snapshot sent seq=3 solid=8191
VOXELCHUNK edit rejected coord=(99,0,0)          <- invalid client payload ignored
VOXELCHUNK edit seq=4 coord=(20,20,20) state=0   <- client-requested removal
VOXELCHUNK edit seq=5 coord=(5,5,5)   state=3    <- client-requested placement
```

Coverage mapping to the acceptance list: initial/late-join sync ✔, server
removal converges ✔, server placement converges ✔, multiple edits in one chunk
(seq 1–3 then 4–5) ✔, client-after-edits join ✔, mesh + collision after
replication ✔, unauthorized/invalid request rejected without disconnect ✔.
Save/load: **N/A** — this slice has no voxel persistence (nothing is claimed).

## 6. Files

Created:
- `Source/Afterfall/Voxel/VoxelChunkTypes.h|.cpp`
- `Source/Afterfall/Voxel/VoxelChunkMesher.cpp`
- `Source/Afterfall/Voxel/VoxelChunkActor.h|.cpp`
- `Source/Afterfall/Voxel/VoxelChunkTest.cpp`
- `Source/Afterfall/Voxel/VoxelNetSubsystem.h|.cpp` (`-afVoxelServer`,
  `-afVoxelClientVerify`, console `afVoxelEdit`, `afVoxelResync`, `afVoxelDump`)
- `Scripts/run_voxel_net.bat`, `Scripts/run_voxel_auto.bat`, `Docs/VOXEL-REPLICATION.md`

Modified:
- `Source/Afterfall/Afterfall.Build.cs` (ProceduralMeshComponent dep)
- `Source/Afterfall/AfterfallCharacter.h|.cpp` (pawn-relayed voxel RPCs)
- `Source/Afterfall/Subsystem/AfterfallSubsystemRegistry.cpp`
  (see below — pre-existing crash found by the suite)
- `RealGame.uproject` (explicit `ProceduralMeshComponent` entry; engine default
  already enabled it)

Not modified: `VoxelBasis.h`, structural destruction, Chaos usage, configs,
`.uproject`.

### Pre-existing bug this task had to fix

`Afterfall.Subsystem.Registry` failed with an engine ensure
"Array has changed during ranged-for iteration" — `ShutdownAll()` iterated the
live array while a subsystem's `Shutdown()` could unregister. Fixed by iterating
a snapshot copy (same semantics). This is why the suite is green at 35/35.

## 6b. Coexistence with the pre-existing voxel files (not mine, left intact)

`Voxel/VoxelChunk.h`, `Voxel/VoxelSubsystem.h|.cpp` were already present when this
implementation started (another worker's files). They were **not modified or
deleted**:

* `UVoxelSubsystem` — a `UWorldSubsystem` + `IAfterfallSubsystem` service that
  registers itself in `UAfterfallSubsystemRegistry` and exposes `WorldToVoxel`.
  Harmless, complementary; nothing in this slice needs it. Its `Shutdown()` calls
  `Registry->Unregister(this)`, which is what exposed the live-array iteration
  bug fixed in `ShutdownAll` (see 6).
* `FVoxelChunk` (`VoxelChunk.h`) — a second, unwired chunk struct with a raw
  `uint8 Cells[32*32*32]`, no replication and no serialization; zero references.
  It is **not** the authority in this implementation (`FVoxelChunkData` is). Its
  owner should either adopt it as the storage type behind `FVoxelChunkData` or
  delete it — two chunk models must not diverge.

## 7. Limitations / not done

* One chunk only; no coordinates for multiple chunks, no relevance/streaming.
  `VoxelToChunk` exists but nothing iterates chunks yet.
* Voxel size is the existing lattice constant `S = 3.06` (≈3 cm): a chunk spans
  ~98 cm. That is the shipped `VoxelBasis` geometry and was **not** changed.
* Full snapshot is 32 KB per client per resync; fine for one chunk and a burst
  of 3, not fine as a steady stream or for many chunks. Next step would be
  run-length/bitmask compression or per-chunk dirty-region deltas.
* No voxel persistence: `FRegionStateSave` has no chunk entry, and nothing is
  written to disk. Do not claim save/load support.
* Destructive gameplay is not wired to voxel edits yet — damage still flows
  through `StructuralActor`; connecting them is the next feature, not done here.
* Mesh rebuild re-creates the whole chunk section on any edit (no partial
  section update, no greedy meshing).
* Editor/PIE feel, and a >1-client session, were not run: only 1 server + 1
  client headless. Multi-client broadcast is the expected behavior of
  `NetMulticast` but is **unverified**.

## 8. Next concrete step

Wire structural destruction to voxel edits: when `AStructuralActor` commits
detached cells, translate the impact into voxel edits on the chunk covering that
actor (server-side), so destruction and voxel state share one authority. Then
add a second chunk + coordinate-keyed delta to prove multi-chunk replication.
