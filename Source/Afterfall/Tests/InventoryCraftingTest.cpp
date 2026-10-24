// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Tests) — mirrors Scripts/validate_inventory_crafting.py.
#include "Misc/AutomationTest.h"
#include "Inventory/InventoryComponent.h"
#include "Crafting/CraftingService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWeightTest, "Afterfall.Inventory.Weight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInventoryWeightTest::RunTest(const FString& P)
{
	TestEqual(TEXT("wood 0.5kg"), UInventoryComponent::WeightForItem(TEXT("res_wood_log")), 0.5f);
	TestEqual(TEXT("nail 0.05kg"), UInventoryComponent::WeightForItem(TEXT("mat_nail")), 0.05f);
	TestEqual(TEXT("unknown 1kg"), UInventoryComponent::WeightForItem(TEXT("nope")), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCraftingInputsTest, "Afterfall.Crafting.Inputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCraftingInputsTest::RunTest(const FString& P)
{
	const FCraftRecipe* Wall = UCraftingService::FindRecipe(TEXT("wall"));
	TestNotNull(TEXT("wall recipe exists"), Wall);
	TMap<FName, int32> Rich; Rich.Add(TEXT("mat_plank"), 4); Rich.Add(TEXT("mat_nail"), 4);
	int32 Missing = -99;
	TestTrue(TEXT("rich can craft wall"), UCraftingService::HasInputs(Rich, *Wall, Missing));
	TMap<FName, int32> Poor; Poor.Add(TEXT("mat_plank"), 4); Poor.Add(TEXT("mat_nail"), 1);
	TestFalse(TEXT("poor blocked"), UCraftingService::HasInputs(Poor, *Wall, Missing));
	TestEqual(TEXT("missing index is nails"), Missing, 1);
	TArray<FCraftIngredient> Ret = UCraftingService::DismantleReturn(*Wall);
	TestEqual(TEXT("dismantle planks 2"), Ret[0].Count, 2);
	TestEqual(TEXT("dismantle nails 2"), Ret[1].Count, 2);
	TestNull(TEXT("unknown recipe null"), UCraftingService::FindRecipe(TEXT("nope")));
	return true;
}

#endif
