// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Animation/ArmContactSolver.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UArmContactSolver::UArmContactSolver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArmContactSolver::SetConfig(const FAdaptiveLimbConfig& InConfig)
{
	Config = InConfig;
}

void UArmContactSolver::SetShoulder(const FVector& InShoulder)
{
	Shoulder = InShoulder;
}

void UArmContactSolver::SetDesired(const FVector& InDesired)
{
	Desired = InDesired;
}

void UArmContactSolver::SetCurrentState(EAdaptiveLimbState InState)
{
	CurrentState = InState;
}

FContactSolverResult UArmContactSolver::Solve(float DeltaTime)
{
	LastResult = FContactSolverResult();
	LastResult.Desired = Desired;

	FVector ToDesired = Desired - Shoulder;
	float DesiredDist = ToDesired.Size();
	LastResult.DesiredDistanceCm = DesiredDist;

	// Clamp desired to max reach.
	FVector ClampedDesired = Desired;
	if (DesiredDist > Config.MaxReachCm && DesiredDist > 0.f)
	{
		ClampedDesired = Shoulder + ToDesired.GetSafeNormal() * Config.MaxReachCm;
		DesiredDist = Config.MaxReachCm;
	}

	// Contact state tracking.
	bool bHeldContact = bPersistentContact &&
		(CurrentState == EAdaptiveLimbState::Contact ||
		 CurrentState == EAdaptiveLimbState::Brace ||
		 CurrentState == EAdaptiveLimbState::Grip);

	if (!bHeldContact)
	{
		TraceToDesired(DeltaTime);
	}
	else
	{
		MaintainContact(DeltaTime);
	}

	// Reachable target: interpolate current reachable toward clamped desired,
	// clamped by max reach.
	FVector CurrentReachable = LastResult.Reachable.IsNearlyZero() ? ClampedDesired : LastResult.Reachable;
	FVector Reachable = InterpolateReachable(CurrentReachable, ClampedDesired, DeltaTime);

	// Clamp reachable to max reach.
	FVector ToReach = Reachable - Shoulder;
	float ReachDist = ToReach.Size();
	if (ReachDist > Config.MaxReachCm && ReachDist > 0.f)
	{
		Reachable = Shoulder + ToReach.GetSafeNormal() * Config.MaxReachCm;
		ReachDist = Config.MaxReachCm;
	}

	LastResult.Reachable = Reachable;
	LastResult.ReachableDistanceCm = ReachDist;

	if (bPersistentContact)
	{
		LastResult.bContacting = true;
		LastResult.ContactPoint = PersistentContactPoint;
		LastResult.ContactNormal = PersistentContactNormal;
		// When contacting, the reachable target is the contact point.
		LastResult.Reachable = PersistentContactPoint;
		LastResult.ReachableDistanceCm = (PersistentContactPoint - Shoulder).Size();
	}

	return LastResult;
}

void UArmContactSolver::TraceToDesired(float DeltaTime)
{
	if (Desired.IsNearlyZero() || Shoulder.IsNearlyZero())
	{
		return;
	}

	FVector ToDesired = Desired - Shoulder;
	float Dist = ToDesired.Size();
	if (Dist <= 0.f)
	{
		return;
	}

	FVector Dir = ToDesired.GetSafeNormal();
	FVector TraceEnd = Shoulder + Dir * FMath::Min(Dist, Config.MaxReachCm);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArmContact), false, nullptr);
	Params.bReturnPhysicalMaterial = false;
	FHitResult Hit;
	bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Shoulder, TraceEnd, FQuat::Identity,
		Config.ContactTraceChannel,
		FCollisionShape::MakeSphere(Config.ContactRadiusCm),
		Params
	);

	if (bHit && Hit.bBlockingHit)
	{
		FVector HitPoint = Hit.ImpactPoint;
		FVector HitNormal = Hit.IsValidBlockingHit() ? Hit.ImpactNormal : FVector::UpVector;

		// Distance from shoulder to hit point.
		float HitDist = (HitPoint - Shoulder).Size();

		// Only consider contact if within acquisition distance of desired.
		bool bNearDesired = (HitPoint - Desired).Size() <= Config.ContactAcquireDistCm;

		if (bNearDesired && HitDist <= Config.MaxReachCm)
		{
			// Acquire contact.
			bPersistentContact = true;
			PersistentContactPoint = HitPoint;
			PersistentContactNormal = HitNormal;
			ContactStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

			LastResult.bContacting = true;
			LastResult.ContactPoint = HitPoint;
			LastResult.ContactNormal = HitNormal;
		}
		else
		{
			// Not close enough to contact; reachable target is the hit point
			// (hand stops at the surface).
			LastResult.Reachable = HitPoint;
			LastResult.ReachableDistanceCm = HitDist;
		}
	}
}

void UArmContactSolver::MaintainContact(float DeltaTime)
{
	if (!bPersistentContact)
	{
		return;
	}

	// Check if the contact point is still valid (surface still there).
	FVector CheckStart = PersistentContactPoint - PersistentContactNormal * 10.f;
	FVector CheckEnd = PersistentContactPoint + PersistentContactNormal * 40.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArmContactMaintain), false, nullptr);
	FHitResult Hit;
	bool bStillHit = GetWorld()->LineTraceSingleByChannel(
		Hit, CheckStart, CheckEnd, Config.ContactTraceChannel, Params
	);

	float DistFromDesired = (PersistentContactPoint - Desired).Size();

	// Release if surface gone OR too far from desired (intent moved away).
	if (!bStillHit || DistFromDesired > Config.ContactReleaseDistCm)
	{
		bPersistentContact = false;
		PersistentContactPoint = FVector::ZeroVector;
		PersistentContactNormal = FVector::UpVector;
		LastResult.bContacting = false;
		return;
	}

	// Maintain contact.
	LastResult.bContacting = true;
	LastResult.ContactPoint = PersistentContactPoint;
	LastResult.ContactNormal = PersistentContactNormal;
}

FVector UArmContactSolver::InterpolateReachable(const FVector& Current, const FVector& Target, float DeltaTime)
{
	if (Current.IsNearlyZero())
	{
		return Target;
	}
	float Speed = Config.ReachSpeedCmS;
	float MaxStep = Speed * DeltaTime;
	FVector Delta = Target - Current;
	float StepDist = Delta.Size();
	if (StepDist <= MaxStep)
	{
		return Target;
	}
	return Current + Delta.GetSafeNormal() * MaxStep;
}

bool UArmContactSolver::CanAcquireContact(const FVector& Hand, const FVector& Normal) const
{
	if (!bPersistentContact && !Hand.IsNearlyZero())
	{
		// Surface must face roughly toward the hand (palm can meet it).
		float FaceDot = FVector::DotProduct(Normal, (Hand - Shoulder).GetSafeNormal());
		return FaceDot > 0.3f; // surface normal points somewhat toward hand
	}
	return bPersistentContact;
}
