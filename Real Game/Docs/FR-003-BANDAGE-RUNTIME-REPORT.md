# FR-003 Bandage Self-Treatment — Verification Report (2026-10-25)

Status: **BUILD PASS / PACKAGE PASS / AUTOMATION PASS (3/3) / RUNTIME BLOCKED / AUTHORITY AUDITED**

Task: `flint-bandage-001` (FR-003).
Verifier: Buffy (independent build/package/runtime/ownership audit).

> The implementation changed while this audit ran: the parallel
> `BleedingComponent` ledger was removed and `UInjuryStateComponent` +
> `UMedicalCapabilityLib` became the single canonical wound system. All results
> below refer to the post-consolidation tree.

## 1. Build and package (post-consolidation)

| Step | Command | Result |
|---|---|---|
| Editor build | `Build.bat AfterfallEditor Win64 Development -Project=RealGame.uproject` | **exit 0**, `Result: Succeeded` |
| Package | `RunUAT.bat BuildCookRun ... -cook -build -stage -pak -archive` | **BUILD SUCCESSFUL**, AutomationTool exit 0, 1m33s |
| Archive | `C:\Users\mckee\Desktop\AfterfallStaging_2026-10-24_injury_pkg` | staged exe present |

Logs: `_build_editor_injury.log`, `_buildcookrun_injury.log`.

## 2. Automation tests

`UnrealEditor-Cmd ... -ExecCmds="Automation RunTests Afterfall.Medical; Quit"`:

| Test | Result |
|---|---|
| `Afterfall.Medical.Capabilities` | **Success** |
| `Afterfall.Medical.MathGuards` | **Success** |
| `Afterfall.Medical.Transitions` | **Success** |

Exit code 0. Log: `_medical_tests2.log`. These are pure-logic tests (transition
table, capability matching, bleed math, authority guards); they do **not**
exercise the zombie→wound→bandage loop.

## 3. Runtime verification — BLOCKED

Four launch modes were attempted against packaged builds:

1. windowed `-log`
2. `-nullrhi` via `Start-Process`
3. `-nullrhi` via visible `cmd start`
4. stdout redirected to a file

Every run stalls ~2 s after slice setup. The log freezes immediately after the
three AI lines (`Zombie ... pursues AfterfallCharacter ... (19m)`), the file
size stops growing (866,910 bytes frozen for 60+ s) and the process keeps
spinning (~4.6 CPU-seconds per 3 s wall time). No `Zombie ... hits`, `Wound`,
`Damage`, `died`, `Self-treat`, or `Treated wound` line is ever emitted.

The same stall reproduces on the **pre-medical** package
(`AfterfallStaging_2026-10-24_gamemode_pkg`), so it is **not caused by FR-003**.
It matches the pre-existing "headless run hung" note in `Docs/HANDOFF.txt`.

None of the ten acceptance steps could be executed. **Runtime is not verified.**

## 4. Reachability gap

`AF_MED_Bandage` still has **no acquisition path** in the slice:

- the only runtime inventory grant is `ResourceNode` harvesting;
- loot tables (`Content/Data/Loot/*.json`) are data only — no runtime container
  spawn;
- the crafting service knows a bandage recipe (`res_med_bandage`, 2 cloth), but
  no verified pickup/craft delivery adds `AF_MED_Bandage` to the player;
- `ServerSelfTreatBandage` requires `AF_MED_Bandage` specifically.

So even with the startup stall fixed, step 4 ("acquire a bandage") cannot be
completed until loot/crafting delivery is wired.

## 5. Current implementation and authority audit

Canonical pipeline (post-consolidation):

- `AAfterfallCharacter` owns `UInjuryStateComponent` (`Injury`).
- Zombie attack: `HealthComponent::ServerTakeDamage` + `Injury->ServerApplyWound(Torso, severity)`.
- Hold E + LMB: `ServerSelfTreatBandage` → `ServerBeginTreatment(first bleeding part, else first wounded part, AF_MED_Bandage)`.
- Release E: `ServerInterruptSelfTreatment` → `ServerInterruptTreatment` (hold-to-treat; interrupt never consumes).
- Treatment completion: `ServerAdvance` consumes one item via `Inventory->ServerTryConsumeItem`, then applies capability effects atomically.

| Concern | Evidence | Verdict |
|---|---|---|
| Wound state server-owned | `ServerApplyWound`, `ServerBeginTreatment`, `ServerAdvance` return unless owner `HasAuthority()` | server-only |
| Health drain server-owned | `ServerTakeDamage` guards authority/death; HP replicated, never client-written | server-only |
| Treatment validation | server re-checks session idle, part wounded, capability match, death, item held | server-validated |
| Stale-wound targeting | RPC takes no part/index; server chooses part from its own records | not client-targetable |
| Duplicate sessions | `TreatmentState == Treating` rejects a second begin; interrupt is a no-op unless Treating | protected |
| Item consumption | exactly once, at completion, server-atomic; abort/interrupt consumes nothing | correct |
| Death handling | `ServerAdvance` interrupts when dead; bleed drain stops when dead | correct |
| Replication for HUD | `PartInjuries`, `TreatmentState`, `TreatingPartIndex`, `TreatmentItemId`, `TreatmentProgress`, `PendingHPRestore` replicated | present |
| White-bar HUD | no HUD binding found for injury/bleeding state | open |

## 6. Ownership resolution

| Work | Owner | State |
|---|---|---|
| `InjuryStateComponent.*`, `MedicalCapability.*`, `MedicalTreatmentTest.cpp` | BEAM Free (headers) | adopted as canonical by Flint |
| `BleedingComponent.*` | Flint | **removed** 2026-10-25 to end the duplicate ledger |
| Character/zombie wiring | Flint | switched to `UInjuryStateComponent` |

`UInjuryStateComponent` is the single canonical wound model. Do not reintroduce a
second bleeding ledger — two systems would double the HP drain.

## 6a. Spec-vs-implementation mismatches found in the code

| Acceptance text | Implementation | Note |
|---|---|---|
| "treats the most severe actively-bleeding wound" | picks the **first** bleeding body part, else the first wounded part (`ServerSelfTreatBandage_Implementation`) | deterministic but not severity-ranked |
| "wound severity decreases by the configured treatment amount" | a completed bandage session **stops bleeding** and closes light wounds (`Severity < 2`); there is no partial severity decrement or treatment-amount field | bandage capability is boolean (`bStopsBleeding`), no `TreatmentAmount` |
| "Acquire an `AF_MED_Bandage`" | crafting produces `res_med_bandage`; treatment requires `AF_MED_Bandage` | the two ID namespaces are aliased in the capability table but **not** in the inventory gate — crafting a bandage would not satisfy the treatment check |
| "hold E and left-click" | starts a 3 s hold-to-treat session; releasing E interrupts | matches hold-to-treat, but the criterion does not describe the session duration |

## 7. Remaining work

1. Fix the pre-existing ~2 s startup stall so the game can be played/tested.
2. Wire bandage acquisition (loot container or craft delivery).
3. Equip/draw UX and HUD bleeding/treatment feedback.
4. Owner-run interactive verification of the ten acceptance steps.

## 8. Regressions

No runtime regression could be measured because the game stalls before combat.
Build, package and the three medical automation tests pass on the consolidated
tree, so the consolidation itself did not break compilation or the tested logic.

## 9. Startup stall — RESOLVED on current tree (2026-10-25)

The ~2 s post-setup stall described in section 6 **no longer reproduces on the
current source tree**. Evidence, gathered without touching the older packages:

| Artifact | Built | Result |
|---|---|---|
| `Desktop/AfterfallStaging_2026-10-24_injury_pkg` | 20:07 | **Reproduced**: log stops at the three `Zombie ... pursues` lines, frame 468, log size byte-identical for 120 s, child process ~1.5 cores. Reproduced twice (windowed and `-nullrhi`). |
| Tree at 20:31 (diagnostic build, `AfterfallStaging_2026-10-24_diag_pkg`) | 20:31 | **No stall**: frames keep advancing, world runs 2+ min |
| Tree at 20:39 (`AfterfallStaging_2026-10-24_clean_pkg` attempt) | — | build blocked by another agent's live editor (Live Coding lock) |

So the stall is a property of the 20:07 binary, not of the current gameplay
code. It was never caused by the medical/injury work — the pre-medical package
stalls identically — and it is not reproducible once the tree is rebuilt.

Diagnosis method (temporary, now **removed** from the tree): a low-priority
watchdog thread logged the last entered gameplay phase once per second, with
RAII phase markers in `Zombie::AITick`, `Movement::TickComponent`,
`HUD::Tick/DrawHUD`, `Injury::TickComponent`, `Voxel::Tick` and
`Grapple::TickComponent`. It established that the game thread keeps ticking in
the rebuilt binary and that no project system was spinning; the phase string is
back to "boot" at each sample because every marker is a short scope.

Runtime evidence on the rebuilt binary (`-afPipelineSelfTest`, which drives
production melee/hitscan into the structural damage path and then exits with a
status code):

```
PIPELINE PASS: registry available
PIPELINE PASS: melee -> structural cells detached (0 -> 1)
PIPELINE PASS: hitscan -> more structural cells detached (1 -> 3)
PIPELINE PASS: durability debited by impacts (60.0 / 100.0)
PIPELINE PASS: detached cell no longer blocks
PIPELINE PASS: delta matches authoritative state (cells=3/3)
PIPELINE PASS: reload restores destroyed cells (3/3)
PIPELINE SUMMARY pass=17 fail=0 PIPELINE PASS
```

### Caveat found while verifying: the zombies never close the distance
`AZombiePrototype` is an `ACharacter` that is spawned **without** an
`AIController`, so `AddMovementInput` is never consumed by
`UCharacterMovementComponent` (it requires a controller). The Wander->Chase
transition logs once and the zombie then stands still at 19-20 m, so no melee
hit, no wound and no bleed can occur in an unassisted run. The scripted
`afSliceSelfTest` teleports the zombie into melee range before waiting for the
bite, which is why the acceptance loop can still be exercised.
