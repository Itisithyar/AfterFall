# MAT-INTERACTION.md — physical inventory mat (2026-10-24)

## What it is

B (`IA_Mat`) toggles a server-authoritative open/close flow on the character's
`UMatInteractionComponent`: crouch → (re)deploy mat in front → timed unroll →
inventory UI window → pack-up → uncrouch → control restored. Movement and
collision code are untouched: restriction is input gating (`Move`/`Jump`
early-out, attack/fire RPC rejects) plus native replicated `Crouch()`.

## Files

- NEW `Source/Afterfall/Interaction/MatInteractionComponent.{h,cpp}` — state
  machine (`Closed/Opening/Open/Closing`, replicated + `OnInteractionChanged`
  delegate for UMG), `ServerRequestMatOpen/Close` RPCs, `NotifyMatRevealed()`
  anim-notify hook, `UnrollDurationSec`/`PackDurationSec` + `UnrollMontage`
  editor knobs, pure `CanTransition()` table (unit-tested).
- EDIT `Source/Afterfall/Storage/PhysicalMat.{h,cpp}` — replicated `bPacked`;
  `SetPacked()` hides mesh + disables collision, keeps slots/identity;
  deposit/withdraw reject while packed; `SetReplicateMovement(true)` so
  reopen-reposition reaches clients.
- EDIT `Source/Afterfall/AfterfallCharacter.{h,cpp}` — `MatInteract`
  subobject; `OnMatPressed` toggles; `Move`/`Jump` gated;
  `ServerAttack`/`ServerFireHitscan` reject while open; `HandleDeath`
  force-closes. Throw/drop/interact stay allowed (deposit flow needs hands).
- NEW `Source/Afterfall/Tests/MatInteractionTest.cpp` — `Afterfall.MatInteraction.*`
  automation: full 16-pair transition table, double-open/double-close safety,
  blocking + UI-visibility predicates.
- No physics, input-map, camera, or movement-logic changes.

## Asset readiness (verified, see Docs/ASSET-INVENTORY.md)

- Mat mesh: `/Engine/BasicShapes/Plane.Plane` via constructor `FObjectFinder`
  with `Succeeded()` guard — engine content, always present. No new assets.
- Character unroll/kneel animation: MISSING from committed `Content/`.
  The Manny set (`AfterFall/Content/Characters/`, gitignored) has no kneel/
  unroll sequence either — only idle/walk/jog/jump/attack/death. So the flow
  is timer-driven scaffolding: `UnrollMontage == nullptr` is valid and the
  transitions still complete. Do NOT set the montage to a path that does not
  exist in committed `Content/`.
- Inventory UI widget: MISSING (no UMG in repo). `OnInteractionChanged`
  (state + mat) is the binding point; UI shows on `Open`, hides otherwise.

## Exact editor steps (when assets exist)

1. **Unroll montage**: import/create a kneel+unroll `UAnimMontage` on the
   character skeleton → select the character BP (or C++ defaults) →
   `MatInteract` component → set `UnrollMontage`. Tune `UnrollDurationSec`
   to match (fallback if the notify is missed).
2. **Reveal notify**: in the montage at the frame the mat is spread, add an
   AnimNotify (any notify class) whose event calls
   `MatInteract->NotifyMatRevealed()` (server). Until then the timer ends
   `Opening` — both paths lead to the same `Open` state.
3. **Mat look**: override the mat's `Mesh` (StaticMeshComponent `MatMesh`) in
   a child BP or swap `SetStaticMesh` source to a rolled/unrolled pair driven
   by `bPacked` (extend `OnRep_Packed`). Default Plane keeps working.
4. **UI**: bind a UMG widget to `OnInteractionChanged`: `Open` → show with
   `Mat->Slots` (replicated); any other state → hide. `Access`/`Pack Up`
   radial options (`URadialMenuComponent::GetAvailableActions` on the mat)
   should call `ServerRequestMatOpen` / `ServerRequestMatClose`.
   IMPLEMENTED 2026-10-24 (Muse Spark): `Release()` routes Access →
   `ServerRequestMatOpen` and Pack Up → `ServerRequestMatClose` through the
   owner's `MatInteract` (same server-validated path as B); `StartHold`
   resolves the controller via the owning character. Other radial actions
   stay no-op pipeline stubs. Headless-verified (build 0, 28/28, 9/9).
5. **Verify**: in-editor PIE — B opens (crouch + timer + UI event), B closes
   (pack + uncrouch), spam B through all phases, kill the pawn mid-open
   (death force-closes, no stuck state), test with packet lag (server owns
   every transition).

## Verification split

- COMPILE: full editor link verified 2026-10-24 (exit 0, twice).
- UNIT: transition table, repeated-input, blocking/UI predicates — PASS
  (`Afterfall.MatInteraction.*` in 28/28 runs); radial Access/Pack Up
  option assertions added 2026-10-24 — PASS (`Afterfall.UI.RadialMenu`).
- RUNTIME (in-editor acceptance, not claimed): crouch replication timing,
  montage/notify sync, UMG binding, mat reposition on slopes, lag behavior.
