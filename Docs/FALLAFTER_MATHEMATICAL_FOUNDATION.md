# FallAfter Unified Causal Physics Model (FA-UCPM) — canonical specification v1 (BEAM, 2026-10-24)

Working name for the math framework; Afterfall remains the game/project name. This spec
standardizes what the codebase ALREADY implements, then lists designed-but-missing pieces.
Nothing here invents new physics laws: Chaos solves rigid-body motion; this framework owns
gameplay rules, damage, state transitions, and events. Status per item: IMPLEMENTED (file:line)
or DESIGNED (not yet code).

## 1. Variable and unit dictionary (Unreal convention: centimeters, seconds, kg)
| Symbol | Meaning | Unit | Owner |
|---|---|---|---|
| q, v, L(=Iw) | position, linear velocity, angular momentum | cm, cm/s, kg·cm²/s | Chaos solver (engine) |
| m, rho, mu, e | mass, density, friction, restitution | kg, kg/m³, -, - | `Combat/BodyRegionPhysics` profiles; `Structural/BoxBuilding` densities |
| E=½mv² | impact energy (SpeedCmS/100 converts to m/s) | J | `Structural/BoxBuilding.cpp:257`; `Combat/BodyRegionPhysics.cpp:75` |
| J=Δp | impulse / momentum transfer | N·s | Chaos contacts; `BoxWallStructure` dirs |
| D (damage), H (HP) | damage points, health pool | pts, HP | `Combat/HealthComponent`, `Combat/InjuryStateComponent` |
| BleedTick(rate,dt) | HP/s × s wounded-bleed integration | HP | `Combat/InjuryStateComponent.cpp:82` |
| GrabRangeCm/HoldDistanceCm | grab reach / hold offset | cm | `Combat/ArticulatedBodyComponent.h:46,50` |
| MaxSpeedCmS/LifeMaxSeconds | debris speed cap / lifetime budget | cm/s, s | `Structural/BoxWallStructure.h:36,37` |
| OriginM/RadiusM/etc. | world-gen config | m (×100 → cm) | `World/WorldGen.h:24-44` |

Rule: meters at the config boundary, centimeters inside. Never mix silently (cf. `/100.f`
at `BoxBuilding.cpp:257`, `CmPerM` in `WorldGen.cpp`).

## 2. Equation registry (approved; assumptions + tests)
| # | Equation | Code | Test |
|---|---|---|---|
| E1 | E = 0.5·m·(v/100)², deny if E < 0.5·BreakEnergy(material) | `BoxBuilding.cpp:257-263` | `BoxBuildingTest.cpp:98-118` (937.5 J exact) |
| E2 | Splash: neighbor += E·SplashFraction(material); detach at durability | `BoxBuilding.cpp:269-293` | same (exactly 1 removal) |
| E3 | Support loss → Detached commit; floor anchors never fall | `BoxBuilding.cpp:313-372` + FIX-A commit | `BoxBuildingTest.cpp:183-220` |
| E4 | Debris v = min(√(2E/m)·100, 350); die at 12 s or settled | `BoxWallStructure.cpp:74-94` | MECHTEST debris (log 21:27) |
| E5 | H[k+1] = max(0, H[k]−D[k]); bleed integrated per part | `HealthComponent`, `InjuryState.cpp:223` | `MedicalTreatmentTest.cpp:73-75` |
| E6 | Treatment FSM + capability match + consume-on-success → PendingHPRestore → ServerHeal | `InjuryStateComponent.cpp:126-305` | `MedicalTreatmentTest.cpp` (transitions, capabilities, guards) |
| E7 | Grab reach/hold/overstretch-release; momentum preserved at release | `ArticulatedBodyComponent.{h,cpp}` | MECHTEST grapple 21:27 (attach/rope/release/momentum) |
| E8 | WorldGen: dx²+dz²≤R², seeded jittered grid, spacing ≥ minimum, stable GUID(seed,index) | `WorldGen.{h,cpp}`, `WorldGen.cpp:31-130` | WorldGenTest 21:28 (determinism, radius, spacing) |
| E9 (DESIGNED) | Unified damage D = η·E·Km·Kg·Kl with calibrated η (pts/J); impulse-based branch where apt | — | — |
| E10 (DESIGNED) | Joint Rj = max(||F||/Fmax, ||τ||/τmax); dDj/dt accumulation; fail at Dcrit | — | — |
| E11 (DESIGNED) | Guard G∈[0,1] update; COM-support balance (advisory, not sole fall criterion) | — | — |

## 3. State-transition registry (all IMPLEMENTED unless noted)
- Block: Intact→Damaged→Detached (+Destroyed reserved) — `BoxBuilding.h` EBoxBlockState.
- Treatment: Idle→Treating→Complete/Interrupted; re-treat allowed; dupe rejected —
  `InjuryStateComponent`, tested transitions.
- Wound: wounded/bleeding/pain/stabilized flags per part; bandage stops bleed, kit +HP,
  pills −pain, antiseptic/antibiotic stabilize — `MedicalCapability` + `InjuryState`.
- Connection: intact/injured/unstable/detached — DESIGNED (ArticulatedBody tracks ragdoll/
  grabbed only; joint-damage states not yet modeled).
- Authority: server commits; GUID dedupe (`RecentEventIds`, cap 8); clients mirror ledger +
  visual-only debris echo — `BoxWallStructure.cpp`.

## 4. Dependency map (producer → consumers)
Chaos contacts → `OnBlockHit`/`ServerApplyBoxImpact` → `EvaluateImpact` → ledger commit →
`CommitResult` (hide comps, spawn debris, multicast echo) → replication (`DetachedBlockIds`,
`Blocks`) → clients. Parallel chain: damage → `ServerTakeDamage`/bleed → treatment →
`ServerHeal`; never written directly to HP. WorldGen → voxel chunks + structural shells →
existing destruction path (no second framework).

## 5. Implementation map (module → contract)
FA Core: `CoreMinimal` + `F BoxBuildingProfile`/`FBoxImpactInput/Result` + `FBodyPart` enums.
FA Dynamics: Chaos + `ArticulatedBodyComponent` + `BodyRegionPhysics` masses.
FA Damage: `HealthComponent` + `InjuryStateComponent` + `MedicalCapabilityLib`.
FA Destruction: `BoxBuildingLib` + `ABoxWallStructure` + `DestructionPropagator` + voxel stack.
FA Network: authority + GUID dedupe + replicated ledger + visual echo (client prediction of
movement only — no independent destruction commits).
No Unity bridge exists; none is built (out of scope until an integration point appears).

## 6. Validation plan and status
Unit: BoxBuilding (4 tests incl. FIX-A), Medical (3), BodyPhysics (2), Limb (4), WorldGen
determinism. Measured on 2026-10-25: `Afterfall.*` automation 51 Success / 1 Fail
(`Afterfall.Interact.Radial.Sectors`, radial-menu work in flight), MECHTEST 14/0, WORLDGEN
23/0 (including stream-out/in, disk persistence and multi-focus streaming). Logs in
`Saved/Logs/`. Runtime/PIE/package: builds succeed; cook + stage + launch of this revision
still unverified. Performance: debris caps + chunk cap measured; world generation costs
~350 ms and ~0.3 GB for 16 chunks + 8 shells. No visual-only consequential outcomes:
every breach updates ledger + collision + replication (framework rule §16 adopted).

## 7. Companion documents (reconciled against the same code)
- `FALLAFTER_EQUATION_REGISTRY.md` — E1–E49 with per-equation status.
- `FALLAFTER_DESTRUCTION_MATHEMATICAL_MODEL.md` — structural + voxel destruction, persistence.
- `FALLAFTER_ANATOMICAL_DAMAGE_AND_BLEEDING_MODEL.md` — wounds, bleeding, treatment, inspect.
- `FALLAFTER_ROPE_PHYSICS_MODEL.md` — the single-segment grapple constraint as built.
- `FALLAFTER_VALIDATION_MATRIX.md` — what is actually verified, and how.
