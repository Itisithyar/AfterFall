// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "InteractComponent.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

UInteractComponent::UInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			FocusTimer, this, &UInteractComponent::UpdateFocus, 0.1f, true);
	}
}

void UInteractComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FocusTimer);
	}
	Super::EndPlay(Reason);
}

void UInteractComponent::UpdateFocus()
{
	FocusedActor = nullptr;
	FocusedPrompt = FText::GetEmpty();

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	AController* PC = Pawn->GetController();
	if (!PC)
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector End = CamLoc + CamRot.Vector() * InteractRangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFInteract), false, Pawn);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params))
	{
		return;
	}
	AActor* Target = Hit.GetActor();
	if (!Target || !Target->Implements<UInteractable>())
	{
		return;
	}
	if (!IInteractable::Execute_CanInteract(Target, PC))
	{
		return;
	}
	FocusedActor = Target;
	FocusedPrompt = IInteractable::Execute_GetInteractPrompt(Target);
}

bool UInteractComponent::ServerValidateTarget(AController* Instigator, AActor* Target) const
{
	if (!Instigator || !Target || Target->IsPendingKillPending())
	{
		return false;
	}
	if (!Target->Implements<UInteractable>())
	{
		return false;
	}
	APawn* Pawn = Instigator->GetPawn();
	if (!Pawn)
	{
		return false;
	}
	// Range check from pawn center; facing check (60-degree cone) from camera.
	const float DistCm = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (DistCm > InteractRangeCm + 150.f) // + capsule/mesh slack
	{
		return false;
	}
	FVector CamLoc;
	FRotator CamRot;
	Instigator->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector ToTarget = (Target->GetActorLocation() - CamLoc).GetSafeNormal();
	if (FVector::DotProduct(CamRot.Vector(), ToTarget) < 0.5f)
	{
		return false;
	}
	return true;
}
