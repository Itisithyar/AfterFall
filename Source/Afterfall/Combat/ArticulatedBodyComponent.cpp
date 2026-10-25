// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Combat/ArticulatedBodyComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Afterfall.h"

namespace
{
	// Regional masses (kg). Sum ~= 77 kg adult placeholder; limbs are light so
	// a grab reads as "dragging a body", not "moving a crate".
	float MassForTag(const FName Tag)
	{
		if (Tag == FName(TEXT("BodyPart.Torso"))) { return 38.f; }
		if (Tag == FName(TEXT("BodyPart.Head")))  { return 5.f; }
		if (Tag == FName(TEXT("BodyPart.ArmL")) ||
			Tag == FName(TEXT("BodyPart.ArmR")))  { return 4.f; }
		if (Tag == FName(TEXT("BodyPart.LegL")) ||
			Tag == FName(TEXT("BodyPart.LegR")))  { return 13.f; }
		return 8.f;
	}

	FName BodyTagOf(const UStaticMeshComponent* Part)
	{
		return Part && Part->ComponentTags.Num() > 0 ? Part->ComponentTags[0] : NAME_None;
	}
}

UArticulatedBodyComponent::UArticulatedBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UArticulatedBodyComponent::BindParts(UCapsuleComponent* InCapsule,
	UStaticMeshComponent* InTorso, const TArray<UStaticMeshComponent*>& InParts)
{
	Capsule = InCapsule;
	Torso = InTorso;
	Parts = InParts;
}

void UArticulatedBodyComponent::BuildConstraints()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Torso)
	{
		return;
	}
	for (UStaticMeshComponent* Part : Parts)
	{
		if (!Part || Part == Torso)
		{
			continue;
		}
		UPhysicsConstraintComponent* Joint = NewObject<UPhysicsConstraintComponent>(Owner);
		Joint->SetupAttachment(Torso);
		Joint->SetRelativeLocation(Part->GetRelativeLocation());
		Joint->RegisterComponent();
		// Torso is parent; the limb swings inside a human-plausible cone.
		Joint->SetConstrainedComponents(Torso, NAME_None, Part, NAME_None);
		Joint->SetDisableCollision(true);
		Joint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 55.f);
		Joint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 55.f);
		Joint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 35.f);
		Joint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Joint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		Joint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
		// Constraints are only built when the body dies: parts do not simulate
		// while alive, so a joint is inert either way and nothing can leak into
		// the live damage path.
		Constraints.Add(Joint);
	}
}

void UArticulatedBodyComponent::EnterRagdoll()
{
	if (bRagdoll || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (Parts.Num() < 2 || !Torso)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("RAGDOLL %s: no bound parts"), *GetNameSafe(GetOwner()));
		return;
	}
	bRagdoll = true;
	BuildConstraints();

	// The actor-level collision latch is used by the alive path (corpse must
	// not block movement); articulated parts need their own physical bodies.
	GetOwner()->SetActorEnableCollision(true);
	if (Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	int32 Simulating = 0;
	for (UStaticMeshComponent* Part : Parts)
	{
		if (!Part)
		{
			continue;
		}
		// Detach first: simulating children of a non-simulating root produces
		// transform fights between the capsule and the rigid body.
		Part->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Part->SetCollisionProfileName(TEXT("PhysicsActor"));
		Part->SetMassOverrideInKg(NAME_None, MassForTag(BodyTagOf(Part)), true);
		Part->SetSimulatePhysics(true);
		Part->WakeAllRigidBodies();
		++Simulating;
	}
	// Tick only while something is holding the body.
	SetComponentTickEnabled(false);
	UE_LOG(LogAfterfall, Log, TEXT("RAGDOLL %s: %d bodies, %d joints"),
		*GetNameSafe(GetOwner()), Simulating, Constraints.Num());
}

bool UArticulatedBodyComponent::ServerGrab(AController* InGrabber)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bRagdoll || !Torso || bGrabbed)
	{
		return false;
	}
	APawn* Pawn = InGrabber ? InGrabber->GetPawn() : nullptr;
	if (!Pawn || Pawn == Owner)
	{
		return false;
	}
	FVector CamLoc;
	FRotator CamRot;
	InGrabber->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector TorsoLoc = Torso->GetComponentLocation();
	if (FVector::Dist(CamLoc, TorsoLoc) > GrabRangeCm)
	{
		return false;
	}
	if (FVector::DotProduct(CamRot.Vector(), (TorsoLoc - CamLoc).GetSafeNormal()) < 0.35f)
	{
		return false;
	}

	UPhysicsHandleComponent* Handle = NewObject<UPhysicsHandleComponent>(Pawn);
	Handle->RegisterComponent();
	Handle->SetLinearDamping(200.f);
	Handle->SetAngularDamping(500.f);
	Handle->SetInterpolationSpeed(20.f);
	Handle->GrabComponentAtLocationWithRotation(Torso, NAME_None, TorsoLoc, Torso->GetComponentRotation());
	if (!Handle->GrabbedComponent)
	{
		Handle->DestroyComponent();
		return false;
	}
	GrabHandle = Handle;
	Grabber = InGrabber;
	bGrabbed = true;
	SetComponentTickEnabled(true);
	UE_LOG(LogAfterfall, Log, TEXT("RAGDOLL %s grabbed by %s"),
		*GetNameSafe(Owner), *GetNameSafe(Pawn));
	return true;
}

void UArticulatedBodyComponent::ServerRelease()
{
	if (!bGrabbed)
	{
		return;
	}
	ReleaseHandle();
	UE_LOG(LogAfterfall, Log, TEXT("RAGDOLL %s released"), *GetNameSafe(GetOwner()));
}

void UArticulatedBodyComponent::ReleaseHandle()
{
	if (UPhysicsHandleComponent* Handle = GrabHandle.Get())
	{
		Handle->ReleaseComponent();
		Handle->DestroyComponent();
	}
	GrabHandle = nullptr;
	Grabber = nullptr;
	bGrabbed = false;
	SetComponentTickEnabled(false);
}

void UArticulatedBodyComponent::UpdateGrabTarget()
{
	AController* C = Grabber.Get();
	UPhysicsHandleComponent* Handle = GrabHandle.Get();
	AActor* Owner = GetOwner();
	if (!C || !Handle || !Owner || !Owner->HasAuthority())
	{
		ReleaseHandle();
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	C->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Target = CamLoc + CamRot.Vector() * HoldDistanceCm;
	// Drop the body rather than stretch it across the map if the holder runs.
	if (FVector::Dist(CamLoc, Handle->GetGrabbedComponent()->GetComponentLocation()) > GrabRangeCm * 1.6f)
	{
		ReleaseHandle();
		return;
	}
	Handle->SetTargetLocationAndRotation(Target, CamRot);
}

void UArticulatedBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bGrabbed)
	{
		UpdateGrabTarget();
	}
}

void UArticulatedBodyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseHandle();
	for (UPhysicsConstraintComponent* Joint : Constraints)
	{
		if (IsValid(Joint))
		{
			Joint->DestroyComponent();
		}
	}
	Constraints.Reset();
	Super::EndPlay(EndPlayReason);
}
