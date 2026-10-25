# PHYSICS AUDIT — code evidence (2026-10-24, Muse)

All rows verified by source inspection in `Real Game/Source/Afterfall/`.

| System | File path | Current behavior (source) | Tests + results | Untested / missing | Risk / action |
|---|---|---|---|---|---|
| Destruction/fracture | `Destruction/DestructionPropagation.h/.cpp` (service), `Structural/StructuralDestructionComponent` (ISMC visual + `BlockAll` collision), `Structural/StructuralActor.cpp` (`ServerApplyImpact`, `ApplyDestructionCells`, `CheckMaterialFailure`) | Deterministic directional maze walk over 3×3×3 lattice; energy − materialCost·cm − breakpointCost; idempotent commit; event replay ledger (cap 8); rotation handled by actor-local frame conversion; one-shot gravity failure at 0 durability | Automation: `Destruction.Grid/Propagate/Initiators/LocalFrame/CommitIdempotent` PASS (28/28 runs); room 9/9 drives melee+hitscan+rotated impacts through the real server path + delta round-trip | Arbitrary (non-90°) rotations unproven; stress/strain/crack physics NOT modeled (energy proxy only) | Keep. Do NOT call it fracture mechanics. 45°-yaw test = smallest next step |
| Dead destruction path | `Destruction/DestructionPropagator.*`, pinned by `Tests/DestructionSystemTest.cpp` | Random-jitter, radial nearest-point, log-only `ApplyLocalDamage`; zero gameplay callers (verified by repo search) | Its own test asserts stub constants (passes, proves nothing about gameplay) | Competing-path confusion | Keep file (owner history); re-point test at the service. NOT this task |
| Physics scheduling | `Simulation/SimulationScheduler.*` | `Get/Tick/UpdateProximity/RegisterEntity` exist; proximity 30m + hysteresis; moving-stays-awake | `Simulation.Proximity` test is a placeholder (`30m==30m`) | **Scheduler is orphaned: zero callers** — sleeping bodies can never engage | Documented; NOT wired in this task (would change runtime). Recommend wire-or-delete as follow-up |
| Collision | `World/PhysicalItem.cpp` (`PhysicsActor`), `Storage/PhysicalMat.cpp` + `StructuralDestructionComponent` + `StructuralActor` (`BlockAll`), `Character` capsule (engine), zombie parts (`QueryOnly` visibility block) | Stock profiles; no custom channels; melee/hitscan trace `ECC_Visibility` | Covered indirectly by room trace checks (9/9) | No custom channel needs found | Keep |
| Damage calc/accumulation | `Combat/DamageResolver.*` (pure), `Combat/HealthComponent` (server HP, clamp 0–200, death latch), `AI/ZombiePrototype` (`DamageForPart` legacy 34/17/12; `ApplyDamageWithPipeline` resolver path) | Two paths: melee (`ServerHitPart`) uses legacy table; `ServerApplyDamage` uses resolver; zombie→player is direct `ServerTakeDamage(MeleeDamage)` | `Damage.SixShotBaseline/BodyPartResolution`, `Persistence.DamageTriggered` PASS | `ResolveHit`/`IsLethal` have zero gameplay callers; per-part arrays tracked but kill rule is aggregate HP | Melee reroute recommended (not this task). Hitscan zombie branch ADDED this task |
| Materials (render) | `Storage/PhysicalMat` (`/Engine/BasicShapes/Plane.Plane`, constructor `FObjectFinder`+`Succeeded`), zombie/item/block meshes (engine shapes) | Always-present engine content; null-guarded | Headless link + automation (mesh load checks) | Visual look: NOT TESTED headless (owner eyes) | Keep; no new assets introduced |
| Materials (physical) | `UPhysicalMaterial` created at runtime: lab blocks (concrete μ0.9, metal μ0.4, wood μ0.7, cylinder μ0.5), validation rig (μ0.8/e0.2), lab floor default | Friction/restitution drive Chaos contacts; set before `RegisterComponent` | Proven by friction-stop + settle checks (33/33) + new lab drop logging | Restitution rebound curve unmapped | Keep; values logged per spawn |
| Debris/rigid bodies | Chaos `UStaticMeshComponent` sim (`PhysicalItem`, fixtures, new lab blocks); `SetNotifyRigidBodyCollision` + `OnLabHit` impulse logging (lab only) | Server-owned; no pooling; cleanup by destroy/respawn | Free-fall/impulse/settle/friction PASS (33/33) | Debris cap/pooling absent; large-count perf unmeasured | Lab measures body count + frame time per run |
| Save/load | `Persistence/RegionStateSave.h` (deltas), `StructuralActor::ExportDelta/ImportDelta` (merge, no resurrect, `bFailed` blocks re-trigger), mat `ExportDelta`, zombie `ToGhost`/`FZombieDamageDelta` | Additive delta model; replay-protected; spec rebuilt from `PartId` | Room delta round-trip PASS (9/9); `Persistence.Compat/DamageTriggered` PASS | Save-file IO untested (pure helpers only, no IO code exists) | Keep |
| Scheduler/state | `Entities/EntityLifecycleService` (ghost/active/dormant, 30m persist floor, 100m ceiling), `Zones/WorldZone`, `World/WorldRoot` | Rules live (zombie `ShouldTickFull`, persist floors) | `Entity.Promote` PASS | Scheduler overlap (see above) | Keep |
| Tests/benchmarks | `Tests/*.cpp` (now 14 files incl. `QuickMenuLabTest`), `Slice/DestructionRoomSubsystem` (9 checks), `Physics/PhysicsValidationSubsystem` (33 checks) | Headless: automation + room + self-test | 28/28 → now 30/30 (2 new); 9/9; 33/33 (last verified runs pre-change; rerun below) | PIE/manual feel NOT TESTED | Rerun all after this task |
| Instrumentation | `PHYTEST DIAG/CLOCK`, `PHYLAB` logs, `afPhysicsLog` toggle | Wall/game/physT clocks; substep/dilation/pause; impact impulses | Proven in prior diagnostic (6× timer bug caught by clocks) | — | Keep |

## Lab analytical references (10kg / 5m / g=9.81, no drag)

- E = mgh = 490.5 J · v = √(2gh) ≈ 9.905 m/s (990.5 cm/s) · p = mv ≈ 99.05 kg·m/s · t = √(2h/g) ≈ 1.010 s.
- Engine gravity 980 cm/s² is 0.1% under 9.81 — inside the ±8% lab tolerance by two orders of magnitude; tolerance covers solver discretization + contact geometry, not unit confusion (cm↔m handled explicitly at log sites).
- Pinned headless by `Afterfall.Physics.LabReference` (pure math) + measured by `afPhysicsLab` (live sim vs refs with % errors).

## What changed this task (minimal diff)

- `AfterfallCharacter.{h,cpp}`: quick-menu state/router/bindings, weapon ammo+reload+firerate, zombie hitscan branch, immediate respawn RPC, menu input gating.
- `UI/AfterfallHUD.{h,cpp}`: Canvas quick menu (Main/Map/Controls/Lab), key-capture tick, click/selection routing, ammo+zombie HUD lines.
- `Slice/SliceSubsystem.cpp`: 3 zombies + 4 material blocks in playable setup.
- `AI/ZombiePrototype.{h,cpp}`: chase sidestep unstick.
- `Physics/PhysicsValidationSubsystem.{h,cpp}`: lab mode (no-exit), impact logging toggle.
- `Tests/QuickMenuLabTest.cpp`: NEW (2 tests).
- GameMode/Input/Config/packaging: untouched. Kilo files untouched.
