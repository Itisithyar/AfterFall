// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Destruction - Propagation system for material destruction
//
// OWNERSHIP NOTE (Kilo, verified by repo search): this class is NOT the active
// gameplay damage path — nothing outside its own test calls it. The authoritative
// engine is UDestructionPropagationService (deterministic, directional, wired to
// AStructuralActor + persistence). Keep this file for its owner's experiments;
// do NOT route gameplay impacts through it (it uses random jitter, radial
// nearest-point selection, and a log-only ApplyLocalDamage stub).
#pragma once

#include "CoreMinimal.h"
#include "DestructionTypes.h"
#include "UObject/NoExportTypes.h"
#include "DestructionPropagator.generated.h"

UCLASS()
class AFTERFALL_API UDestructionPropagator : public UObject
{
	GENERATED_BODY()

public:
	// Main propagation entry point
	static FPropagationState PropagateDestruction(const FDestructionImpact& Impact, UWorld* World);

	// Calculate material resistance
	static float GetMaterialResistance(EMaterialType Material);

	// Calculate distance-based damage modifier
	static float CalculateDistanceModifier(float Distance, float InitialEnergy);

	// Find next reachable breaking point
	static FVector FindNextBreakingPoint(const FPropagationState& State, const FStructuralReferenceGrid& Grid, UWorld* World);

	// Apply damage to geometry
	static void ApplyLocalDamage(AActor* Target, const FVector& ImpactPoint, const FVector& Direction, float Damage);

	// Check if propagation should be denied
	static bool ShouldDenyPropagation(const FPropagationState& State, float MaterialResistance);

protected:
	static float CalculateEnergyLoss(float Distance, EMaterialType Material);
	static bool IsValidBreakpoint(const FVector& Point, UWorld* World);
};
