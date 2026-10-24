// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Character) — Phase 3b: full server validation wiring.
#include "AfterfallCharacter.h"
#include "Structural/StructuralActor.h"
#include "Structural/ConstructionValidator.h"
#include "Inventory/InventoryComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Afterfall.h"

AAfterfallCharacter::AAfterfallCharacter()
{
	bReplicates = true;
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
}

FPlacementRequest AAfterfallCharacter::BuildPreviewRequest(EStructuralPart Part, FVector HitLocation, FRotator ViewRotation) const
{
	// Canonical 50cm snap. Pure client-side preview helper — not authoritative.
	FPlacementRequest Req;
	Req.Part = Part;
	Req.GridLocation = UConstructionValidator::SnapToGrid(HitLocation);
	Req.Rotation = FRotator(0.f, FMath::GridSnap(ViewRotation.Yaw, 90.f), 0.f);
	return Req;
}

bool AAfterfallCharacter::ServerRequestPlacement_Validate(const FPlacementRequest& Request)
{
	return Request.GridLocation.IsFinite();
}

static int32 GetRequesterPlayerId(const AAfterfallCharacter* Char)
{
	// Vertical-slice stub: PlayerState ID when present, else 0 (listen-server host).
	// Replaced by persistent account ID in Phase 6.
	if (Char && Char->GetPlayerState())
	{
		return Char->GetPlayerState()->GetPlayerId();
	}
	return 0;
}

void AAfterfallCharacter::ServerRequestPlacement_Implementation(const FPlacementRequest& Request)
{
	if (!HasAuthority() || !Inventory)
	{
		return;
	}

	// 1. Grid re-enforcement via shared validator (never trust client).
	FString Reason;
	const FVector Snapped = UConstructionValidator::SnapToGrid(Request.GridLocation);
	if (!UConstructionValidator::IsOnGrid(Request.GridLocation))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: off-grid"));
		return;
	}

	// 2. Support query: count structural parts near support socket (50cm below center).
	int32 NumSupporting = 0;
	{
		const FVector Socket = Snapped - FVector(0.f, 0.f, 25.f);
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UConstructionSupport), false);
		GetWorld()->OverlapMultiByChannel(
			Overlaps, Socket, FQuat::Identity, ECC_WorldStatic,
			FCollisionShape::MakeBox(FVector(30.f, 30.f, 30.f)), Params);
		for (const FOverlapResult& O : Overlaps)
		{
			if (Cast<AStructuralActor>(O.GetActor()))
			{
				NumSupporting++;
			}
		}
		// Foundations always count as self-anchored for bootstrapping.
		if (Request.Part == EStructuralPart::Foundation)
		{
			NumSupporting = FMath::Max(NumSupporting, 1);
		}
	}

	// 3. Ownership: reject when overlapping a part claimed by another player.
	bool bClaimedByOther = false;
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UConstructionOwnership), false);
		GetWorld()->OverlapMultiByChannel(
			Overlaps, Snapped, FQuat::Identity, ECC_WorldStatic,
			FCollisionShape::MakeBox(FVector(25.f, 25.f, 25.f)), Params);
		const int32 Me = GetRequesterPlayerId(this);
		for (const FOverlapResult& O : Overlaps)
		{
			if (AStructuralActor* Other = Cast<AStructuralActor>(O.GetActor()))
			{
				if (Other->bIsClaimed && Other->OwnerPlayerId != Me)
				{
					bClaimedByOther = true;
					break;
				}
			}
		}
	}

	// 4. Full shared validation (grid + support + ownership + cost).
	if (!UConstructionValidator::ValidatePlacement(
		Request.Part, Snapped, NumSupporting, bClaimedByOther, Inventory->GetWood(), Reason))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: %s"), *Reason);
		return;
	}

	// 5. Debit resources FIRST (server-authoritative wallet), then spawn.
	const int32 Cost = UConstructionValidator::GetMaterialCost(Request.Part);
	if (!Inventory->ServerTryConsume(Cost))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: consume failed"));
		return;
	}

	FStructuralSpec Spec;
	Spec.Part = Request.Part;
	Spec.MaterialCost = Cost;
	Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Request.Part);
	Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Request.Part);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	if (AStructuralActor* NewPart = GetWorld()->SpawnActor<AStructuralActor>(AStructuralActor::StaticClass(), Snapped, Request.Rotation, SpawnParams))
	{
		NewPart->ServerInitialize(Spec, GetRequesterPlayerId(this));
		UE_LOG(LogAfterfall, Log, TEXT("Placement approved %d at %s (cost %d)"), (int32)Request.Part, *Snapped.ToString(), Cost);
	}
	else
	{
		// Refund on spawn collision failure so rejects never eat resources.
		Inventory->ServerAddResource(Cost);
		UE_LOG(LogAfterfall, Warning, TEXT("Placement failed: collision, refunded %d"), Cost);
	}
}

bool AAfterfallCharacter::ServerRequestRepair_Validate(AStructuralActor* Target)
{
	return Target != nullptr;
}

void AAfterfallCharacter::ServerRequestRepair_Implementation(AStructuralActor* Target)
{
	if (!HasAuthority() || !Target || !Inventory)
	{
		return;
	}
	const float Missing = Target->Spec.MaxDurability - Target->Durability;
	if (Missing <= 1.f)
	{
		return;
	}
	// Rule: 1 Wood per 50 missing durability, rounded up.
	const int32 Cost = FMath::Clamp(FMath::CeilToInt(Missing / 50.f), 1, 20);
	if (!Inventory->ServerTryConsume(Cost))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Repair rejected: need %d wood"), Cost);
		return;
	}
	Target->ServerRepair();
}

bool AAfterfallCharacter::ServerRequestUpgrade_Validate(AStructuralActor* Target)
{
	return Target != nullptr;
}

void AAfterfallCharacter::ServerRequestUpgrade_Implementation(AStructuralActor* Target)
{
	if (!HasAuthority() || !Target || !Inventory)
	{
		return;
	}
	if (Target->UpgradeTier != 0)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Upgrade rejected: already tier %d"), Target->UpgradeTier);
		return;
	}
	static constexpr int32 UpgradeCost = 2;
	if (!Inventory->ServerTryConsume(UpgradeCost))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Upgrade rejected: need %d wood"), UpgradeCost);
		return;
	}
	Target->ServerUpgrade();
}

void AAfterfallCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// EnhancedInput bindings land in Phase 2b (Input Mapping Context asset).
}
