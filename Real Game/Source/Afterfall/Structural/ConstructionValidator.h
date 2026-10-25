// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Structural team — server-only validation. Pure functions so they
// are unit-testable via automation (Phase 3b) and reusable by proc-gen (Phase 4).
#pragma once

#include "CoreMinimal.h"
#include "StructuralTypes.h"
#include "UObject/NoExportTypes.h"
#include "ConstructionValidator.generated.h"

UCLASS()
class AFTERFALL_API UConstructionValidator : public UObject
{
	GENERATED_BODY()

public:
	// Canonical cost table — server is the ONLY authority. Client must not send costs.
	static int32 GetMaterialCost(EStructuralPart Part);

	static float GetMaxDurability(EStructuralPart Part);

	// True if Part requires a supporting part beneath/adjacent.
	static bool RequiresSupport(EStructuralPart Part);

	// Grid re-enforcement: 50cm snap within 1cm tolerance.
	static bool IsOnGrid(const FVector& Location);

	static FVector SnapToGrid(const FVector& Location);

	// Returns false with OutReason set when placement must be rejected.
	// NumSupportingParts = overlap query count of foundations/floors/walls at support socket.
	// bLotClaimedByOther = ownership/lock check from persistent region state.
	// ResourcesAvailable = server-side inventory count for the single cost currency (stub: Wood).
	static bool ValidatePlacement(
		EStructuralPart Part,
		const FVector& GridLocation,
		int32 NumSupportingParts,
		bool bLotClaimedByOther,
		int32 ResourcesAvailable,
		FString& OutReason);
};
