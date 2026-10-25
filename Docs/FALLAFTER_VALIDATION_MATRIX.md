# FallAfter validation matrix

Every equation in `FALLAFTER_EQUATION_REGISTRY.md` must name a way it is actually
checked. This file is the map from equation groups to tests, the exact commands,
and the latest measured results. A passing build is **not** evidence of behaviour.

## 1. Test layers

| Layer | What it proves | Where |
|---|---|---|
| Unit / automation | Pure math and state machines, no world | `Source/Afterfall/Tests/*.cpp`, `Afterfall.*` |
| Server fixtures | Real actors, real damage/streaming paths, headless | `-afMechSelfTest`, `-afWorldGenSelfTest`, `-afRoomSelfTest` |
| Replication | Server→client convergence | `Scripts/run_ue_replication_test.bat`, `Scripts/run_ue_live_multiclient.bat` |
| Packaging | Cook + stage + launch | `Scripts/build_*.bat`, staging folders, **not re-run since the latest code** |

Commands (all headless, engine Entry map, no authored content needed):

```bash
# build
Scripts/build_editor_tmp.bat            # AfterfallEditor Win64 Development
Scripts/build_game_tmp.bat              # Afterfall Win64 Development

# unit
UnrealEditor-Cmd.exe RealGame.uproject -unattended -nullrhi \
  -ExecCmds="Automation RunTests Afterfall; Quit" -TestExit="Automation Test Queue Empty"

# fixtures
Scripts/run_world_fixtures.bat          # WORLDGEN + MECHTEST
UnrealEditor.exe RealGame.uproject /Engine/Maps/Entry -server -unattended \
  -nullrhi -afMechSelfTest -log
```

## 2. Latest measured results (this workstation, 2026-10-25)

| Check | Result | Log |
|---|---|---|
| `AfterfallEditor Win64 Development` | `Result: Succeeded`, `BUILD_EXIT:0` | build console |
| `Afterfall Win64 Development` | `Result: Succeeded` | build console |
| `WORLDGEN SUMMARY` | **pass=23 fail=0** — plan, determinism, chunk cap, stream in/out, restore, disk persistence, multi-focus, wall break | `Saved/Logs/WorldGenTest.log` |
| `MECHTEST SUMMARY` | **pass=14 fail=0** — playground, 3 buildings/680-part world, melee + glass, grapple attach/constraint/release | `Saved/Logs/MechTest.log` |
| `Afterfall.*` automation | **51 Success**, 1 Fail: `Afterfall.Interact.Radial.Sectors` (radial-menu work in flight by another agent) | `Saved/Logs/AutomationFinal.log` |
| `Afterfall.Limb.*` | 4/4 Success (was 1 fail on `Target.Validity`) | same |
| `FItemInstance::InstanceId` struct-init error | 0 occurrences (was 2 per run) | same |
| Cook / package / launch | **not verified** — last staged build predates these changes | — |

## 3. Coverage by system

| System | Equations | Verification today | Gap |
|---|---|---|---|
| Chaos rigid bodies | E1–E5 | Engine solver; `Afterfall.Physics.LabReference`, gravity/bounce checks in `PhysicsValidationSubsystem`; `Afterfall.Stage2.FixedStep/ThrowModel` | No direct impulse-vs-velocity assertion for the zombie body |
| Articulated body | E6–E8 | `Ragdoll` fixture: 6 bodies / 5 joints on death; region masses exposed (`BodyRegionPhysics`) | COM and total angular momentum are never computed or asserted |
| Grab / drag / throw | E9–E12 | MECHTEST release-preserves-momentum; `SLICETEST` ragdoll grab + release | No drag-path torque or joint-load assertion; no spring law (constraint instead) |
| Balance / guard / joint damage | E13–E16 | none | Entire group is DESIGNED — no variables in source |
| Shared impact accounting | E17–E18 | Destruction commit tests; `Afterfall.Destruction.CommitIdempotent` | No cross-subsystem energy ledger (deliberately not duplicated) |
| Voxel + structural destruction | E19–E26 | `Afterfall.Destruction.{Grid,LocalFrame,Initiators,Propagate,CommitIdempotent}`; `…Propagation.Regression`; WORLDGEN wall break; MECHTEST glass | No load redistribution, no per-cell damage accumulation, no debris from voxels |
| Wounds / bleeding / treatment | E27–E33 | `Afterfall.Medical.{Transitions,Capabilities,MathGuards}`; `MECHTEST`/`SLICETEST` bite→wound; bandage loop | No per-wound blood volume, no natural stabilization decay, no P(B) staging |
| Wound visuals / ALT inspect | E34–E35 | none for body inspection; radial `Inspect` shows target text only | ALT mode, wound decals/attachment not present |
| Rope | E36–E40 | MECHTEST: attach, length, constraint after 30 ticks, release, momentum | No tension, no particles, anchor destruction does not detach |
| Fracture Lab | E41–E42 | Solver unit tests; HUD lab page exists | Control→parameter mapping unverified; no determinism replay test |
| Multiplayer / dedupe / resync | E43–E46 | `CommitIdempotent` (GUID ledger), chunk `ServerSeq` + gap detection + resync pacing, replication scripts | No automated two-client convergence assertion for voxel `S_client → S_server` |
| Build / package | E47–E49 | Builds succeed; fixtures run from the built editor binary | Cook, stage, launch acceptance all unverified for this revision |

## 4. What must be added before a claim is safe

1. **Two-client voxel convergence test** (E44–E46): edit on server, assert client
   `LastAppliedSeq` and `CountSolid()` match after the resync window.
2. **Joint/guard slice tests** (E13–E16) only after the model exists in code.
3. **Rope tension test** (E38–E39) — or delete those equations from the design
   until a tension term exists.
4. **Fresh cook + stage + launch** (E47–E49) on the current binaries, with the
   last-known-good package preserved (`Scripts` staging convention).
5. **World streaming soak**: walk a long path with edits, assert no chunk leaks
   (actor count stays at cap) and damage persists across restarts — the fixture
   covers a single jump and restart today.

## 5. Honest summary

The systems that are genuinely verified end-to-end today are destruction
(structural + voxel), wound state and bandage treatment, grappling, ragdoll grab,
inventory/crafting math, and the procedural world with streaming persistence.
The systems that exist only as design are balance/guard, joint injury and
detachment, ALT body inspection, rope tension, load-based structural collapse,
and packaging acceptance.
