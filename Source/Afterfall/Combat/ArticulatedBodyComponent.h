// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Physics) — articulated body for the placeholder mannequin.
//
// The zombie's six basic-shape parts stay trace-only while alive so the
// existing damage path (part hit -> ServerHitPart -> HealthComponent) is
// untouched. On death the body becomes an articulated rigid-body assembly:
// every part detaches from the capsule, simulates physics with a regional
// mass, and is joined to the torso by a limited angular constraint. Grab is a
// server-only physics-handle drag started through the normal IInteractable
// channel (E on a ragdoll), never a client-applied transform.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArticulatedBodyComponent.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;
class UPhysicsHandleComponent;
class UCapsuleComponent;

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UArticulatedBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArticulatedBodyComponent();

	/** Bind the owner's body parts. Torso must be one of Parts. */
	void BindParts(UCapsuleComponent* InCapsule, UStaticMeshComponent* InTorso,
		const TArray<UStaticMeshComponent*>& InParts);

	/** Server-only: turn the body into a simulated articulated assembly. */
	void EnterRagdoll();

	/** Server-only: grab/release the torso with the caller's physics handle. */
	bool ServerGrab(AController* Grabber);
	void ServerRelease();

	bool IsRagdoll() const { return bRagdoll; }
	bool IsGrabbed() const { return bGrabbed; }

	/** Max grab reach (cm) from the grabber's camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Body")
	float GrabRangeCm = 350.f;

	/** Distance the held torso is dragged in front of the grabber's camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Body")
	float HoldDistanceCm = 160.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UStaticMeshComponent* Torso = nullptr;
	UCapsuleComponent* Capsule = nullptr;
	TArray<UStaticMeshComponent*> Parts;
	TArray<UPhysicsConstraintComponent*> Constraints;

	TWeakObjectPtr<AController> Grabber;
	TWeakObjectPtr<UPhysicsHandleComponent> GrabHandle;

	bool bRagdoll = false;
	bool bGrabbed = false;

	/**
	 * Spring stiffness of the grab handle (N/cm) — this is a STIFFNESS, not a
	 * force. It feeds UPhysicsHandleComponent::LinearStiffness directly.
	 */
	UPROPERTY(EditAnywhere, Category = "Afterfall|Grab")
	float GrabLinearStiffness = 2000.f;

	/**
	 * REAL force cap (N) for the grab spring. The physics handle produces
	 * F = k * |hand-to-target error|, so the error fed to the handle is clamped
	 * to MaxGrabForce / GrabLinearStiffness in UpdateGrabTarget(); that makes
	 * |F| <= MaxGrabForce by construction instead of growing with distance.
	 */
	UPROPERTY(EditAnywhere, Category = "Afterfall|Grab")
	float MaxGrabForce = 2000.f;

	void BuildConstraints();
	void UpdateGrabTarget();
	void ReleaseHandle();
};
