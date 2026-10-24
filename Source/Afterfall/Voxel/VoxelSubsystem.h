// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — world-scoped voxel service.
//
// Stage B: this is the real provider behind the typed service path. It exposes
// the EXISTING FVoxelBasis lattice conversion (it is not a voxel world) and
// reports its lifecycle honestly: requests are only served while Ready, and a
// stopped/failed provider returns an error value instead of a cell.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Subsystem/SubsystemService.h"
#include "Voxel/VoxelBasis.h"
#include "VoxelSubsystem.generated.h"

UCLASS()
class AFTERFALL_API UVoxelSubsystem : public UWorldSubsystem,
	public IAfterfallSubsystem,
	public IVoxelCellService
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// IAfterfallSubsystem
	virtual ESubsystemState GetState() const override { return State; }
	virtual bool Initialize() override;
	virtual void Shutdown() override;
	virtual const TCHAR* GetName() const override { return TEXT("Voxel"); }
	virtual void* GetService(EAfterfallServiceId Id) override;

	// IVoxelCellService
	virtual FVoxelCellResponse QueryCell(const FVoxelCellRequest& Request) const override;

	/** Lattice cell for a world point, ignoring lifecycle (pure basis math). */
	FIntVector WorldToVoxel(const FVector& P, const FVector& Origin) const
	{
		return FVoxelBasis::WorldToVoxel(P, Origin);
	}

	/** Number of requests served (instrumentation/tests). */
	int32 GetServedCount() const { return ServedCount; }

private:
	ESubsystemState State = ESubsystemState::Uninitialized;
	int32 ServedCount = 0;
	/** Applies a transition through the shared legality table; false = illegal. */
	bool SetState(ESubsystemState NewState);
};
