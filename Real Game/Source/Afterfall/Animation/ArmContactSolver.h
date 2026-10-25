// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Animation (Limb|Contact) — resolves hand targets against world
// geometry: collision-aware target solving, palm orientation, contact
// persistence with hysteresis.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AdaptiveLimbTypes.h"
#include "ArmContactSolver.generated.h"

class USkeletalMeshComponent;
class ACharacter;

/** Result of one contact-solver pass (reuses the limb target struct). */
using FContactSolverResult = FAdaptiveLimbTarget;

/** Collision-aware arm contact solver. Computes the reachable hand target
 *  and persistent surface contact from a desired target and shoulder anchor.
 */
UCLASS(ClassGroup = (Afterfall), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UArmContactSolver : public UActorComponent
{
	GENERATED_BODY()

public:
	UArmContactSolver(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Contact")
	void SetConfig(const FAdaptiveLimbConfig& InConfig);

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Contact")
	void SetShoulder(const FVector& InShoulder);

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Contact")
	void SetDesired(const FVector& InDesired);

	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Contact")
	void SetCurrentState(EAdaptiveLimbState InState);

	/** Run one solve pass. Returns the result. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Contact")
	FAdaptiveLimbTarget Solve(float DeltaTime);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb|Contact")
	FAdaptiveLimbTarget GetResult() const { return LastResult; }

	/** Validate whether a contact at Hand with surface Normal can be acquired. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb|Contact")
	bool CanAcquireContact(const FVector& Hand, const FVector& Normal) const;

private:
	FAdaptiveLimbConfig Config;
	FVector Shoulder = FVector::ZeroVector;
	FVector Desired = FVector::ZeroVector;
	EAdaptiveLimbState CurrentState = EAdaptiveLimbState::Animated;

	FAdaptiveLimbTarget LastResult;
	bool bPersistentContact = false;
	FVector PersistentContactPoint = FVector::ZeroVector;
	FVector PersistentContactNormal = FVector::UpVector;
	float ContactStartTime = 0.0;

	void TraceToDesired(float DeltaTime);
	void MaintainContact(float DeltaTime);
	FVector InterpolateReachable(const FVector& Current, const FVector& Target, float DeltaTime);
};
