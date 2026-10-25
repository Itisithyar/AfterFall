# AFTERFALL — Build monitor: mechanic / dependency registry

## What exists

| Piece | Path | Role |
|---|---|---|
| Mechanic registry | `Docs/build_monitor/mechanics.registry.json` | declarative entry per mechanic |
| Validator | `Scripts/mechanic_registry.py` | checks files, modules, asset paths; prints states |
| Existing pipeline | `Scripts/build_editor_tmp.bat` | the only build entry point (RealGame.uproject) |
| Feature ledger | `Docs/AFTERFALL_FEATURE_REGISTRY.txt` | human-readable feature status |

This **extends the existing monitor** (TASKS/FEATURE_REGISTRY + build script); no
parallel monitor was created.

## Registry entry shape

```json
{
  "id": "zombie-ragdoll",
  "name": "Zombie ragdoll (articulated death body)",
  "sources": ["Source/Afterfall/Combat/ArticulatedBodyComponent.cpp", "..."],
  "modules": ["Engine", "PhysicsCore"],
  "actorClass": "AZombiePrototype",
  "animations": [],
  "skeleton": null,
  "animBlueprint": null,
  "physicsAsset": null,
  "inputs": [],
  "tags": ["Body.Articulated", "Body.Ragdoll"],
  "ui": [],
  "status": { "compile": "PASS", "assets": "BLOCKOUT_PARTS_OK", "runtime": "PASS" },
  "evidence": "RAGDOLL ZombiePrototype: 6 bodies, 5 joints on death"
}
```

Status is deliberately **three separate axes** — `compile`, `assets`,
`runtime` — so a green compile can never imply a verified asset or runtime.

## Validator behaviour (`python Scripts/mechanic_registry.py`)

1. Loads the registry and `RealGame.uproject`.
2. Checks every declared C++ file exists on disk.
3. Checks every declared module is an engine module or enabled in the `.uproject`.
4. Resolves every `/Game/...` reference to `Content/...(.uasset|.umap)` and
   reports the ones that do not exist. `/Engine/...` is flagged as
   engine-provided and **not** claimed as validated.
5. Emits a verdict per mechanic: `READY`, `WAITING_FOR_ASSETS`, `BLOCKED`.
6. Exit code 1 only when a *source/module* dependency is missing.

Current output: `READY=7 WAITING_FOR_ASSETS=1 BLOCKED=0`; the single waiting
entry is `skeletal-animation`, which lists the skeleton/animation/physics-asset
paths that must be imported (none exist yet). `world-generation` was added for
the procedural 1 km world (see `Docs/WORLD_GENERATION.md`), with its own
evidence line from the headless `-afWorldGenSelfTest` run.

## What it does NOT do

* It does not import or download assets, and it never marks a soft reference as
  loaded. Animation assets must be imported into the project and then re-run.
* It does not start builds. Building stays with the controlled workflow
  (`Scripts/build_editor_tmp.bat` for the editor target,
  `RunUAT BuildCookRun` for packages) and one editor/build at a time.
* Runtime status is copied from the registry's `evidence` field, i.e. it must be
  backed by an actual log line. Refresh the field when a suite is rerun.
