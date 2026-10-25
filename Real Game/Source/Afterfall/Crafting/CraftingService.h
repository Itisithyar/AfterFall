// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Survival) — server-only recipe validation.
// Recipes mirror Content/Data/Recipes (slice 12). No client cost trust.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CraftingService.generated.h"

USTRUCT(BlueprintType)
struct FCraftIngredient
{
	GENERATED_BODY()
	UPROPERTY() FName ItemId;
	UPROPERTY() int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FCraftRecipe
{
	GENERATED_BODY()
	UPROPERTY() FName RecipeId;
	/** Rule version recorded on crafted children for provenance/migration. */
	UPROPERTY() int32 RecipeVersion = 1;
	UPROPERTY() FName OutputItem;
	UPROPERTY() int32 OutputCount = 1;
	UPROPERTY() TArray<FCraftIngredient> Inputs;
	UPROPERTY() float CraftSeconds = 3.f;
	UPROPERTY() bool bNeedsHammer = false;
	UPROPERTY() bool bNoisy = false;
};

UCLASS()
class AFTERFALL_API UCraftingService : public UObject
{
	GENERATED_BODY()

public:
	// Slice recipe table (mirrors DT_Recipes v1). Server is authority.
	static const TArray<FCraftRecipe>& GetSliceRecipes();

	static const FCraftRecipe* FindRecipe(FName RecipeId);

	// Pure checks (unit-tested): have all inputs? On fail, MissingIndex >= 0.
	static bool HasInputs(const TMap<FName, int32>& Wallet, const FCraftRecipe& Recipe, int32& MissingIndex);

	// Dismantle returns floor(50% of craft inputs). Never profits.
	static TArray<FCraftIngredient> DismantleReturn(const FCraftRecipe& Recipe);
};
