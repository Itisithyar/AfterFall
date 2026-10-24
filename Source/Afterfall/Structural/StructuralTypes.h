// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Structural team — SHARED definitions used by BOTH procedural
// buildings (Phase 4) and player construction (Phase 3). Do not fork.
#pragma once

#include "CoreMinimal.h"
#include "StructuralTypes.generated.h"

UENUM(BlueprintType)
enum class EStructuralPart : uint8
{
	Foundation UMETA(DisplayName = "Foundation"),
	Floor      UMETA(DisplayName = "Floor"),
	Wall       UMETA(DisplayName = "Wall"),
	Door       UMETA(DisplayName = "Door"),
	Window     UMETA(DisplayName = "Window"),
	Roof       UMETA(DisplayName = "Roof"),
	Barricade  UMETA(DisplayName = "Barricade"),
	Lock       UMETA(DisplayName = "Lock")
};

USTRUCT(BlueprintType)
struct FStructuralSpec
{
	GENERATED_BODY()

	// 50cm grid is canonical for snapping (matches procedural lots in Phase 4).
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	EStructuralPart Part = EStructuralPart::Wall;

	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	FVector GridSize = FVector(50.f, 50.f, 50.f);

	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	int32 MaterialCost = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	float MaxDurability = 100.f;

	// Requires structural support (foundation/floor/wall below or adjacent).
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	bool bRequiresSupport = true;
};

USTRUCT(BlueprintType)
struct FPlacementRequest
{
	GENERATED_BODY()

	UPROPERTY()
	EStructuralPart Part = EStructuralPart::Wall;

	// Quantized grid location proposed by client preview.
	UPROPERTY()
	FVector_NetQuantize GridLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	// Client MUST NOT send cost/durability/ownership — server recomputes.
};
