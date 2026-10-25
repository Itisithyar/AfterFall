// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: AI (Entities) â€” pure transition rules. Server executes, all authority server-side.
#pragma once

#include "CoreMinimal.h"
#include "EntityLifecycle.h"
#include "UObject/NoExportTypes.h"
#include "EntityLifecycleService.generated.h"

UCLASS()
class AFTERFALL_API UEntityLifecycleService : public UObject
{
	GENERATED_BODY()

public:
	// Promote when ANY holds: damaged/interacted, detects/pursues/attacks,
	// joins persistent encounter, or server flags meaningful event.
	// Hit-only promotion is REJECTED (would erase chases on unload).
	static bool ShouldPromote(bool bDamagedOrInteracted, bool bDetectsOrPursuesOrAttacks,
		bool bJoinedPersistentEncounter, bool bServerFlagged);

	// Demote only when ALL hold: no pursuer/attacker, far from every player,
	// no shared dependency, loot/death resolved. Never on camera direction.
	static bool CanDemote(bool bHasPursuerOrAttacker, float NearestPlayerDistM,
		bool bHasSharedDependency, bool bLootOrDeathUnresolved);

	// 30m persistence floor: inside 30m never despawn for perf reasons.
	static bool MustKeepForPersistence(float NearestPlayerDistM);

	// Death container rule: exactly one grouped container per death.
	static bool IsValidDeathContainerCount(int32 ContainerCount);

	// Damage-triggered persistence: undamaged zombies must not get individual records
	static bool ShouldCreateDamageRecord(bool bHasBeenDamaged, bool bIsConfirmedHit);
	static bool ShouldPersistZombieDamage(bool bHasDamageRecord);
};
