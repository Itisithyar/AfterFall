// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — reusable procedural-object pipeline.
//
// Explicit stages: describe -> scalar field -> deform (domain warp, before
// fracture) -> fracture plan (existing solver rules) -> carve -> extract
// mesh + collision. All functions are pure and deterministic: same inputs
// give identical outputs on server and clients. No global RNG; every random
// draw derives from stable object/event IDs. Unreal objects (components,
// physics state) are touched only by the owning actor, on the game thread.
#pragma once

#include "CoreMinimal.h"
#include "Procedural/ProceduralObjectDesc.h"
#include "Procedural/ProceduralFractureBridge.h"

namespace AfterfallProcObj
{
	/** Clamp bounds for lattice resolution (samples per axis). */
	inline int32 ClampGridSize(int32 N) { return FMath::Clamp(N, 2, 64); }

	/** Field sample count for N (N^3); N=64 -> 262144 floats (~1 MiB). */
	inline int64 FieldSampleCount(int32 N) { const int64 C = ClampGridSize(N); return C * C * C; }

	/** Raw mesh buffers produced by extraction. */
	struct FProcMeshBuffers
	{
		TArray<FVector> Positions;
		TArray<int32> Indices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		int32 SkippedTris = 0;
		int32 BadIndices = 0;
		int32 DegenerateTris = 0;
		FBox Bounds = FBox(ForceInit);
	};

	/**
	 * Evaluate the scalar field for a description plus ordered deform ops.
	 * Deform ops apply BEFORE the shape/noise evaluation (domain warp).
	 * Duplicate OpIds are skipped by the caller (actor); this function
	 * applies the array in order. OutField has N^3 entries or is empty
	 * when inputs are invalid (CellSize <= 0).
	 */
	void EvaluateField(const FProceduralObjectDesc& Desc,
		const TArray<FProceduralDeformOp>& DeformOps, TArray<float>& OutField);

	/**
	 * Extract mesh buffers from a field. MC path uses the shared
	 * AfterfallMC tables with the same inside convention (F <= IsoLevel),
	 * gradient normals, tri decode, and degenerate guards as the existing
	 * shape generator; blocky path emits outward-facing quads per exposed
	 * face with the same winding fix.
	 */
	void ExtractMesh(const TArray<float>& Field, const FProceduralObjectDesc& Desc,
		FProcMeshBuffers& OutBuffers);

	/** Index-range / finiteness / winding validation shared by tests + actor. */
	bool ValidateBuffers(const FProcMeshBuffers& Buf, FString* OutReason = nullptr);

	/** Deterministic 64-bit hash of a field (test comparisons, no crypto). */
	uint64 HashField(const TArray<float>& Field);

	/**
	 * Plan a fracture event through the EXISTING solver rules
	 * (severity gate + material capacity + lattice propagation). Pure:
	 * returns bInitiated=false for invalid input or rejected impacts.
	 * The lattice StartCell is derived from LocalPos in field space.
	 */
	AfterfallProcedural::FImpactPlan PlanFractureEvent(
		const FProceduralObjectDesc& Desc, const FVector& LocalPos,
		const FVector& LocalDir, float EnergyJ, EMaterialType Material, uint32 Seed);

	/**
	 * Carve a planned (initiated) fracture into the field: convert each
	 * lattice path cell to field space and clear a bounded sphere around
	 * it. Returns carved sample count. Uninitiated plans carve nothing.
	 * Carve radius scales with CellSize so detail loss is explicit.
	 */
	int32 ApplyFractureCarve(TArray<float>& Field, const FProceduralObjectDesc& Desc,
		const AfterfallProcedural::FImpactPlan& Plan);

	/** Validate a deform op (finite, in-range). Pure; actor + tests share it. */
	bool IsDeformOpValid(const FProceduralObjectDesc& Desc, const FProceduralDeformOp& Op);

	/** Validate a fracture request (finite, in-range, positive energy). */
	bool IsFractureRequestValid(const FProceduralObjectDesc& Desc, const FVector& LocalPos,
		const FVector& LocalDir, float EnergyJ);
}
