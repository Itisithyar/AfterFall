# FallAfter rope physics model (E36–E40)

Status labels: **IMPLEMENTED** · **PARTIAL** · **DESIGNED** · **UNVERIFIED**.
Owner of rope state: `UAfterfallGrappleComponent` (server-authoritative; clients
receive `State`, `Anchor`, `RopeLength`). Units: cm, cm/s, cm/s².

## 1. What the rope actually is

There is **no particle chain**. The rope is one rigid segment of `n = 1` between
the character and a world-space anchor point acquired by a line trace:

```
R = (x_0, x_1, L, A_0, A_1)      with n = 1
x_0 = character location,  x_1 = Anchor (hit point)
L   = clamp(|x_1 - x_0|, MinRopeLengthCm, MaxRopeLengthCm)  = clamp(d, 100, 2000)
```

Attach requires a blocking hit within `MaxRangeCm = 2000` and `d ≥ 100`. Tuning
(`FAfterfallGrappleTuning`): `ConstraintToleranceCm = 5`,
`ConstraintCorrectionRate = 10`, `SwingSteerStrength = 600`, max swing speed
1500 cm/s (hardcoded in `ApplySteering`), `MaxRopeLengthCm = 2000`.

## 2. Constraint (E37)

Each server tick while `State == Attached`:

```
err      = |x_0 - x_1| - L
if err ≤ +5 cm:                 slack, no correction
else:
  r̂        = (x_1 - x_0)/|x_1 - x_0|
  v_radial = v · r̂
  v       ← v - r̂·max(0, v_radial)              // remove outward radial velocity
  x_0     ← x_0 - r̂·err·clamp(10·Δt, 0, 1)      // positional correction
```

This is a **position-level inequality constraint with slack**, applied once per
tick, not an impulse or tension solver. Consequences worth stating plainly:

* The rope is inextensible in effect (no stretch term), so there is no `T_i`
  and no elastic energy: E38/E39 are not implemented as forces.
* The correction term `clamp(10·Δt,0,1)` is rate-limited; at 60 Hz it removes
  ~17 % of the excess per frame and is tool-verified against
  `RopeLength + Tolerance + 1 cm` in the mechanics fixture.
* Momentum is preserved on release: `Release()` clears state and never touches
  the movement component's velocity.

Steering (E39 proxy) is bounded user input, not rope tension:

```
v ← v + Input · 600 · Δt,      then clamp |v| ≤ 1500 cm/s
```

## 3. Attachment states (E40)

```
EAfterfallGrappleState: Idle → Attempting → Attached → Released → Idle
```

Server-only entry: `TryGrapple()` / `Release()` refuse without authority; clients
enter through `AAfterfallCharacter::ServerRequestGrapple` /
`ServerReleaseGrapple` (reliable, with validation). `State`, `Anchor` and
`RopeLength` are replicated, so remote clients draw the same rope.

Because `Anchor` is a **world point**, destroying the anchor object does not
detach the rope — there is no dangling-actor reference to clean up, but also no
"anchor destroyed" transition. Detachment only happens on release, death, or the
movement component going away.

## 4. Equation status

| ID | Statement | Status | Evidence / gap |
|---|---|---|---|
| E36 | `L_0 = Σ ℓ_i`, length bound | **IMPLEMENTED** (n = 1) | `RopeLength` clamp [100, 2000]; MECHTEST `rope length assigned` |
| E37 | `C_i = |x_{i+1}-x_i| - ℓ_i ≤ 0` | **PARTIAL** | Enforced with 5 cm slack by position correction, one segment only; MECHTEST `rope constraint respected after ticks` |
| E38 | Tension `T = max(0, kC + cĊ)` | **DESIGNED** | No stiffness/damping terms; rope cannot stretch or spring |
| E39 | Segment forces `F = T·d̂` | **PARTIAL** | Radial velocity removal + bounded steering approximate the constraint; no force is ever applied to other bodies |
| E40 | Attachment state machine, replicated, safe on endpoint loss | **IMPLEMENTED / PARTIAL** | States + replication + authority done; anchor destruction does not detach (world point, not an actor) |

## 5. Verification

`MECHTEST` (headless server fixture, `-afMechSelfTest`) asserts, against a real
building wall: attach succeeds, rope length assigned, constraint respected after
30 simulated ticks, release detaches, release preserves momentum. Latest run:
`MECHTEST SUMMARY pass=14 fail=0`.

Not covered: two-player rope interaction, rope rendering to a spline, rope-vs-rope
or rope-vs-debris collision, swinging energy accounting, rope attached to a moving
physics body.
