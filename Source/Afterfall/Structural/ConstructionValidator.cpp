// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Structural/ConstructionValidator.h"
#include "Afterfall.h"

int32 UConstructionValidator::GetMaterialCost(EStructuralPart Part)
{
	// Single-currency stub (Wood) for vertical slice. Multi-resource lands later.
	switch (Part)
	{
	case EStructuralPart::Foundation: return 4;
	case EStructuralPart::Floor:      return 2;
	case EStructuralPart::Wall:       return 2;
	case EStructuralPart::Door:       return 3;
	case EStructuralPart::Window:     return 3;
	case EStructuralPart::Roof:       return 3;
	case EStructuralPart::Barricade:  return 1;
	case EStructuralPart::Lock:       return 1;
	default: return 2;
	}
}

float UConstructionValidator::GetMaxDurability(EStructuralPart Part)
{
	switch (Part)
	{
	case EStructuralPart::Foundation: return 300.f;
	case EStructuralPart::Floor:      return 150.f;
	case EStructuralPart::Wall:       return 100.f;
	case EStructuralPart::Door:       return 80.f;
	case EStructuralPart::Window:     return 60.f;
	case EStructuralPart::Roof:       return 120.f;
	case EStructuralPart::Barricade:  return 50.f;
	case EStructuralPart::Lock:       return 40.f;
	default: return 100.f;
	}
}

bool UConstructionValidator::RequiresSupport(EStructuralPart Part)
{
	// Foundations anchor everything; everything else needs at least 1 support.
	return Part != EStructuralPart::Foundation;
}

bool UConstructionValidator::IsOnGrid(const FVector& Location)
{
	const FVector Snapped = SnapToGrid(Location);
	return Snapped.Equals(Location, 1.f);
}

FVector UConstructionValidator::SnapToGrid(const FVector& Location)
{
	return FVector(
		FMath::GridSnap(Location.X, 50.f),
		FMath::GridSnap(Location.Y, 50.f),
		FMath::GridSnap(Location.Z, 50.f));
}

bool UConstructionValidator::ValidatePlacement(
	EStructuralPart Part,
	const FVector& GridLocation,
	int32 NumSupportingParts,
	bool bLotClaimedByOther,
	int32 ResourcesAvailable,
	FString& OutReason)
{
	if (!IsOnGrid(GridLocation))
	{
		OutReason = TEXT("off-grid");
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: off-grid at %s"), *GridLocation.ToString());
		return false;
	}
	if (bLotClaimedByOther)
	{
		OutReason = TEXT("lot-claimed-by-other");
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: lot claimed"));
		return false;
	}
	if (RequiresSupport(Part) && NumSupportingParts < 1)
	{
		OutReason = TEXT("no-support");
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: no structural support"));
		return false;
	}
	const int32 Cost = GetMaterialCost(Part);
	if (ResourcesAvailable < Cost)
	{
		OutReason = FString::Printf(TEXT("insufficient-resources (need %d, have %d)"), Cost, ResourcesAvailable);
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: %s"), *OutReason);
		return false;
	}
	OutReason.Empty();
	return true;
}
