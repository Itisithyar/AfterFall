// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "UI/InventoryMatWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Items/AfterfallInventoryComponent.h"
#include "UI/InventorySlotWidget.h"

void UInventoryMatWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureTree();
}

void UInventoryMatWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureTree();
}

void UInventoryMatWidget::EnsureTree()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.92f));
	Panel->SetPadding(FMargin(16.f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(FText::FromString(TEXT("INVENTORY — click a slot, then click another to swap")));
	Column->AddChild(Title);

	Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SlotGrid"));
	Column->AddChild(Grid);

	Panel->AddChild(Column);
	WidgetTree->RootWidget = Root;

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	if (PanelSlot)
	{
		// Centre of the screen, fixed size, so the game stays visible around it.
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(780.f, 460.f));
	}
}

int32 UInventoryMatWidget::ResolveClick(int32 Selected, int32 Clicked, int32 NumSlots, bool& bOutSwap)
{
	bOutSwap = false;
	if (NumSlots <= 0 || Clicked < 0 || Clicked >= NumSlots)
	{
		return Selected; // nothing to pick
	}
	if (Selected < 0 || Selected >= NumSlots)
	{
		return Clicked; // first pick selects
	}
	if (Selected == Clicked)
	{
		return INDEX_NONE; // pick again = clear
	}
	bOutSwap = true; // a different slot = swap the two
	return INDEX_NONE;
}

void UInventoryMatWidget::SetInventory(UAfterfallInventoryComponent* InInventory)
{
	if (Inventory && Inventory != InInventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryMatWidget::HandleInventoryChanged);
	}
	Inventory = InInventory;
	if (Inventory)
	{
		// Remove first so re-binding can never double-fire a refresh.
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryMatWidget::HandleInventoryChanged);
		Inventory->OnInventoryChanged.AddDynamic(this, &UInventoryMatWidget::HandleInventoryChanged);
	}
	SelectedSlot = INDEX_NONE;
	EnsureTree();
	RefreshSlots();
}

void UInventoryMatWidget::HandleInventoryChanged()
{
	RefreshSlots();
}

void UInventoryMatWidget::HandleSlotClicked(int32 SlotIndex)
{
	if (!Inventory)
	{
		return;
	}
	bool bDoSwap = false;
	const int32 NewSelection = ResolveClick(SelectedSlot, SlotIndex, Inventory->Slots.Num(), bDoSwap);
	if (bDoSwap)
	{
		Inventory->SwapSlots(SelectedSlot, SlotIndex);
	}
	SelectedSlot = NewSelection;
	RefreshSlots();
}

void UInventoryMatWidget::RefreshSlots()
{
	EnsureTree();
	if (!Grid)
	{
		return;
	}
	Grid->ClearChildren();
	SlotWidgets.Reset();

	if (!Inventory)
	{
		return;
	}
	if (SelectedSlot >= Inventory->Slots.Num())
	{
		SelectedSlot = INDEX_NONE;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return; // no owner yet; SetInventory is called again on open
	}

	const TArray<FItemStack>& Stacks = Inventory->Slots;
	const int32 Count = FMath::Min(Stacks.Num(), MaxVisibleSlots);
	for (int32 i = 0; i < Count; ++i)
	{
		UInventorySlotWidget* Cell = CreateWidget<UInventorySlotWidget>(
			PC, UInventorySlotWidget::StaticClass());
		if (!Cell)
		{
			continue;
		}
		Cell->OnSlotClicked.RemoveDynamic(this, &UInventoryMatWidget::HandleSlotClicked);
		Cell->OnSlotClicked.AddDynamic(this, &UInventoryMatWidget::HandleSlotClicked);
		Cell->SetSlot(i, Stacks[i], i == SelectedSlot);
		Grid->AddChildToUniformGrid(Cell, i / Columns, i % Columns);
		SlotWidgets.Add(Cell);
	}
}