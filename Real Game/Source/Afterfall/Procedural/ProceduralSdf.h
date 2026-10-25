// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — shared primitive SDF evaluation.
//
// Single home for the project's primitive signed-distance equations
// (sphere / box / cylinder / capsule) plus the deterministic seed-derived
// noise term and the bounded domain-warp deform used before evaluation.
// AShapeGenerator32::EvaluateSDF and the procedural-object pipeline both
// delegate here, so the equations cannot diverge. No global RNG: the noise
// offset is derived from the object's seed by the caller.
#pragma once

#include "CoreMinimal.h"
#include "Procedural/ShapeGenerator32.h"

namespace AfterfallSdf
{
	/** Primitive shape parameters (mirrors AShapeGenerator32 fields). */
	struct FSdfParams
	{
		EShapeType32 Shape = EShapeType32::Sphere;
		FVector HalfExtents = FVector(60.0f, 60.0f, 60.0f);
		float Radius = 120.0f;
		float Height = 60.0f;
		float NoiseStrength = 0.0f;
		/** Deterministic seed-derived offset (FRandomStream from seed). */
		FVector NoiseOffset = FVector::ZeroVector;
	};

	/** One bounded domain warp: Q' = Q + Strength * smoothstep(1 - d/R) * Dir. */
	struct FWarpOp
	{
		FVector Center = FVector::ZeroVector;
		FVector Direction = FVector(1.0f, -1.0f, 0.0f);
		float Radius = 50.0f;
		float Strength = 0.0f;
	};

	/** Smoothstep falloff weight in [0,1]; 0 outside the radius. */
	inline float WarpWeight(const FVector& Q, const FVector& Center, float Radius)
	{
		if (Radius <= 0.0f)
		{
			return 0.0f;
		}
		const float T = FMath::Clamp(FVector::Dist(Q, Center) / Radius, 0.0f, 1.0f);
		return 1.0f - T * T * (3.0f - 2.0f * T);
	}

	inline FVector ApplyWarp(const FVector& Q, const FWarpOp& Warp)
	{
		if (Warp.Strength == 0.0f || Warp.Radius <= 0.0f)
		{
			return Q;
		}
		return Q + Warp.Strength * WarpWeight(Q, Warp.Center, Warp.Radius) * Warp.Direction;
	}

	/** Primitive SDF only (no warp, no noise). Units: world units (cm). */
	inline float EvalPrimitive(const FSdfParams& P, const FVector& Qp)
	{
		switch (P.Shape)
		{
		case EShapeType32::Sphere:
			return Qp.Size() - P.Radius;
		case EShapeType32::Box:
		{
			const FVector A = Qp.GetAbs() - P.HalfExtents;
			return FMath::Max3(A.X, A.Y, A.Z);
		}
		case EShapeType32::Cylinder:
		{
			const float Radial = FVector2D(Qp.X, Qp.Y).Size() - P.Radius;
			const float Axial = FMath::Abs(Qp.Z) - P.Height;
			return FMath::Max(Radial, Axial);
		}
		case EShapeType32::Capsule:
		{
			const float ClampedZ = FMath::Clamp(Qp.Z, -P.Height, P.Height);
			return FVector::Dist(Qp, FVector(0.0f, 0.0f, ClampedZ)) - P.Radius;
		}
		default:
			return Qp.Size() - P.Radius;
		}
	}

	/**
	 * Full field evaluation: ordered warps applied to the sample point,
	 * then the primitive, then deterministic noise. Warps apply in array
	 * order; callers deduplicate by operation ID so replays are idempotent.
	 */
	inline float EvalField(const FSdfParams& P, const FVector& Q, const TArray<FWarpOp>& Warps)
	{
		FVector Qp = Q;
		for (const FWarpOp& W : Warps)
		{
			Qp = ApplyWarp(Qp, W);
		}
		float F = EvalPrimitive(P, Qp);
		if (P.NoiseStrength != 0.0f)
		{
			// Fixed 0.02 frequency; seed enters via NoiseOffset (UE 5.8
			// PerlinNoise3D takes position only).
			F += P.NoiseStrength * FMath::PerlinNoise3D(Q * 0.02f + P.NoiseOffset);
		}
		return F;
	}

	/** Deterministic noise offset from a seed (no global RNG state). */
	inline FVector MakeNoiseOffset(int32 Seed)
	{
		FRandomStream RS(Seed);
		return FVector(RS.FRandRange(-512.0f, 512.0f),
			RS.FRandRange(-512.0f, 512.0f), RS.FRandRange(-512.0f, 512.0f));
	}
}
