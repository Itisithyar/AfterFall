// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: UI - Radial interaction menu
//
// Unified Mouse1/RMB interaction layer (client-side presentation + selection
// math only). This component NEVER simulates physics and NEVER applies state:
// it builds the contextual option list, tracks the crosshair-anchored wheel,
// and maps drag displacement to a sector. Execution lives on
// AAfterfallCharacter::ExecuteRadialAction, which routes every choice into an
// existing server-authoritative path (IInteractable, ArticulatedBody grab,
// Carry throw/drop, InjuryState treatment, mat open/close). The server
// re-validates target, range, and state on every call, so availability shown
// here is a prompt heuristic, never a grant.
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
	PackUp UMETA(DisplayName = "Pack Up"),
	// Unified-interaction actions (appended; existing values untouched).
	Grab UMETA(DisplayName = "Grab"),
	Drag UMETA(DisplayName = "Drag"),
	Throw UMETA(DisplayName = "Throw"),
	Release UMETA(DisplayName = "Release"),
	Treat UMETA(DisplayName = "Treat"),
	Use UMETA(DisplayName = "Use"),
	Cancel UMETA(DisplayName = "Cancel")
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

	// ---- Tuning (MenuOpen: t_h >= T_r, T_r initial 0.25s test value) ----
	/** Hold duration (s) before the wheel opens: LMB hold 330 ms (spec). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Radial")
	float HoldThresholdSec = 0.33f;
	/** Cursor displacement inside this radius (px) selects nothing. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Radial")
	float SelectionDeadZonePx = 24.f;
	/** Intended visible wheel diameter (inches); pixel size scales with DPI. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Radial")
	float WheelDiameterInches = 3.f;
	/** Baseline pixels per inch; retune per platform DPI. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Radial")
	float PixelsPerInch = 96.f;
	/** Extra UI-scale multiplier (platform accessibility scaling). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Radial")
	float WheelUIScale = 1.f;

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

	// ---- Unified-interaction API (new flow) ----

	/** Pure hold rule: MenuOpen = (HoldSec >= ThresholdSec). */
	UFUNCTION(BlueprintPure, Category = "RadialMenu")
	static bool ShouldOpenMenu(double HoldSec, double ThresholdSec);

	/**
	 * Pure sector map: screen-space drag (X right, Y down) to an option index.
	 * Sector 0 starts at +X (screen right) and increases clockwise on screen.
	 * Returns -1 inside the dead zone or when there is nothing to select.
	 */
	UFUNCTION(BlueprintPure, Category = "RadialMenu")
	static int32 SectorForDrag(const FVector2D& Drag, float DeadZonePx, int32 OptionCount);

	/**
	 * Build the contextual option list for the owning character's current
	 * focus + self state. Only actions with a live gameplay path are listed:
	 * zombie corpse -> Inspect/Grab/Drag/Release; item -> PickUp/Inspect;
	 * storage mat -> Access/PackUp/Inspect; other interactable -> Use/Inspect;
	 * carried item -> Throw/Drop; wounds -> Treat; Cancel always closes.
	 * No target and no self action -> empty (caller must not open the wheel).
	 */
	TArray<FRadialMenuOption> BuildContextualOptions() const;

	/** Open the wheel anchored on the crosshair (screen center), not a corner. */
	void OpenAtCrosshair(const TArray<FRadialMenuOption>& InOptions);

	/** Feed accumulated look displacement while the wheel is open. */
	void UpdateDragFromHold(const FVector2D& AccumulatedDrag);

	/** Currently selected action, or Cancel when nothing is highlighted. */
	ERadialAction GetSelectedAction() const;

	/** Intended wheel radius in screen px (DPI-aware sizing target). */
	UFUNCTION(BlueprintPure, Category = "RadialMenu")
	float WheelRadiusPx() const;

protected:
	virtual void BeginPlay() override;
	float HoldStartTime = 0.0f;
	FVector2D DragDirection;
	bool bHoldActive = false;
};
