#include "Destruction/FractureSolver.h"
#include "Voxel/VoxelBasis.h"

namespace AfterfallFracture
{
	static float MatResistance(EMaterialType M)
	{
		switch (M)
		{
		case EMaterialType::Wood: return 0.3f;
		case EMaterialType::Plaster: return 0.2f;
		case EMaterialType::Stone: return 0.7f;
		case EMaterialType::Concrete: return 0.85f;
		case EMaterialType::Metal: return 0.95f;
		default: return 0.5f;
		}
	}

	FVertexWeakness ComputeVertexWeakness(const FIntVector& Coord, uint32 Seed, EMaterialType Material)
	{
		// Deterministic hash from coord + seed.
		uint32 h = Seed;
		h ^= (uint32)(Coord.X * 73856093);
		h ^= (uint32)(Coord.Y * 19349663);
		h ^= (uint32)(Coord.Z * 83492791);
		h = h * 2654435761u;
		const float r = (float)(h & 0xFFFF) / 65535.0f;
		FVertexWeakness W;
		W.Weakness = r;
		W.Resistance = MatResistance(Material) * (1.0f - 0.3f * r);
		W.bPotential = r > 0.65f;
		return W;
	}

	FFractureResult Solve(const FIntVector& Start, uint32 Seed, EMaterialType Material,
		float InitialEnergy, int32 MaxDepth, const FVector& LoadDir)
	{
		FFractureResult R;
		R.RemainingEnergy = InitialEnergy;
		// neighbor offsets in lattice coords
		const FIntVector Neigh[6] = {
			FIntVector(1,0,0), FIntVector(-1,0,0),
			FIntVector(0,1,0), FIntVector(0,-1,0),
			FIntVector(0,0,1), FIntVector(0,0,-1)
		};
		TSet<FIntVector> Visited;
		TArray<FIntVector> Frontier;
		Frontier.Add(Start);
		Visited.Add(Start);
		R.InitiationPoints.Add(Start);
		const float BaseResist = MatResistance(Material);
		float Energy = InitialEnergy;
		int32 Depth = 0;
		while (Frontier.Num() > 0 && Energy > 0 && Depth < MaxDepth)
		{
			TArray<FIntVector> Next;
			for (const FIntVector& Cur : Frontier)
			{
				for (int i = 0; i < 6; ++i)
				{
					FIntVector N = Cur + Neigh[i];
					if (Visited.Contains(N)) continue;
					Visited.Add(N);
					const FVertexWeakness W = ComputeVertexWeakness(N, Seed, Material);
					// cost: resistance + directional bias
					FVector NWorld = FVoxelBasis::VoxelToWorld(N, FVector::ZeroVector);
					FVector CurWorld = FVoxelBasis::VoxelToWorld(Cur, FVector::ZeroVector);
					FVector Dir = (NWorld - CurWorld).GetSafeNormal();
					float Dot = FVector::DotProduct(Dir, LoadDir.GetSafeNormal());
					float Cost = W.Resistance * BaseResist * 10.0f * (1.0f - 0.3f * FMath::Max(0.0f, Dot));
					if (Cost > Energy) continue;
					Energy -= Cost;
					R.Path.Add(N);
					R.EnergyConsumed += Cost;
					Next.Add(N);
					// branching: weak points branch
					if (W.bPotential && Energy > Cost * 2.0f)
					{
						// add lateral neighbor
						FIntVector L = N + Neigh[(i + 2) % 6];
						if (!Visited.Contains(L))
						{
							Visited.Add(L);
							Next.Add(L);
						}
					}
				}
			}
			Frontier = Next;
			++Depth;
		}
		R.RemainingEnergy = Energy;
		R.Depth = Depth;
		// hole/disconnect heuristic: path surrounds origin
		if (R.Path.Num() > 8) R.bHole = true;
		if (Depth >= MaxDepth && Energy <= 0) R.bDisconnected = true;
		return R;
	}
}
