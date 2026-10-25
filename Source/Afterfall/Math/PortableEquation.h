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
}
