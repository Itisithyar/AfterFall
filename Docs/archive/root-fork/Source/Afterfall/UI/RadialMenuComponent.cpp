// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "RadialMenuComponent.h"
#include "Engine/World.h"
#include "World/PhysicalItem.h"
#include "Storage/PhysicalMat.h"
#include "Kismet/GameplayStatics.h"

URadialMenuComponent::URadialMenuComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URadialMenuComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URadialMenuComponent::StartHold()
{
	bHoldActive = true;
	HoldStartTime = GetWorld()->GetTimeSeconds();
	bIsMenuOpen = true;
	SelectedOption = -1;
	Options.Empty();
	
	// Get target under cursor - simplified
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC)
	{
		FHitResult Hit;
		PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
		if (Hit.GetActor())
		{
			Options = GetAvailableActions(Hit.GetActor());
		}
		else
		{
			// Default options
			FRadialMenuOption Opt1;
			Opt1.Action = ERadialAction::Inspect;
			Opt1.DisplayName = FText::FromString("Inspect");
			Opt1.bIsEnabled = true;
			Options.Add(Opt1);
		}
	}
}

void URadialMenuComponent::UpdateDrag(const FVector2D& DragVector)
{
	if (!bIsMenuOpen || Options.Num() == 0) return;
	
	float Angle = FMath::Atan2(DragVector.Y, DragVector.X);
	// Convert angle to option index (8 sectors)
	int32 Index = FMath::FloorToInt(((Angle + PI) / (2 * PI)) * Options.Num()) % Options.Num();
	SelectedOption = Index;
}

void URadialMenuComponent::Release()
{
	if (!bIsMenuOpen) return;
	
	if (SelectedOption >= 0 && SelectedOption < Options.Num() && Options[SelectedOption].bIsEnabled)
	{
		// Execute selected action
		// Would submit through gameplay command pipeline
	}
	
	bIsMenuOpen = false;
	bHoldActive = false;
	SelectedOption = -1;
}

void URadialMenuComponent::Cancel()
{
	bIsMenuOpen = false;
	bHoldActive = false;
	SelectedOption = -1;
}

TArray<FRadialMenuOption> URadialMenuComponent::GetAvailableActions(AActor* Target) const
{
	TArray<FRadialMenuOption> Actions;
	
	if (!Target) return Actions;
	
	if (Cast<APhysicalItem>(Target))
	{
		FRadialMenuOption Opt1;
		Opt1.Action = ERadialAction::PickUp;
		Opt1.DisplayName = FText::FromString("Pick Up");
		Opt1.bIsEnabled = true;
		Actions.Add(Opt1);
		
		FRadialMenuOption Opt2;
		Opt2.Action = ERadialAction::Inspect;
		Opt2.DisplayName = FText::FromString("Inspect");
		Opt2.bIsEnabled = true;
		Actions.Add(Opt2);
	}
	else if (Cast<APhysicalStorageMat>(Target))
	{
		FRadialMenuOption Opt1;
		Opt1.Action = ERadialAction::Access;
		Opt1.DisplayName = FText::FromString("Access");
		Opt1.bIsEnabled = true;
		Actions.Add(Opt1);
		
		FRadialMenuOption Opt2;
		Opt2.Action = ERadialAction::Craft;
		Opt2.DisplayName = FText::FromString("Craft");
		Opt2.bIsEnabled = true;
		Actions.Add(Opt2);
		
		FRadialMenuOption Opt3;
		Opt3.Action = ERadialAction::PackUp;
		Opt3.DisplayName = FText::FromString("Pack Up");
		Opt3.bIsEnabled = true;
		Actions.Add(Opt3);
	}
	else
	{
		FRadialMenuOption Opt1;
		Opt1.Action = ERadialAction::Inspect;
		Opt1.DisplayName = FText::FromString("Inspect");
		Opt1.bIsEnabled = true;
		Actions.Add(Opt1);
	}
	
	return Actions;
}
