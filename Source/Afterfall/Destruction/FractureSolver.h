#pragma once
#include "CoreMinimal.h"
#include "DestructionTypes.h"

namespace AfterfallFracture
{
	// Stable identity for a rhombohedral lattice vertex.
	struct FVertexId
	{
		FIntVector Coord;
		bool operator==(const FVertexId& O) const { return Coord == O.Coord; }
	};

	// Deterministic weak-point attributes derived from vertex coord + seed.
	struct FVertexWeakness
	{
		float Weakness = 0.5f; // 0..1
		float Resistance = 0.5f;
		bool bPotential = false;
	};

	FVertexWeakness ComputeVertexWeakness(const FIntVector& Coord, uint32 Seed, EMaterialType Material);

	// Fracture propagation result.
	struct FFractureResult
	{
		TArray<FIntVector> InitiationPoints;
		TArray<FIntVector> Path;
		float EnergyConsumed = 0.0f;
		float RemainingEnergy = 0.0f;
		bool bHole = false;
		bool bDisconnected = false;
		int32 Depth = 0;
	};

	// Solver operating on lattice coords; energy budget.
	FFractureResult Solve(const FIntVector& Start, uint32 Seed, EMaterialType Material,
		float InitialEnergy, int32 MaxDepth, const FVector& LoadDir);
}
