// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Animation (Limb) — owns the limb's runtime state, config, targets,
// limits, and state-machine transitions. Coordinates the joint anchor and
// contact solver each tick. No physics simulation runs by default.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AdaptiveLimbTypes.h"
#include "AdaptiveLimbComponent.generated.h"

class UChestAnchoredJointComponent;
class UArmContactSolver;
class USkeletalMeshComponent;
class UCameraComponent;

/** Runtime result of one limb update. */
USTRUCT(BlueprintType)
struct FAdaptiveLimbUpdateResult
{
	GENERATED_BODY()

	FAdaptiveLimbUpdateResult() = default;

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Update")
	EAdaptiveLimbState State = EAdaptiveLimbState::Animated;

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Update")
	FAdaptiveLimbPose Pose;

	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Update")
	FAdaptiveLimbTarget Target;

	/** True when the solver changed the target this frame (contact acquired/released). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Update")
	bool bTargetChanged = false;
};

/** Chest-relative adaptive limb component. Drives one arm (left or right)
 *  from camera intent through reach limits, joint limits, and world contact.
 */
UCLASS(ClassGroup = (Afterfall), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UAdaptiveLimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAdaptiveLimbComponent(const FObjectInitializer& ObjectInitializer);

	/** Tunable configuration (reach, joints, brace, contact, debug). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb")
	FAdaptiveLimbConfig Config;

	/** Current state (Animated / Reach / Contact / ...). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|State")
	EAdaptiveLimbState State = EAdaptiveLimbState::Animated;

	/** Latest target computation (desired / reachable / final / contact). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|State")
	FAdaptiveLimbTarget Target;

	/** Latest solved pose exposed to animation / Control Rig / IK. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|State")
	FAdaptiveLimbPose Pose;

	/** Joint anchor component (subobject). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|Subobjects", meta = (AllowPrivateAccess = "true"))
	UChestAnchoredJointComponent* Joint = nullptr;

	/** Contact solver component (subobject). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|Subobjects", meta = (AllowPrivateAccess = "true"))
	UArmContactSolver* Solver = nullptr;

	// -------- Public API --------

	/** Set the skeletal mesh used to locate the shoulder anchor bone/socket. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void SetSkeletalMesh(USkeletalMeshComponent* InMesh);

	/** Set the first-person camera used to derive reach intent. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void SetCamera(UCameraComponent* InCamera);

	/** Request a reach toward a world-space target (cm). Called from gameplay
	 *  input or animation intent. The solver clamps it to reach/contact limits. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void SetReachTarget(const FVector& WorldTarget);

	/** Clear any active reach target and return to animated/idle pose. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void ClearReachTarget();

	/** Begin contact with the current reachable target (plant the hand). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void AcquireContact();

	/** Release contact and return to free reaching. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void ReleaseContact();

	/** True when the hand is maintaining surface contact. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb")
	bool IsContacting() const;

	/** Get the latest solved pose (for Animation Blueprint / Control Rig). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb")
	FAdaptiveLimbPose GetLimbPose() const;

	/** Get the latest target computation. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb")
	FAdaptiveLimbTarget GetLimbTarget() const;

	/** Manually drive the state machine (used by impact/fall/grip code). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	void SetState(EAdaptiveLimbState NewState);

	/** Pure helper: compute an anatomically-plausible elbow position between
	 *  shoulder and wrist, bent outward by Config.ElbowPreferredBendDeg.
	 *  Unit-tested; no engine state required. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb")
	static FVector ComputeElbowPosition(const FVector& Shoulder, const FVector& Wrist,
		const FAdaptiveLimbConfig& InConfig);

	/** Tick the limb (called by the owning character each frame). */
	void UpdateLimb(float DeltaTime);

	/** Debug draw (called when Config.bDebugDraw). */
	void DebugDraw(const FVector& CameraLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	USkeletalMeshComponent* Mesh = nullptr;
	UCameraComponent* Camera = nullptr;

	FVector DesiredIntent = FVector::ZeroVector; // raw camera intent (cm)
	FVector SmoothedReachable = FVector::ZeroVector;
	FVector SmoothedElbow = FVector::ZeroVector;
	float BlendWeight = 0.f;
	double LastStateChange = 0.0;

	bool bHasValidAnchor = false;
	FVector CachedShoulder = FVector::ZeroVector;

	// -------- Internal pipeline --------
	void ComputeShoulderAnchor();
	void ComputeDesiredTarget();
	void SolveReachableTarget();
	void SolvePose();
	void UpdateStateMachine(float DeltaTime);
	void UpdateBlend(float DeltaTime);
	FVector ComputeElbow(const FVector& Shoulder, const FVector& Wrist) const;
	bool CanAcquireContact(const FVector& Hand, const FVector& Normal) const;
};
