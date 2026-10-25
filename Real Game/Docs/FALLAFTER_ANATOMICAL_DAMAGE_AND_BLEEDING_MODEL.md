# FallAfter anatomical damage & bleeding model — round two (BEAM, 2026-10-24)

Extends `Docs/FALLAFTER_MATHEMATICAL_FOUNDATION.md`. Rule enforced throughout: every item is
IMPLEMENTED (exact file:line) or DESIGNED. The handoff's "reported" items were inspected —
`Combat/BleedingComponent.h/.cpp` DOES NOT EXIST anywhere in `Source/` (verified by filename
search). Bleeding is implemented inside `Combat/InjuryStateComponent`, not a separate component.
Do not create a duplicate bleeding system; the registry below reflects this.

## A1–A3 impact → anatomical mapping → regional damage: IMPLEMENTED (parts)
- Impact descriptor ≈ `FDamageResult` (`Combat/DamageResolver.h:11-25`): confirmed-hit flag,
  resolved part, pre/post-mitigation damage, lethal flags. Severity comes from weapon config
  damage + armor mitigation (`ResolveHit(HitPart, WeaponConfig, ArmorConfig, …)`), NOT from
  recomputed joules — consistent with the framework's "use the source's documented units" rule.
- Skeletal mapping ≈ `EBodyPart` (Head/Torso/ArmL/ArmR/LegL/LegR, `Combat/BodyDamageTypes.h`)
  + `PartToIndex` (`InjuryStateComponent.cpp:67`). Missing-bone fallback: DESIGNED (no explicit
  unmapped-bone path found; default resolves to Torso).
- Regional accumulation ≈ per-part wound flags + severity + `PendingHPRestore`
  (`InjuryStateComponent.h:61-116`, `.cpp:126-305`). Coefficients Kr/Mr/Lr: DESIGNED (current
  model uses capability matching + flat HPRestore, not per-region multipliers).

## A4–A5 joints and impairment: DESIGNED (scaffold exists)
- `ArticulatedBodyComponent` (server-authoritative ragdoll/grab/release, grab ranges cm,
  momentum-preserving release) proves the body graph exists, but joint damage states
  (Intact→Injured→Critical→Failed), Rj overload ratios, and Cr capability functions are NOT
  modeled. Dismemberment: no assets, no rules — explicitly out of scope until defined.

## A6–A9 wounds and bleeding: IMPLEMENTED (inside InjuryStateComponent, see correction above)
- Wound state ≈ per-part `bWounded/bBleeding/bPain/bStabilized + Severity`
  (`InjuryStateComponent.h`); rate ≈ `BleedRateHpS` per profile; progression ≈
  `BleedTick(rate,dt)` (`InjuryStateComponent.cpp:82`, tested exact values).
- Treatment ≈ capability match → `ServerTryConsumeItem` (server-atomic, once) →
  stops bleed / `PendingHPRestore` → `ServerHeal` (`InjuryStateComponent.cpp:246-281`).
  ηT=1 (bandage stops bleed); partial ηT: DESIGNED.
- Wound persists after bleeding stops (flags independent of rate) — matches A8's
  qi=0 ⇏ Si=0 rule by construction. Natural stopping: DESIGNED (no λ decay; only
  treatment stops bleeding today).

## A10 blood-loss consequences: DESIGNED
- No Bcrit/P(B) factor exists. Bleed damage routes into the single HP pool via
  `ServerTakeDamage(TotalBleed)` (`.cpp:228`) — no competing death logic, consistent with
  A10's constraint, but no stamina/consciousness staging.

## A11 wound visuals: PARTIAL
- Visuals must project from wound state (rule adopted); actual skeletal attachment
  implementation: UNVERIFIED (no attachment code found; Manny staged but no Physics Asset).

## A12 ALT inspection: DESIGNED
- No ALT input, camera, HUD, or inspect-view code found in `Source/` (filename search empty).
  Contract recorded: local-only presentation state reading the authoritative injury ledger;
  must not create/modify state; restore camera on release; cleanup on death/interrupt.

## A13 authority: IMPLEMENTED (pattern exists, anatomical path partial)
- Universal event shape ≈ `FInteractionEvent` (FGuid Id + Type + Position/Normal/Velocity,
  `Combat/InteractionEvent.h`). Dedupe precedent: `RecentEventIds` cap-8
  (`BoxWallStructure.cpp`), GUID-deduped `ServerApplyImpact`. Server owns injury/bleed/
  treatment (`HasAuthority` gates + `ServerValidated` damage). Client prediction of anatomy:
  DESIGNED.

## Priority loop (unchanged, now mapped): hit (`DamageResolver`) → wound (`InjuryState`) →
bleeding (`BleedTick`/rates) → treatment (capability + consume + `ServerHeal`) → ALT inspect
(DESIGNED) → verify. Joint failure + impairment after the loop is proven.
