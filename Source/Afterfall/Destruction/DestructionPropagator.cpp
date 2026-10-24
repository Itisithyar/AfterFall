// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "DestructionPropagator.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

FPropagationState UDestructionPropagator::PropagateDestruction(const FDestructionImpact& Impact, UWorld* World)
{
	FPropagationState State;
	State.RemainingEnergy = Impact.InitialEnergy;
	State.CurrentPosition = Impact.ImpactPoint;
	State.CurrentDirection = Impact.Direction;
	State.PropagationDepth = 0;
	State.bHasExhausted = false;
	State.bHasBeenDenied = false;
	
	float DistanceTraveled = 0.0f;
	const int32 MaxPropagationDepth = 50; // Prevent infinite propagation
	
	while (State.RemainingEnergy > 0.0f && State.PropagationDepth < MaxPropagationDepth && !State.bHasBeenDenied)
	{
		float MaterialResistance = GetMaterialResistance(Impact.Material);
		
		if (ShouldDenyPropagation(State, MaterialResistance))
		{
			State.bHasBeenDenied = true;
			break;
		}
		
		// Calculate damage at current point
		float DistanceMod = CalculateDistanceModifier(DistanceTraveled, Impact.InitialEnergy);
		float Damage = State.RemainingEnergy * DistanceMod * (1.0f - MaterialResistance);
		
		// Apply local damage
		if (Impact.TargetActor)
		{
			ApplyLocalDamage(Impact.TargetActor, State.CurrentPosition, State.CurrentDirection, Damage);
		}
		
		// Calculate energy loss
		float EnergyLoss = CalculateEnergyLoss(DistanceTraveled, Impact.Material);
		State.RemainingEnergy -= EnergyLoss;
		
		if (State.RemainingEnergy <= 0.0f)
		{
			State.bHasExhausted = true;
			break;
		}
		
		// Advance propagation
		float StepSize = 50.0f; // 50cm steps
		State.CurrentPosition += State.CurrentDirection * StepSize;
		DistanceTraveled += StepSize;
		State.PropagationDepth++;
		State.PropagationPath.Add(State.CurrentPosition);
		
		// Add some directional variation for irregular holes
		if (State.PropagationDepth % 3 == 0)
		{
			FVector RandomDir = FMath::VRandCone(State.CurrentDirection, FMath::DegreesToRadians(15.0f));
			State.CurrentDirection = RandomDir;
		}
	}
	
	return State;
}

float UDestructionPropagator::GetMaterialResistance(EMaterialType Material)
{
	switch (Material)
	{
	case EMaterialType::Wood: return 0.3f;
	case EMaterialType::Plaster: return 0.2f;
	case EMaterialType::Stone: return 0.6f;
	case EMaterialType::Concrete: return 0.8f;
	case EMaterialType::Metal: return 0.9f;
	default: return 0.3f;
	}
}

float UDestructionPropagator::CalculateDistanceModifier(float Distance, float InitialEnergy)
{
	// Distance-dependent damage - not simple linear falloff
	float DistanceInMeters = Distance / 100.0f;
	if (DistanceInMeters < 1.0f)
	{
		return 1.0f - (DistanceInMeters * 0.2f); // Slight reduction up close
	}
	else if (DistanceInMeters < 3.0f)
	{
		return 0.8f - ((DistanceInMeters - 1.0f) * 0.3f);
	}
	else
	{
		return FMath::Max(0.1f, 0.5f - (DistanceInMeters * 0.1f));
	}
}

FVector UDestructionPropagator::FindNextBreakingPoint(const FPropagationState& State, const FStructuralReferenceGrid& Grid, UWorld* World)
{
	if (Grid.BreakingPoints.Num() == 0) return State.CurrentPosition;
	
	FVector ClosestPoint = Grid.BreakingPoints[0];
	float ClosestDist = FVector::Dist(State.CurrentPosition, ClosestPoint);
	
	for (int32 i = 1; i < Grid.BreakingPoints.Num(); ++i)
	{
		float Dist = FVector::Dist(State.CurrentPosition, Grid.BreakingPoints[i]);
		if (Dist < ClosestDist)
		{
			ClosestDist = Dist;
			ClosestPoint = Grid.BreakingPoints[i];
		}
	}
	
	return ClosestPoint;
}

void UDestructionPropagator::ApplyLocalDamage(AActor* Target, const FVector& ImpactPoint, const FVector& Direction, float Damage)
{
	if (!Target) return;
	
	// Apply local damage - would interact with mesh destruction system
	UE_LOG(LogTemp, Log, TEXT("Applying local damage %.2f at %s"), Damage, *ImpactPoint.ToString());
}

bool UDestructionPropagator::ShouldDenyPropagation(const FPropagationState& State, float MaterialResistance)
{
	// Deny if energy too low or resistance too high
	if (State.RemainingEnergy < 10.0f && MaterialResistance > 0.7f)
	{
		return true;
	}
	return false;
}

float UDestructionPropagator::CalculateEnergyLoss(float Distance, EMaterialType Material)
{
	float BaseLoss = 5.0f + (Distance / 100.0f) * 2.0f;
	float MaterialFactor = GetMaterialResistance(Material) * 10.0f;
	return BaseLoss + MaterialFactor;
}

bool UDestructionPropagator::IsValidBreakpoint(const FVector& Point, UWorld* World)
{
	return true;
}
