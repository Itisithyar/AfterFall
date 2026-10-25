// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Subsystems) — registry lifecycle + typed resolution.
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Subsystem/SubsystemService.h"
#include "Afterfall.h"

bool AfterfallSubsystem::IsValidTransition(ESubsystemState From, ESubsystemState To)
{
	if (From == To)
	{
		return true; // idempotent re-entry (repeated shutdown, duplicate init)
	}
	switch (From)
	{
	case ESubsystemState::Uninitialized:
		return To == ESubsystemState::Initializing || To == ESubsystemState::Failed;
	case ESubsystemState::Initializing:
		return To == ESubsystemState::Ready || To == ESubsystemState::Failed;
	case ESubsystemState::Ready:
		return To == ESubsystemState::Stopping || To == ESubsystemState::Failed;
	case ESubsystemState::Stopping:
		return To == ESubsystemState::Stopped || To == ESubsystemState::Failed;
	case ESubsystemState::Stopped:
	case ESubsystemState::Failed:
		return false; // terminal: a dead subsystem is never revived
	default:
		return false;
	}
}

bool AfterfallSubsystem::IsServable(ESubsystemState State)
{
	// Only a fully ready provider may answer requests. Initializing/Stopping are
	// transitions, not serviceable states.
	return State == ESubsystemState::Ready;
}

bool UAfterfallSubsystemRegistry::Register(IAfterfallSubsystem* S)
{
	if (!S)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("SubsystemRegistry: Register(null) ignored"));
		return false;
	}
	if (bShuttingDown)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("SubsystemRegistry: Register(%s) refused (shutting down)"),
			S->GetName());
		return false;
	}
	if (Subsystems.Contains(S))
	{
		return false; // already registered: no duplicate entry, no double shutdown
	}
	Subsystems.Add(S);
	UE_LOG(LogAfterfall, Log, TEXT("SubsystemRegistry: registered %s (%d total)"),
		S->GetName(), Subsystems.Num());
	return true;
}

bool UAfterfallSubsystemRegistry::Unregister(IAfterfallSubsystem* S)
{
	if (!S)
	{
		return false;
	}
	// Removal is allowed during shutdown: providers unregister themselves from
	// their own Shutdown.
	return Subsystems.Remove(S) > 0;
}

IAfterfallSubsystem* UAfterfallSubsystemRegistry::Find(const TCHAR* Name) const
{
	if (bShuttingDown || !Name)
	{
		return nullptr;
	}
	for (IAfterfallSubsystem* S : Subsystems)
	{
		if (S && FCString::Stricmp(S->GetName(), Name) == 0)
		{
			return S;
		}
	}
	return nullptr;
}

IAfterfallSubsystem* UAfterfallSubsystemRegistry::Resolve(const TCHAR* Name,
	AfterfallService::EError& OutError) const
{
	using AfterfallService::EError;
	if (bShuttingDown)
	{
		OutError = EError::ShuttingDown;
		return nullptr;
	}
	IAfterfallSubsystem* S = Find(Name);
	if (!S)
	{
		OutError = EError::Unavailable;
		return nullptr;
	}
	const ESubsystemState State = S->GetState();
	if (!AfterfallSubsystem::IsServable(State))
	{
		OutError = (State == ESubsystemState::Failed) ? EError::Failed : EError::NotReady;
		return nullptr;
	}
	OutError = EError::None;
	return S;
}

int32 UAfterfallSubsystemRegistry::ShutdownAll()
{
	if (bShuttingDown && Subsystems.Num() == 0)
	{
		return 0; // already fully shut down: repeated shutdown is a no-op
	}
	bShuttingDown = true;
	// Snapshot before iterating: a provider's Shutdown may unregister itself (or
	// its peers), which reallocates the live array and would invalidate a
	// ranged-for over it.
	const TArray<IAfterfallSubsystem*> Snapshot = Subsystems;
	int32 Count = 0;
	for (IAfterfallSubsystem* S : Snapshot)
	{
		if (!S)
		{
			continue;
		}
		if (S->GetState() == ESubsystemState::Stopped)
		{
			continue; // already stopped: not counted, not shut down twice
		}
		S->Shutdown();
		++Count;
	}
	Subsystems.Empty();
	UE_LOG(LogAfterfall, Log, TEXT("SubsystemRegistry: ShutdownAll stopped %d provider(s)"), Count);
	return Count;
}
