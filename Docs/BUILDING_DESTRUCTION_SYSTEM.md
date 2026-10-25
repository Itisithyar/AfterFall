# AFTERFALL — Building destruction system (implemented)

## One system, no duplicates

Breakable geometry is `AStructuralActor` + `UStructuralDestructionComponent`
(`Source/Afterfall/Structural/`). There is **no second destruction framework**;
the playground only spawns parts through the production path
(`ServerInitialize` + `RefreshDestructionVisual`).

Data model (`Structural/StructuralTypes.h`):

* `EStructuralPart` — Foundation, Floor, Wall, Door, Window, Roof, Barricade, Lock
* `FStructuralSpec` — part, 50 cm snap grid, material cost, `MaxDurability`,
  `bRequiresSupport`
* per-actor `DetachedCells` + `Durability`, both replicated

Entry points (server-authoritative):

* `ServerApplyImpact(FDestructionEvent, DamagePerCell, EventId)` — validates
  direction, damage bounds, event origin within 500 cm of the actor, converts
  the event into actor-local space, propagates, commits
* `ServerApplyDestruction(cells, dmg, eventId)` — direct cell commit
* `ServerApplyDamage(amount)` — durability only
* `ApplyDestructionCells` — single commit point: event-id replay guard, cell
  dedupe (`MergeDetached`), durability debit per **new** cell, visual rebuild,
  then `CheckMaterialFailure()`

## Measured behaviour (headless, `-afMechSelfTest`)

```
Destruction commit +1 cells (1 total), durability 90   <- Wall hit
Destruction commit +1 cells (1 total), durability 50   <- Window hit
MECHTEST PASS: melee impact detaches building cells
MECHTEST PASS: window/glass part breaks from an impact
```

and separately `-afPipelineSelfTest`: **17/17 PASS**, including
`detached cell no longer blocks`, `intact cell still blocks`,
`delta matches authoritative state (cells=3/3)` and reload round-trip
(destroyed cells restored, stable identity kept, no duplicate structures).

## Collision and mobility

* Destroyed cells stop blocking (proved by a real line trace in the pipeline
  test); intact cells still block. That is how a breach becomes passable.
* Structural parts are the only runtime-mutated geometry. Ground/road/props are
  spawned with `SetMobility(Movable)` **only while `SetStaticMesh` is called**,
  then returned to `Static`. That removes the `SetStaticMesh ... Mobility is
  Static` warning at its source (0 occurrences of "mobility" in the run log);
  nothing is suppressed.
* Props are currently visual only (no mass/physics). Debris physics beyond the
  structural cell visuals is **not** implemented.

## Replication

Parts replicate: `Spec`, `Durability`, `DetachedCells` (with `OnRep_DetachedCells`
rebuild), ownership and stable ids. Clients rebuild from `DetachedCells`; they
never compute cells themselves. Verified only in-process (listen server);
a two-process client check was not run this pass.

## Not implemented / not claimed

* No physically accurate collapse: `CheckMaterialFailure` is a durability-gated
  propagation, not rigid-body simulation.
* No debris pooling budget beyond the structural actor's own cell budget.
* No glass fragment meshes or particle effects (fallback = removed cells).
* Doors do not swing; "destroyed door no longer blocks" holds because removed
  cells no longer collide.
