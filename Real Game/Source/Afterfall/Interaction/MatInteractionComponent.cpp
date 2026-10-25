// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "MatInteractionComponent.h"
#include "Storage/PhysicalMat.h"
#include "AfterfallCharacter.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

UMatInteractionComponent::UMatInteractionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UMatInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMatInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTransitionTimer();
	Super::EndPlay(EndPlayReason);
}

void UMatInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMatInteractionComponent, InteractionState);
	DOREPLIFETIME(UMatInteractionComponent, ActiveMat);
}

bool UMatInteractionComponent::CanTransition(EMatInteractionState From, EMatInteractionState To)
{
	switch (From)
	{
	case EMatInteractionState::Closed:
		return To == EMatInteractionState::Opening;
	case EMatInteractionState::Opening:
		// Open on reveal; Closed on interrupt (death/cancel).
		return To == EMatInteractionState::Open || To == EMatInteractionState::Closed;
	case EMatInteractionState::Open:
		// Close normally; Closed directly on force-close (death).
		return To == EMatInteractionState::Closing || To == EMatInteractionState::Closed;
	case EMatInteractionState::Closing:
		return To == EMatInteractionState::Closed;
	default:
		return false;
	}
}

bool UMatInteractionComponent::IsInteractionBlocking() const
{
	return InteractionState == EMatInteractionState::Opening
		|| InteractionState == EMatInteractionState::Open
		|| InteractionState == EMatInteractionState::Closing;
}

AAfterfallCharacter* UMatInteractionComponent::OwnerCharacter() const
{
	return Cast<AAfterfallCharacter>(GetOwner());
}

void UMatInteractionComponent::ClearTransitionTimer()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(TransitionTimer);
	}
}

bool UMatInteractionComponent::ServerRequestMatOpen_Validate()
{
	return true;
}

void UMatInteractionComponent::ServerRequestMatOpen_Implementation()
{
	AAfterfallCharacter* C = OwnerCharacter();
	if (!C || !C->HasAuthority() || (C->Health && C->Health->bIsDead))
	{
		return;
	}
	if (InteractionState != EMatInteractionState::Closed)
	{
		return; // repeated B press while Opening/Open/Closing: no-op
	}
	APhysicalStorageMat* Mat = EnsureMat();
	if (!Mat)
	{
		return; // deploy failed (e.g. no authority/world): stay Closed
	}
	ActiveMat = Mat;
	C->Crouch(); // native replicated crouch; collision/movement code untouched
	InteractionState = EMatInteractionState::Opening;
	OnRep_InteractionState();
	// Cosmetic-only montage (assigned in-editor; null = timer scaffolding).
	// The timer below still guarantees the transition even if the montage
	// or its notify is missing (missing-asset safety).
	if (UnrollMontage && C->GetMesh())
	{
		if (UAnimInstance* Anim = C->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(UnrollMontage);
		}
	}
	ClearTransitionTimer();
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(TransitionTimer, this,
			&UMatInteractionComponent::CompleteOpening, UnrollDurationSec, false);
	}
}

bool UMatInteractionComponent::ServerRequestMatClose_Validate()
{
	return true;
}

void UMatInteractionComponent::ServerRequestMatClose_Implementation()
{
	AAfterfallCharacter* C = OwnerCharacter();
	if (!C || !C->HasAuthority())
	{
		return;
	}
	if (InteractionState != EMatInteractionState::Open)
	{
		return; // repeated B press while Closing/Closed/Opening: no-op
	}
	InteractionState = EMatInteractionState::Closing;
	OnRep_InteractionState();
	ClearTransitionTimer();
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(TransitionTimer, this,
			&UMatInteractionComponent::CompleteClosing, PackDurationSec, false);
	}
}

void UMatInteractionComponent::NotifyMatRevealed()
{
	if (OwnerCharacter() && OwnerCharacter()->HasAuthority()
		&& InteractionState == EMatInteractionState::Opening)
	{
		CompleteOpening();
	}
}

void UMatInteractionComponent::CompleteOpening()
{
	ClearTransitionTimer();
	if (!CanTransition(InteractionState, EMatInteractionState::Open))
	{
		return;
	}
	InteractionState = EMatInteractionState::Open;
	OnRep_InteractionState();
}

void UMatInteractionComponent::CompleteClosing()
{
	ClearTransitionTimer();
	if (!CanTransition(InteractionState, EMatInteractionState::Closed))
	{
		return;
	}
	if (ActiveMat)
	{
		ActiveMat->SetPacked(true);
	}
	if (AAfterfallCharacter* C = OwnerCharacter())
	{
		C->UnCrouch();
	}
	InteractionState = EMatInteractionState::Closed;
	OnRep_InteractionState();
}

void UMatInteractionComponent::ForceClose()
{
	AAfterfallCharacter* C = OwnerCharacter();
	if (!C || !C->HasAuthority())
	{
		return;
	}
	ClearTransitionTimer();
	if (InteractionState == EMatInteractionState::Closed)
	{
		return;
	}
	if (ActiveMat)
	{
		ActiveMat->SetPacked(true);
	}
	// Dead pawns stay down: only uncrouch the living (respawn is a fresh pawn).
	if (C->Health && !C->Health->bIsDead)
	{
		C->UnCrouch();
	}
	InteractionState = EMatInteractionState::Closed;
	OnRep_InteractionState();
}

APhysicalStorageMat* UMatInteractionComponent::EnsureMat()
{
	AAfterfallCharacter* C = OwnerCharacter();
	if (!C)
	{
		return nullptr;
	}
	if (!C->DeployedMat)
	{
		// Reuses the existing single-mat deploy path (server-side call runs
		// the implementation directly; one mat per player is preserved).
		C->ServerDeployMat();
	}
	APhysicalStorageMat* Mat = C->DeployedMat;
	if (!Mat)
	{
		return nullptr;
	}
	// Returning mat: unhide where the player stands if they wandered off
	// since (deposit/withdraw reach is 400cm).
	const float Dist = FVector::Dist(C->GetActorLocation(), Mat->GetActorLocation());
	if (Dist > 350.f)
	{
		const FVector At = C->GetActorLocation() + C->GetActorForwardVector() * 150.f;
		Mat->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	}
	Mat->SetPacked(false);
	return Mat;
}

void UMatInteractionComponent::OnRep_InteractionState()
{
	OnInteractionChanged.Broadcast(InteractionState, ActiveMat);
}
