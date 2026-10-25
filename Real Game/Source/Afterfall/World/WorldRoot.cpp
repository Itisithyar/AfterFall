// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "WorldRoot.h"
#include "Net/UnrealNetwork.h"

AWorldRoot::AWorldRoot()
{
	bReplicates = true;
	RootId = FGuid::NewGuid();
	RootData.RootId = RootId;
}

void AWorldRoot::RegisterLiveRepresentation(AActor* LiveActor)
{
	if (!LiveActor) return;
	TWeakObjectPtr<AActor> WeakPtr(LiveActor);
	if (!LiveRepresentations.Contains(WeakPtr))
	{
		LiveRepresentations.Add(WeakPtr);
	}
}

void AWorldRoot::UnregisterLiveRepresentation(AActor* LiveActor)
{
	if (!LiveActor) return;
	for (int32 i = LiveRepresentations.Num() - 1; i >= 0; --i)
	{
		if (LiveRepresentations[i].Get() == LiveActor)
		{
			LiveRepresentations.RemoveAt(i);
		}
	}
}

void AWorldRoot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorldRoot, RootId);
	DOREPLIFETIME(AWorldRoot, RootData);
}
