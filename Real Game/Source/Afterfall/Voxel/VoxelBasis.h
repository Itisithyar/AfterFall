#pragma once
#include "CoreMinimal.h"

struct FVoxelBasis
{
	static constexpr float S = 3.06f;
	// a = (s, 0, 0)
	// b = (s/2, s*sqrt(3)/2, 0)
	// c = (s/2, s/(2*sqrt(3)), s*sqrt(2/3))
	static FVector A() { return FVector(S, 0, 0); }
	static FVector B() { return FVector(S/2.f, S*FMath::Sqrt(3.f)/2.f, 0); }
	static FVector C() { return FVector(S/2.f, S/(2.f*FMath::Sqrt(3.f)), S*FMath::Sqrt(2.f/3.f)); }
	// World to lattice: p = origin + u*a + v*b + w*c
	// u,v,w = inverse basis * (p - origin)
	static FIntVector WorldToVoxel(const FVector& P, const FVector& Origin);
	static FVector VoxelToWorld(const FIntVector& V, const FVector& Origin);
	static FIntVector VoxelToChunk(const FIntVector& V);
	static FIntVector ChunkLocal(const FIntVector& V);
};

inline FIntVector FVoxelBasis::WorldToVoxel(const FVector& P, const FVector& Origin)
{
	FVector d = P - Origin;
	// Inverse of basis matrix (a,b,c as columns)
	// Compute using cross products
	FVector a = A(), b = B(), c = C();
	float det = FVector::DotProduct(a, FVector::CrossProduct(b, c));
	FVector invA = FVector::CrossProduct(b, c) / det;
	FVector invB = FVector::CrossProduct(c, a) / det;
	FVector invC = FVector::CrossProduct(a, b) / det;
	float u = FVector::DotProduct(d, invA);
	float v = FVector::DotProduct(d, invB);
	float w = FVector::DotProduct(d, invC);
	return FIntVector(FMath::FloorToInt(u), FMath::FloorToInt(v), FMath::FloorToInt(w));
}

inline FVector FVoxelBasis::VoxelToWorld(const FIntVector& V, const FVector& Origin)
{
	FVector a = A(), b = B(), c = C();
	return Origin + a * V.X + b * V.Y + c * V.Z;
}

inline FIntVector FVoxelBasis::VoxelToChunk(const FIntVector& V)
{
	return FIntVector(FMath::FloorToInt((float)V.X / 32.f), FMath::FloorToInt((float)V.Y / 32.f), FMath::FloorToInt((float)V.Z / 32.f));
}

inline FIntVector FVoxelBasis::ChunkLocal(const FIntVector& V)
{
	FIntVector c = VoxelToChunk(V);
	return FIntVector(V.X - c.X * 32, V.Y - c.Y * 32, V.Z - c.Z * 32);
}