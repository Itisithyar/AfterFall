// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Crafting) — pure combination engine, no world/IO.
#include "Crafting/ItemCombiner.h"

bool UItemCombiner::PlanConsumption(const FCraftRecipe& Recipe,
	const TArray<FItemInstance>& Provided,
	TArray<FCombineConsumeStep>& OutPlan, FString& OutError)
{
	OutPlan.Reset();
	OutError.Reset();

	if (Recipe.Inputs.Num() == 0)
	{
		OutError = TEXT("recipe declares no inputs");
		return false;
	}
	if (Provided.Num() == 0)
	{
		OutError = TEXT("no input instances supplied");
		return false;
	}

	// Distinct, live instances only: an id repeated in one request would let
	// the same unit satisfy two inputs (dup vector).
	TSet<FGuid> Seen;
	for (const FItemInstance& P : Provided)
	{
		if (P.ItemId.IsNone() || P.Count <= 0)
		{
			OutError = TEXT("supplied instance is empty");
			return false;
		}
		if (Seen.Contains(P.InstanceId))
		{
			OutError = FString::Printf(TEXT("instance %s supplied twice"), *P.InstanceId.ToString(EGuidFormats::Digits));
			return false;
		}
		Seen.Add(P.InstanceId);
	}

	// Greedy, order-given: for each recipe input, draw from the supplied
	// instances of that item id until satisfied.
	TArray<int32> Taken;
	Taken.Init(0, Provided.Num());
	for (const FCraftIngredient& In : Recipe.Inputs)
	{
		if (In.ItemId.IsNone() || In.Count <= 0)
		{
			OutError = FString::Printf(TEXT("recipe %s has invalid input entry"), *Recipe.RecipeId.ToString());
			return false;
		}
		int32 Need = In.Count;
		for (int32 i = 0; i < Provided.Num() && Need > 0; ++i)
		{
			if (Provided[i].ItemId != In.ItemId)
			{
				continue;
			}
			const int32 Available = Provided[i].Count - Taken[i];
			if (Available <= 0)
			{
				continue;
			}
			const int32 Take = FMath::Min(Need, Available);
			Taken[i] += Take;
			Need -= Take;
		}
		if (Need > 0)
		{
			OutError = FString::Printf(TEXT("missing input %s x%d (%d short)"),
				*In.ItemId.ToString(), In.Count, Need);
			return false;
		}
	}

	// Strict request semantics: every supplied instance must take part.
	// An unused instance means the caller's intent is ambiguous.
	for (int32 i = 0; i < Provided.Num(); ++i)
	{
		if (Taken[i] <= 0)
		{
			OutError = FString::Printf(TEXT("instance %s not required by this recipe"),
				*Provided[i].InstanceId.ToString(EGuidFormats::Digits));
			return false;
		}
		FCombineConsumeStep Step;
		Step.ProvidedIndex = i;
		Step.Count = Taken[i];
		OutPlan.Add(Step);
	}
	return true;
}

float UItemCombiner::InheritCondition(const TArray<FItemInstance>& Parents)
{
	// Weakest-link: the child cannot be better than the worst consumed
	// input. Deterministic, bounded, no averaging of unrelated properties.
	bool bAny = false;
	float MinCondition = 1.f;
	for (const FItemInstance& P : Parents)
	{
		if (P.ItemId.IsNone() || P.Count <= 0)
		{
			continue;
		}
		const float C = FMath::Clamp(P.Condition, 0.f, 1.f);
		MinCondition = bAny ? FMath::Min(MinCondition, C) : C;
		bAny = true;
	}
	return bAny ? MinCondition : 1.f;
}

FCombineOutcome UItemCombiner::BuildChild(const FCraftRecipe& Recipe,
	const TArray<FItemInstance>& Parents, const FGuid& TransactionId)
{
	FCombineOutcome Out;
	if (Recipe.OutputItem.IsNone() || Recipe.OutputCount <= 0)
	{
		Out.Error = FString::Printf(TEXT("recipe %s has no valid output"), *Recipe.RecipeId.ToString());
		return Out;
	}
	if (Parents.Num() == 0)
	{
		Out.Error = TEXT("no parents to record");
		return Out;
	}

	// Fresh identity: a crafted product is a new tracked item, never a
	// reuse of a parent id.
	Out.Child = FItemInstance(Recipe.OutputItem, Recipe.OutputCount);
	Out.Child.Condition = InheritCondition(Parents);
	for (const FItemInstance& P : Parents)
	{
		if (!Out.Child.ParentInstanceIds.Contains(P.InstanceId))
		{
			Out.Child.ParentInstanceIds.Add(P.InstanceId);
		}
	}
	Out.Child.ProducedByRecipe = Recipe.RecipeId;
	Out.Child.RecipeVersion = Recipe.RecipeVersion;
	Out.Child.CreationTransactionId = TransactionId;
	Out.Child.CustomData = FString::Printf(TEXT("made_by=%s@v%d tx=%s"),
		*Recipe.RecipeId.ToString(), Recipe.RecipeVersion,
		*TransactionId.ToString(EGuidFormats::Digits));
	Out.bSuccess = true;
	return Out;
}
