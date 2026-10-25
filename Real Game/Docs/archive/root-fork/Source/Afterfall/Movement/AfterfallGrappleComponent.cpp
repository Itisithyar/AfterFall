// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallGrappleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UAfterfallGrappleComponent::UAfterfallGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAfterfallGrappleComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAfterfallGrappleComponent::TryGrapple()
{
	if (State != EAfterfallGrappleState::Idle) return;
	State = EAfterfallGrappleState::Attempting;
	AcquireAnchor();
}

void UAfterfallGrappleComponent::AcquireAnchor()
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C) { State = EAfterfallGrappleState::Idle; return; }
	FVector Origin = C->GetActorLocation();
	FVector Fwd = C->GetActorForwardVector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Grapple), false, C);
	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Fwd * Tuning.MaxRangeCm, ECC_Visibility, Params);
	if (!bHit || !Hit.bBlockingHit)
	{
		State = EAfterfallGrappleState::Idle;
		return;
	}
	float Dist = FVector::Dist(Origin, Hit.ImpactPoint);
	if (Dist < Tuning.MinRopeLengthCm) { State = EAfterfallGrappleState::Idle; return; }
	Anchor = Hit.ImpactPoint;
	RopeLength = FMath::Clamp(Dist, Tuning.MinRopeLengthCm, Tuning.MaxRopeLengthCm);
	State = EAfterfallGrappleState::Attached;
}

void UAfterfallGrappleComponent::Release()
{
	if (State != EAfterfallGrappleState::Attached) { State = EAfterfallGrappleState::Idle; return; }
	State = EAfterfallGrappleState::Released;
	// preserve momentum: leave velocity as-is on movement component
	State = EAfterfallGrappleState::Idle;
}

void UAfterfallGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (State != EAfterfallGrappleState::Attached) return;
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C) { State = EAfterfallGrappleState::Idle; return; }
	UCharacterMovementComponent* M = C->GetCharacterMovement();
	if (!M) { State = EAfterfallGrappleState::Idle; return; }
	ApplyRopeConstraint(DeltaTime);
	ApplySteering(DeltaTime);
	if (Tuning.bDebugDraw)
	{
		DrawDebugLine(GetWorld(), C->GetActorLocation(), Anchor, FColor::Cyan, false, 0.f, 0, 2.f);
	}
}

void UAfterfallGrappleComponent::ApplyRopeConstraint(float DeltaTime)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C) return;
	UCharacterMovementComponent* M = C->GetCharacterMovement();
	FVector ToAnchor = Anchor - C->GetActorLocation();
	float Dist = ToAnchor.Size();
	LastConstraintError = Dist - RopeLength;
	if (Dist <= RopeLength + Tuning.ConstraintToleranceCm) return; // slack
	// project velocity to tangent, remove radial outward component
	FVector Radial = ToAnchor.GetSafeNormal();
	FVector Vel = M->Velocity;
	float RadialVel = FVector::DotProduct(Vel, Radial);
	if (RadialVel > 0.f)
	{
		Vel -= Radial * RadialVel;
	}
	// correct position toward anchor along radial
	float Excess = Dist - RopeLength;
	FVector Correction = -Radial * Excess * FMath::Clamp(Tuning.ConstraintCorrectionRate * DeltaTime, 0.f, 1.f);
	C->SetActorLocation(C->GetActorLocation() + Correction, true);
	M->Velocity = Vel;
}

void UAfterfallGrappleComponent::ApplySteering(float DeltaTime)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C) return;
	UCharacterMovementComponent* M = C->GetCharacterMovement();
	// bounded steering: apply small horizontal input scaled, clamp energy
	FVector Input = M->GetLastInputVector();
	if (Input.IsNearlyZero()) return;
	FVector Steer = Input * Tuning.SwingSteerStrength * DeltaTime;
	FVector NewVel = M->Velocity + Steer;
	// clamp speed to avoid unlimited energy
	float MaxSwingSpeed = 1500.f;
	if (NewVel.Size() > MaxSwingSpeed) NewVel = NewVel.GetSafeNormal() * MaxSwingSpeed;
	M->Velocity = NewVel;
}