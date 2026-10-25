// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — provider lifecycle + typed query implementation.
#include "VoxelSubsystem.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Afterfall.h"

void UVoxelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Initialize();
}

bool UVoxelSubsystem::Initialize()
{
	if (State == ESubsystemState::Ready)
	{
		return true; // idempotent: a second Initialize is not a second registration
	}
	if (!SetState(ESubsystemState::Initializing))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VoxelSubsystem: Initialize from state %d refused"),
			(int32)State);
		return false;
	}
	UWorld* W = GetWorld();
	UAfterfallSubsystemRegistry* Registry =
		W ? W->GetSubsystem<UAfterfallSubsystemRegistry>() : nullptr;
	if (!Registry)
	{
		// No registry in this context (e.g. a bare test world): not a failure of
		// the service itself, but it cannot be discovered. Report Failed so
		// callers get a definite answer instead of a silent null.
		SetState(ESubsystemState::Failed);
		UE_LOG(LogAfterfall, Warning, TEXT("VoxelSubsystem: no registry, marked Failed"));
		return false;
	}
	if (!Registry->Register(this))
	{
		SetState(ESubsystemState::Failed);
		UE_LOG(LogAfterfall, Warning, TEXT("VoxelSubsystem: registration refused, marked Failed"));
		return false;
	}
	return SetState(ESubsystemState::Ready);
}

void UVoxelSubsystem::Shutdown()
{
	if (State == ESubsystemState::Stopped)
	{
		return; // repeated shutdown is a no-op
	}
	SetState(ESubsystemState::Stopping);
	UWorld* W = GetWorld();
	if (UAfterfallSubsystemRegistry* Registry =
		W ? W->GetSubsystem<UAfterfallSubsystemRegistry>() : nullptr)
	{
		Registry->Unregister(this);
	}
	SetState(ESubsystemState::Stopped);
}

void UVoxelSubsystem::Deinitialize()
{
	Shutdown();
	Super::Deinitialize();
}

bool UVoxelSubsystem::SetState(ESubsystemState NewState)
{
	if (!AfterfallSubsystem::IsValidTransition(State, NewState))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("VoxelSubsystem: illegal transition %d -> %d"),
			(int32)State, (int32)NewState);
		return false;
	}
	State = NewState;
	return true;
}

void* UVoxelSubsystem::GetService(EAfterfallServiceId Id)
{
	if (Id != EAfterfallServiceId::VoxelCell)
	{
		return nullptr;
	}
	// The id check is what makes this downcast safe without RTTI.
	return static_cast<IVoxelCellService*>(this);
}

FVoxelCellResponse UVoxelSubsystem::QueryCell(const FVoxelCellRequest& Request) const
{
	FVoxelCellResponse Response;
	switch (State)
	{
	case ESubsystemState::Ready:
		break;
	case ESubsystemState::Failed:
		Response.Error = AfterfallService::EError::Failed;
		return Response;
	case ESubsystemState::Stopping:
	case ESubsystemState::Stopped:
		Response.Error = AfterfallService::EError::Unavailable;
		return Response;
	default:
		Response.Error = AfterfallService::EError::NotReady;
		return Response;
	}
	if (Request.WorldPoint.ContainsNaN() || Request.Origin.ContainsNaN()
		|| !FMath::IsFinite(Request.WorldPoint.X) || !FMath::IsFinite(Request.WorldPoint.Y)
		|| !FMath::IsFinite(Request.WorldPoint.Z) || !FMath::IsFinite(Request.Origin.X)
		|| !FMath::IsFinite(Request.Origin.Y) || !FMath::IsFinite(Request.Origin.Z))
	{
		Response.Error = AfterfallService::EError::InvalidRequest;
		return Response;
	}
	Response.Cell = FVoxelBasis::WorldToVoxel(Request.WorldPoint, Request.Origin);
	Response.Error = AfterfallService::EError::None;
	const_cast<UVoxelSubsystem*>(this)->ServedCount++;
	return Response;
}
