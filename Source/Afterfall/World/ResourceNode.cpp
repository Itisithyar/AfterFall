// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "ResourceNode.h"
#include "Net/UnrealNetwork.h"

AResourceNode::AResourceNode()
{
	bReplicates = true;
}

int32 AResourceNode::YieldFor(EResourceKind K, float T01)
{
	const float T = FMath::Clamp(T01, 0.f, 0.9999f);
	switch (K)
	{
	case EResourceKind::Wood:  return 3 + FMath::FloorToInt(T * 3); // 3-5
	case EResourceKind::Stone: return 4 + FMath::FloorToInt(T * 5); // 4-8
	case EResourceKind::Hemp:  return 2 + FMath::FloorToInt(T * 3); // 2-4
	default: return 1;
	}
}

bool AResourceNode::ServerHarvest_Validate(bool bHasTool)
{
	return true;
}

void AResourceNode::ServerHarvest_Implementation(bool bHasTool)
{
	LastHarvestYield = 0;
	if (!HasAuthority())
	{
		return;
	}
	// Depletion + range checks need server clock + instigator distance;
	// caller (character interaction, 3m) enforces range. Time check stub:
	// real Unix check lands with RegionStateSave clock; slice uses direct grant.
	int32 Yield = YieldMin + (YieldMax - YieldMin) / 2;
	if (bHasTool)
	{
		Yield = FMath::CeilToInt(Yield * 1.5f); // hatchet +50%
	}
	LastHarvestYield = FMath::Max(Yield, 0);
}

void AResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AResourceNode, DepletedUntilUnix);
	DOREPLIFETIME(AResourceNode, LastHarvestYield);
}
