// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Destruction - Structural reference grid and material destruction
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructionTypes.generated.h"

UENUM(BlueprintType)
enum class EDestructionMode : uint8
{
	Bullet UMETA(DisplayName = "Bullet"),
	Attack UMETA(DisplayName = "Attack"),
	MaterialFailure UMETA(DisplayName = "Material Failure")
};

UENUM(BlueprintType)
enum class EMaterialType : uint8
{
	Wood UMETA(DisplayName = "Wood"),
	Stone UMETA(DisplayName = "Stone"),
	Metal UMETA(DisplayName = "Metal"),
	Concrete UMETA(DisplayName = "Concrete"),
	Plaster UMETA(DisplayName = "Plaster")
};

USTRUCT(BlueprintType)
struct FStructuralReferenceGrid
{
	GENERATED_BODY()
	
	// 3x3x3 structural reference grid (explicit zero defaults: bare struct
	// members trip "not initialized properly" LogClass errors at runtime).
	UPROPERTY() FVector GridCenter = FVector::ZeroVector;
	UPROPERTY() FVector GridMin = FVector::ZeroVector;
	UPROPERTY() FVector GridMax = FVector::ZeroVector;
	UPROPERTY() float CellSize = 100.0f; // 1m cells
	
	// Anchor points (center and corners)
	UPROPERTY() TArray<FVector> AnchorPoints;
	
	// Potential breaking points
	UPROPERTY() TArray<FVector> BreakingPoints;
};

USTRUCT(BlueprintType)
struct FDestructionImpact
{
	GENERATED_BODY()
	
	UPROPERTY() FVector Origin = FVector::ZeroVector;
	UPROPERTY() FVector Direction = FVector::ZeroVector;
	UPROPERTY() float InitialEnergy = 100.0f;
	UPROPERTY() EDestructionMode Mode = EDestructionMode::Bullet;
	UPROPERTY() EMaterialType Material = EMaterialType::Wood;
	UPROPERTY() AActor* TargetActor = nullptr;
	UPROPERTY() FVector ImpactPoint = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FPropagationState
{
	GENERATED_BODY()
	
	UPROPERTY() float RemainingEnergy = 100.0f;
	UPROPERTY() FVector CurrentPosition = FVector::ZeroVector;
	UPROPERTY() FVector CurrentDirection = FVector::ZeroVector;
	UPROPERTY() int32 PropagationDepth = 0;
	UPROPERTY() TArray<FVector> PropagationPath;
	UPROPERTY() bool bHasExhausted = false;
	UPROPERTY() bool bHasBeenDenied = false;
};
