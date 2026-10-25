// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "RadialMenuComponent.h"
#include "Engine/World.h"
#include "World/PhysicalItem.h"
#include "Storage/PhysicalMat.h"
#include "AfterfallCharacter.h"
#include "AI/ZombiePrototype.h"
#include "Interaction/InteractComponent.h"
#include "Interaction/MatInteractionComponent.h"
#include "Interaction/CarryComponent.h"
#include "Interaction/Interactable.h"
#include "Combat/InjuryStateComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

URadialMenuComponent::URadialMenuComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URadialMenuComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool URadialMenuComponent::ShouldOpenMenu(double HoldSec, double ThresholdSec)
{
	return HoldSec >= ThresholdSec;
}

int32 URadialMenuComponent::SectorForDrag(const FVector2D& Drag, float DeadZonePx, int32 OptionCount)
{
	if (OptionCount <= 0)
	{
		return -1;
	}
	if (Drag.Size() < DeadZonePx)
	{
		return -1;
	}
	// Sector 0 is centred on screen-right (+X) and grows clockwise on screen
	// (Y grows downward): east -> 0, south-east -> 1, south -> 2, ... Centre-
	// anchored bins keep all N directions distinct and in range.
	float Angle = FMath::Atan2(Drag.Y, Drag.X); // [-pi, pi]
	if (Angle < 0.f)
	{
		Angle += 2.f * PI; // [0, 2pi) clockwise from east
	}
	const float Sector = 2.f * PI / (float)OptionCount;
	int32 Index = FMath::FloorToInt((Angle + 0.5f * Sector) / Sector) % OptionCount;
	return Index;
}

void URadialMenuComponent::OpenAtCrosshair(const TArray<FRadialMenuOption>& InOptions)
{
	Options = InOptions;
	SelectedOption = -1;
	DragDirection = FVector2D::ZeroVector;
	bHoldActive = true;
	bIsMenuOpen = true;

	// Anchor on the crosshair: screen center of the owning local player.
	// The camera never moves; only this data point changes.
	MenuCenter = FVector::ZeroVector;
	if (const AAfterfallCharacter* C = Cast<AAfterfallCharacter>(GetOwner()))
	{
		if (const APlayerController* PC = Cast<APlayerController>(C->GetController()))
		{
			int32 SX = 0, SY = 0;
			PC->GetViewportSize(SX, SY);
			MenuCenter = FVector((float)SX * 0.5f, (float)SY * 0.5f, 0.f);
		}
	}
	HoldStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void URadialMenuComponent::UpdateDragFromHold(const FVector2D& AccumulatedDrag)
{
	if (!bIsMenuOpen || Options.Num() == 0)
	{
		return;
	}
	DragDirection = AccumulatedDrag;
	SelectedOption = SectorForDrag(AccumulatedDrag, SelectionDeadZonePx, Options.Num());
}

ERadialAction URadialMenuComponent::GetSelectedAction() const
{
	if (bIsMenuOpen && Options.IsValidIndex(SelectedOption) && Options[SelectedOption].bIsEnabled)
	{
		return Options[SelectedOption].Action;
	}
	return ERadialAction::Cancel;
}

float URadialMenuComponent::WheelRadiusPx() const
{
	return 0.5f * WheelDiameterInches * PixelsPerInch * WheelUIScale;
}

static void AddRadialOption(TArray<FRadialMenuOption>& Out, ERadialAction Action, const TCHAR* Label)
{
	FRadialMenuOption Opt;
	Opt.Action = Action;
	Opt.DisplayName = FText::FromString(Label);
	Opt.bIsEnabled = true;
	Out.Add(Opt);
}

TArray<FRadialMenuOption> URadialMenuComponent::BuildContextualOptions() const
{
	TArray<FRadialMenuOption> Out;
	const AAfterfallCharacter* C = Cast<AAfterfallCharacter>(GetOwner());
	if (!C)
	{
		return Out;
	}

	AActor* Target = C->Interact ? C->Interact->FocusedActor.Get() : nullptr;
	if (Target && !Target->IsPendingKillPending())
	{
		if (const AZombiePrototype* Z = Cast<AZombiePrototype>(Target))
		{
			AddRadialOption(Out, ERadialAction::Inspect, TEXT("Inspect"));
			// Corpse heuristic: ragdoll state is server-only (never replicated),
			// so HP<=0 stands in for "body available". The server re-validates
			// ragdoll + hold ownership before any constraint is touched.
			if (Z->HP <= 0.f)
			{
				AddRadialOption(Out, ERadialAction::Grab, TEXT("Grab"));
				AddRadialOption(Out, ERadialAction::Drag, TEXT("Drag"));
				AddRadialOption(Out, ERadialAction::Release, TEXT("Release"));
			}
		}
		else if (Cast<APhysicalItem>(Target))
		{
			// Pickup executes through the item's own IInteractable path
			// (server-validated), never a client-side attach.
			AddRadialOption(Out, ERadialAction::PickUp, TEXT("Pick Up"));
			AddRadialOption(Out, ERadialAction::Inspect, TEXT("Inspect"));
		}
		else if (Cast<APhysicalStorageMat>(Target))
		{
			AddRadialOption(Out, ERadialAction::Access, TEXT("Access"));
			AddRadialOption(Out, ERadialAction::PackUp, TEXT("Pack Up"));
			AddRadialOption(Out, ERadialAction::Inspect, TEXT("Inspect"));
		}
		else if (Target->Implements<UInteractable>())
		{
			AddRadialOption(Out, ERadialAction::Use, TEXT("Use"));
			AddRadialOption(Out, ERadialAction::Inspect, TEXT("Inspect"));
		}
	}

	// Self actions: no target required. Treat availability = any wound; the
	// server validates bandage ownership inside ServerBeginTreatment.
	if (C->Injury)
	{
		for (const FInjuryRecord& R : C->Injury->PartInjuries)
		{
			if (R.bWounded)
			{
				AddRadialOption(Out, ERadialAction::Treat, TEXT("Treat"));
				break;
			}
		}
	}
	if (C->Carry && C->Carry->bIsCarrying)
	{
		AddRadialOption(Out, ERadialAction::Throw, TEXT("Throw"));
		AddRadialOption(Out, ERadialAction::Drop, TEXT("Drop"));
	}

	if (Out.Num() == 0)
	{
		return Out; // nothing useful: caller must not open the wheel
	}
	AddRadialOption(Out, ERadialAction::Cancel, TEXT("Cancel"));
	return Out;
}

void URadialMenuComponent::StartHold()
{
	bHoldActive = true;
	HoldStartTime = GetWorld()->GetTimeSeconds();
	bIsMenuOpen = true;
	SelectedOption = -1;
	Options.Empty();
	
	// Get target under cursor - simplified. The component lives on the
	// character, so resolve the controller through the owning pawn.
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(GetOwner()))
		{
			PC = Cast<APlayerController>(C->GetController());
		}
	}
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
	
	DragDirection = DragVector;
	SelectedOption = SectorForDrag(DragVector, SelectionDeadZonePx, Options.Num());
}

void URadialMenuComponent::Release()
{
	if (!bIsMenuOpen) return;

	if (SelectedOption >= 0 && SelectedOption < Options.Num() && Options[SelectedOption].bIsEnabled)
	{
		// Only mat actions execute here (MAT design §4): Access opens and
		// Pack Up closes through the owner's server-authoritative mat
		// interaction component. Client calls the Server RPC exactly like
		// the B-key toggle (OnMatPressed); the server validates the state.
		// All other actions remain future pipeline work (no-op).
		const ERadialAction A = Options[SelectedOption].Action;
		if (A == ERadialAction::Access || A == ERadialAction::PackUp)
		{
			if (AAfterfallCharacter* C = Cast<AAfterfallCharacter>(GetOwner()))
			{
				if (UMatInteractionComponent* M = C->MatInteract)
				{
					if (A == ERadialAction::Access)
					{
						M->ServerRequestMatOpen();
					}
					else
					{
						M->ServerRequestMatClose();
					}
				}
			}
		}
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

	if (const AZombiePrototype* Z = Cast<AZombiePrototype>(Target))
	{
		FRadialMenuOption Opt;
		Opt.Action = ERadialAction::Inspect;
		Opt.DisplayName = FText::FromString("Inspect");
		Opt.bIsEnabled = true;
		Actions.Add(Opt);
		if (Z->HP <= 0.f)
		{
			FRadialMenuOption G;
			G.Action = ERadialAction::Grab;
			G.DisplayName = FText::FromString("Grab");
			G.bIsEnabled = true;
			Actions.Add(G);
			FRadialMenuOption D;
			D.Action = ERadialAction::Drag;
			D.DisplayName = FText::FromString("Drag");
			D.bIsEnabled = true;
			Actions.Add(D);
			FRadialMenuOption R;
			R.Action = ERadialAction::Release;
			R.DisplayName = FText::FromString("Release");
			R.bIsEnabled = true;
			Actions.Add(R);
		}
		return Actions;
	}
	
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
