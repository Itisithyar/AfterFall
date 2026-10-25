// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "StructuralActor.h"
#include "Destruction/DestructionPropagation.h"
#include "ConstructionValidator.h"
#include "Net/UnrealNetwork.h"
#include "Afterfall.h"

AStructuralActor::AStructuralActor()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// Minimal placeholder root; real static meshes assigned per-spec in Phase 3.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Destruction = CreateDefaultSubobject<UStructuralDestructionComponent>(TEXT("Destruction"));
}

void AStructuralActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshDestructionVisual();
}

void AStructuralActor::RefreshDestructionVisual()
{
	if (Destruction)
	{
		Destruction->Rebuild(DetachedCells);
	}
}

void AStructuralActor::OnRep_DetachedCells()
{
	// Replicated state changed (new client, late join, or live update):
	// rebuild local geometry/collision from authority. Idempotent.
	RefreshDestructionVisual();
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
	if (!StableGuid.IsValid())
	{
		StableGuid = FGuid::NewGuid();
	}
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
	CheckMaterialFailure();
}

bool AStructuralActor::ServerRepair_Validate()
{
	return true;
}

void AStructuralActor::ServerRepair_Implementation()
{
	// Cost pre-consumed by caller (AAfterfallCharacter) via UInventoryComponent::ServerTryConsume.
	// This keeps a single authority path for resources while repair stays server-only.
	// Repair cost rule: 1 Wood per 50 missing durability, rounded up (see ConstructionValidator docs).
	Durability = Spec.MaxDurability;
}

bool AStructuralActor::ServerUpgrade_Validate()
{
	return true;
}

void AStructuralActor::ServerUpgrade_Implementation()
{
	// Cost pre-consumed by caller (2 Wood). Vertical slice allows tier 0 -> 1 only.
	if (UpgradeTier != 0)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Upgrade rejected: already tier %d"), UpgradeTier);
		return;
	}
	UpgradeTier = 1;
	Spec.MaxDurability *= 1.5f;
	Durability = Spec.MaxDurability;
}

bool AStructuralActor::ServerClaim_Validate(int32 ClaimerPlayerId)
{
	return ClaimerPlayerId >= 0;
}

void AStructuralActor::ServerClaim_Implementation(int32 ClaimerPlayerId)
{
	// First-claimer wins: server processes RPCs in order, rejects late claims.
	if (bIsClaimed)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Claim rejected: already owned by %d"), OwnerPlayerId);
		return;
	}
	OwnerPlayerId = ClaimerPlayerId;
	bIsClaimed = true;
}

static constexpr int32 MaxTrackedDestructionEvents = 8;

bool AStructuralActor::ServerApplyDestruction_Validate(const TArray<int32>& Cells, float DamagePerCell, const FGuid& EventId)
{
	return Cells.Num() <= 27 && DamagePerCell >= 0.f && DamagePerCell <= 1000.f;
}

void AStructuralActor::ServerApplyDestruction_Implementation(const TArray<int32>& Cells, float DamagePerCell, const FGuid& EventId)
{
	ApplyDestructionCells(Cells, DamagePerCell, EventId);
}

int32 AStructuralActor::ApplyDestructionCells(const TArray<int32>& Cells, float DamagePerCell, const FGuid& EventId)
{
	if (!HasAuthority())
	{
		return 0;
	}
	// Event-level replay protection (distinct from cell dedupe below): the same
	// event never applies twice; two distinct events may share cells freely.
	if (EventId.IsValid())
	{
		if (RecentEventIds.Contains(EventId))
		{
			UE_LOG(LogAfterfall, Verbose, TEXT("Destruction event replay ignored (%s)"), *EventId.ToString());
			return 0;
		}
		RecentEventIds.Add(EventId);
		while (RecentEventIds.Num() > MaxTrackedDestructionEvents)
		{
			RecentEventIds.RemoveAt(0); // bounded ledger: oldest window expires
		}
	}
	// Single commit point: dedupe first (duplicate replay adds nothing), then
	// debit durability exactly once per NEW cell. Unloading is not destruction:
	// DetachedCells persist on the root and are re-applied via ImportDelta.
	const int32 Added = UDestructionPropagationService::MergeDetached(DetachedCells, Cells);
	if (Added > 0)
	{
		Durability = FMath::Clamp(Durability - DamagePerCell * Added, 0.f, Spec.MaxDurability);
		UE_LOG(LogAfterfall, Log, TEXT("Destruction commit +%d cells (%d total), durability %.0f"),
			Added, DetachedCells.Num(), Durability);
		RefreshDestructionVisual();
	}
	CheckMaterialFailure();
	return Added;
}

int32 AStructuralActor::ServerApplyImpact(const FDestructionEvent& Event, float DamagePerCell, const FGuid& EventId)
{
	if (!HasAuthority())
	{
		return 0;
	}
	// Server-side validation: never trust client-supplied cells — they are
	// recomputed here. Clamp energy, require a real direction, require the
	// origin near this structure (impacts are local by construction).
	const FVector Dir = Event.Direction.GetSafeNormal();
	if (Dir.IsNearlyZero() || DamagePerCell < 0.f || DamagePerCell > 1000.f)
	{
		return 0;
	}
	if (FVector::Dist(Event.Origin, GetActorLocation()) > 500.f)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Impact rejected: origin far from target"));
		return 0;
	}
	// Frame conversion: propagation indices live in actor-local space — the same
	// frame as the visual cell grid (ISMC instances are actor-local). Without
	// this, rotated actors would detach world-frame cells that do not match
	// their rotated visuals. Translation + 90° yaw are covered exactly.
	const FTransform Inv = GetActorTransform().Inverse();
	FDestructionEvent Validated = Event;
	Validated.Origin = Inv.TransformPosition(Event.Origin);
	Validated.Direction = Inv.TransformVector(Dir).GetSafeNormal();
	if (Validated.Direction.IsNearlyZero())
	{
		return 0;
	}
	Validated.InitialEnergy = FMath::Clamp(Event.InitialEnergy, 0.f, 500.f);
	const float Extent = Destruction ? Destruction->CellExtentCm : 50.f;
	const FPropagationResult R = UDestructionPropagationService::Propagate(
		Validated, FVector::ZeroVector, Extent);
	return ApplyDestructionCells(R.DetachedBreakpoints, DamagePerCell, EventId);
}

void AStructuralActor::CheckMaterialFailure()
{
	if (!HasAuthority() || bMaterialFailed || Durability > 0.f)
	{
		return;
	}
	// Structural failure initiates its own propagation (gravity-driven, downward
	// from the center) without a new projectile. Fires exactly once per life;
	// the flag persists via FStructureDelta so reload never re-triggers it.
	bMaterialFailed = true;
	FDestructionEvent Failure;
	Failure.Origin = GetActorLocation();
	Failure.Direction = FVector::DownVector;
	Failure.InitialEnergy = UDestructionPropagationService::FailureEnergy();
	Failure.Initiator = EDestructionInitiator::MaterialFailure;
	Failure.MaterialId = UDestructionPropagationService::MaterialForPart(Spec.Part);
	UE_LOG(LogAfterfall, Log, TEXT("Material failure triggered (%d)"), (int32)Spec.Part);
	ServerApplyImpact(Failure, UDestructionPropagationService::FailureDamagePerCell(), FGuid::NewGuid());
}

FStructureDelta AStructuralActor::ExportDelta() const
{
	FStructureDelta D;
	D.Guid = StableGuid;
	D.PartId = FName(StaticEnum<EStructuralPart>()->GetNameStringByValue((int64)Spec.Part));
	D.Location = GetActorLocation();
	D.Rotation = GetActorRotation();
	D.Durability = Durability;
	D.Tier = UpgradeTier;
	D.OwnerPlayerId = OwnerPlayerId;
	D.bClaimed = bIsClaimed;
	D.bDestroyed = Durability <= 0.f;
	D.DetachedCells = DetachedCells;
	D.bFailed = bMaterialFailed;
	D.RecentEventIds.Reset();
	for (int32 i = FMath::Max(0, RecentEventIds.Num() - MaxTrackedDestructionEvents);
		i < RecentEventIds.Num(); ++i)
	{
		D.RecentEventIds.Add(RecentEventIds[i]);
	}
	return D;
}

void AStructuralActor::ImportDelta(const FStructureDelta& Delta)
{
	if (!HasAuthority())
	{
		return;
	}
	// Restore is authoritative and idempotent: cells merge, values clamp.
	// A re-instantiated actor never resurrects intact geometry over damage.
	// Spec is reconstructed from the recorded PartId so durability clamps and
	// material lookups match the saved part even on a default-constructed actor.
	if (Delta.Guid.IsValid())
	{
		StableGuid = Delta.Guid;
	}
	const int64 PartValue = StaticEnum<EStructuralPart>()->GetValueByNameString(Delta.PartId.ToString());
	if (PartValue != INDEX_NONE)
	{
		Spec.Part = static_cast<EStructuralPart>(PartValue);
		Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Spec.Part);
		Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Spec.Part);
	}
	UDestructionPropagationService::MergeDetached(DetachedCells, Delta.DetachedCells);
	bMaterialFailed = Delta.bFailed;
	RecentEventIds.Reset();
	for (const FGuid& Id : Delta.RecentEventIds)
	{
		if (Id.IsValid() && !RecentEventIds.Contains(Id))
		{
			RecentEventIds.Add(Id);
		}
	}
	while (RecentEventIds.Num() > MaxTrackedDestructionEvents)
	{
		RecentEventIds.RemoveAt(0);
	}
	Durability = FMath::Clamp(Delta.Durability, 0.f, Spec.MaxDurability > 0.f ? Spec.MaxDurability : Delta.Durability);
	RefreshDestructionVisual();
	UpgradeTier = Delta.Tier;
	OwnerPlayerId = Delta.OwnerPlayerId;
	bIsClaimed = Delta.bClaimed;
	SetActorLocation(Delta.Location);
	SetActorRotation(Delta.Rotation);
}

void AStructuralActor::OnRep_Spec()
{
	// Phase 3 TODO: swap mesh/material + damage-state visual.
	// DetachedCells replication drives the local damage visual (next slice).
}

void AStructuralActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStructuralActor, Spec);
	DOREPLIFETIME(AStructuralActor, Durability);
	DOREPLIFETIME(AStructuralActor, OwnerPlayerId);
	DOREPLIFETIME(AStructuralActor, bIsClaimed);
	DOREPLIFETIME(AStructuralActor, UpgradeTier);
	DOREPLIFETIME(AStructuralActor, StableGuid);
	DOREPLIFETIME(AStructuralActor, DetachedCells);
}
