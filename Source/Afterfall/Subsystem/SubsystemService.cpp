// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Subsystems) — typed service resolution.
#include "Subsystem/SubsystemService.h"
#include "Afterfall.h"

const TCHAR* AfterfallService::ErrorText(EError Error)
{
	switch (Error)
	{
	case EError::None: return TEXT("ok");
	case EError::Unavailable: return TEXT("service unavailable");
	case EError::NotReady: return TEXT("service not ready");
	case EError::Failed: return TEXT("service failed");
	case EError::ShuttingDown: return TEXT("registry shutting down");
	case EError::InvalidRequest: return TEXT("invalid request");
	default: return TEXT("unknown error");
	}
}

FVoxelCellResponse QueryVoxelCell(const UAfterfallSubsystemRegistry* Registry,
	const FVoxelCellRequest& Request, const TCHAR* ProviderName)
{
	FVoxelCellResponse Response;
	if (!Registry)
	{
		Response.Error = AfterfallService::EError::Unavailable;
		return Response;
	}
	// Reject payloads the lattice math cannot interpret (NaN/inf): the floor
	// conversion of a non-finite float is undefined.
	if (Request.WorldPoint.ContainsNaN() || Request.Origin.ContainsNaN()
		|| !FMath::IsFinite(Request.WorldPoint.X) || !FMath::IsFinite(Request.WorldPoint.Y)
		|| !FMath::IsFinite(Request.WorldPoint.Z) || !FMath::IsFinite(Request.Origin.X)
		|| !FMath::IsFinite(Request.Origin.Y) || !FMath::IsFinite(Request.Origin.Z))
	{
		Response.Error = AfterfallService::EError::InvalidRequest;
		return Response;
	}

	AfterfallService::EError ErrorCode = AfterfallService::EError::Unavailable;
	IAfterfallSubsystem* Provider = Registry->Resolve(ProviderName, ErrorCode);
	Response.Error = ErrorCode;
	if (!Provider)
	{
		return Response; // Resolve already classified the failure
	}

	// The id guards the cast: only a provider that advertises VoxelCell can be
	// reinterpreted as IVoxelCellService (no RTTI in UE builds).
	IVoxelCellService* Service = static_cast<IVoxelCellService*>(
		Provider->GetService(EAfterfallServiceId::VoxelCell));
	if (!Service)
	{
		Response.Error = AfterfallService::EError::Unavailable;
		return Response;
	}
	return Service->QueryCell(Request);
}
