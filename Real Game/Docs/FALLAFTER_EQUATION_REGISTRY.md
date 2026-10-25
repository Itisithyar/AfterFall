# FallAfter master equation registry E1–E49

Status: **IMPLEMENTED** (code + test/log evidence) | **PARTIAL** (code, gaps noted) |
**DESIGNED** (spec only) | **UNVERIFIED** (claimed elsewhere, not confirmed).
Units: cm / s / kg; "energy" is a documented gameplay proxy unless a file converts to SI.
Last reconciled against the code at 2026-10-25 (quadratic contract pass; see
`FALLAFTER_VALIDATION_MATRIX.md` for the measured results behind each row).

| ID | Statement | Status | Evidence / gap |
|---|---|---|---|
| E1–E2 | Linear/angular motion, L=Iω | IMPLEMENTED | Chaos solver (engine); gravity/rebound checks in `PhysicsValidationSubsystem.cpp` |
| E3 | p=mv, J=Δp; hits change velocity/rotation, not just HP | IMPLEMENTED | `BoxWallStructure` impulse dirs; `ArticulatedBodyComponent` release momentum; MECHTEST `release preserves momentum` |
| E4 | τ=r×F lever arm | PARTIAL | Torque arises inside Chaos constraints (`ArmContactSolver`, grab handle); no explicit lever-arm term in game code |
| E5 | E_rel=½m_eff·v² (reduced mass acknowledged as approximation) | IMPLEMENTED | `BoxBuilding.cpp` (cm→m /100); exact-energy test |
| E6–E8 | COM, total P, total L_O over the body graph | PARTIAL | Per-region masses (`BodyRegionPhysics::RegionMassKg`, shares 0.08/0.50/0.05/0.05/0.16/0.16 of 80 kg); COM/P/L_O never summed or asserted |
| E9–E10 | Grab spring-damper + clamp; dragging via moving target | PARTIAL | Constraint/physics-handle grab with `GrabRangeCm=350`, `HoldDistanceCm=160` (`ArticulatedBodyComponent`); no literal k/c spring law (the spec allows the constraint alternative) |
| E11 | Release preserves v; optional J_release; ΔL=r×J | IMPLEMENTED | `ArticulatedBodyComponent::ServerRelease`; MECHTEST momentum assertion |
| E12 | Thrown-body impact feeds anatomy + destruction | IMPLEMENTED | `BoxWallStructure::OnBlockHit`; MECHTEST melee detaches cells, glass breaks at its own durability |
| E13–E14 | Balance B, Guard G ∈ [0,1] | DESIGNED | No `Balance`/`Guard` variables in source (only an unused data-asset category name) |
| E15–E16 | R_j overload → Intact→Failed; 6-step detachment | DESIGNED | No joint-damage state or detachment path; ragdoll/grab exist without joint integrity |
| E17–E18 | Energy allocation across subsystems; clamped accumulation | PARTIAL | Per-subsystem thresholds exist (E1/E2/E5); no shared allocation ledger — and none should duplicate the same energy |
| E19 | Voxel mass ρh³ | IMPLEMENTED | `VoxelBasis` cells of 3.06 m; chunk init/edit tests |
| E20 | Material set (ρ, Sc, St, Gf, μ, e) | PARTIAL | One resistance number per material (`MaterialResistancePerCm`, `FractureSolver::MatResistance`), densities/friction in box structures; Sc/St/Gf not separated |
| E21 | Normalized voxel damage ηE/E_crit | PARTIAL | Budget walk + binary cell removal (`FractureSolver::Solve`, `ServerApplyFractureImpact`); **quadratic severity gate added** (Q3 below): sub-threshold impacts remove nothing. No per-cell accumulated damage. |
| E22 | Directional propagation cost C_ij, budget B_j | IMPLEMENTED | `UDestructionPropagationService::Propagate` (directional nearest-first, 8 steps, min energy 1); `Afterfall.Destruction.Propagate` + `…Regression` |
| E23–E24 | Support graph + load redistribution → cascade | PARTIAL | Support is a placement flag (`ConstructionValidator::RequiresSupport`); support-loss collapse exists only in the experimental `BoxBuilding`/`BoxWallRuntimeSubsystem`; no load values |
| E25–E26 | Debris Newtonian motion; secondary impacts via the same pipeline | PARTIAL | `ABoxBlockDebris` (bounded v, 12 s budget) for box structures only; voxel fracture spawns no debris and secondary impacts do not re-enter the voxel path |
| E27–E29 | Wound creation, q(0), blood-loss integration | PARTIAL | `InjuryStateComponent` holds 6 records (severity 0–3, `bBleeding`, `BleedRateHpS`); `BleedTick = Rate·Δt` (`Afterfall.Medical.MathGuards`); no per-wound blood volume b_i, no `g(S,A,M)`; no `BleedingComponent` file exists |
| E30 | Treatment reduces bleeding by η_T | PARTIAL | Dressing sets rate to 0 (η_T = 1) and queues `PendingHPRestore`; partial η_T not modelled |
| E31 | Natural stabilization dq/dt = −λq | DESIGNED | No decay term anywhere |
| E32 | q_i = 0 ⇏ S_i = 0 | IMPLEMENTED | `bBleeding` and `bWounded` are independent flags; light dressed wounds close, severe ones stay stabilized |
| E33 | P(B) condition factor; death stays with the death system | PARTIAL | Bleeding drains the `HealthComponent` pool (single death authority, good); no consciousness/stamina staging from blood loss |
| E34–E35 | Visuals project wound state; ALT inspection local-only | DESIGNED | No ALT mode, wound decals or attachment; only a radial-menu `Inspect` action that prints target text (`LastInspectText`, display-only) |
| E36 | Rope length L_0 = Σℓ_i, bounded | IMPLEMENTED | Single-segment `RopeLength = clamp(d, 100, 2000)` (`AfterfallGrappleComponent`); MECHTEST `rope length assigned` |
| E37 | C_i = |x_{i+1}−x_i| − ℓ_i ≤ 0 | PARTIAL | Enforced per tick with 5 cm slack by velocity projection + rate-limited position correction (10/s); one segment only; MECHTEST `rope constraint respected after ticks` |
| E38 | Tension T = max(0, kC + cĊ) | DESIGNED | No stiffness/damping; rope cannot stretch or pull other bodies |
| E39 | Segment force F = T·d̂ | PARTIAL | Radial velocity removal + bounded steering (600, cap 1500 cm/s) stand in for rope force |
| E40 | Attachment states, replicated, safe on endpoint loss | IMPLEMENTED / PARTIAL | `Idle→Attempting→Attached→Released`, server-only entry, replicated State/Anchor/RopeLength; anchor is a world point, so anchor destruction does not detach |
| E41–E42 | Solver config Θ_F; determinism on seed | PARTIAL | `FractureSolver` is deterministic for fixed seed/material/energy; HUD lab page exists; control→parameter mapping unverified, no replay test |
| E43 | Commit-once with an id ledger | IMPLEMENTED | GUID ledger (`RecentEventIds`, cap 8) + cell dedupe in `ServerApplyImpact`; `Afterfall.Destruction.CommitIdempotent` |
| E44 | Prediction error + reconciliation | PARTIAL | Server-authoritative commits, replicated sequences, client visual prediction only; no reconciliation code |
| E45 | Voxel resync readiness R_ready | IMPLEMENTED | `bHasSnapshot` + `LastAppliedSeq` gating, `VoxelChunkTypes::IsGap` detection, 3 s resync pacing, request relayed through the local pawn; late-join snapshot burst |
| E46 | S_client → S_server convergence | PARTIAL | Mechanism exists (snapshot adoption rejects older seq, edits dedupe by seq); **no automated two-client convergence assertion** |
| E47–E49 | Build/cook/package/launch state machine; last-good preserved; launch acceptance | PARTIAL | Editor and game targets build (`BUILD_EXIT:0`, 2026-10-25); cook/stage/launch of this revision not run, so no validated package exists for it |

Laws §16 adopted as repo rules: declare units; one authoritative owner per state;
one authoritative application per event; consequences come from state; every
equation needs a test path.

## Quadratic contract applications (2026-10-25)

Equation contract applied per the project rule: variables + units, assumptions,
mechanic it changes, one application per event, engine solver not double-applied,
test path. **Physical quadratics and designed response curves stay distinct**
(the distinction is load-bearing — designed curves are gameplay approximation,
not universal law).

| ID | Statement | Kind | Status | Evidence / assumption |
|---|---|---|---|---|
| Q1 | Throw/impact energy E = ½·m_eff·v² (E5, E12 chain) | **Physical** | IMPLEMENTED (pre-existing) | `BoxBuilding.cpp:257` (cm→m conversion), `BodyRegionPhysics.cpp:68` proxy energy, `PhysicsValidationSubsystem.cpp:777` sampled-impact energy + exact-energy test; grab→release→collision chain asserted by MECHTEST (E3/E11/E12) |
| Q2 | Fall/airborne trajectory y(t) = y₀ + v₀y·t − ½gt² | **Physical** | ENGINE-OWNED (by contract rule 4) | Chaos + `CharacterMovement` integrate gravity; re-applying in game code would double-count. No code added — registered so nobody "implements" it twice. |
| Q3 | Fracture severity D(x) = K·x², x = E/E_ref, threshold 100x² = 64 → x = 0.8 | **Designed curve** | IMPLEMENTED | `FractureSolver.h/.cpp` (`FFractureSeverityResponse`, `SeverityD`, `InitiationSeverityX`, `InitiatesFracture`, `EffectivePropagationEnergy`); gate + quadratic budget `E²/E_ref` applied once inside `Solve`; caller gate in `VoxelChunkActor::ServerApplyFractureImpact`. Tuning exposed via `Get/SetSeverityResponse` + HUD Lab page. Tests: `Afterfall.Destruction.Quadratic.{Threshold,Scaling,Gate,Utilization,MaterialChain,TuningGuard}`. Assumptions: monotone quadratic; defaults E_ref = 1000 J, K = 100, D_thr = 64 — initiation at x = 0.8 → 800 J ≈ 80 kg at 4.47 m/s, between the table's 4 m/s (640 J, dent) and 6 m/s (1440 J, fracture) rows. **Not a material-science claim.** |
| Q4 | Failure utilization U = E_demand / C_material (dimensionless demand-to-capacity ratio; U ≥ 1 = capacity reached, NOT a probability) | **Physically informed (energy criterion)** | IMPLEMENTED | `FailureUtilization` + `MaterialStrengthJ` in `FractureSolver` — capacity reuses the EXISTING box-path `UBoxBuildingLib::BreakEnergyJ` table via documented cross-map (Stone→Brick, Plaster→Drywall); no strength values invented. `Solve` now requires BOTH Q3 severity ≥ D_thr AND U ≥ 1 as separate checks (never blended). Guards: non-finite/≤0 inputs → 0, overflow clamped 1e6, unknown material → 0 (refuses initiation). Lab page shows live U inputs (capacity row). **Energy-based, not stress-based: the engine has no σ/τ model.** |

Deliberately NOT quadraticized: bleeding (`BleedTick = Rate·Δt`, linear —
needs medical justification first) and stamina (no depletion model exists).
Nonlinearity without justification is excluded by the contract, not by omission.

**Unsupported by the engine (reported, not approximated away):** stress
quantities (σ_t/σ_c/τ), strengths f_t/f_c/f_s, fracture toughness K_IC,
elastic modulus E / Poisson ν — no stress model exists anywhere in `Source`.
Thermal (ε_th = αΔT), moisture degradation, differential settlement/soil,
and vibration/dynamic loading have **no state variables** (no temperature,
moisture, foundation, or time-varying-load representation), so they cannot
be modeled honestly yet. Construction defects are only partially present:
`FBoxBlock.AccDamage` accumulates damage on the box path; the voxel path has
no per-cell damage memory. AFTERFALL remains a physically informed
destruction model, **not a validated building-safety simulator.**

## Next reconciliation targets

1. E13–E16 (balance, guard, joint damage/detachment) — currently design only.
2. E34–E35 (ALT body inspection) — design only; the radial `Inspect` is not it.
3. E45–E46 — add the two-client convergence test.
4. E47–E49 — cook, stage and launch the current revision, preserving last good.
5. E23–E24, E38 — decide whether load redistribution and rope tension ship, or
   remove them from the design instead of leaving them as assumed features.
