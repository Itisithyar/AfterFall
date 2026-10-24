// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "EntityLifecycleService.h"

bool UEntityLifecycleService::ShouldPromote(bool bDamagedOrInteracted, bool bDetectsOrPursuesOrAttacks,
	bool bJoinedPersistentEncounter, bool bServerFlagged)
{
	return bDamagedOrInteracted || bDetectsOrPursuesOrAttacks
		|| bJoinedPersistentEncounter || bServerFlagged;
}

bool UEntityLifecycleService::CanDemote(bool bHasPursuerOrAttacker, float NearestPlayerDistM,
	bool bHasSharedDependency, bool bLootOrDeathUnresolved)
{
	if (bHasPursuerOrAttacker || bHasSharedDependency || bLootOrDeathUnresolved)
	{
		return false;
	}
	// Active ceiling 100m; demote only beyond it.
	return NearestPlayerDistM > 100.f;
}

bool UEntityLifecycleService::MustKeepForPersistence(float NearestPlayerDistM)
{
	// 30m zone: persistent, never despawn for perf or look-away.
	return NearestPlayerDistM <= 30.f;
}

bool UEntityLifecycleService::IsValidDeathContainerCount(int32 ContainerCount)
{
	return ContainerCount == 1;
}

bool UEntityLifecycleService::ShouldCreateDamageRecord(bool bHasBeenDamaged, bool bIsConfirmedHit)
{
	// Core rule: undamaged zombie must not receive individual persistent damage record
	// Only on first confirmed damage event
	if (!bIsConfirmedHit)
	{
		return false;
	}
	return bHasBeenDamaged || true; // First confirmed hit creates record
}

bool UEntityLifecycleService::ShouldPersistZombieDamage(bool bHasDamageRecord)
{
	// Only persist if there's an actual damage record (damaged zombie)
	return bHasDamageRecord;
}
