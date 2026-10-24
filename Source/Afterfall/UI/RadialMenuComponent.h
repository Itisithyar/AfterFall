// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: UI - Radial interaction menu
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RadialMenuComponent.generated.h"

UENUM(BlueprintType)
enum class ERadialAction : uint8
{
	PickUp UMETA(DisplayName = "Pick Up"),
	Drop UMETA(DisplayName = "Drop"),
	Open UMETA(DisplayName = "Open"),
	Close UMETA(DisplayName = "Close"),
	Move UMETA(DisplayName = "Move"),
	Inspect UMETA(DisplayName = "Inspect"),
	Barricade UMETA(DisplayName = "Barricade"),
	Access UMETA(DisplayName = "Access"),
	Craft UMETA(DisplayName = "Craft"),
	TransferItems UMETA(DisplayName = "Transfer Items"),
	PackUp UMETA(DisplayName = "Pack Up")
};

USTRUCT(BlueprintType)
struct FRadialMenuOption
{
	GENERATED_BODY()
	UPROPERTY() ERadialAction Action = ERadialAction::Inspect;
	UPROPERTY() FText DisplayName;
	UPROPERTY() bool bIsEnabled = true;
	UPROPERTY() FText DisabledReason;
};

UCLASS()
class AFTERFALL_API URadialMenuComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URadialMenuComponent();

	UPROPERTY() bool bIsMenuOpen = false;
	UPROPERTY() FVector MenuCenter;
	UPROPERTY() TArray<FRadialMenuOption> Options;
	UPROPERTY() int32 SelectedOption = -1;

	UFUNCTION(BlueprintCallable, Category = "RadialMenu")
	void StartHold();
	UFUNCTION(BlueprintCallable, Category = "RadialMenu")
	void UpdateDrag(const FVector2D& DragVector);
	UFUNCTION(BlueprintCallable, Category = "RadialMenu")
	void Release();
	UFUNCTION(BlueprintCallable, Category = "RadialMenu")
	void Cancel();

	TArray<FRadialMenuOption> GetAvailableActions(AActor* Target) const;

protected:
	virtual void BeginPlay() override;
	float HoldStartTime = 0.0f;
	FVector2D DragDirection;
	bool bHoldActive = false;
};
