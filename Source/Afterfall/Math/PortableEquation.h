#pragma once
#include "CoreMinimal.h"
#include "PortableEquation.generated.h"

// Portable, engine-independent equation definition.
USTRUCT()
struct FPortableEquation
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() float A = 0.f, B = 0.f, C = 0.f;
};

namespace Afterfall::Portable
{
	/**
	 * Evaluate A*x^2 + B*x + C. This is the ONE evaluation path for a
	 * portable quadratic: gameplay (the voxel fracture severity gate), the
	 * automation tests and the exported JSON all read the same coefficients,
	 * so the game and the engine-independent copy cannot drift apart.
	 */
	inline float Eval(const FPortableEquation& E, float X)
	{
		if (!FMath::IsFinite(X))
		{
			return 0.f;
		}
		return E.A * X * X + E.B * X + E.C;
	}

	/** Build the quadratic definition used by a curve. */
	inline FPortableEquation MakeQuadratic(float A, float B = 0.f, float C = 0.f)
	{
		FPortableEquation E;
		E.A = A;
		E.B = B;
		E.C = C;
		return E;
	}

	inline FString ToJson(const FPortableEquation& E)
	{
		return FString::Printf(TEXT("{\"version\":%d,\"a\":%.9g,\"b\":%.9g,\"c\":%.9g}"), E.Version, E.A, E.B, E.C);
	}
}
