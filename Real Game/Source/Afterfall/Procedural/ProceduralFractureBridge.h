// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Adapter that connects the project's impact-energy equation to its existing
// material-aware fracture solver. This plans a fracture; owning geometry code
// remains responsible for applying the result to a supported lattice/voxel body.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/FractureSolver.h"

namespace AfterfallProcedural
{
	struct FImpactRequest
	{
		FIntVector StartCell = FIntVector::ZeroValue;
		uint32 Seed = 1337;
		EMaterialType Material = EMaterialType::Concrete;
		float MassKg = 0.0f;
		/** Incoming velocity in meters per second. */
		FVector ImpactVelocityMps = FVector::ZeroVector;
		int32 MaxDepth = 8;
	};

	struct FImpactPlan
	{
		float ImpactEnergyJ = 0.0f;
		AfterfallFracture::FFractureResult Fracture;
		bool IsValidInput = false;
	};

	/**
	 * Convert an impact's mass and SI velocity to joules using the canonical
	 * ImpactEnergy equation (0.5*v^2, multiplied by mass), then evaluate the
	 * existing severity/material/lattice solver exactly once.
	 *
	 * This function is deterministic and has no world or actor side effects.
	 * It does not fracture arbitrary triangle meshes; the caller must apply the
	 * returned lattice path to a compatible voxel/SDF representation.
	 */
	FImpactPlan BuildImpactPlan(const FImpactRequest& Request);
}
