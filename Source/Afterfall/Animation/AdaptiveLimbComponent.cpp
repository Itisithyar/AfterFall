// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Animation/AdaptiveLimbComponent.h"
#include "Animation/ChestAnchoredJointComponent.h"
#include "Animation/ArmContactSolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAdaptiveLimbComponent::UAdaptiveLimbComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false); // limb is client-side visual intent; server validates interaction

	Joint = CreateDefaultSubobject<UChestAnchoredJointComponent>(TEXT("JointAnchor"));
	Solver = CreateDefaultSubobject<UArmContactSolver>(TEXT("ContactSolver"));
}

void UAdaptiveLimbComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (C)
	{
		if (!Mesh)
		{
			Mesh = C->GetMesh();
		}
		if (!Camera)
		{
			Camera = Cast<UCameraComponent>(C->FindComponentByClass<UCameraComponent>());
		}
	}
	if (Joint)
	{
		Joint->SetSkeletalMesh(Mesh);
		Joint->SetOwnerCharacter(C);
	}
	if (Solver)
	{
		// Solver uses world traces only; no character reference needed.
	}
}

void UAdaptiveLimbComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateLimb(DeltaTime);
}

void UAdaptiveLimbComponent::UpdateLimb(float DeltaTime)
{
	if (!Joint || !Solver)
	{
		return;
	}

	// 1. Chest-relative shoulder anchor (from skeleton or fallback).
	ComputeShoulderAnchor();

	// 2. Camera/intent desired target.
	ComputeDesiredTarget();

	// 3. Solver clamps to reach + joint limits + world contact.
	Solver->SetConfig(Config);
	Solver->SetShoulder(CachedShoulder);
	Solver->SetDesired(Target.Desired);
	Solver->SetCurrentState(State);
	Solver->Solve(DeltaTime);
	Target = Solver->GetResult();

	// 4. Solve the full pose (shoulder/elbow/wrist).
	SolvePose();

	// 5. State machine transitions.
	UpdateStateMachine(DeltaTime);
	UpdateBlend(DeltaTime);

	Pose.State = State;
	Pose.BlendWeight = BlendWeight;
}

void UAdaptiveLimbComponent::SetSkeletalMesh(USkeletalMeshComponent* InMesh)
{
	Mesh = InMesh;
	if (Joint) Joint->SetSkeletalMesh(Mesh);
	// Solver uses world traces only; no skeletal mesh needed.
}

void UAdaptiveLimbComponent::SetCamera(UCameraComponent* InCamera)
{
	Camera = InCamera;
}

void UAdaptiveLimbComponent::SetReachTarget(const FVector& WorldTarget)
{
	if (WorldTarget.IsZero())
	{
		return;
	}
	DesiredIntent = WorldTarget;
	if (State == EAdaptiveLimbState::Animated)
	{
		SetState(EAdaptiveLimbState::Reach);
	}
}

void UAdaptiveLimbComponent::ClearReachTarget()
{
	DesiredIntent = FVector::ZeroVector;
	if (State == EAdaptiveLimbState::Reach || State == EAdaptiveLimbState::Contact)
	{
		SetState(EAdaptiveLimbState::Recovery);
	}
}

void UAdaptiveLimbComponent::AcquireContact()
{
	if (Target.bContacting || State == EAdaptiveLimbState::Contact || State == EAdaptiveLimbState::Brace)
	{
		return;
	}
	SetState(EAdaptiveLimbState::Contact);
}

void UAdaptiveLimbComponent::ReleaseContact()
{
	if (State == EAdaptiveLimbState::Animated || State == EAdaptiveLimbState::Reach)
	{
		return;
	}
	SetState(EAdaptiveLimbState::Recovery);
}

bool UAdaptiveLimbComponent::IsContacting() const
{
	return Target.bContacting &&
		(State == EAdaptiveLimbState::Contact || State == EAdaptiveLimbState::Brace ||
		 State == EAdaptiveLimbState::Grip);
}

FAdaptiveLimbPose UAdaptiveLimbComponent::GetLimbPose() const
{
	return Pose;
}

FAdaptiveLimbTarget UAdaptiveLimbComponent::GetLimbTarget() const
{
	return Target;
}

void UAdaptiveLimbComponent::SetState(EAdaptiveLimbState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	LastStateChange = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UAdaptiveLimbComponent::ComputeShoulderAnchor()
{
	if (Joint)
	{
		CachedShoulder = Joint->GetAnchorWorld();
		bHasValidAnchor = Joint->HasValidAnchor();
	}
	else if (Mesh)
	{
		FTransform SocketT = Mesh->GetSocketTransform(Config.ShoulderSocketName);
		CachedShoulder = SocketT.GetLocation();
		bHasValidAnchor = true;
	}
	else
	{
		// Fallback: chest-relative to owner rotation.
		AActor* Owner = GetOwner();
		if (Owner)
		{
			FVector Fwd = Owner->GetActorForwardVector();
			FVector Right = Owner->GetActorRightVector();
			FVector Up = Owner->GetActorUpVector();
			CachedShoulder = Owner->GetActorLocation()
				+ Fwd * Config.ShoulderOffsetCm.X
				+ Right * Config.ShoulderOffsetCm.Y
				+ Up * Config.ShoulderOffsetCm.Z;
			bHasValidAnchor = true;
		}
	}
}

void UAdaptiveLimbComponent::ComputeDesiredTarget()
{
	Target.Desired = FVector::ZeroVector;
	Target.DesiredDistanceCm = 0.f;

	if (!Camera || !bHasValidAnchor)
	{
		return;
	}

	FVector CamLoc = Camera->GetComponentLocation();
	FVector CamFwd = Camera->GetForwardVector();

	// Raw intent: camera forward at max reach, or explicit intent if set.
	FVector Intent = DesiredIntent.IsNearlyZero()
		? CamLoc + CamFwd * Config.MaxReachCm
		: DesiredIntent;

	// Clamp to max reach from shoulder.
	FVector ToIntent = Intent - CachedShoulder;
	float Dist = ToIntent.Size();
	if (Dist > Config.MaxReachCm)
	{
		Intent = CachedShoulder + ToIntent.GetSafeNormal() * Config.MaxReachCm;
		Dist = Config.MaxReachCm;
	}

	Target.Desired = Intent;
	Target.DesiredDistanceCm = Dist;
}

void UAdaptiveLimbComponent::SolvePose()
{
	Target.Final = Target.Reachable;

	// Elbow: anatomically plausible bend between shoulder and wrist.
	Pose.Elbow = ComputeElbow(CachedShoulder, Target.Final);
	Pose.Wrist = Target.Final;
	Pose.WristRotation = Target.bContacting
		? FRotationMatrix::MakeFromZ(Target.ContactNormal).Rotator()
		: (Camera ? Camera->GetComponentRotation() : FRotator::ZeroRotator);
	Pose.Shoulder = CachedShoulder;

	// Shoulder angles relative to chest-forward.
	if (AActor* Owner = GetOwner())
	{
		FVector OwnerFwd = Owner->GetActorForwardVector();
		FVector ToWrist = (Target.Final - CachedShoulder).GetSafeNormal();
		FVector ToWristLocal = FVector(
			FVector::DotProduct(ToWrist, OwnerFwd),
			FVector::DotProduct(ToWrist, Owner->GetActorRightVector()),
			FVector::DotProduct(ToWrist, Owner->GetActorUpVector())
		);
		Pose.ShoulderAnglesDeg.X = FMath::RadiansToDegrees(FMath::Asin(ToWristLocal.Z));
		Pose.ShoulderAnglesDeg.Y = FMath::RadiansToDegrees(FMath::Asin(ToWristLocal.Y));
	}

	// Elbow bend degrees.
	FVector ShoulderToWrist = (Target.Final - CachedShoulder).GetSafeNormal();
	FVector ShoulderToElbow = (Pose.Elbow - CachedShoulder).GetSafeNormal();
	float BendDot = FVector::DotProduct(ShoulderToWrist, ShoulderToElbow);
	Pose.ElbowBendDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(BendDot, -1.f, 1.f)));
}

FVector UAdaptiveLimbComponent::ComputeElbow(const FVector& Shoulder, const FVector& Wrist) const
{
	return ComputeElbowPosition(Shoulder, Wrist, Config);
}

FVector UAdaptiveLimbComponent::ComputeElbowPosition(const FVector& Shoulder, const FVector& Wrist,
	const FAdaptiveLimbConfig& InConfig)
{
	FVector Mid = (Shoulder + Wrist) * 0.5f;
	FVector Axis = (Wrist - Shoulder).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return Mid;
	}
	FVector Outward = FVector::CrossProduct(Axis, FVector::UpVector);
	if (Outward.IsNearlyZero())
	{
		Outward = FVector::CrossProduct(Axis, FVector::ForwardVector);
	}
	Outward = Outward.GetSafeNormal();
	float BendAmount = FMath::Clamp(InConfig.ElbowPreferredBendDeg, 0.f, InConfig.ElbowMaxBendDeg);
	float BendCm = FMath::Tan(FMath::DegreesToRadians(BendAmount)) * (Wrist - Shoulder).Size() * 0.25f;
	if (InConfig.LimbSide == EAdaptiveLimbSide::Left)
	{
		BendCm = -BendCm;
	}
	return Mid + Outward * BendCm;
}

void UAdaptiveLimbComponent::UpdateStateMachine(float DeltaTime)
{
	if (!Mesh)
	{
		return;
	}

	bool bContacting = Target.bContacting;
	bool bHasReach = !DesiredIntent.IsNearlyZero() || Target.DesiredDistanceCm > 0.f;

	switch (State)
	{
	case EAdaptiveLimbState::Animated:
		if (bContacting)
		{
			SetState(EAdaptiveLimbState::Contact);
		}
		else if (bHasReach)
		{
			SetState(EAdaptiveLimbState::Reach);
		}
		break;
	case EAdaptiveLimbState::Reach:
		if (bContacting)
		{
			SetState(EAdaptiveLimbState::Contact);
		}
		else if (!bHasReach)
		{
			SetState(EAdaptiveLimbState::Recovery);
		}
		break;
	case EAdaptiveLimbState::Contact:
		if (!bContacting)
		{
			SetState(EAdaptiveLimbState::Recovery);
		}
		break;
	case EAdaptiveLimbState::Brace:
		if (!bContacting)
		{
			SetState(EAdaptiveLimbState::Recovery);
		}
		break;
	case EAdaptiveLimbState::Recovery:
		// Blend back to animated; once blended, clear intent.
		if (BlendWeight <= 0.01f)
		{
			SetState(EAdaptiveLimbState::Animated);
			DesiredIntent = FVector::ZeroVector;
		}
		break;
	case EAdaptiveLimbState::Yield:
	case EAdaptiveLimbState::Impact:
		if (BlendWeight <= 0.01f)
		{
			SetState(EAdaptiveLimbState::Recovery);
		}
		break;
	case EAdaptiveLimbState::Ragdoll:
		// Handled by existing ragdoll system; limb stays out of the way.
		break;
	default:
		break;
	}
}

void UAdaptiveLimbComponent::UpdateBlend(float DeltaTime)
{
	float TargetBlend = 0.f;
	switch (State)
	{
	case EAdaptiveLimbState::Reach:
	case EAdaptiveLimbState::Contact:
	case EAdaptiveLimbState::Brace:
	case EAdaptiveLimbState::Grip:
		TargetBlend = 1.f;
		break;
	case EAdaptiveLimbState::Impact:
	case EAdaptiveLimbState::Yield:
		TargetBlend = 1.f;
		break;
	case EAdaptiveLimbState::Recovery:
		TargetBlend = 0.f;
		break;
	case EAdaptiveLimbState::Animated:
	default:
		TargetBlend = 0.f;
		break;
	}

	float Speed = Config.ReachSpeedCmS / 100.f; // convert to blend/s (rough scale)
	BlendWeight = FMath::FInterpTo(BlendWeight, TargetBlend, DeltaTime, Speed * 3.f);
	BlendWeight = FMath::Clamp(BlendWeight, 0.f, 1.f);
}

void UAdaptiveLimbComponent::DebugDraw(const FVector& CameraLocation) const
{
	if (!Config.bDebugDraw || !GetWorld())
	{
		return;
	}
	FColor ShoulderCol = FColor::Green;
	FColor TargetCol = Target.bContacting ? FColor::Cyan : FColor::Yellow;
	FColor ElbowCol = FColor::Orange;

	DrawDebugPoint(GetWorld(), CachedShoulder, 8.f, ShoulderCol, false, 0.f, 0);
	DrawDebugPoint(GetWorld(), Target.Desired, 8.f, TargetCol, false, 0.f, 0);
	DrawDebugPoint(GetWorld(), Target.Reachable, 8.f, TargetCol, false, 0.f, 0);
	DrawDebugPoint(GetWorld(), Pose.Elbow, 6.f, ElbowCol, false, 0.f, 0);

	DrawDebugLine(GetWorld(), CachedShoulder, Target.Desired, FColor::Red, false, 0.f, 0, 2.f);
	DrawDebugLine(GetWorld(), CachedShoulder, Target.Reachable, FColor::Green, false, 0.f, 0, 2.f);
	DrawDebugLine(GetWorld(), Target.Reachable, Pose.Elbow, FColor::Orange, false, 0.f, 0, 2.f);
	DrawDebugLine(GetWorld(), Pose.Elbow, Target.Final, FColor::Orange, false, 0.f, 0, 2.f);

	if (Target.bContacting)
	{
		DrawDebugPoint(GetWorld(), Target.ContactPoint, 8.f, FColor::Cyan, false, 0.f, 0);
		DrawDebugLine(GetWorld(), Target.ContactPoint, Target.ContactPoint + Target.ContactNormal * 30.f,
			FColor::Cyan, false, 0.f, 0, 2.f);
	}

	FString StateStr = UEnum::GetValueAsString(State);
	DrawDebugString(GetWorld(), CachedShoulder + FVector(0.f, 0.f, 20.f), StateStr, nullptr,
		FColor::White, 0.f);
}

bool UAdaptiveLimbComponent::CanAcquireContact(const FVector& Hand, const FVector& Normal) const
{
	return Solver && Solver->CanAcquireContact(Hand, Normal);
}
