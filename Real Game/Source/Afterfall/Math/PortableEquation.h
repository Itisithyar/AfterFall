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
	inline FString ToJson(const FPortableEquation& E)
	{
		return FString::Printf(TEXT("{\"version\":%d,\"a\":%.9g,\"b\":%.9g,\"c\":%.9g}"), E.Version, E.A, E.B, E.C);
	}

	/** Evaluate y = A*x^2 + B*x + C (Horner form, single evaluation point). */
	inline float Eval(const FPortableEquation& E, float X)
	{
		return (E.A * X + E.B) * X + E.C;
	}

	/** First-order curve factory (A = 0): y = Slope*x + Intercept. */
	inline FPortableEquation Linear(float Slope, float Intercept, int32 Version = 1)
	{
		FPortableEquation E;
		E.Version = Version;
		E.A = 0.f;
		E.B = Slope;
		E.C = Intercept;
		return E;
	}
}