// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Subsystems) — service request/response layer.
//
// Two things live here:
//   1. The Stage B *typed* path (FVoxelCellRequest/Response + IVoxelCellService +
//      QueryVoxelCell): one real integration, used by gameplay code and tests.
//   2. The earlier untyped envelope (FSubsystemRequest/ISubsystemService). It is
//      kept for compatibility but has no callers; new work must use the typed
//      path so failures are values, not void* conventions.
#pragma once

#include "CoreMinimal.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"

namespace AfterfallService
{
	/** Why a service request could not be served. Never a silent null. */
	enum class EError : uint8
	{
		None,
		Unavailable,   // no provider registered under that name
		NotReady,      // provider exists but is not Ready (initializing/stopping)
		Failed,        // provider reported a failed initialization
		ShuttingDown,  // registry is shutting down; do not start new work
		InvalidRequest // caller payload cannot be interpreted
	};

	AFTERFALL_API const TCHAR* ErrorText(EError Error);
}

// ------------------------------------------------------- legacy (unused) ---

struct FSubsystemRequest
{
	FName RequestType;
	void* Payload = nullptr;
};

struct FSubsystemResponse
{
	bool bSuccess = false;
	FString Error;
	void* Result = nullptr;
};

class ISubsystemService
{
public:
	virtual ~ISubsystemService() = default;
	virtual FSubsystemResponse HandleRequest(const FSubsystemRequest& Req) = 0;
};

// --------------------------------------------------------- typed: voxel ---

/** World point -> voxel lattice cell, interpreted through FVoxelBasis. */
struct FVoxelCellRequest
{
	FVector WorldPoint = FVector::ZeroVector;
	/** Lattice origin (chunk (0,0,0) in world space). */
	FVector Origin = FVector::ZeroVector;
};

struct FVoxelCellResponse
{
	AfterfallService::EError Error = AfterfallService::EError::Unavailable;
	/** Valid only when Error == None. */
	FIntVector Cell = FIntVector::ZeroValue;

	bool IsSuccess() const { return Error == AfterfallService::EError::None; }
};

/** Implemented by providers of the voxel-cell service. */
class IVoxelCellService
{
public:
	virtual ~IVoxelCellService() = default;
	/** Never throws/blocks; reports refusal through the response Error. */
	virtual FVoxelCellResponse QueryCell(const FVoxelCellRequest& Request) const = 0;
};

/**
 * Resolves the voxel provider through the registry and answers the request.
 * Returns Error (never a bare null) for: null registry / registry shutting down /
 * provider missing / provider not ready / provider failed / malformed payload.
 * World-thread only: the registry and providers are world-scoped UObjects.
 */
AFTERFALL_API FVoxelCellResponse QueryVoxelCell(const UAfterfallSubsystemRegistry* Registry,
	const FVoxelCellRequest& Request, const TCHAR* ProviderName = TEXT("Voxel"));
