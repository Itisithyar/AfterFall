// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Animation/ChestAnchoredJointComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UChestAnchoredJointComponent::UChestAnchoredJointComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false; // anchor computed on demand by the limb component
}

void UChestAnchoredJointComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UChestAnchoredJointComponent::SetSkeletalMesh(USkeletalMeshComponent* InMesh)
{
	Mesh = InMesh;
}

void UChestAnchoredJointComponent::SetOwnerCharacter(ACharacter* InChar)
{
	Character = InChar;
}

void UChestAnchoredJointComponent::ComputeAnchor()
{
	AnchorWorld = FVector::ZeroVector;
	bHasAnchor = false;

	if (!Mesh && !Character)
	{
		return;
	}

	FVector Result = FVector::ZeroVector;
	bool bGot = false;

	if (bUseBoneAnchor && Mesh)
	{
		// Prefer an actual skeleton bone if one exists.
		const FName BoneNames[] = { AnchorSocketName, TEXT("spine_01"), TEXT("spine_02"), TEXT("clavicle_r"), TEXT("clavicle_l") };
		for (FName Bone : BoneNames)
		{
			if (Mesh->DoesSocketExist(Bone))
			{
				FTransform SocketT = Mesh->GetSocketTransform(Bone);
				Result = SocketT.GetLocation();
				bGot = true;
				break;
			}
		}
	}

	if (!bGot && Mesh)
	{
		// Fallback to socket.
		FTransform SocketT = Mesh->GetSocketTransform(AnchorSocketName);
		FVector SocketLoc = SocketT.GetLocation();
		if (!SocketLoc.IsNearlyZero())
		{
			Result = SocketLoc;
			bGot = true;
		}
	}

	if (!bGot && Character)
	{
		// Fallback: chest-relative to character rotation.
		FVector Fwd = Character->GetActorForwardVector();
		FVector Right = Character->GetActorRightVector();
		FVector Up = Character->GetActorUpVector();
		Result = Character->GetActorLocation()
			+ Fwd * OffsetCm.X
			+ Right * OffsetCm.Y
			+ Up * OffsetCm.Z;
		bGot = true;
	}

	if (bGot)
	{
		AnchorWorld = Result;
		bHasAnchor = true;
	}
}

FVector UChestAnchoredJointComponent::ClampDirection(const FVector& OwnerFwd, const FVector& DesiredDir) const
{
	if (DesiredDir.IsNearlyZero())
	{
		return OwnerFwd;
	}
	FVector Dir = DesiredDir.GetSafeNormal();

	// Decompose into pitch (up/down) and yaw (left/right) relative to owner forward.
	FVector Local = FVector(
		FVector::DotProduct(Dir, OwnerFwd),
		FVector::DotProduct(Dir, FVector::CrossProduct(OwnerFwd, FVector::UpVector).GetSafeNormal()),
		FVector::DotProduct(Dir, FVector::UpVector)
	);
	Local = Local.GetSafeNormal();

	float Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Local.Z, -1.f, 1.f)));
	float Yaw = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Local.Y, -1.f, 1.f)));

	Pitch = FMath::Clamp(Pitch, -PitchLimitDeg, PitchLimitDeg);
	Yaw = FMath::Clamp(Yaw, -YawLimitDeg, YawLimitDeg);

	// Recompose.
	FVector Out = FVector(
		FMath::Cos(FMath::DegreesToRadians(Pitch)) * FMath::Cos(FMath::DegreesToRadians(Yaw)),
		FMath::Sin(FMath::DegreesToRadians(Yaw)),
		FMath::Sin(FMath::DegreesToRadians(Pitch))
	);
	return Out.GetSafeNormal();
}
