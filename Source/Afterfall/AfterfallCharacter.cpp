// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "AfterfallCharacter.h"
#include "Structural/StructuralActor.h"
#include "Afterfall.h"

AAfterfallCharacter::AAfterfallCharacter()
{
	bReplicates = true;
}

FPlacementRequest AAfterfallCharacter::BuildPreviewRequest(EStructuralPart Part, FVector HitLocation, FRotator ViewRotation) const
{
	// Canonical 50cm snap. Pure client-side preview helper — not authoritative.
	FPlacementRequest Req;
	Req.Part = Part;
	Req.GridLocation = FVector(
		FMath::GridSnap(HitLocation.X, 50.f),
		FMath::GridSnap(HitLocation.Y, 50.f),
		FMath::GridSnap(HitLocation.Z, 50.f));
	// Yaw-only snap to 90 degrees for walls/doors; free for others (stub).
	Req.Rotation = FRotator(0.f, FMath::GridSnap(ViewRotation.Yaw, 90.f), 0.f);
	return Req;
}

bool AAfterfallCharacter::ServerRequestPlacement_Validate(const FPlacementRequest& Request)
{
	// Cheap pre-checks only; full validation in _Implementation.
	return Request.GridLocation.IsFinite();
}

void AAfterfallCharacter::ServerRequestPlacement_Implementation(const FPlacementRequest& Request)
{
	if (!HasAuthority())
	{
		return;
	}

	// --- Server validation (Phase 3 acceptance) ---
	// 1. Grid snap re-enforced (never trust client quantization).
	const FVector Snapped(
		FMath::GridSnap(Request.GridLocation.X, 50.f),
		FMath::GridSnap(Request.GridLocation.Y, 50.f),
		FMath::GridSnap(Request.GridLocation.Z, 50.f));
	if (!Snapped.Equals(Request.GridLocation, 1.f))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: off-grid"));
		return;
	}

	// 2. TODO Phase 3: support check (foundation/floor/wall query).
	// 3. TODO Phase 3: resource cost + inventory debit (server-side inventory).
	// 4. TODO Phase 3: ownership/lock check on claimed lots.

	// Minimal spawn for vertical-slice prototype (wall only).
	FStructuralSpec Spec;
	Spec.Part = Request.Part;
	Spec.MaxDurability = 100.f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	if (AStructuralActor* NewPart = GetWorld()->SpawnActor<AStructuralActor>(AStructuralActor::StaticClass(), Snapped, Request.Rotation, Params))
	{
		// OwnerPlayerId stub: use PlayerState ID later. -1 = unclaimed for now.
		NewPart->ServerInitialize(Spec, -1);
		UE_LOG(LogAfterfall, Log, TEXT("Placement approved at %s"), *Snapped.ToString());
	}
}

void AAfterfallCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// EnhancedInput bindings land in Phase 2b (Input Mapping Context asset).
}
