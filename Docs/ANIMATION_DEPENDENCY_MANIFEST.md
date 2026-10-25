# AFTERFALL — Animation dependency manifest

## Current state: NOTHING ANIMATED IS INTEGRATED

Verified facts, this session, against the canonical project:

* `Content/` contains only the UE first-person template: `BP_FirstPersonCharacter`,
  `BP_FirstPersonPlayerController`, `BP_FirstPersonGameMode`, `Lvl_FirstPerson`,
  and `anims.bak`. There is **no skeletal mesh, no skeleton, no Physics Asset,
  no Animation Blueprint, no AnimSequence** in the project.
* `Content/FirstPerson/Anims/ABP_FP_Copy` is referenced by
  `BP_FirstPersonCharacter` and **is missing** — the editor log reports
  `LoadErrors: While trying to load package /Game/FirstPerson/Blueprints/BP_FirstPersonCharacter,
  a dependent package /Game/FirstPerson/Anims/ABP_FP_Copy was not available`.
  This is also why the template pawn spams
  `LogSkinnedMeshComp: GetSocketInfoByName(Head): No SkeletalMesh` every frame.
* `UE_5.8\Engine\Content\Characters\` does not exist in this installation, so the
  engine mannequin cannot be borrowed either.

C++ cannot create animation assets. Nothing in the registry is marked as having
loaded animation because no animation exists to load.

## What each mechanic would need

| Mechanic | Anim set | Skeleton | ABP / layer | Physics Asset |
|---|---|---|---|---|
| player-locomotion | idle/walk/jog/crouch/jump | one humanoid skeleton shared by all | `ABP_Afterfall` locomotion state machine | n/a |
| zombie-pursuit | walk/run/attack | same skeleton (retargeted) | zombie layer or separate ABP | n/a |
| zombie-ragdoll | death transition | same skeleton | ragdoll blend node | `PHYS_Mannequin` |
| articulated-grab | drag pose / hold | same skeleton | upper-body layer | n/a |
| injury-treatment | bandage apply / flinch | same skeleton | additive hit/flinch layer | n/a |
| breakable-environment | none | n/a | n/a | n/a |

Registry paths reserved for imported content (`Docs/build_monitor/mechanics.registry.json`,
mechanic `skeletal-animation`):

```
/Game/AF_External/Characters/SK_Mannequin_Skeleton
/Game/AF_External/Characters/ABP_Afterfall
/Game/AF_External/Characters/PHYS_Mannequin
/Game/AF_External/Animations/Player/AM_Locomotion
/Game/AF_External/Animations/Zombie/AM_ZombieAttack
/Game/AF_External/Animations/Zombie/AM_ZombieDeath
```

These are **declared destinations, not existing assets**.

## Migration requirements (when assets are authorized)

1. One skeleton for all humanoid content; retarget rather than adding a second
   skeleton per pack.
2. Physics Asset must match the skeleton's bone names; ragdoll constraints come
   from it, not from the blockout `UArticulatedBodyComponent` joints.
3. `ABP_Afterfall` must be created as a child of the imported skeleton and
   referenced by the mesh; a missing ABP currently breaks the template pawn.
4. After import, re-run `python Scripts/mechanic_registry.py` — it will flip
   `skeletal-animation` from `WAITING_FOR_ASSETS` to `READY` only when the files
   exist on disk.

## Blockout fallback in force today

The character is a capsule + camera; the zombie is six engine-basic-shape
meshes with constraint-driven physics on death (`UArticulatedBodyComponent`:
6 bodies / 5 joints, verified at runtime). No code depends on animation to
function, which is why the gameplay loops above are verifiable without art.
