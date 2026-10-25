// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Animation (Limb) — chest-relative arm target, joint limits, contact
// solving configuration. Pure data + enums; no engine simulation dependencies.
#pragma once

#include "CoreMinimal.h"
#include "AdaptiveLimbTypes.generated.h"

/** Conceptual limb state machine. Drives animation/physics blending. */
UENUM(BlueprintType)
enum class EAdaptiveLimbState : uint8
{
	Animated   UMETA(DisplayName = "Animated"),
	Reach      UMETA(DisplayName = "Reach"),
	Contact    UMETA(DisplayName = "Contact"),
	Brace      UMETA(DisplayName = "Brace"),
	Grip       UMETA(DisplayName = "Grip"),
	Impact     UMETA(DisplayName = "Impact"),
	Yield      UMETA(DisplayName = "Yield"),
	Recovery   UMETA(DisplayName = "Recovery"),
	Ragdoll    UMETA(DisplayName = "Ragdoll"),
	MAX        UMETA(Hidden)
};

/** Which arm this component drives. Left/right share one architecture. */
UENUM(BlueprintType)
enum class EAdaptiveLimbSide : uint8
{
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	MAX   UMETA(Hidden)
};

/** One runtime target for the hand/wrist. Computed from camera intent, then
 *  constrained by reach limits, joint limits, and world contact. */
USTRUCT(BlueprintType)
struct FAdaptiveLimbTarget
{
	GENERATED_BODY()

	FAdaptiveLimbTarget() = default;

	/** Camera/intent desired world position. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Target")
	FVector Desired = FVector::ZeroVector;

	/** Solver-constrained reachable world position (what the limb actually aims at). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Target")
	FVector Reachable = FVector::ZeroVector;

	/** Final applied hand/wrist world position. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Target")
	FVector Final = FVector::ZeroVector;

	/** Surface normal at contact (zero when not contacting). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Contact")
	FVector ContactNormal = FVector::UpVector;

	/** World-space contact point (zero when not contacting). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Contact")
	FVector ContactPoint = FVector::ZeroVector;

	/** True when the hand is maintaining contact with a surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Contact")
	bool bContacting = false;

	/** Distance from shoulder anchor to desired target (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Target")
	float DesiredDistanceCm = 0.f;

	/** Distance from shoulder anchor to reachable target (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Target")
	float ReachableDistanceCm = 0.f;

	/** Valid when it carries an aim point OR an active contact (a contact
	 *  target may have no desired/reachable point of its own). */
	bool IsValid() const
	{
		return !Desired.IsZero() || !Reachable.IsZero() || bContacting;
	}
};

/** Solved pose data exposed to the Animation Blueprint / Control Rig / IK. */
USTRUCT(BlueprintType)
struct FAdaptiveLimbPose
{
	GENERATED_BODY()

	FAdaptiveLimbPose() = default;

	/** Shoulder anchor world location (chest-relative). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	FVector Shoulder = FVector::ZeroVector;

	/** Elbow world location (anatomically plausible). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	FVector Elbow = FVector::ZeroVector;

	/** Wrist/hand world location. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	FVector Wrist = FVector::ZeroVector;

	/** Wrist rotation (palm orientation). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	FRotator WristRotation = FRotator::ZeroRotator;

	/** Shoulder pitch/yaw relative to chest-forward (degrees). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	FVector2D ShoulderAnglesDeg = FVector2D::ZeroVector;

	/** Elbow bend angle (degrees, 0 = straight). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	float ElbowBendDeg = 0.f;

	/** Current limb state. */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	EAdaptiveLimbState State = EAdaptiveLimbState::Animated;

	/** Blend weight from Animated -> Reach/Contact (0..1). */
	UPROPERTY(BlueprintReadOnly, Category = "Afterfall|Limb|Pose")
	float BlendWeight = 0.f;
};

/** Tunable configuration for one adaptive limb. All units cm / cm/s / degrees. */
USTRUCT(BlueprintType)
struct FAdaptiveLimbConfig
{
	GENERATED_BODY()

	FAdaptiveLimbConfig() = default;

	/** Maximum reach distance from shoulder anchor (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Config", Meta = (ClampMin = "50", UIMin = "50"))
	float MaxReachCm = 120.f;

	/** Chest-relative shoulder anchor offset (cm). X forward, Y right, Z up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Config")
	FVector ShoulderOffsetCm = FVector(20.f, 0.f, 10.f);

	/** Preferred elbow bend angle (degrees). 0 = straight arm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Config", Meta = (ClampMin = "0", UIMin = "0", ClampMax = "160", UIMax = "160"))
	float ElbowPreferredBendDeg = 45.f;

	/** How fast the reach target interpolates (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Config", Meta = (ClampMin = "10", UIMin = "10"))
	float ReachSpeedCmS = 600.f;

	/** Contact trace channel (default visibility). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Contact")
	TEnumAsByte<ECollisionChannel> ContactTraceChannel = ECC_Visibility;

	/** Radius of the contact sweep capsule (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Contact", Meta = (ClampMin = "1", UIMin = "1"))
	float ContactRadiusCm = 12.f;

	/** Contact acquisition hysteresis: target must be within this distance (cm) to acquire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Contact", Meta = (ClampMin = "1", UIMin = "1"))
	float ContactAcquireDistCm = 8.f;

	/** Contact release hysteresis: target must exceed this distance (cm) to release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Contact", Meta = (ClampMin = "1", UIMin = "1"))
	float ContactReleaseDistCm = 12.f;

	/** Max force the limb can brace against (kgf, converted internally). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Brace", Meta = (ClampMin = "0", UIMin = "0"))
	float BraceMaxForceKg = 15.f;

	/** Shoulder pitch limit relative to chest-forward (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joints", Meta = (ClampMin = "0", UIMin = "0"))
	float ShoulderPitchLimitDeg = 80.f;

	/** Shoulder yaw limit relative to chest-forward (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joints", Meta = (ClampMin = "0", UIMin = "0"))
	float ShoulderYawLimitDeg = 70.f;

	/** Elbow max bend (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joints", Meta = (ClampMin = "0", UIMin = "0", ClampMax = "160", UIMax = "160"))
	float ElbowMaxBendDeg = 150.f;

	/** Enable debug line/point drawing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Debug")
	bool bDebugDraw = false;

	/** Bone/socket name on the skeletal mesh to use as the shoulder anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Anchor")
	FName ShoulderSocketName = TEXT("spine_01");

	/** Whether this is the left or right arm (mirrors elbow preference). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Config")
	EAdaptiveLimbSide LimbSide = EAdaptiveLimbSide::Right;
};
