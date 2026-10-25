// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — exposed-face mesh generation over the existing
// rhombohedral lattice (VoxelBasis). No internal faces; no cube substitution.
#include "Voxel/VoxelChunkTypes.h"
#include "Voxel/VoxelBasis.h"

void VoxelMesher::FMeshBuffer::Reset()
{
	Positions.Reset();
	Indices.Reset();
	Normals.Reset();
	UVs.Reset();
	Colors.Reset();
	QuadCount = 0;
}

namespace
{
	/** Add one quad (4 corners, in loop order) with a forced outward normal. */
	void AddQuad(VoxelMesher::FMeshBuffer& Out,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
		const FVector& Outward, const FLinearColor& Color)
	{
		// The rhombic face corners are given in loop order; fix winding so the
		// geometric normal matches the outward direction (front faces outward).
		FVector N = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
		const bool bFlipped = FVector::DotProduct(N, Outward) < 0.f;
		if (bFlipped)
		{
			N = -N;
		}
		const int32 Base = Out.Positions.Num();
		Out.Positions.Add(P0);
		Out.Positions.Add(bFlipped ? P3 : P1);
		Out.Positions.Add(P2);
		Out.Positions.Add(bFlipped ? P1 : P3);
		for (int32 i = 0; i < 4; ++i)
		{
			Out.Normals.Add(N);
			Out.Colors.Add(Color);
		}
		Out.UVs.Add(FVector2D(0.f, 0.f));
		Out.UVs.Add(FVector2D(1.f, 0.f));
		Out.UVs.Add(FVector2D(1.f, 1.f));
		Out.UVs.Add(FVector2D(0.f, 1.f));
		Out.Indices.Add(Base + 0);
		Out.Indices.Add(Base + 1);
		Out.Indices.Add(Base + 2);
		Out.Indices.Add(Base + 0);
		Out.Indices.Add(Base + 2);
		Out.Indices.Add(Base + 3);
		++Out.QuadCount;
	}
}

void VoxelMesher::BuildChunkMesh(const FVoxelChunkData& Data, FMeshBuffer& Out)
{
	Out.Reset();
	if (!Data.IsValid())
	{
		return;
	}
	// Lattice basis: cell (u,v,w) occupies origin + u*A + v*B + w*C for u,v,w
	// in [0,1). Faces are the parallelograms spanned by the two other vectors.
	const FVector A = FVoxelBasis::A();
	const FVector B = FVoxelBasis::B();
	const FVector C = FVoxelBasis::C();
	const FVector NA = FVector::CrossProduct(B, C).GetSafeNormal();
	const FVector NB = FVector::CrossProduct(C, A).GetSafeNormal();
	const FVector NC = FVector::CrossProduct(A, B).GetSafeNormal();

	for (int32 Z = 0; Z < AfterfallVoxel::ChunkSize; ++Z)
	{
		for (int32 Y = 0; Y < AfterfallVoxel::ChunkSize; ++Y)
		{
			for (int32 X = 0; X < AfterfallVoxel::ChunkSize; ++X)
			{
				const FIntVector Cell(X, Y, Z);
				const uint8 State = Data.Get(Cell);
				if (State == 0)
				{
					continue;
				}
				const FVector O = FVoxelBasis::VoxelToWorld(Cell, FVector::ZeroVector);
				const FLinearColor Color = AfterfallVoxel::MaterialColor(State);

				// +X / -X faces (spanned by B and C).
				if (Data.Get(Cell + FIntVector(1, 0, 0)) == 0)
				{
					const FVector P = O + A;
					AddQuad(Out, P, P + B, P + B + C, P + C, NA, Color);
				}
				if (Data.Get(Cell + FIntVector(-1, 0, 0)) == 0)
				{
					AddQuad(Out, O, O + B, O + B + C, O + C, -NA, Color);
				}
				// +Y / -Y faces (spanned by C and A).
				if (Data.Get(Cell + FIntVector(0, 1, 0)) == 0)
				{
					const FVector P = O + B;
					AddQuad(Out, P, P + C, P + C + A, P + A, NB, Color);
				}
				if (Data.Get(Cell + FIntVector(0, -1, 0)) == 0)
				{
					AddQuad(Out, O, O + C, O + C + A, O + A, -NB, Color);
				}
				// +Z / -Z faces (spanned by A and B).
				if (Data.Get(Cell + FIntVector(0, 0, 1)) == 0)
				{
					const FVector P = O + C;
					AddQuad(Out, P, P + A, P + A + B, P + B, NC, Color);
				}
				if (Data.Get(Cell + FIntVector(0, 0, -1)) == 0)
				{
					AddQuad(Out, O, O + A, O + A + B, O + B, -NC, Color);
				}
			}
		}
	}
}
