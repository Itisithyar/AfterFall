// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "CarryComponent.h"
#include "AfterfallCharacter.h"
#include "World/PhysicalItem.h"
#include "Movement/AfterfallMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

UCarryComponent::UCarryComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UCarryComponent::ServerPickup(APhysicalItem* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsCarrying || !Item)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Item->IsFree())
	{
		return; // duplicate pickup rejected: item already held/stored
	}
	if (FVector::Dist(Pawn->GetActorLocation(), Item->GetActorLocation()) > PickupRangeCm + 100.f)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Pickup rejected: out of range"));
		return;
	}
	// Single commit point: item transitions Free->Held, then attaches.
	Item->ServerPickupBy(Pawn);
	Item->AttachToComponent(Pawn->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Item->SetActorRelativeLocation(FVector(60.f, 30.f, 40.f));
	CarriedItem = Item;
	bIsCarrying = true;
	SyncCarryFlag();
	UE_LOG(LogAfterfall, Log, TEXT("Picked up %s (%s)"), *Item->ItemType.ToString(),
		*Item->ItemId.ToString());
}

void UCarryComponent::ServerDrop()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bIsCarrying || !CarriedItem)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || CarriedItem->GetHolder() != Pawn)
	{
		return; // holder mismatch: not ours to drop
	}
	CarriedItem->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CarriedItem->SetActorLocation(Pawn->GetActorLocation() + FVector(80.f, 0.f, 0.f));
	CarriedItem->ServerReleaseToWorld(FVector::ZeroVector);
	UE_LOG(LogAfterfall, Log, TEXT("Dropped %s"), *CarriedItem->ItemType.ToString());
	CarriedItem = nullptr;
	bIsCarrying = false;
	SyncCarryFlag();
}

void UCarryComponent::ServerThrow(const FVector& Direction)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bIsCarrying || !CarriedItem)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || CarriedItem->GetHolder() != Pawn)
	{
		return;
	}
	UWorld* W = GetWorld();
	if (!W || W->TimeSeconds - LastReleaseTime < 0.25f)
	{
		return; // release spam guard
	}
	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return;
	}
	const float Speed = FMath::Clamp(ThrowSpeedCmS, 0.f, MaxThrowSpeedCmS);
	const FVector Release = APhysicalItem::ComputeReleaseVelocity(
		Pawn->GetVelocity(), Dir, Speed, ThrowInheritFactor);
	CarriedItem->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CarriedItem->SetActorLocation(Pawn->GetActorLocation() + Dir * 80.f);
	CarriedItem->ServerReleaseToWorld(Release);
	LastReleaseTime = W->TimeSeconds;
	UE_LOG(LogAfterfall, Log, TEXT("Threw %s at %.0f cm/s (player %.0f)"),
		*CarriedItem->ItemType.ToString(), Release.Size(), Pawn->GetVelocity().Size());
	CarriedItem = nullptr;
	bIsCarrying = false;
	SyncCarryFlag();
}

float UCarryComponent::GetCarriedMassKg() const
{
	return CarriedItem ? CarriedItem->MassKg : 0.f;
}

void UCarryComponent::ServerClearAfterStore()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	CarriedItem = nullptr;
	bIsCarrying = false;
	SyncCarryFlag();
}

void UCarryComponent::OnRep_Carried() {}

UAfterfallMovementComponent* UCarryComponent::MoveComp()
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (ACharacter* C = Cast<ACharacter>(Pawn))
		{
			return Cast<UAfterfallMovementComponent>(C->GetMovementComponent());
		}
	}
	return nullptr;
}

void UCarryComponent::SyncCarryFlag()
{
	// Route through the character's single load-recompute point so the wallet
	// and held-slot writers can never clobber each other's state (the old
	// direct SetCarryingHeavy call raced SetCarriedMass). Recompute happens
	// after CarriedItem is assigned/cleared by every pickup/drop/throw/store.
	if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(GetOwner()))
	{
		C->OnInventoryChangedForLoad();
		return;
	}
	// Fallback for character-less fixtures: legacy single-writer path.
	if (UAfterfallMovementComponent* M = MoveComp())
	{
		M->SetCarryingHeavy(bIsCarrying && GetCarriedMassKg() >= M->Tuning.HeavyMassKg);
	}
}

void UCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCarryComponent, CarriedItem);
	DOREPLIFETIME(UCarryComponent, bIsCarrying);
}
