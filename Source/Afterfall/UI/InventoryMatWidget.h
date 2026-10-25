// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: UI — the Tab inventory mat. Grid of FItemStack slots over the
// UAfterfallInventoryComponent: click to select, click another to swap.
// Local-only and non-modal: the game keeps running while it is open.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryMatWidget.generated.h"

class UAfterfallInventoryComponent;
class UInventorySlotWidget;
class UUniformGridPanel;

UCLASS()
class AFTERFALL_API UInventoryMatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to the component (idempotent) and paint the current slots. */
	void SetInventory(UAfterfallInventoryComponent* InInventory);

	/** Currently selected slot, or INDEX_NONE. */
	int32 GetSelectedSlot() const { return SelectedSlot; }

	/**
	 * Select/swap policy, kept pure so automation can verify it without a
	 * renderer: first click selects, clicking the selection clears it, any
	 * other click requests a swap and clears the selection.
	 *
	 * @param Selected    current selection, INDEX_NONE when nothing picked
	 * @param Clicked     slot the user clicked
	 * @param NumSlots    slot count (bounds the inputs)
	 * @param bOutSwap    [out] true when the caller should swap Selected/Clicked
	 * @return            the selection to store after the click
	 */
	static int32 ResolveClick(int32 Selected, int32 Clicked, int32 NumSlots, bool& bOutSwap);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleSlotClicked(int32 SlotIndex);

	void EnsureTree();
	void RefreshSlots();

	UPROPERTY(Transient)
	TObjectPtr<UAfterfallInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> Grid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> SlotWidgets;

	int32 SelectedSlot = INDEX_NONE;

	static constexpr int32 Columns = 6;
	static constexpr int32 MaxVisibleSlots = 24;
};