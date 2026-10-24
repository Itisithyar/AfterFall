// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Minimal rope/grapple traversal component. Integrates with UAfterfallMovementComponent.
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
	UPROPERTY(VisibleAnywhere, Category="Afterfall|Grapple") EAfterfallGrappleState State = EAfterfallGrappleState::Idle;
	UPROPERTY(VisibleAnywhere, Category="Afterfall|Grapple") FVector Anchor = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, Category="Afterfall|Grapple") float RopeLength = 0.f;
	UPROPERTY(VisibleAnywhere, Category="Afterfall|Grapple") float LastConstraintError = 0.f;

	UFUNCTION(BlueprintCallable, Category="Afterfall|Grapple") void TryGrapple();
	UFUNCTION(BlueprintCallable, Category="Afterfall|Grapple") void Release();
	UFUNCTION(BlueprintCallable, Category="Afterfall|Grapple") bool IsAttached() const { return State==EAfterfallGrappleState::Attached; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
private:
	void AcquireAnchor();
	void ApplyRopeConstraint(float DeltaTime);
	void ApplySteering(float DeltaTime);
};