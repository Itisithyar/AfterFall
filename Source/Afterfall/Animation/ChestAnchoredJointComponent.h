// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Animation (Limb|Joint) — establishes the body-relative shoulder anchor
// from the skeletal mesh (socket or bone) and evaluates reach constraints.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AdaptiveLimbTypes.h"
#include "ChestAnchoredJointComponent.generated.h"

class USkeletalMeshComponent;
class ACharacter;

/** Chest-relative shoulder anchor with reach/joint constraint evaluation. */
UCLASS(ClassGroup = (Afterfall), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UChestAnchoredJointComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChestAnchoredJointComponent(const FObjectInitializer& ObjectInitializer);

	/** Skeletal mesh used to locate the shoulder bone/socket. */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Joint")
	void SetSkeletalMesh(USkeletalMeshComponent* InMesh);

	/** Owning character (for fallback anchor computation). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Joint")
	void SetOwnerCharacter(ACharacter* InChar);

	/** Bone/socket name used as the shoulder anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joint")
	FName AnchorSocketName = TEXT("spine_01");

	/** Chest-relative offset (cm): X forward, Y right, Z up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joint")
	FVector OffsetCm = FVector(20.f, 0.f, 10.f);

	/** Use the actual skeleton bone instead of a socket when possible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joint")
	bool bUseBoneAnchor = true;

	/** Shoulder pitch limit relative to chest-forward (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joint|Limits", Meta = (ClampMin = "0"))
	float PitchLimitDeg = 80.f;

	/** Shoulder yaw limit relative to chest-forward (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterfall|Limb|Joint|Limits", Meta = (ClampMin = "0"))
	float YawLimitDeg = 70.f;

	/** Current anchor world location (cm). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|Joint")
	FVector AnchorWorld = FVector::ZeroVector;

	/** Whether a valid anchor was computed this frame. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Limb|Joint")
	bool bHasAnchor = false;

	/** Get the computed anchor world location. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb|Joint")
	FVector GetAnchorWorld() const { return AnchorWorld; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Afterfall|Limb|Joint")
	bool HasValidAnchor() const { return bHasAnchor; }

	/** Clamp a desired world direction against shoulder joint limits, returning
	 *  the clamped direction (unit vector). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Limb|Joint")
	FVector ClampDirection(const FVector& OwnerFwd, const FVector& DesiredDir) const;

protected:
	virtual void BeginPlay() override;

private:
	USkeletalMeshComponent* Mesh = nullptr;
	ACharacter* Character = nullptr;

	void ComputeAnchor();
};
