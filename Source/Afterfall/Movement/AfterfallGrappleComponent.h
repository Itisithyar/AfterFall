// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Grapple author (root-tree implementation) — ported for Real Game
// integration by Muse. Original: Source/Afterfall/Movement/
// AfterfallGrappleComponent.h (same names, same tuning defaults).
//
// Real Game deltas vs the root original (all additive, gameplay-safe):
//   * Server-authoritative: TryGrapple/Release refuse on non-authority.
//     Clients enter only through AAfterfallCharacter grapple RPCs.
//   * Replicated State/Anchor/RopeLength so remote clients see the rope.
//   * Movement ownership unchanged: the component only reads/writes the
//     owning CMC's Velocity (plus one swept correction move), exactly as
//     the original. Release preserves momentum (no velocity reset).
//   * Tick is a no-op unless Attached; Idle costs nothing per frame.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AfterfallGrappleComponent.generated.h"

UENUM(BlueprintType)
enum class EAfterfallGrappleState : uint8
{
	Idle,
	Attempting,
	Attached,
	Released
};

USTRUCT(BlueprintType)
struct FAfterfallGrappleTuning
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") float MaxRangeCm = 2000.f;
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") float MinRopeLengthCm = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") float MaxRopeLengthCm = 2000.f;
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") float SwingSteerStrength = 600.f;
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") float ConstraintToleranceCm = 5.f;
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") float ConstraintCorrectionRate = 10.f;
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") bool bDebugDraw = false;
};

UCLASS(ClassGroup=(Afterfall), Blueprintable)
class AFTERFALL_API UAfterfallGrappleComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAfterfallGrappleComponent();
	UPROPERTY(EditDefaultsOnly, Category="Afterfall|Grapple") FAfterfallGrappleTuning Tuning;
	UPROPERTY(Replicated, VisibleAnywhere, Category="Afterfall|Grapple")
	EAfterfallGrappleState State = EAfterfallGrappleState::Idle;
	UPROPERTY(Replicated, VisibleAnywhere, Category="Afterfall|Grapple")
	FVector Anchor = FVector::ZeroVector;
	UPROPERTY(Replicated, VisibleAnywhere, Category="Afterfall|Grapple")
	float RopeLength = 0.f;
	UPROPERTY(VisibleAnywhere, Category="Afterfall|Grapple") float LastConstraintError = 0.f;

	/** Server only: raycast for an anchor within MaxRangeCm. */
	UFUNCTION(BlueprintCallable, Category="Afterfall|Grapple") void TryGrapple();
	/** Server only: detach, preserving current momentum. */
	UFUNCTION(BlueprintCallable, Category="Afterfall|Grapple") void Release();
	UFUNCTION(BlueprintCallable, Category="Afterfall|Grapple") bool IsAttached() const { return State==EAfterfallGrappleState::Attached; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
private:
	void AcquireAnchor();
	void ApplyRopeConstraint(float DeltaTime);
	void ApplySteering(float DeltaTime);
};
