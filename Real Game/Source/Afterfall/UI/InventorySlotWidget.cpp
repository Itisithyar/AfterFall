// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "UI/InventorySlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Items/AfterfallItemDefinition.h"
#include "Items/ItemRegistry.h"

void UInventorySlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureTree();
}

void UInventorySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureTree();
}

void UInventorySlotWidget::EnsureTree()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	// Already built (or nothing to build with): leave it alone.
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Body"));
	IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	QtyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Qty"));

	Body->AddChild(IconImage);
	Body->AddChild(NameText);
	Body->AddChild(QtyText);
	SlotButton->AddChild(Body);

	WidgetTree->RootWidget = SlotButton;
	SlotButton->OnClicked.AddDynamic(this, &UInventorySlotWidget::HandleClick);
}

void UInventorySlotWidget::HandleClick()
{
	OnSlotClicked.Broadcast(SlotIndex);
}

void UInventorySlotWidget::SetSlot(int32 InIndex, const FItemStack& Stack, bool bSelected)
{
	EnsureTree();
	SlotIndex = InIndex;

	if (NameText)
	{
		NameText->SetText(Stack.ItemID.IsNone()
			? FText::GetEmpty()
			: FText::FromName(Stack.ItemID));
	}
	if (QtyText)
	{
		QtyText->SetText(FText::AsNumber(Stack.Quantity));
	}

	// Icon comes from the item definition when one is authored; otherwise the
	// cell draws no image rather than a broken one.
	UTexture2D* Icon = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UAfterfallItemRegistry* Reg = GI->GetSubsystem<UAfterfallItemRegistry>())
			{
				if (const UAfterfallItemDefinition* Def = Reg->GetItem(Stack.ItemID))
				{
					Icon = Def->Icon;
				}
			}
		}
	}
	if (IconImage)
	{
		FSlateBrush Brush;
		if (Icon)
		{
			Brush.SetResourceObject(Icon);
			Brush.ImageSize = FVector2D(48.f, 48.f);
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		}
		IconImage->SetBrush(Brush);
	}

	if (SlotButton)
	{
		SlotButton->SetBackgroundColor(bSelected
			? FLinearColor(0.16f, 0.45f, 0.16f, 1.f)
			: FLinearColor(0.07f, 0.07f, 0.07f, 0.92f));
	}
}