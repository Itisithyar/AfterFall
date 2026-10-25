# AFTERFALL — Mechanics & Destruction Test Report (2026-10-24, Muse)

Authoritative tree: `C:\Users\mckee\Desktop\Come Up Game\Real Game`
(`RealGame.uproject`, module `Afterfall`, UE 5.8, Win64).
Status inventory: `AFTERFALL_FULL_MECHANICS_STATUS.md`. Verification baseline:
`BEAM_FREE_INTEGRATION_REVIEW.md` (editor build exit 0 incl. grapple; packaged
cook exit 25 — no executable; grapple source-level only at that time).

Editor was LIVE during this work (Unreal/Zen running). Per instructions: no
editor process was touched, no cook/packaging job was started, no `.umap` was
created or modified, no `Config/` was changed.

## 1. Files changed (new, untracked — nothing else touched)

- `Real Game/Source/Afterfall/Slice/MechPlaygroundSubsystem.h` (new)
- `Real Game/Source/Afterfall/Slice/MechPlaygroundSubsystem.cpp` (new)
- `Real Game/Source/Afterfall/Movement/AfterfallGrappleComponent.h` (new —
  port of root-tree `Source/Afterfall/Movement/AfterfallGrappleComponent.h`,
  + server-authority gates + replicated State/Anchor/RopeLength)
- `Real Game/Source/Afterfall/Movement/AfterfallGrappleComponent.cpp` (new —
  same port; constraint/steering math identical to root original)
- `Real Game/Source/Afterfall/AfterfallCharacter.h` (grapple member, RPCs,
  `IA_Grapple`, `OnGrapplePressed` — additive only)
- `Real Game/Source/Afterfall/AfterfallCharacter.cpp` (subobject creation,
  code-built `Slice_Grapple` action, T + Gamepad_DPad_Right bindings,
  `ServerRequestGrapple`/`ServerReleaseGrapple` — additive only)

Untouched by design: `AfterfallGameMode.*` (Kilo-owned kilo-box task),
`Config/*`, all `.umap`/assets, `Slice/SliceSubsystem.*`,
`Slice/DestructionRoomSubsystem.*`, physics/mat/storage/tests.

## 2. Test map path — runtime fixture, NO map replacement

There is no new `.umap` and the main map is byte-untouched. `TestNeighborhood`
is still absent (commented refs in `Real Game/Config/DefaultEngine.ini`
lines 25–28 kept as-is; live maps remain `Lvl_FirstPerson`). The dedicated
mechanics area is a **runtime-spawned playground** built by
`UMechPlaygroundSubsystem::EnsurePlayground()` on the currently loaded map:

- Anchor: the map's `APlayerStart` + `FVector(1500, 0, 100)`; fallback
  `(1500, 0, 300)` when headless with no start.
- Contents (all tagged `MechPlay`): 2×2 foundation floor, destroyable
  building (foundation + 2 walls + roof), 1 `AResourceNode`, 2
  `APhysicalItem` (2 kg scrap + 12 kg stone), 1 `AZombiePrototype`.
- Reset: `ClearPlayground()` destroys every `MechPlay`-tagged actor
  (returns destroy count); the self-test asserts count→0 then rebuilds and
  re-asserts counts. No level reload, no asset change.

## 3. Destruction — actual script and invocation (no replacement system)

There is no standalone "destruction script" asset. The destruction path is
C++ production code, reused verbatim — nothing invented:

1. Spawn: `SpawnActor<AStructuralActor>` + `Destruction->CellExtentCm = 100`
   + `FStructuralSpec{Part, UConstructionValidator::GetMaxDurability(Part),
   RequiresSupport(Part)}` + `ServerInitialize(Spec, 0)` +
   `RefreshDestructionVisual()` — identical to placement
   (`AfterfallCharacter.cpp:1155`) and the slice seed
   (`SliceSubsystem.cpp:251-274`). Local helper inside
   `MechPlaygroundSubsystem.cpp` because `SpawnStructuralPart` is private
   on the slice subsystem (no locked header was modified).
2. Impact: `AStructuralActor::ServerApplyImpact(Event, DamagePerCell,
   EventId)` (`Structural/StructuralActor.cpp:172`), fed by
   `UDestructionPropagationService::{AttackEnergy,AttackDamagePerCell,
   MaterialForPart}` — the same call the melee RPC makes
   (`AfterfallCharacter.cpp:1041`).
3. Fixtures around it (`Slice/DestructionRoomSubsystem`, `afRoomSelfTest`)
   are test harnesses, not the destruction system itself.

Self-test impact check runs one wall impact with `Initiator=Attack`,
`Direction=(0,0,-1)` and asserts `>0` newly detached cells.

## 4. Grapple integration (through the CMC owner, momentum preserved)

Correction to the status doc §2 (now stale): grapple is no longer
"root-tree only". The Real Game port reads/writes **only** the owning
`UCharacterMovementComponent`'s `Velocity` (plus one swept correction move),
exactly like the root original — no competing movement controller exists
(grep for `SetMovementMode|Fly|GhostMovement|CustomMovement` in the port:
zero hits). Deltas vs root: server-authority refusal in
`TryGrapple`/`Release`/`Tick`, replicated State/Anchor/RopeLength.

- Input: `IA_Grapple` (`Slice_Grapple`), `T` + `Gamepad_DPad_Right`
  (`AfterfallCharacter.cpp:166,187-188,255`); `OnGrapplePressed` toggles.
- Authority: `ServerRequestGrapple` refuses dead/null owners; component
  refuses non-authority callers. Release sets `Idle` with **no velocity
  write** — momentum preserved by construction; self-test asserts
  `GetVelocity().Equals(VelBefore)`.
- Movement stays owned by `UAfterfallMovementComponent` (sprint state
  machine, 89.4 cm/s heavy-carry cap); grapple never sets movement mode.

## 5. Mechanics connected (dependency order, all reuse, no new systems)

1. Input — code-built `SliceIMC`: Move WASD/arrows+LeftX/Y, Look mouse+RightX/Y,
   Jump Space/Bottom, Sprint Shift, Interact E, Attack LMB, Crouch C,
   Throw Q, Drop G, Mat B, Fire F, Reload R, Pause Esc, Build V,
   Grapple T (+gamepad map). Verified by grep this session.
2. Carry/throw — `CarryComponent::ServerPickup/Drop/Throw` +
   `APhysicalItem::ComputeReleaseVelocity` (v_player + dir·speed), dupe/spam
   guards; playground spawns light (2 kg) + heavy (12 kg) items.
3. Physics — stock `PhysicsActor`/`BlockAll` collision; no channels added.
4. Damage/destruction — `ServerAttack`/`ServerFireHitscan` → `ServerApplyImpact`
   (`AfterfallCharacter.cpp:490,1041`); self-test drives the same call.
5. Health/death/respawn — `HealthComponent` latch → `HandleDeath` →
   `DoRespawn`; `ServerRequestRespawn` converges (menu + auto timer).
6. Inventory/loot — `ResourceNode::ServerHarvest` →
   `Inventory::ServerAddItem` (`ResourceNode.cpp:123`); `LootService` +
   6 `DA_Location_*.json` untouched.
7. HUD — `AAfterfallHUD` swapped on pawn possession incl. death overlay +
   Respawn button (`SliceSubsystem.cpp:228,241`).

## 6. Kilo-box — authorization checked, NOT implemented (correct owner: Kilo)

`Real Game/Docs/TASKS.json`: `kilo-box-001`, owner Kilo, state `authorized`,
files limited to `AfterfallGameMode.h/.cpp`, map asset must stay
byte-identical. `Real Game/Docs/PHYSICS-BOX-TASK.md` is the delegation order
(runtime `APhysicalItem` spawn on `Lvl_FirstPerson` server `BeginPlay` only).
Verified this session: `AfterfallGameMode.{h,cpp}` contain **zero**
`PhysicalItem/kilo/Box` references — no diff has landed, no boundary was
crossed. Implementation stays with Kilo; the playground's own items are
separate test actors, not the kilo box.

## 7. Controls (playground / verification)

- Move WASD · Look mouse · Jump Space · Sprint Shift · Interact E (pickup/harvest)
- Attack LMB (melee → destruction) · Fire F (hitscan → destruction) · Reload R
- Throw Q · Drop G · Mat B · Build V · Pause Esc
- Grapple T or gamepad DPad-Right (attach when idle, release when attached)
- `afMechSelfTest` (console) or `-afMechSelfTest` (headless server flag):
  builds playground, asserts counts, impacts a wall, attaches/30-ticks/
  releases grapple, clears, rebuilds; prints `MECHTEST SUMMARY pass=N
  fail=M MECHTEST PASS|FAIL` and exits 0/1 only on the flag path. Shipping
  builds exclude the dev-only paths; the command is inert there.

## 8. Reset procedure

Automatic (self-test tail): `ClearPlayground()` → assert zero `MechPlay`
actors → `EnsurePlayground()` → assert counts restored. Manual in-editor:
run `afMechSelfTest`, or (without the command) delete actors tagged
`MechPlay` in the outliner and re-run the command.

## 9. Test evidence (non-disruptive only — editor was live)

- `git status`: six new source files only; zero `.umap`/asset/config
  modifications (the `?? Content/...` entries are pre-existing untracked
  template copies, unmodified).
- Grep audits (exit 0): grapple RPC chain (`ServerRequestGrapple` 970–995,
  `IA_Grapple` T + DPad_Right, `Grapple->TryGrapple()` server body);
  destruction chain (`ServerInitialize` + `RefreshDestructionVisual` +
  `ServerApplyImpact` in playground; melee/hitscan call sites 490/1041);
  health/respawn (`HandleDeath`, `DoRespawn`, `ServerRequestRespawn`);
  carry (`ServerThrowHeld/DropHeld/Pickup`, `ComputeReleaseVelocity`);
  loot (`ServerAddItem` in `ResourceNode.cpp:123`); HUD swap on possession.
- Compile-safety review: private-access violations fixed before writing
  (local structural-spawn helper; direct `Grapple->TryGrapple()/Release()`
  on the server instead of private RPCs; `Combat/HealthComponent.h` +
  `HAL/PlatformMisc.h` includes added).
- NOT run (would disrupt the live session): editor build, Automation
  (`Afterfall.*`), `afRoomSelfTest`, packaging, any PIE/in-game pass.

## 10. Status classification (honest)

- IMPLEMENTED (code complete, grep-audited): playground fixture, grapple
  port + wiring, destroyable building, mechanics connections, self-test.
- BUILD-VERIFIED: nothing new this session (BEAM's earlier editor build
  exit 0 predates the playground files; rebuild is still required).
- IN-GAME-VERIFIED: nothing this session (no PIE while the editor is held
  by the user).

## 11. Remaining blockers / gaps (recorded, not fabricated)

1. Build + automation re-run with the new files (owner session or idle
   window): `Scripts/run_realgame_tests.bat` (build → Automation →
   `afRoomSelfTest` → `afPipelineSelfTest`) + new `afMechSelfTest` headless
   run; confirm prior 37/37 + 9/9 + 17/17 still green.
2. In-game verification (manual, ~10 min, steps in §12).
3. `kilo-box-001` implementation — owner Kilo per TASKS.json.
4. `TestNeighborhood` map creation — owner Art per FIRST-PLAYABLE-BACKLOG.
5. Animation: template anims quarantined, no working character anims.
   Audio: zero sound assets. Checkpoints/game-over/win: none exist.
   Disk save/load round-trip through real slots: unproven (structs only).
6. Zen :8558 contention (PID 4) still blocks agent-driver end-to-end runs;
   unrelated to gameplay code.

## 12. Exact manual steps (run when the editor is free)

1. Build editor target; open `Real Game/RealGame.uproject`; PIE on
   `Lvl_FirstPerson`.
2. Console: `afMechSelfTest` → expect `MECHTEST SUMMARY … MECHTEST PASS`.
3. Walk to the `MechPlay` compound (near PlayerStart +1500 X). E: harvest
   node (+Wood) and pick up scrap; Q/G: throw/drop; E on stone: heavy cap.
4. LMB a building wall (cells detach, debris collides); F hitscan a second
   wall. Die (console damage or zombie) → auto-respawn; B mat open/close;
   Esc menu → Respawn button.
5. T: grapple to a wall (rope line if `bDebugDraw`), swing with WASD, T:
   release (momentum carries); Esc resumes.
6. Headless (no editor): `UnrealEditor.exe RealGame.uproject
   /Engine/Maps/Entry -server -unattended -nopause -nosplash -nosound
   -ExecCmds="afMechSelfTest" -log` → `MECHTEST PASS`, exit 0.

---

# PHASE 5 — Compile readiness + runtime entry-point audit (2026-10-24, Muse)

Live-session rules honored: no editor/Zen process touched, no build/cook/
package started. No Kilo-owned file altered (`AfterfallGameMode.h/.cpp`
re-verified: zero `PhysicalItem`/`Kilo` references). No source fix was
required or applied this phase — HANDOFF.txt boundary (`no source changes
until runtime verification resolved or owner approves`) left intact.
Owning records read: `Real Game/Docs/TASKS.json`, `Real Game/Docs/HANDOFF.txt`,
`Docs/HANDOFF.txt`, Tony `README.md`, `BEAM_FREE_INTEGRATION_REVIEW.md`, and
this report §§1–12.

## P5-1. Files inspected (full read, not grep-only)

- `Real Game/Source/Afterfall/Slice/MechPlaygroundSubsystem.h` (44 lines)
- `Real Game/Source/Afterfall/Slice/MechPlaygroundSubsystem.cpp` (275 lines)
- `Real Game/Source/Afterfall/Movement/AfterfallGrappleComponent.h` (66 lines)
- `Real Game/Source/Afterfall/Movement/AfterfallGrappleComponent.cpp` (109 lines)
- `AfterfallCharacter.h` grapple block (lines 19/72/247–248/271–278) +
  `AfterfallCharacter.cpp` ctor/input/RPC bodies (lines 45/58/166/187–188/
  255/956–995) + ctor `bReplicates`/subobjects (lines 48–75)
- `Real Game/Source/Afterfall/Afterfall.Build.cs` (module deps)

## P5-2. Fixes made: NONE (no demonstrated compile blocker found)

## P5-3. Exact runtime entry point and invocation

`UMechPlaygroundSubsystem : public UWorldSubsystem` auto-registers in every
game world — no map change, no GameMode change, no editor interaction needed
to install it. Developer invocations (live-session safe, server-side only):

- In-editor/PIE console: `afMechSelfTest` (registered via
  `FAutoConsoleCommandWithWorld` + `FConsoleCommandWithWorldDelegate`, same
  pattern as `afSliceSelfTest`/`afRoomSelfTest`/`afPipelineSelfTest`).
- Headless server flag: `-afMechSelfTest` (parsed in `Initialize`, 2 s
  deferred `RunAutoTest` → `RunMechSelfTest(true)` →
  `FPlatformMisc::RequestExitWithStatus` 0/1).
- Single-player PIE qualifies: NetMode is Standalone (not `NM_Client`), so
  the `IsNetMode(NM_Client)` guard passes and the fixture runs.

## P5-4. Compile-readiness findings (likely blockers vs confirmed errors)

CONFIRMED ERRORS: none found by inspection. Specifically cleared:

- UHT: `.generated.h` is the last include in both new headers; `UCLASS`/
  `USTRUCT`/`UENUM`/`UFUNCTION`/`UPROPERTY` specifiers well-formed;
  `GetLifetimeReplicatedProps` signature matches (`const override`);
  private `UFUNCTION() RunAutoTest` satisfies the timer-delegate requirement;
  non-`UPROPERTY` `TArray<TWeakObjectPtr<…>> Building` needs no reflection;
  `static FName PlaygroundTag()` is a plain static (no UHT involvement).
- Includes: every symbol used in `MechPlaygroundSubsystem.cpp` resolves to
  an included header — spawn/initialize (`StructuralActor`,
  `StructuralTypes`, `ConstructionValidator`), impact service
  (`DestructionPropagation`: `AttackEnergy`/`AttackDamagePerCell`/
  `MaterialForPart` all exist, lines 105–113), actors (`PhysicalItem`,
  `ResourceNode`, `ZombiePrototype` class names verified), pawn/component
  access (`AfterfallCharacter`, `AfterfallGrappleComponent`,
  `HealthComponent::bIsDead`), logging (`Afterfall.h` → `LogAfterfall`),
  world/iteration (`Engine/World.h`, `EngineUtils.h`, `PlayerStart`,
  `GameplayStatics`), console+exit (`HAL/IConsoleManager.h`,
  `HAL/PlatformMisc.h`).
- RPCs: `Server/ Reliable/ WithValidation` declarations each have matching
  `_Validate` + `_Implementation` (character.cpp 970/975, 984/989);
  `ServerRequestGrapple_Validate` null-safe (`HealthIsDead()` uses `&&`).
- Module deps (`Afterfall.Build.cs`): no new module needed — GameplayStatics,
  `DOREPLIFETIME` (`Net/UnrealNetwork.h`), `SCENE_QUERY_STAT`, draw debug
  all live in listed modules (`Engine`, `NetCore`, `PhysicsCore` present).
- Collisions: single definitions of both new classes in `Real Game/Source`
  (grep); root-tree twins are a separate project build, no ODR risk.
- Character ctor: `bReplicates = true` (line 51) + default-subobject
  `Grapple` (line 58, needs no attachment — `UActorComponent`); no
  character-level `DOREPLIFETIME` needed (component self-replicates via
  `SetIsReplicatedByDefault(true)`).
- Balance: braces/parens/brackets balanced in all six files (python audit).

LIKELY (not confirmed — requires UBT/UHT to rule out): nothing outstanding.
Weakest points, all judged low-risk: `LEVELTICK_All` via `Engine/World.h`
transitive include; `FConsoleCommandWithWorldDelegate::CreateStatic(&fn)`
with a file-local `void(UWorld*)` (same shape as the lambda pattern used by
four sibling subsystems); `RequestExitWithStatus(false, 1)` int→uint8
conversion (pre-existing pattern in room/pipeline fixtures).

## P5-5. Grapple call-path findings (source trace)

`T`/`Gamepad_DPad_Right` → `IA_Grapple` → `OnGrapplePressed` (toggle) →
`ServerRequestGrapple`/`ServerReleaseGrapple` (dead/null refused at validate
+ implementation) → `Grapple->TryGrapple()/Release()` → owner-CMC
`Velocity` projection + swept correction (`ApplyRopeConstraint`), bounded
swing steering clamped to 1500 cm/s (`ApplySteering`), tick no-op unless
`Attached` and server. Release writes no velocity (momentum preserved —
self-test asserts `GetVelocity().Equals(VelBefore)`). Anchor edge cases:
whiff and sub-`MinRopeLengthCm` anchors reset to `Idle` (no stuck
`Attempting`); destroyed-anchor leaves a stale air point (rope purely
visual/physical constraint, no crash path — noted limitation, no fix applied).
Death while attached is not explicitly released (`HandleDeath` untouched) —
recorded as a gameplay-logic gap for the owner, not a compile issue.
Replication: `State`/`Anchor`/`RopeLength` replicated, server-simulated,
clients read-only; `IsAttached()` client-safe.

## P5-6. Destruction call-path findings (source trace)

Fixture helper mirrors the two production call sites exactly: `SpawnActor` +
`Destruction->CellExtentCm = 100` + `FStructuralSpec{Part,
UConstructionValidator::GetMaxDurability/RequiresSupport}` +
`ServerInitialize(Spec, 0)` + `RefreshDestructionVisual()` (cf. placement
`AfterfallCharacter.cpp:1155`, slice seed `SliceSubsystem.cpp:251–274`);
impact via `ServerApplyImpact(swing, AttackDamagePerCell(), NewGuid)` on a
`Wall` part (cf. melee `AfterfallCharacter.cpp:1041`). Debris/collision
behavior is owned by `StructuralDestructionComponent` (ISMC rebuild +
`BlockAll`) — unchanged, no new collision path. Reset = `Destroy()` all
`MechPlay`-tagged actors (replicas clear via replication) + rebuild;
`Building` weak refs reset so no dangling pointers.

## P5-7. Checks actually run (all non-disruptive, exit 0)

- Full read of all six files (this phase) + prior grep audits.
- `class AFTERFALL_API UAfterfallGrappleComponent/UMechPlaygroundSubsystem`:
exactly one definition each in `Real Game/Source`.
- Kilo files: `grep -c PhysicalItem|Kilo` → 0 / 0 (GameMode untouched).
- `.generated.h` last-include order confirmed in both new headers.
- Brace/paren/bracket balance: BALANCED on all six files (python).
- RPC `_Validate`/`_Implementation` pairs present (4/4).
- NOT run (live session): UBT/UHT compile, Automation, `afRoomSelfTest`,
  `afPipelineSelfTest`, `afMechSelfTest`, cook/package, PIE.

## P5-8. What still requires an Unreal build and in-game test

Unchanged from §§10–12: editor build → Automation 37/37 + room 9/9 +
pipeline 17/17 + new `afMechSelfTest` headless PASS → manual PIE pass.
No compilation or working gameplay is claimed.

## P5-9. First safe PIE/gameplay verification checklist (editor available)

1. [ ] Idle-window editor build of `RealGame.uproject` (Win64 Development)
   → exit 0 with the six files (first real compile evidence).
2. [ ] `Automation RunTests Afterfall` → all green (no regressions).
3. [ ] Headless `-server -ExecCmds="afMechSelfTest"` → `MECHTEST PASS`, exit 0.
4. [ ] Headless `afRoomSelfTest` 9/9 + `afPipelineSelfTest` 17/17 (baselines).
5. [ ] PIE `Lvl_FirstPerson` → console `afMechSelfTest` → `MECHTEST PASS`.
6. [ ] On foot at the `MechPlay` compound: E harvest/pickup, Q/G throw/drop,
   heavy stone caps speed; LMB/F detach wall cells (debris collides).
7. [ ] T grapple → swing → T release (momentum carries); die → auto-respawn
   clean (note rope state if attached at death — known gap P5-5).
8. [ ] Esc menu → Respawn button; B mat open/close; HUD ammo/zombie lines live.
