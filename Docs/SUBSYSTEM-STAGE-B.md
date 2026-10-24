# Stage B — subsystem registry + one real service integration

Status: implemented, built, and verified on 2026-10-24 (Win64 Editor target).
No commit, no push, no new persistence format, no physics replacement.

## 1. Registry contract (`Subsystem/AfterfallSubsystemRegistry.h|.cpp`)

The registry is a **non-owning index** of world-scoped providers. It never
dereferences a provider after `Unregister`, and it never keeps a provider alive.

| API | Semantics |
|---|---|
| `bool Register(S)` | `false` for null, for an already-registered provider (no duplicates), and once the registry is shutting down. |
| `bool Unregister(S)` | `false` for null / not registered. Allowed during shutdown, because providers unregister themselves from their own `Shutdown`. |
| `Find(Name)` | `nullptr` for null/unknown names **and while shutting down**, so callers cannot start work against a dying set. Case-insensitive. |
| `Resolve(Name, OutError)` | Typed resolution: returns a provider only if it is `Ready`, otherwise a reason (`Unavailable` / `NotReady` / `Failed` / `ShuttingDown`). |
| `ShutdownAll()` | Snapshot-iterates, skips already-`Stopped` providers, returns how many were shut down, then clears. **Idempotent** (second call returns 0). Sets `IsShuttingDown()`. |
| `Num()`, `IsShuttingDown()` | Introspection for tests/diagnostics. |

Lifecycle is one shared legality table (`AfterfallSubsystem::IsValidTransition`):

```
Uninitialized -> Initializing -> Ready -> Stopping -> Stopped
        \\             \\          \\         \\
         +------------- Failed (terminal) ---+
```
* same-state is a legal no-op (repeated shutdown/init),
* terminal states (`Stopped`, `Failed`) can never be revived,
* skipping stages is illegal (`Ready -> Stopped` is rejected),
* only `Ready` is serviceable (`AfterfallSubsystem::IsServable`).

## 2. The real integration (`Subsystem/SubsystemService.h|.cpp`)

Typed request/response, one provider, one consumer, no `void*` at the call site:

```cpp
struct FVoxelCellRequest  { FVector WorldPoint; FVector Origin; };
struct FVoxelCellResponse { AfterfallService::EError Error; FIntVector Cell; bool IsSuccess(); };
class IVoxelCellService   { virtual FVoxelCellResponse QueryCell(const FVoxelCellRequest&) const = 0; };
FVoxelCellResponse QueryVoxelCell(const UAfterfallSubsystemRegistry*, const FVoxelCellRequest&,
                                  const TCHAR* ProviderName = TEXT("Voxel"));
```

* **Provider** — `UVoxelSubsystem` (`Voxel/VoxelSubsystem.h|.cpp`) implements
  `IAfterfallSubsystem` + `IVoxelCellService`, registers itself on `Initialize`,
  unregisters on `Shutdown`, and applies every transition through the shared
  legality table. `GetService(EAfterfallServiceId::VoxelCell)` is the single
  RTTI-free downcast point.
* **Consumer** — `UAfterfallSliceSubsystem::DoSliceSetup` resolves its world
  anchor to a lattice cell and logs it; a refusal is logged with the error text
  and setup continues (the slice does not hard-depend on the voxel service).
* **Errors are values**, never silent nulls: `Unavailable` (no provider),
  `NotReady` (initializing/stopping), `Failed`, `ShuttingDown`, `InvalidRequest`
  (non-finite payload, rejected before any provider is called).
* **Lifetime** — the registry holds raw pointers only while the provider is
  alive; the provider removes itself before it dies; the service returns a value
  type, never a pointer into a subsystem, so nothing can be used after shutdown.
* **Thread affinity** — all of this is world-scoped UObject state and is used
  from the game thread only; no locks, no queues, no async.

The earlier untyped `ISubsystemService`/`FSubsystemRequest` envelope is kept in
the same header for compatibility but has no callers; new work uses the typed
path.

## 3. Tests

`Tests/SubsystemRegistryTest.cpp` (`Afterfall.Subsystem.Registry`) — rewritten to
exercise behavior instead of a single happy path: register/lookup (incl.
case-insensitivity), duplicates, null inputs, unregister-twice, all legal and
illegal transitions, `IsServable`, every `Resolve` failure reason, shutdown-all
counting, a provider that unregisters itself during iteration, repeated
shutdown, and "no new work once shutting down". It uses a **local registry
instance** so it cannot stop the live world's providers.

`Tests/VoxelCellServiceTest.cpp` (`Afterfall.Voxel.CellService`) — null registry,
missing provider, success (cell cross-checked against `FVoxelBasis`),
`Initializing`/`Stopping`/`Failed` providers, non-finite payload (rejected and
*not served*), provider that does not advertise the service, registry after
`ShutdownAll`, and a live check against the shipped `UVoxelSubsystem` when the
world has it registered.

## 4. Verification (actual output)

| Check | Command | Result |
|---|---|---|
| Build | `Build.bat AfterfallEditor Win64 Development RealGame.uproject` | **Result: Succeeded**, exit 0 (`Saved/build-stageb2.log`, 5 actions incl. UHT + link) |
| Automation | `Automation RunTests Afterfall; Quit` | **36/36 `Result={Success}`, 0 fail**, `**** TEST COMPLETE. EXIT CODE: 0 ****` (`Saved/Logs/VoxelAuto.log`, 20:39) |
| New tests | — | `Afterfall.Subsystem.Registry` **PASS**, `Afterfall.Voxel.CellService` **PASS** |
| Room baseline | `afRoomSelfTest` | **ROOMTEST SUMMARY pass=9 fail=0 ROOMTEST PASS**, exit 0 (`Saved/RoomTest/menulab_room.out`, 20:43) |
| Live consumer | room-test server log | `SubsystemRegistry: registered Voxel (1 total)` then `Slice anchor voxel cell (-25,2,83)` — the real provider answered through the typed path in a running world |

Baseline note: the checkpoint said 28/28 automation; the tree actually contained
35 passing tests before this increment (30 gameplay + 4 voxel + registry), so the
comparable baseline is 35 → **36** after adding `Voxel.CellService`. The room
suite is unchanged at 9/9.

`afPhysicsTest` (owner-run, editor) remains **pending and not run here**.

## 5. Outstanding

* **Event routing: not implemented, and not needed for this integration.** The
  chosen path is synchronous request/response; there is no notification need yet.
  When one appears, the minimal shape is a typed delegate list owned by the
  *provider*, cleared in `Shutdown()`, dispatched only while `Ready` — the same
  lifecycle gate as `QueryCell`.
* Only one service id (`VoxelCell`) exists. Adding a second means a new enum id
  plus one downcast site in its own service header.
* `UVoxelSubsystem` is a lattice-math provider; it is **not** a voxel world (see
  `Docs/VOXEL-REPLICATION.md` for the separate single-chunk proof).

## 6. Next single implementation step

Add the second typed service id (`AfterfallServiceId::StructuralQuery`) answered
by the existing structural destruction service, consumed by the slice, so the
registry is proven with more than one provider — then add the provider-owned
typed event list only if a consumer actually needs notifications.
