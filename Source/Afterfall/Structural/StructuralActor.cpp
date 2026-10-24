// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "StructuralActor.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

AStructuralActor::AStructuralActor()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// Minimal placeholder root; real static meshes assigned per-spec in Phase 3.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AStructuralActor::ServerInitialize(const FStructuralSpec& InSpec, int32 InOwnerId)
{
	if (!HasAuthority())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("ServerInitialize called on client — rejected"));
		return;
	}
	Spec = InSpec;
	Durability = InSpec.MaxDurability;
	OwnerPlayerId = InOwnerId;
	bIsClaimed = InOwnerId >= 0;
	OnRep_Spec();
}

bool AStructuralActor::ServerApplyDamage_Validate(float DamageAmount)
{
	return DamageAmount >= 0.f && DamageAmount <= 1000.f;
}

void AStructuralActor::ServerApplyDamage_Implementation(float DamageAmount)
{
	// Server-authoritative durability. No client value trusted.
	Durability = FMath::Clamp(Durability - DamageAmount, 0.f, Spec.MaxDurability);
	if (Durability <= 0.f)
	{
		// Phase 3: breach/destroy + persistence event. Keep actor for now (vertical slice).
		UE_LOG(LogAfterfall, Log, TEXT("Structural part destroyed (%d)"), (int32)Spec.Part);
	}
}

bool AStructuralActor::ServerRepair_Validate()
{
	return true;
}

void AStructuralActor::ServerRepair_Implementation()
{
	// Phase 3 TODO: check resources + ownership before restoring.
	// Stub keeps server authority while gameplay logic lands.
	Durability = Spec.MaxDurability;
}

void AStructuralActor::OnRep_Spec()
{
	// Phase 3 TODO: swap mesh/material + damage-state visual.
}

void AStructuralActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStructuralActor, Spec);
	DOREPLIFETIME(AStructuralActor, Durability);
	DOREPLIFETIME(AStructuralActor, OwnerPlayerId);
	DOREPLIFETIME(AStructuralActor, bIsClaimed);
}
