// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: UI — one cell of the inventory mat (icon + id + quantity).
// Built entirely from C++ (no .uasset widget): the tree is constructed in
// EnsureTree(), so the widget works in any build without editor content.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ItemStack.h"
#include "InventorySlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotClicked, int32, SlotIndex);

UCLASS()
class AFTERFALL_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Index into UAfterfallInventoryComponent::Slots, set by the mat on refresh. */
	UPROPERTY(Transient)
	int32 SlotIndex = INDEX_NONE;

	/** Fires with SlotIndex on click; the mat owns select/swap policy. */
	UPROPERTY(BlueprintAssignable)
	FOnInventorySlotClicked OnSlotClicked;

	/** Refresh icon, label, quantity and selection highlight from one stack. */
	void SetSlot(int32 InIndex, const FItemStack& Stack, bool bSelected);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleClick();

	/** Constructs the widget tree once (native widget, no Blueprint asset). */
	void EnsureTree();

	UPROPERTY(Transient)
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QtyText;
};