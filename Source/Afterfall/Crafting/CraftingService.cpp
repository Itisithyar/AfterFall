// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Crafting/CraftingService.h"

const TArray<FCraftRecipe>& UCraftingService::GetSliceRecipes()
{
	static TArray<FCraftRecipe> Recipes;
	if (Recipes.Num() == 0)
	{
		auto R = [&](const TCHAR* Id, const TCHAR* Out, int32 OutN, float Sec, bool Hammer, bool Noisy,
			std::initializer_list<std::pair<const TCHAR*, int32>> Ins) {
			FCraftRecipe Rec;
			Rec.RecipeId = Id; Rec.OutputItem = Out; Rec.OutputCount = OutN;
			Rec.CraftSeconds = Sec; Rec.bNeedsHammer = Hammer; Rec.bNoisy = Noisy;
			for (auto& P : Ins) { FCraftIngredient I; I.ItemId = P.first; I.Count = P.second; Rec.Inputs.Add(I); }
			Recipes.Add(Rec);
		};
		R(TEXT("plank"), TEXT("mat_plank"), 1, 3.f, false, false, { {TEXT("res_wood_log"), 2} });
		R(TEXT("rope"), TEXT("mat_rope"), 1, 4.f, false, false, { {TEXT("res_hemp_fiber"), 4} });
		R(TEXT("cloth"), TEXT("res_cloth"), 1, 4.f, false, false, { {TEXT("res_hemp_fiber"), 3} });
		R(TEXT("hatchet"), TEXT("tool_hatchet"), 1, 8.f, false, false, { {TEXT("res_wood_log"), 3}, {TEXT("res_stone"), 3}, {TEXT("res_hemp_fiber"), 2} });
		R(TEXT("barricade"), TEXT("bld_barricade"), 1, 5.f, false, true, { {TEXT("res_wood_log"), 4}, {TEXT("mat_rope"), 2} });
		R(TEXT("wall"), TEXT("bld_wall_wood"), 1, 10.f, true, true, { {TEXT("mat_plank"), 4}, {TEXT("mat_nail"), 4} });
		R(TEXT("door"), TEXT("bld_door_wood"), 1, 12.f, true, true, { {TEXT("mat_plank"), 6}, {TEXT("mat_nail"), 4} });
		R(TEXT("lock"), TEXT("lock_padlock"), 1, 8.f, true, false, { {TEXT("res_scrap_metal"), 4}, {TEXT("mat_rope"), 2} });
		R(TEXT("storage"), TEXT("sto_crate_wood"), 1, 10.f, true, true, { {TEXT("mat_plank"), 6}, {TEXT("mat_nail"), 4} });
		R(TEXT("bandage"), TEXT("res_med_bandage"), 1, 3.f, false, false, { {TEXT("res_cloth"), 2} });
		R(TEXT("pack"), TEXT("cloth_backpack_small"), 1, 15.f, false, false, { {TEXT("res_cloth"), 6}, {TEXT("mat_rope"), 2} });
		R(TEXT("bat"), TEXT("wp_bat"), 1, 5.f, false, false, { {TEXT("res_wood_log"), 2}, {TEXT("mat_nail"), 2} });
	}
	return Recipes;
}

const FCraftRecipe* UCraftingService::FindRecipe(FName RecipeId)
{
	for (const FCraftRecipe& R : GetSliceRecipes())
	{
		if (R.RecipeId == RecipeId)
		{
			return &R;
		}
	}
	return nullptr;
}

bool UCraftingService::HasInputs(const TMap<FName, int32>& Wallet, const FCraftRecipe& Recipe, int32& MissingIndex)
{
	MissingIndex = -1;
	for (int32 i = 0; i < Recipe.Inputs.Num(); i++)
	{
		const int32* Have = Wallet.Find(Recipe.Inputs[i].ItemId);
		if (!Have || *Have < Recipe.Inputs[i].Count)
		{
			MissingIndex = i;
			return false;
		}
	}
	return true;
}

TArray<FCraftIngredient> UCraftingService::DismantleReturn(const FCraftRecipe& Recipe)
{
	// Floor of 50%. Tools/weapons cap: scrap path handled by caller (max 1 scrap).
	TArray<FCraftIngredient> Out;
	for (const FCraftIngredient& I : Recipe.Inputs)
	{
		const int32 Ret = I.Count / 2;
		if (Ret > 0)
		{
			FCraftIngredient R; R.ItemId = I.ItemId; R.Count = Ret;
			Out.Add(R);
		}
	}
	return Out;
}
