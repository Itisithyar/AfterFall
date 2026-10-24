// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AI/ZombiePrototype.h"
#include "Net/UnrealNetwork.h"

AZombiePrototype::AZombiePrototype()
{
	bReplicates = true;
	HP = 100.f;
	// No per-frame tick by default; AI controller / dormant manager drives updates.
	PrimaryActorTick.bCanEverTick = false;
}

bool AZombiePrototype::ServerTakeDamage_Validate(float Amount)
{
	return Amount >= 0.f && Amount <= 500.f;
}

bool AZombiePrototype::ServerTakeDamage_Implementation(float Amount)
{
	if (!HasAuthority() || HP <= 0.f)
	{
		return HP > 0.f;
	}
	HP = FMath::Max(HP - Amount, 0.f);
	return HP > 0.f;
}

bool AZombiePrototype::ShouldTickFull(float NearestPlayerDistM, bool bHasPursuer) const
{
	// 30m persistent zone always relevant; pursuer always relevant; else dormant.
	if (bHasPursuer)
	{
		return true;
	}
	return NearestPlayerDistM <= ActiveRangeM;
}

void AZombiePrototype::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZombiePrototype, HP);
	DOREPLIFETIME(AZombiePrototype, StableId);
}
