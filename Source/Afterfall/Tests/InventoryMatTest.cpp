// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: UI (Tests) — inventory mat rules. Pure data: no renderer, no world,
// so these cover the logic half of the feature; the widget's own layout and
// click delivery still need PIE (see Docs handoff checklist).
#include "Misc/AutomationTest.h"
#include "Items/AfterfallInventoryComponent.h"
#include "UI/InventoryMatWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryMatSwapTest, "Afterfall.Inventory.MatSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInventoryMatSwapTest::RunTest(const FString& P)
{
	UAfterfallInventoryComponent* Inv =
		NewObject<UAfterfallInventoryComponent>(GetTransientPackage());

	Inv->AddItem(TEXT("mat_plank"), 4);
	Inv->AddItem(TEXT("mat_nail"), 8);
	TestTrue(TEXT("two slots after two adds"), Inv->Slots.Num() == 2);
	TestTrue(TEXT("slot0 = planks x4"),
		Inv->Slots.Num() == 2 && Inv->Slots[0].ItemID == FName(TEXT("mat_plank")) && Inv->Slots[0].Quantity == 4);

	Inv->SwapSlots(0, 1);
	TestTrue(TEXT("swap moves nails to slot0"),
		Inv->Slots.Num() == 2 && Inv->Slots[0].ItemID == FName(TEXT("mat_nail")));
	TestTrue(TEXT("quantity travels with the stack"), Inv->Slots.Num() == 2 && Inv->Slots[0].Quantity == 8);
	TestTrue(TEXT("swap is symmetric"), Inv->Slots[1].ItemID == FName(TEXT("mat_plank")));

	// Out-of-range and same-index swaps must be no-ops, not corruption.
	Inv->SwapSlots(0, 5);
	Inv->SwapSlots(1, 1);
	TestTrue(TEXT("invalid swaps leave the mat intact"), Inv->Slots.Num() == 2);
	TestTrue(TEXT("still nails first"), Inv->Slots[0].ItemID == FName(TEXT("mat_nail")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryMatClickTest, "Afterfall.Inventory.MatClick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInventoryMatClickTest::RunTest(const FString& P)
{
	bool bSwap = false;

	// Empty selection: first click selects and must not swap.
	int32 Sel = UInventoryMatWidget::ResolveClick(INDEX_NONE, 2, 6, bSwap);
	TestTrue(TEXT("first click selects"), Sel == 2);
	TestTrue(TEXT("first click does not swap"), !bSwap);

	// Clicking the selection clears it.
	Sel = UInventoryMatWidget::ResolveClick(2, 2, 6, bSwap);
	TestTrue(TEXT("re-click clears selection"), Sel == INDEX_NONE);
	TestTrue(TEXT("re-click does not swap"), !bSwap);

	// Clicking a different slot swaps and clears.
	Sel = UInventoryMatWidget::ResolveClick(2, 5, 6, bSwap);
	TestTrue(TEXT("second click requests swap"), bSwap);
	TestTrue(TEXT("selection cleared after swap"), Sel == INDEX_NONE);

	// Out-of-range clicks change nothing.
	bSwap = false;
	Sel = UInventoryMatWidget::ResolveClick(2, 99, 6, bSwap);
	TestTrue(TEXT("bad click keeps selection"), Sel == 2);
	TestTrue(TEXT("bad click does not swap"), !bSwap);

	// Empty inventory: nothing selectable.
	Sel = UInventoryMatWidget::ResolveClick(INDEX_NONE, 0, 0, bSwap);
	TestTrue(TEXT("no slots -> no selection"), Sel == INDEX_NONE);
	return true;
}

#endif