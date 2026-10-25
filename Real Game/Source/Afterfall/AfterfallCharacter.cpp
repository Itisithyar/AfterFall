// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Character) — Phase 3b: full server validation wiring +
// Slice: runtime input, interaction/combat RPCs, death + auto-respawn.
#include "AfterfallCharacter.h"
#include "Structural/StructuralActor.h"
#include "Animation/AdaptiveLimbComponent.h"
#include "Structural/ConstructionValidator.h"
#include "Destruction/DestructionPropagation.h"
#include "Procedural/ProceduralObjectActor.h"
#include "Inventory/InventoryComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/InjuryStateComponent.h"
#include "Structural/BoxWallStructure.h"
#include "Interaction/InteractComponent.h"
#include "Interaction/CarryComponent.h"
#include "UI/RadialMenuComponent.h"
#include "Interaction/MatInteractionComponent.h"
#include "Simulation/SimulationScheduler.h"
#include "Interaction/Interactable.h"
#include "Interaction/ClickArbitration.h"
#include "World/PhysicalItem.h"
#include "Components/SceneComponent.h"
#include "Movement/AfterfallMovementComponent.h"
#include "Items/AfterfallInventoryComponent.h"
#include "Storage/PhysicalMat.h"
#include "AI/ZombiePrototype.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
// Runtime input (no .uasset IA/IMC required — built in code, injected for the
// local player; the template BP bindings we cannot edit headless stay untouched).
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputModifiers.h"
#include "UI/AfterfallHUD.h"

namespace
{
	/**
	 * Route an authoritative weapon hit into the universal procedural-object
	 * pipeline. The dent is committed before the fracture event so rebuilds
	 * replay deformation first and carve the already-deformed field second.
	 * The energy-to-depth coefficient is a conservative gameplay tuning value,
	 * not a claim of calibrated material plasticity.
	 */
	bool ApplyProceduralObjectImpact(AProceduralObjectActor* Object, const FVector& WorldPoint,
		const FVector& WorldOutwardNormal, const FVector& WorldImpactDirection, float EnergyJ)
	{
		if (!Object || !Object->HasAuthority() || !FMath::IsFinite(EnergyJ) || EnergyJ <= 0.f)
		{
			return false;
		}

		const FTransform Transform = Object->GetActorTransform();
		const FVector LocalPoint = Transform.InverseTransformPosition(WorldPoint);
		FVector Outward = WorldOutwardNormal.GetSafeNormal();
		if (Outward.IsNearlyZero())
		{
			Outward = -WorldImpactDirection.GetSafeNormal();
		}
		const FVector LocalOutward = Transform.InverseTransformVectorNoScale(Outward).GetSafeNormal();
		const FVector LocalImpactDirection = Transform.InverseTransformVectorNoScale(
			WorldImpactDirection.GetSafeNormal()).GetSafeNormal();
		if (LocalOutward.IsNearlyZero() || LocalImpactDirection.IsNearlyZero())
		{
			return false;
		}

		FProceduralDeformOp Deform;
		Deform.OpId = FGuid::NewGuid();
		Deform.Center = LocalPoint;
		Deform.Direction = LocalOutward;
		Deform.Radius = FMath::Clamp(Object->Desc.CellSize * 4.f, 24.f, 96.f);
		Deform.Strength = FMath::Clamp(5.f * FMath::Sqrt(EnergyJ / 100.f), 0.5f, 12.f);
		const bool bDeformed = Object->ServerApplyDeform(Deform);

		const uint32 FractureSeed = static_cast<uint32>(Object->Desc.Seed)
			^ static_cast<uint32>(Object->ServerSeq + 1);
		const bool bFractured = Object->ServerApplyFracture(LocalPoint, LocalImpactDirection,
			EnergyJ, Object->Desc.MaterialProfile, FractureSeed, FGuid::NewGuid());
		if (bDeformed || bFractured)
		{
			UE_LOG(LogAfterfall, Log, TEXT("Procedural impact %s E=%.1fJ deform=%d fracture=%d"),
				*Object->GetName(), EnergyJ, bDeformed ? 1 : 0, bFractured ? 1 : 0);
		}
		return bDeformed || bFractured;
	}
}
#include "UI/InventoryMatWidget.h"
#include "Voxel/VoxelChunkActor.h"
#include "Voxel/VoxelChunkTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Afterfall.h"
#include "Movement/AfterfallGrappleComponent.h"
#include "Combat/ArticulatedBodyComponent.h"

AAfterfallCharacter::AAfterfallCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAfterfallMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	AfterfallInventory = CreateDefaultSubobject<UAfterfallInventoryComponent>(TEXT("AfterfallInventory"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Injury = CreateDefaultSubobject<UInjuryStateComponent>(TEXT("Injury"));
	Interact = CreateDefaultSubobject<UInteractComponent>(TEXT("Interact"));
	Carry = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
	MatInteract = CreateDefaultSubobject<UMatInteractionComponent>(TEXT("MatInteract"));
	RadialMenu = CreateDefaultSubobject<URadialMenuComponent>(TEXT("RadialMenu"));
	Grapple = CreateDefaultSubobject<UAfterfallGrappleComponent>(TEXT("Grapple"));
	RightArm = CreateDefaultSubobject<UAdaptiveLimbComponent>(TEXT("RightArm"));

	WeaponHandPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponHandPivot"));
	WeaponHandPivot->SetupAttachment(GetCapsuleComponent());
	WeaponHandPivot->SetRelativeLocation(FVector(20.f, 30.f, -10.f));
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(30.f, 0.f, 150.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Canonical momentum tuning lives on the movement component; keep the
	// legacy knobs in sync so child BPs see coherent values.
	WalkSpeed = 500.f;
	SprintSpeed = 800.f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 550.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2048.f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 200.f;
}

UAfterfallMovementComponent* AAfterfallCharacter::GetAfterfallMovement() const
{
	return Cast<UAfterfallMovementComponent>(GetCharacterMovement());
}

void AAfterfallCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AAfterfallCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && Health)
	{
		Health->OnHealthDepleted.AddDynamic(this, &AAfterfallCharacter::HandleDeath);
	}
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &AAfterfallCharacter::OnInventoryChangedForLoad);
		OnInventoryChangedForLoad();
	}
}

void AAfterfallCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (IsLocallyControlled())
	{
		InjectSliceInput();
	}
}

// ---------------------------------------------------------------- Input ---

static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* A = NewObject<UInputAction>(Outer, FName(Name), RF_NoFlags, nullptr);
	A->ValueType = Type;
	return A;
}

// Digital 1D keys (WASD, mouse axes, stick axes) inject (v,0,0): without a
// swizzle they all drive action-X. bSwizzleYX inserts a YXZ swizzle FIRST so
// the key value lands on action-Y (forward/pitch), then negate applies.
static UInputMappingContext* MapKey(UInputMappingContext* Ctx, UInputAction* Action, FKey Key,
	bool bNegX = false, bool bNegY = false, bool bSwizzleYX = false)
{
	FEnhancedActionKeyMapping& M = Ctx->MapKey(Action, Key);
	if (bSwizzleYX)
	{
		// Default order is YXZ: (x,y,z) -> (y,x,z).
		M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Ctx));
	}
	if (bNegX || bNegY)
	{
		UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(Ctx);
		Neg->bX = bNegX;
		Neg->bY = bNegY;
		Neg->bZ = false;
		M.Modifiers.Add(Neg);
	}
	return Ctx;
}

void AAfterfallCharacter::InjectSliceInput()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->Player)
	{
		return;
	}
	ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Sub = LP ?
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
	if (!Sub)
	{
		return;
	}
	if (!SliceIMC)
	{
		SliceIMC = NewObject<UInputMappingContext>(this, TEXT("SliceIMC"));
		IA_Move = MakeAction(this, TEXT("Slice_Move"), EInputActionValueType::Axis2D);
		IA_Look = MakeAction(this, TEXT("Slice_Look"), EInputActionValueType::Axis2D);
		IA_Jump = MakeAction(this, TEXT("Slice_Jump"), EInputActionValueType::Boolean);
		IA_Sprint = MakeAction(this, TEXT("Slice_Sprint"), EInputActionValueType::Boolean);
		IA_Interact = MakeAction(this, TEXT("Slice_Interact"), EInputActionValueType::Boolean);
		IA_Attack = MakeAction(this, TEXT("Slice_Attack"), EInputActionValueType::Boolean);
		IA_Crouch = MakeAction(this, TEXT("Slice_Crouch"), EInputActionValueType::Boolean);
		IA_Throw = MakeAction(this, TEXT("Slice_Throw"), EInputActionValueType::Boolean);
		IA_Drop = MakeAction(this, TEXT("Slice_Drop"), EInputActionValueType::Boolean);		IA_Mat = MakeAction(this, TEXT("Slice_Mat"), EInputActionValueType::Boolean);
		IA_Fire = MakeAction(this, TEXT("Slice_Fire"), EInputActionValueType::Boolean);
		IA_Reload = MakeAction(this, TEXT("Slice_Reload"), EInputActionValueType::Boolean);
		IA_Pause = MakeAction(this, TEXT("Slice_Pause"), EInputActionValueType::Boolean);
		IA_Build = MakeAction(this, TEXT("Slice_Build"), EInputActionValueType::Boolean);
		IA_Grapple = MakeAction(this, TEXT("Slice_Grapple"), EInputActionValueType::Boolean);
		IA_Inventory = MakeAction(this, TEXT("Slice_Inventory"), EInputActionValueType::Boolean);
		IA_Reach = MakeAction(this, TEXT("Slice_Reach"), EInputActionValueType::Boolean);
		IA_GrabHold = MakeAction(this, TEXT("Slice_GrabHold"), EInputActionValueType::Boolean);
		// W/S need the YX swizzle (keys inject X): W -> (0,+1), S -> (0,-1).
		// A/D already land on X. MouseY likewise drives pitch via swizzle.
		MapKey(SliceIMC, IA_Move, EKeys::W, false, false, true);
		MapKey(SliceIMC, IA_Move, EKeys::S, false, true, true);
		MapKey(SliceIMC, IA_Move, EKeys::A, true, false);
		MapKey(SliceIMC, IA_Move, EKeys::D);
		MapKey(SliceIMC, IA_Look, EKeys::MouseX);
		MapKey(SliceIMC, IA_Look, EKeys::MouseY, false, true, true);
		MapKey(SliceIMC, IA_Jump, EKeys::SpaceBar);
		MapKey(SliceIMC, IA_Sprint, EKeys::LeftShift);
		MapKey(SliceIMC, IA_Interact, EKeys::E); // Pickup / regular focused interaction.
		MapKey(SliceIMC, IA_Attack, EKeys::LeftMouseButton);
		MapKey(SliceIMC, IA_Crouch, EKeys::C);
		MapKey(SliceIMC, IA_Throw, EKeys::Q); // Throw held item.
		MapKey(SliceIMC, IA_Drop, EKeys::G);
		MapKey(SliceIMC, IA_Mat, EKeys::B);
		MapKey(SliceIMC, IA_Fire, EKeys::F);
		MapKey(SliceIMC, IA_Reload, EKeys::R);
		MapKey(SliceIMC, IA_Pause, EKeys::Escape);
		MapKey(SliceIMC, IA_Build, EKeys::V);
		MapKey(SliceIMC, IA_Grapple, EKeys::T);
		MapKey(SliceIMC, IA_Grapple, EKeys::Gamepad_DPad_Right);
		MapKey(SliceIMC, IA_Inventory, EKeys::Tab);
		MapKey(SliceIMC, IA_Inventory, EKeys::Gamepad_DPad_Left);
		MapKey(SliceIMC, IA_GrabHold, EKeys::RightMouseButton); // Hold to grab/release.
		MapKey(SliceIMC, IA_Reach, EKeys::LeftAlt);
		MapKey(SliceIMC, IA_GrabHold, EKeys::Gamepad_LeftTrigger);
		MapKey(SliceIMC, IA_Reach, EKeys::Gamepad_LeftShoulder);
		// Binding table (keyboard, remappable; Move/Look stay fixed).
		// Recipes preserve the swizzle/negate wiring across remaps.
		BindingRecipes.Add(TEXT("Jump"), { false, false, false });
		BindingRecipes.Add(TEXT("Sprint"), { false, false, false });
		BindingRecipes.Add(TEXT("Interact"), { false, false, false });
		BindingRecipes.Add(TEXT("Attack"), { false, false, false });
		BindingRecipes.Add(TEXT("Crouch"), { false, false, false });
		BindingRecipes.Add(TEXT("Throw"), { false, false, false });
		BindingRecipes.Add(TEXT("Drop"), { false, false, false });
		BindingRecipes.Add(TEXT("Mat"), { false, false, false });
		BindingRecipes.Add(TEXT("Fire"), { false, false, false });
		BindingRecipes.Add(TEXT("Reload"), { false, false, false });
		BindingRecipes.Add(TEXT("Pause"), { false, false, false });
		BindingRecipes.Add(TEXT("GrabHold"), { false, false, false });
		CurrentKeys.Add(TEXT("Jump"), EKeys::SpaceBar);
		CurrentKeys.Add(TEXT("Sprint"), EKeys::LeftShift);
		CurrentKeys.Add(TEXT("Interact"), EKeys::E);
		CurrentKeys.Add(TEXT("Attack"), EKeys::LeftMouseButton);
		CurrentKeys.Add(TEXT("Crouch"), EKeys::C);
		CurrentKeys.Add(TEXT("Throw"), EKeys::Q);
		CurrentKeys.Add(TEXT("Drop"), EKeys::G);
		CurrentKeys.Add(TEXT("Mat"), EKeys::B);
		CurrentKeys.Add(TEXT("Fire"), EKeys::F);
		CurrentKeys.Add(TEXT("Reload"), EKeys::R);
		CurrentKeys.Add(TEXT("Pause"), EKeys::Escape);
		CurrentKeys.Add(TEXT("GrabHold"), EKeys::RightMouseButton);
		LoadBindingOverrides();
		// Gamepad parity: sticks are 1D keys, Y axes need the same swizzle.
		// Stick-up is +Y, so no negate (unlike mouse, which is down-positive).
		MapKey(SliceIMC, IA_Move, EKeys::Gamepad_LeftX);
		MapKey(SliceIMC, IA_Move, EKeys::Gamepad_LeftY, false, false, true);
		MapKey(SliceIMC, IA_Look, EKeys::Gamepad_RightX);
		MapKey(SliceIMC, IA_Look, EKeys::Gamepad_RightY, false, false, true);
		MapKey(SliceIMC, IA_Jump, EKeys::Gamepad_FaceButton_Bottom);
		MapKey(SliceIMC, IA_Sprint, EKeys::Gamepad_LeftThumbstick);
		MapKey(SliceIMC, IA_Interact, EKeys::Gamepad_FaceButton_Left);
		MapKey(SliceIMC, IA_Attack, EKeys::Gamepad_RightShoulder);
		MapKey(SliceIMC, IA_Crouch, EKeys::Gamepad_FaceButton_Right);
		MapKey(SliceIMC, IA_Throw, EKeys::Gamepad_FaceButton_Top);
		MapKey(SliceIMC, IA_Drop, EKeys::Gamepad_DPad_Down);
		MapKey(SliceIMC, IA_Mat, EKeys::Gamepad_DPad_Up);
		MapKey(SliceIMC, IA_Fire, EKeys::Gamepad_RightTrigger);
		MapKey(SliceIMC, IA_Pause, EKeys::Gamepad_Special_Right);
	}
	Sub->AddMappingContext(SliceIMC, 10);
}

void AAfterfallCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AAfterfallCharacter::Move);
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AAfterfallCharacter::Look);
		EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AAfterfallCharacter::OnSprintStarted);
		EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AAfterfallCharacter::OnSprintReleased);
		EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AAfterfallCharacter::OnInteractPressed);
		EIC->BindAction(IA_Interact, ETriggerEvent::Completed, this, &AAfterfallCharacter::OnInteractReleased);
		EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AAfterfallCharacter::OnAttackPressed);
		EIC->BindAction(IA_Attack, ETriggerEvent::Completed, this, &AAfterfallCharacter::OnAttackReleased);
		EIC->BindAction(IA_GrabHold, ETriggerEvent::Started, this, &AAfterfallCharacter::OnGrabHoldPressed);
		EIC->BindAction(IA_GrabHold, ETriggerEvent::Completed, this, &AAfterfallCharacter::OnGrabHoldReleased);
		EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AAfterfallCharacter::OnCrouchPressed);
		EIC->BindAction(IA_Throw, ETriggerEvent::Started, this, &AAfterfallCharacter::OnThrowPressed);
		EIC->BindAction(IA_Drop, ETriggerEvent::Started, this, &AAfterfallCharacter::OnDropPressed);
		EIC->BindAction(IA_Mat, ETriggerEvent::Started, this, &AAfterfallCharacter::OnMatPressed);
		EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &AAfterfallCharacter::OnFirePressed);
		EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &AAfterfallCharacter::OnReloadPressed);
		EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &AAfterfallCharacter::OnPausePressed);
		EIC->BindAction(IA_Build, ETriggerEvent::Started, this, &AAfterfallCharacter::OnBuildPressed);
		EIC->BindAction(IA_Grapple, ETriggerEvent::Started, this, &AAfterfallCharacter::OnGrapplePressed);
		EIC->BindAction(IA_Inventory, ETriggerEvent::Started, this, &AAfterfallCharacter::OnInventoryPressed);
		EIC->BindAction(IA_Reach, ETriggerEvent::Started, this, &AAfterfallCharacter::OnReachPressed);
		EIC->BindAction(IA_Reach, ETriggerEvent::Completed, this, &AAfterfallCharacter::OnReachReleased);
	}
}

bool AAfterfallCharacter::IsMatInventoryOpen() const
{
	return MatInteract && MatInteract->IsInteractionBlocking();
}

void AAfterfallCharacter::Jump()
{
	if (IsMatInventoryOpen() || IsQuickMenuOpen())
	{
		return; // kneeling over the mat, or menu open: no jumping
	}
	Super::Jump();
}void AAfterfallCharacter::Move(const FInputActionValue& Value)
{
	if (IsMatInventoryOpen() || bInventoryOpen)
	{
		return; // inventory open: no movement input
	}
	// Gamepad menu navigation: stick-Y cycles the highlighted button.
	// (Kept for the unpaused path; while paused the HUD tick polls the
	// stick via the player controller because actor input is halted.)
	if (bQuickMenuOpen)
	{
		const float Y = Value.Get<FVector2D>().Y;
		if (FMath::Abs(Y) > 0.6f && (GetWorld()->TimeSeconds - LastMenuNavTime) > 0.25)
		{
			LastMenuNavTime = GetWorld()->TimeSeconds;
			if (APlayerController* NPC = Cast<APlayerController>(GetController()))
			{
				if (AAfterfallHUD* NH = Cast<AAfterfallHUD>(NPC->GetHUD()))
				{
					NH->CycleSelection(Y > 0.f ? -1 : 1);
				}
			}
		}
		return;
	}
	const FVector2D V = Value.Get<FVector2D>();
	const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	GetCharacterMovement()->MaxWalkSpeed = bSprintHeld ? SprintSpeed : WalkSpeed;
	AddMovementInput(Fwd, V.Y);
	AddMovementInput(Right, V.X);
}

void AAfterfallCharacter::Look(const FInputActionValue& Value)
{
	if (bWeaponRotating)
	{
		// Double-click rotation active: the mouse drives the weapon hand,
		// not the camera, until the button is released.
		const FVector2D V = Value.Get<FVector2D>();
		RotateWeaponHand(V);
		return;
	}
	if (IsMatInventoryOpen() || IsQuickMenuOpen() || bInventoryOpen)
	{
		return; // menu/inventory open: mouse drives the cursor, never the camera
	}
	if (IsRadialMenuOpen())
	{
		// Wheel open: the camera stays fixed on the crosshair. Look
		// displacement becomes wheel selection instead. Mouse and gamepad
		// stick share this path, so selection is never mouse-only.
		if (RadialMenu)
		{
			RadialDragAccum += Value.Get<FVector2D>();
			RadialMenu->UpdateDragFromHold(RadialDragAccum);
		}
		return;
	}
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void AAfterfallCharacter::OnSprintStarted()
{
	bSprintHeld = true;
	if (UAfterfallMovementComponent* M = GetAfterfallMovement())
	{
		M->SetSprinting(true);
	}
}
void AAfterfallCharacter::OnSprintReleased()
{
	bSprintHeld = false;
	if (UAfterfallMovementComponent* M = GetAfterfallMovement())
	{
		M->SetSprinting(false);
	}
}

void AAfterfallCharacter::OnThrowPressed()
{
	if (!IsLocallyControlled() || IsQuickMenuOpen())
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	if (AController* PC = GetController())
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		ServerThrowHeld(CamRot.Vector());
	}
}

void AAfterfallCharacter::OnDropPressed()
{
	if (IsLocallyControlled() && !IsQuickMenuOpen())
	{
		ServerDropHeld();
	}
}

void AAfterfallCharacter::OnMatPressed()
{
	if (!IsLocallyControlled() || !MatInteract || IsQuickMenuOpen())
	{
		return;
	}
	// Toggle: repeated presses are safe (the component honors requests only
	// from the states that allow them; every other press is a no-op).
	if (MatInteract->IsInteractionBlocking())
	{
		MatInteract->ServerRequestMatClose();
	}
	else
	{
		MatInteract->ServerRequestMatOpen();
	}
}

void AAfterfallCharacter::OnBuildPressed()
{
	if (!IsLocallyControlled() || IsQuickMenuOpen() || IsMatInventoryOpen() || !Inventory)
	{
		return; // never build through the menu or while bound to the mat
	}
	AController* PC = GetController();
	if (!PC)
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFBuildTrace), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamRot.Vector() * BuildRangeCm,
		ECC_Visibility, Params))
	{
		return; // nothing in range: no request, no cost
	}
	// Ground/actor surface bootstraps a foundation; aiming at an existing part
	// stacks a wall on it. Both go through the same validation + spawn path.
	const EStructuralPart Part = Cast<AStructuralActor>(Hit.GetActor())
		? EStructuralPart::Wall : EStructuralPart::Foundation;
	ServerRequestPlacement(BuildPreviewRequest(Part, Hit.ImpactPoint, CamRot));
}

void AAfterfallCharacter::OnFirePressed()
{
	if (!IsLocallyControlled() || IsQuickMenuOpen() || IsRadialMenuOpen())
	{
		return; // menu eats fire input: no weapon discharge through the UI
	}
	FVector CamLoc;
	FRotator CamRot;
	if (AController* PC = GetController())
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		// FR-004: register the local prediction first, then send its id
		// with the unchanged server-authoritative hitscan.
		const FVector FireDir = CamRot.Vector();
		ServerFireHitscanWithPrediction(FireDir, RegisterPredictedHit(false, FireDir));
	}
}

void AAfterfallCharacter::OnReloadPressed()
{
	if (IsLocallyControlled() && !IsQuickMenuOpen() && !IsRadialMenuOpen())
	{
		ServerReload();
	}
}

bool AAfterfallCharacter::ServerFireHitscan_Validate(const FVector& Direction)
{
	return !Direction.IsNearlyZero() && Direction.SizeSquared() < 4.f;
}

void AAfterfallCharacter::ServerFireHitscan_Implementation(const FVector& Direction)
{
	if (!HasAuthority() || (Health && Health->bIsDead) || IsMatInventoryOpen())
	{
		return;
	}
	// Fire-rate gate (server owns the clock): 0.25s between shots.
	const double Now = GetWorld() ? GetWorld()->TimeSeconds : 0.0;
	if (Now - LastFireTime < (double)FireIntervalSec)
	{
		return;
	}
	LastFireTime = Now;
	AController* PC = GetController();
	if (!PC)
	{
		return;
	}
	// Magazine gate: dry-fire gives feedback but spends nothing.
	if (AmmoInMag <= 0)
	{
		UE_LOG(LogAfterfall, Log, TEXT("Dry fire — reload (R)"));
		return;
	}
	// Bullet initiator: server owns the trace, the energy proxy, and the
	// propagation. The client contributes aim only — never cells or damage.
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Dir = Direction.GetSafeNormal();
	static constexpr float HitscanRangeCm = 5000.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFFireHitscan), false, this);
	FHitResult Hit;
	--AmmoInMag;
	++AmmoConsumed;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + Dir * HitscanRangeCm,
		ECC_Visibility, Params))
	{
		return; // whiff: round spent, nothing hit
	}
	// Zombie branch (same server-validated path as melee): part-resolved,
	// range-gated inside ServerHitPart, never client-chosen damage.
	if (AZombiePrototype* Z = Cast<AZombiePrototype>(Hit.GetActor()))
	{
		EBodyPart Part = EBodyPart::Torso;
		AZombiePrototype::PartFromComponent(Hit.GetComponent(), Part);
		Z->ServerHitPart(Part, PC);
		++ConfirmedHits;
		UE_LOG(LogAfterfall, Log, TEXT("Hit %s (%s) ammo %d/%d"),
			*Z->GetName(), *UEnum::GetValueAsString(Part), AmmoInMag, MagSize);
		return;
	}
	if (AStructuralActor* S = Cast<AStructuralActor>(Hit.GetActor()))
	{
		FDestructionEvent Bullet;
		Bullet.Origin = Hit.ImpactPoint;
		Bullet.Direction = Dir;
		Bullet.InitialEnergy = UDestructionPropagationService::BulletEnergy();
		Bullet.Initiator = EDestructionInitiator::Bullet;
		Bullet.MaterialId = UDestructionPropagationService::MaterialForPart(S->Spec.Part);
		if (S->ServerApplyImpact(Bullet,
			UDestructionPropagationService::BulletDamagePerCell(), FGuid::NewGuid()) > 0)
		{
			ConfirmedHits++;
		}
	}
	if (AProceduralObjectActor* Object = Cast<AProceduralObjectActor>(Hit.GetActor()))
	{
		if (ApplyProceduralObjectImpact(Object, Hit.ImpactPoint, Hit.ImpactNormal, Dir,
			UDestructionPropagationService::BulletEnergy()))
		{
			++ConfirmedHits;
		}
	}
}

bool AAfterfallCharacter::ServerFireHitscanWithPrediction_Validate(const FVector& Direction, FGuid PredictionId)
{
	return ServerFireHitscan_Validate(Direction);
}

void AAfterfallCharacter::ServerFireHitscanWithPrediction_Implementation(const FVector& Direction, FGuid PredictionId)
{
	// Thin reconcile wrapper: the existing body stays the single authority
	// path. ConfirmedHits moves only on authoritative damage branches, so
	// the before/after delta IS the authoritative answer (whiffs, dry-fire,
	// rate-gate and dead/mat rejections all leave it unchanged -> false).
	const int32 BeforeHits = ConfirmedHits;
	ServerFireHitscan_Implementation(Direction);
	if (PredictionId.IsValid())
	{
		ClientResolveHitPrediction(PredictionId, ConfirmedHits != BeforeHits);
	}
}

void AAfterfallCharacter::ClientResolveHitPrediction_Implementation(FGuid PredictionId, bool bAuthoritativeHit)
{
	// Reconcile runs client-side only (Client RPC). HP is never touched
	// here — only the local prediction feedback counters move.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	switch (PendingPredictions.Resolve(PredictionId, bAuthoritativeHit, Now))
	{
	case Afterfall::EHitPredictionOutcome::Confirmed:
		++AuthoritativeHits;
		++ReconciledPredictions;
		break;
	case Afterfall::EHitPredictionOutcome::Retracted:
		++RetractedPredictions;
		if (PredictedHits > 0)
		{
			--PredictedHits;
		}
		UE_LOG(LogAfterfall, Log, TEXT("Prediction retracted by server (authoritative miss)"));
		break;
	case Afterfall::EHitPredictionOutcome::AuthoritativeOnly:
		++AuthoritativeHits;
		UE_LOG(LogAfterfall, Log, TEXT("Server-confirmed hit had no local prediction"));
		break;
	case Afterfall::EHitPredictionOutcome::AgreedNoEvent:
		break;
	case Afterfall::EHitPredictionOutcome::Expired:
		UE_LOG(LogAfterfall, Log, TEXT("Prediction answer arrived after TTL"));
		break;
	case Afterfall::EHitPredictionOutcome::AlreadyResolved:
		break;
	case Afterfall::EHitPredictionOutcome::UnknownId:
	default:
		UE_LOG(LogAfterfall, Warning, TEXT("Reconcile for unknown prediction id"));
		break;
	}
}

FGuid AAfterfallCharacter::RegisterPredictedHit(bool bMelee, const FVector& Direction)
{
	// Client-local prediction: mirror the server's camera trace (melee
	// reach for swings, 5 m hitscan range for test-fire) and register the
	// expectation. Damageable set matches the server branches exactly:
	// zombie + structural for hitscan, plus box wall for melee.
	if (!IsLocallyControlled() || !GetWorld())
	{
		return FGuid();
	}
	AController* PC = GetController();
	if (!PC)
	{
		return FGuid();
	}
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	static constexpr float PredictHitscanRangeCm = 5000.f;
	const FVector End = bMelee
		? CamLoc + CamRot.Vector() * MeleeRangeCm
		: CamLoc + Direction.GetSafeNormal() * PredictHitscanRangeCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFPredictHit), false, this);
	FHitResult Hit;
	bool bDamageable = false;
	FName TargetName = NAME_None;
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params)
		&& Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();
		TargetName = HitActor->GetFName();
		if (Cast<AZombiePrototype>(HitActor) || Cast<AStructuralActor>(HitActor))
		{
			bDamageable = true;
		}
		else if (bMelee && Cast<ABoxWallStructure>(HitActor))
		{
			bDamageable = true;
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FGuid PredictionId = PendingPredictions.Add(TargetName, bDamageable, Now);
	if (bDamageable)
	{
		++PredictedHits; // immediate local feedback; retracted if the server disagrees
	}
	return PredictionId;
}

bool AAfterfallCharacter::ServerSelfTreatBandage_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerSelfTreatBandage_Implementation()
{
	if (!HasAuthority() || !Injury)
	{
		return;
	}
	if (Health && Health->bIsDead)
	{
		return; // no treating while dead
	}
	// Priority rule: first actively-bleeding part, else first wounded part.
	int32 BleedingIdx = -1;
	int32 FirstWoundedIdx = -1;
	for (int32 i = 0; i < Injury->PartInjuries.Num(); ++i)
	{
		const FInjuryRecord& R = Injury->PartInjuries[i];
		if (!R.bWounded)
		{
			continue;
		}
		if (R.bBleeding)
		{
			BleedingIdx = i;
			break; // deterministic: first bleeding part wins
		}
		if (FirstWoundedIdx < 0)
		{
			FirstWoundedIdx = i;
		}
	}
	const int32 ChosenIdx = BleedingIdx >= 0 ? BleedingIdx : FirstWoundedIdx;
	if (ChosenIdx < 0)
	{
		return; // nothing to treat; single no-op request, nothing consumed
	}
	static const FName BandageId = TEXT("AF_MED_Bandage");
	Injury->ServerBeginTreatment(static_cast<EBodyPart>(ChosenIdx), BandageId);
	// Item is consumed exactly once at successful completion by the
	// InjuryState/MedicalCapability pipeline (server-atomic).
}

bool AAfterfallCharacter::ServerInterruptSelfTreatment_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerInterruptSelfTreatment_Implementation()
{
	if (HasAuthority() && Injury)
	{
		Injury->ServerInterruptTreatment();
	}
}

bool AAfterfallCharacter::ServerThrowHeld_Validate(const FVector& Direction)
{
	return !Direction.IsNearlyZero() && Direction.SizeSquared() < 4.f;
}

void AAfterfallCharacter::ServerThrowHeld_Implementation(const FVector& Direction)
{
	if (Carry)
	{
		Carry->ServerThrow(Direction);
	}
}

bool AAfterfallCharacter::ServerDropHeld_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerDropHeld_Implementation()
{
	if (Carry)
	{
		Carry->ServerDrop();
	}
}

bool AAfterfallCharacter::ServerDeployMat_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerDeployMat_Implementation()
{
	if (!HasAuthority() || DeployedMat)
	{
		return; // one deployed mat per player for the slice
	}
	FActorSpawnParameters P;
	P.Owner = this;
	P.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector At = GetActorLocation() + GetActorForwardVector() * 150.f;
	if (APhysicalStorageMat* Mat = GetWorld()->SpawnActor<APhysicalStorageMat>(
		APhysicalStorageMat::StaticClass(), At, FRotator::ZeroRotator, P))
	{
		DeployedMat = Mat;
		Mat->ContainerGuid = FGuid::NewGuid();
		UE_LOG(LogAfterfall, Log, TEXT("Mat deployed at %s"), *At.ToString());
	}
}

void AAfterfallCharacter::OnInteractPressed()
{
	if (IsLocallyControlled())
	{
		// E is the self-treatment modifier: held E + LMB treats a wound.
		bEKeyHeld = true;
	}
	if (IsQuickMenuOpen())
	{
		// Gamepad menu confirm: activate the highlighted button.
		if (APlayerController* MPC = Cast<APlayerController>(GetController()))
		{
			if (AAfterfallHUD* MH = Cast<AAfterfallHUD>(MPC->GetHUD()))
			{
				MH->ActivateSelectedButton();
				return;
			}
		}
		return;
	}
	if (IsLocallyControlled() && Interact && Interact->FocusedActor.IsValid())
	{
		ServerInteractRequest(Interact->FocusedActor.Get());
	}
}

void AAfterfallCharacter::OnInteractReleased()
{
	bEKeyHeld = false;
	// Hold-to-treat: releasing E aborts an in-progress bandage application.
	if (Injury && Injury->TreatmentState == ETreatmentState::Treating)
	{
		ServerInterruptSelfTreatment();
	}
}

bool AAfterfallCharacter::IsRadialMenuOpen() const
{
	return RadialMenu && RadialMenu->bIsMenuOpen;
}

void AAfterfallCharacter::OnAttackPressed()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (IsQuickMenuOpen())
	{
		// LMB click while the menu is open: hit-test menu buttons.
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (AAfterfallHUD* H = Cast<AAfterfallHUD>(PC->GetHUD()))
			{
				H->HandleMenuClick();
			return;
			}
		}
		return;
	}
	if (IsMatInventoryOpen() || !RadialMenu)
	{
		// Mat open (or no wheel): preserve the legacy immediate path.
		DefaultLMBAction();
		return;
	}
	// Mouse-1 press: start the 330 ms wheel hold. A press that completes a
	// double click instead claims the gesture for weapon-hand rotation, so
	// neither rotation nor the wheel can also produce an attack.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bPendingClick && AFClick::IsDoubleClick(Now, LMBReleaseSec,
		(double)DoubleClickWindowSec, (double)ClickResolveDelaySec))
	{
		// Second click: swallow the pending first click (it never attacks)
		// and rotate while this press is held. No wheel on the second press.
		bPendingClick = false;
		GetWorldTimerManager().ClearTimer(ClickResolveTimer);
		bWeaponRotating = true;
		bLMBHeld = true;
		LMBHoldStartSec = Now;
		GetWorldTimerManager().ClearTimer(RadialOpenTimer);
		return;
	}
	bLMBHeld = true;
	LMBHoldStartSec = Now;
	LMBHoldTarget = Interact ? Interact->FocusedActor.Get() : nullptr;
	RadialDragAccum = FVector2D::ZeroVector;
	GetWorldTimerManager().ClearTimer(RadialOpenTimer);
	GetWorldTimerManager().SetTimer(RadialOpenTimer, this,
		&AAfterfallCharacter::OpenRadialFromHold, RadialMenu->HoldThresholdSec, false);
}

void AAfterfallCharacter::OnAttackReleased()
{
	if (!IsLocallyControlled() || !bLMBHeld)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double Held = Now - LMBHoldStartSec;
	bLMBHeld = false;
	GetWorldTimerManager().ClearTimer(RadialOpenTimer);
	if (IsRadialMenuOpen())
	{
		// Hold converted the input into a wheel action: confirm the choice
		// and never fall through to an attack.
		ConfirmRadialFromHold();
		return;
	}
	if (bWeaponRotating)
	{
		// Exit action for rotation: the hand returns to rest.
		bWeaponRotating = false;
		if (WeaponHandPivot)
		{
			WeaponHandPivot->SetRelativeRotation(FRotator::ZeroRotator);
		}
		return;
	}
	if (!AFClick::ReleaseIsClick(Held, (double)RadialMenu->HoldThresholdSec, false))
	{
		return; // held past 330 ms with no options: nothing to fire
	}
	// Click: defer the attack behind the resolve delay so a second click
	// inside the double-click window can claim the gesture instead.
	bPendingClick = true;
	LMBReleaseSec = Now;
	GetWorldTimerManager().ClearTimer(ClickResolveTimer);
	GetWorldTimerManager().SetTimer(ClickResolveTimer, this,
		&AAfterfallCharacter::ResolveSingleClick, ClickResolveDelaySec, false);
}

void AAfterfallCharacter::DefaultLMBAction()
{
	if (!IsMatInventoryOpen())
	{
		// Bandage self-treatment dispatch: hold E + LMB treats an active wound
		// instead of a melee swing. Without E held the melee path is unchanged.
		bool bAnyWound = false;
		if (Injury)
		{
			for (const FInjuryRecord& R : Injury->PartInjuries)
			{
				if (R.bWounded) { bAnyWound = true; break; }
			}
		}
		if (bEKeyHeld && bAnyWound)
		{
			ServerSelfTreatBandage();
			return;
		}
		// FR-004: register the local prediction first, then send its id
		// with the unchanged server-authoritative melee swing.
		ServerAttackWithPrediction(RegisterPredictedHit(true, FVector::ZeroVector));
	}
}

void AAfterfallCharacter::ResolveSingleClick()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (!AFClick::ResolvesToAttack(bPendingClick, IsRadialMenuOpen(), bWeaponRotating))
	{
		bPendingClick = false;
		return;
	}
	bPendingClick = false;
	LMBReleaseSec = 0.0;
	// Drive the adaptive-limb solver for a resolved click; radial holds bypass this path.
	SetReachTargetFromCamera();
	GetWorldTimerManager().ClearTimer(RightArmRecoveryTimer);
	GetWorldTimerManager().SetTimer(RightArmRecoveryTimer, this, &AAfterfallCharacter::OnReachReleased, 0.22f, false);
	// A normal Mouse-1 click performs the focused interaction (including item
	// pickup). If there is no interactable target, preserve the melee fallback.
	if (bEKeyHeld)
	{
		DefaultLMBAction(); // preserve the E+Mouse-1 treatment chord
	}
	else if (Interact && Interact->FocusedActor.IsValid())
	{
		ServerInteractRequest(Interact->FocusedActor.Get());
	}
	else
	{
		DefaultLMBAction();
	}
}

void AAfterfallCharacter::RotateWeaponHand(const FVector2D& MouseDelta)
{
	if (!WeaponHandPivot)
	{
		return;
	}
	FRotator R = WeaponHandPivot->GetRelativeRotation();
	R.Yaw += MouseDelta.X * WeaponRotationSpeedDeg;
	R.Pitch = FMath::Clamp(R.Pitch - MouseDelta.Y * WeaponRotationSpeedDeg,
		-WeaponPitchClampDeg, WeaponPitchClampDeg);
	WeaponHandPivot->SetRelativeRotation(R);
}

void AAfterfallCharacter::FireGrabHold()
{
	if (!IsLocallyControlled() || bGrabHoldFired)
	{
		return;
	}
	AActor* Target = GrabHoldTarget.Get();
	if (!Target || Target->IsPendingKillPending())
	{
		UE_LOG(LogAfterfall, Log, TEXT("GRAB rejected: target gone before the 230ms hold"));
		return;
	}
	// One entry point for every grabbable thing; the server re-validates
	// range/facing/state and rejects unsupported classes cleanly.
	bGrabHoldFired = true;
	ServerRequestBodyGrab(Target, true);
}

void AAfterfallCharacter::OpenRadialFromHold()
{
	if (!bLMBHeld || !IsLocallyControlled() || IsQuickMenuOpen() || IsMatInventoryOpen() || !RadialMenu)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!URadialMenuComponent::ShouldOpenMenu(Now - LMBHoldStartSec, (double)RadialMenu->HoldThresholdSec))
	{
		return;
	}
	// Re-resolve the target at open time: aim may have drifted during the hold.
	LMBHoldTarget = Interact ? Interact->FocusedActor.Get() : nullptr;
	TArray<FRadialMenuOption> Opts = RadialMenu->BuildContextualOptions();
	if (Opts.Num() == 0)
	{
		return; // nothing useful: release falls through to the default action
	}
	RadialMenu->OpenAtCrosshair(Opts);
}

void AAfterfallCharacter::ConfirmRadialFromHold()
{
	if (!RadialMenu || !RadialMenu->bIsMenuOpen)
	{
		return;
	}
	const ERadialAction Action = RadialMenu->GetSelectedAction();
	AActor* Target = LMBHoldTarget.Get();
	RadialMenu->Cancel();
	LMBHoldTarget = nullptr;
	RadialDragAccum = FVector2D::ZeroVector;
	if (Action == ERadialAction::Cancel)
	{
		return;
	}
	// Target loss: drop target-bound choices, never retarget silently.
	// The server re-validates range/facing/state before executing anyway.
	const bool bNeedsTarget = Action != ERadialAction::Treat
		&& Action != ERadialAction::Throw
		&& Action != ERadialAction::Drop
		&& Action != ERadialAction::Cancel;
	if (bNeedsTarget)
	{
		if (!Target || Target->IsPendingKillPending())
		{
			return;
		}
		if (Interact && Interact->FocusedActor.IsValid() && Interact->FocusedActor.Get() != Target)
		{
			return; // aim moved to something else mid-hold: no silent retarget
		}
	}
	ExecuteRadialAction(Action, Target);
}

void AAfterfallCharacter::CancelRadialMenu()
{
	bLMBHeld = false;
	bPendingClick = false;
	bWeaponRotating = false;
	GetWorldTimerManager().ClearTimer(ClickResolveTimer);
	if (WeaponHandPivot)
	{
		WeaponHandPivot->SetRelativeRotation(FRotator::ZeroRotator);
	}
	LMBHoldTarget = nullptr;
	RadialDragAccum = FVector2D::ZeroVector;
	GetWorldTimerManager().ClearTimer(RadialOpenTimer);
	if (RadialMenu)
	{
		RadialMenu->Cancel();
	}
}

void AAfterfallCharacter::ExecuteRadialAction(ERadialAction Action, AActor* Target)
{
	switch (Action)
	{
	case ERadialAction::Inspect:
		// Display-only: surface the prompt, never touch state.
		if (Target && Target->Implements<UInteractable>())
		{
			LastInspectText = IInteractable::Execute_GetInteractPrompt(Target);
		}
		else if (Target)
		{
			LastInspectText = FText::FromString(Target->GetName());
		}
		else
		{
			LastInspectText = FText::GetEmpty();
		}
		break;
	case ERadialAction::Grab:
	case ERadialAction::Drag:
		// Same physics-handle constraint as the E-channel grab; the server
		// validates ragdoll state, so Drag cannot move a living zombie.
		if (Target)
		{
			ServerRequestBodyGrab(Target, true);
		}
		break;
	case ERadialAction::Release:
		if (Target)
		{
			ServerRequestBodyGrab(Target, false);
		}
		break;
	case ERadialAction::Use:
	case ERadialAction::PickUp:
		// One shared entry for usable objects: the item's own
		// IInteractable::ServerInteract runs the validated transaction.
		if (Target)
		{
			ServerInteractRequest(Target);
		}
		break;
	case ERadialAction::Throw:
		if (Carry && Carry->bIsCarrying)
		{
			if (AController* PC = GetController())
			{
				FVector CamLoc;
				FRotator CamRot;
				PC->GetPlayerViewPoint(CamLoc, CamRot);
				ServerThrowHeld(CamRot.Vector());
			}
		}
		break;
	case ERadialAction::Drop:
		ServerDropHeld();
		break;
	case ERadialAction::Treat:
		// Canonical treatment pipeline: priority part + bandage item,
		// validated and consumed server-side. Same as the E+LMB path.
		ServerSelfTreatBandage();
		break;
	case ERadialAction::Access:
		if (MatInteract)
		{
			MatInteract->ServerRequestMatOpen();
		}
		break;
	case ERadialAction::PackUp:
		if (MatInteract)
		{
			MatInteract->ServerRequestMatClose();
		}
		break;
	case ERadialAction::Craft:
		// Wheel only offers Craft on a storage mat (the station context); the
		// server re-validates target + range before touching the inventory.
		if (Target)
		{
			ServerCraftCombine(Target, FGuid::NewGuid());
		}
		break;
	default:
		break; // legacy/no-op actions never execute state
	}
}

void AAfterfallCharacter::OnGrabHoldPressed()
{
	if (!IsLocallyControlled() || IsQuickMenuOpen() || IsMatInventoryOpen())
	{
		return;
	}
	if (IsRadialMenuOpen())
	{
		// RMB is the wheel's cancel chord: never start a grab through the menu.
		CancelRadialMenu();
		return;
	}
	// Hold 230 ms (elapsed time) before committing: a tap never grabs.
	// The target is captured at press and re-validated by the server at commit.
	GrabHoldTarget = Interact ? Interact->FocusedActor.Get() : nullptr;
	bGrabHoldFired = false;
	GetWorldTimerManager().ClearTimer(GrabHoldTimer);
	if (GrabHoldTarget.IsValid())
	{
		GetWorldTimerManager().SetTimer(GrabHoldTimer, this,
			&AAfterfallCharacter::FireGrabHold, GrabHoldSec, false);
	}
}

void AAfterfallCharacter::OnGrabHoldReleased()
{
	GetWorldTimerManager().ClearTimer(GrabHoldTimer);
	if (!IsLocallyControlled())
	{
		return;
	}
	// A tap that never reached 230 ms releases nothing (clean no-op).
	// A committed body grab detaches with its momentum preserved; a carried
	// item stays held (Q/G own drop/throw), so letting go never launches it.
	AActor* Target = GrabHoldTarget.Get();
	if (bGrabHoldFired && Target)
	{
		ServerRequestBodyGrab(Target, false);
	}
	bGrabHoldFired = false;
	GrabHoldTarget = nullptr;
}

void AAfterfallCharacter::OnCrouchPressed()
{
	if (IsQuickMenuOpen())
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AAfterfallCharacter::OnInventoryChangedForLoad()
{
	// Single recompute point for the physical load state (one authoritative
	// writer): wallet mass + held-slot mass are summed here, and
	// CarryComponent::SyncCarryFlag calls this same path after every
	// pickup/drop/throw/store, so neither writer can clobber the other.
	if (!Inventory) return;
	if (UAfterfallMovementComponent* Move = GetAfterfallMovement())
	{
		Move->SetCarriedMass(Inventory->GetWeightKg(),
			Carry ? Carry->GetCarriedMassKg() : 0.f);
	}
}
void AAfterfallCharacter::OnPausePressed()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (IsRadialMenuOpen())
	{
		// Esc cancels the wheel; the pause menu stays untouched.
		CancelRadialMenu();
		return;
	}
	ToggleQuickMenu();
}

void AAfterfallCharacter::ToggleQuickMenu()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	UWorld* W = GetWorld();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!W || !PC)
	{
		return;
	}
	bQuickMenuOpen = !bQuickMenuOpen;
	MenuAnimStart = FPlatformTime::Seconds();
	MenuSelectedIndex = 0;
	if (bQuickMenuOpen)
	{
		if (QuickMenuPage != EQuickMenuPage::Main && !HealthIsDead())
		{
			// Preserve page across toggles only while alive; death returns
			// to Main so Respawn is immediately visible.
		}
		if (HealthIsDead())
		{
			QuickMenuPage = EQuickMenuPage::Main;
		}
		// Established pause mechanism: gameplay + AI halt; HUD (Canvas)
		// keeps drawing so the menu animates while paused.
		UGameplayStatics::SetGamePaused(W, true);
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		ControlsCaptureAction = NAME_None;
		UGameplayStatics::SetGamePaused(W, false);
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void AAfterfallCharacter::CloseQuickMenu()
{
	if (bQuickMenuOpen)
	{
		ToggleQuickMenu();
	}
}

void AAfterfallCharacter::OnInventoryPressed()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	ToggleInventory();
}

void AAfterfallCharacter::ToggleInventory()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (IsRadialMenuOpen())
	{
		// Inventory takes cursor ownership; the wheel must not fight it.
		CancelRadialMenu();
	}
	bInventoryOpen = !bInventoryOpen;
	if (bInventoryOpen)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
	else
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{				PC->bShowMouseCursor = false;
				PC->SetInputMode(FInputModeGameOnly());
			}
		}
		// The mat widget follows the flag: open on Tab, close on Tab again.
		ShowInventoryMat(bInventoryOpen);
	}

void AAfterfallCharacter::OpenInventory()
{
	if (!bInventoryOpen)
	{
		ToggleInventory();
	}
}

void AAfterfallCharacter::CloseInventory()
{
	if (bInventoryOpen)
	{
		ToggleInventory();
	}
}

bool AAfterfallCharacter::IsInventoryMatOpen() const
{
	return InventoryMatWidget && InventoryMatWidget->IsInViewport() && InventoryMatWidget->IsVisible();
}

void AAfterfallCharacter::ShowInventoryMat(bool bShow)
{
	if (!IsLocallyControlled())
	{
		return; // only the local pawn ever owns UI
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	if (!InventoryMatWidget)
	{
		InventoryMatWidget = CreateWidget<UInventoryMatWidget>(PC, UInventoryMatWidget::StaticClass());
	}
	if (!InventoryMatWidget)
	{
		return;
	}
	// Re-bind every open: the component is fixed, but re-pushing the data
	// guarantees the grid matches the inventory at the moment it appears.
	InventoryMatWidget->SetInventory(AfterfallInventory);
	if (bShow)
	{
		if (!InventoryMatWidget->IsInViewport())
		{
			InventoryMatWidget->AddToViewport(10);
		}
		InventoryMatWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		InventoryMatWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------- Inventory UI Actions ---

bool AAfterfallCharacter::ServerUseInventorySlot_Validate(int32 SlotIndex)
{
	return SlotIndex >= 0 && Inventory != nullptr;
}

void AAfterfallCharacter::ServerUseInventorySlot_Implementation(int32 SlotIndex)
{
	if (!HasAuthority() || !Inventory)
	{
		return;
	}
	if (SlotIndex < 0 || SlotIndex >= Inventory->Slots.Num())
	{
		return;
	}
	Inventory->ServerUseItem(Inventory->Slots[SlotIndex].Item.InstanceId);
}

bool AAfterfallCharacter::ServerDropInventorySlot_Validate(int32 SlotIndex, int32 Count)
{
	return SlotIndex >= 0 && Count > 0 && Inventory != nullptr;
}

void AAfterfallCharacter::ServerDropInventorySlot_Implementation(int32 SlotIndex, int32 Count)
{
	if (!HasAuthority() || !Inventory)
	{
		return;
	}
	if (SlotIndex < 0 || SlotIndex >= Inventory->Slots.Num())
	{
		return;
	}
	Inventory->ServerDropItem(Inventory->Slots[SlotIndex].Item.InstanceId, Count);
}

bool AAfterfallCharacter::ServerEquipInventorySlot_Validate(int32 SlotIndex)
{
	return SlotIndex >= 0 && Inventory != nullptr;
}

void AAfterfallCharacter::ServerEquipInventorySlot_Implementation(int32 SlotIndex)
{
	if (!HasAuthority() || !Inventory)
	{
		return;
	}
	if (SlotIndex < 0 || SlotIndex >= Inventory->Slots.Num())
	{
		return;
	}
	Inventory->ServerEquipItem(Inventory->Slots[SlotIndex].Item.InstanceId);
}

bool AAfterfallCharacter::ServerUnequipInventorySlotByName_Validate(FName SlotName)
{
	return !SlotName.IsNone();
}

void AAfterfallCharacter::ServerUnequipInventorySlotByName_Implementation(FName SlotName)
{
	if (!HasAuthority() || !Inventory)
	{
		return;
	}
	Inventory->ServerUnequipItem(SlotName);
}

void AAfterfallCharacter::OnReachPressed()
{
	if (!IsLocallyControlled() || IsQuickMenuOpen() || IsMatInventoryOpen() || bInventoryOpen)
	{
		return;
	}
	SetReachTargetFromCamera();
	if (RightArm)
	{
		RightArm->AcquireContact();
	}
}

void AAfterfallCharacter::OnReachReleased()
{
	if (RightArm)
	{
		RightArm->ReleaseContact();
	}
}

void AAfterfallCharacter::SetReachTargetFromCamera()
{
	if (!FirstPersonCamera || !RightArm)
	{
		return;
	}
	FVector CamLoc = FirstPersonCamera->GetComponentLocation();
	FVector CamFwd = FirstPersonCamera->GetForwardVector();
	FVector Target = CamLoc + CamFwd * RightArm->Config.MaxReachCm;
	RightArm->SetReachTarget(Target);
}

bool AAfterfallCharacter::IsRightArmReaching() const
{
	return RightArm && (RightArm->GetLimbTarget().DesiredDistanceCm > 0.f);
}

FAdaptiveLimbPose AAfterfallCharacter::GetRightArmPose() const
{
	return RightArm ? RightArm->GetLimbPose() : FAdaptiveLimbPose();
}

bool AAfterfallCharacter::HealthIsDead() const
{
	return Health && Health->bIsDead;
}

EQuickMenuPage AAfterfallCharacter::QuickMenuNextPage(EQuickMenuPage From, const FName& ButtonId)
{
	if (ButtonId == FName(TEXT("Map"))) return EQuickMenuPage::Map;
	if (ButtonId == FName(TEXT("Controls"))) return EQuickMenuPage::Controls;
	if (ButtonId == FName(TEXT("Lab"))) return EQuickMenuPage::Lab;
	if (ButtonId == FName(TEXT("Back"))) return EQuickMenuPage::Main;
	return From; // action buttons (Resume/Respawn/Close/...) keep the page
}

void AAfterfallCharacter::QuickMenuAction(const FName& ButtonId)
{
	if (ButtonId == FName(TEXT("Resume")) || ButtonId == FName(TEXT("Close")))
	{
		CloseQuickMenu();
	}
	else if (ButtonId == FName(TEXT("Map")))
	{
		QuickMenuPage = EQuickMenuPage::Map;
		MenuAnimStart = FPlatformTime::Seconds();
	}
	else if (ButtonId == FName(TEXT("Controls")))
	{
		QuickMenuPage = EQuickMenuPage::Controls;
		MenuAnimStart = FPlatformTime::Seconds();
	}
	else if (ButtonId == FName(TEXT("Lab")))
	{
		QuickMenuPage = EQuickMenuPage::Lab;
		MenuAnimStart = FPlatformTime::Seconds();
	}
	else if (ButtonId == FName(TEXT("Back")))
	{
		QuickMenuPage = EQuickMenuPage::Main;
		ControlsCaptureAction = NAME_None;
		MenuAnimStart = FPlatformTime::Seconds();
	}
	else if (ButtonId == FName(TEXT("Respawn")))
	{
		if (HealthIsDead())
		{
			ServerRequestRespawn();
			CloseQuickMenu();
		}
	}
	else if (ButtonId == FName(TEXT("LabRun")))
	{
		// Lab needs a simulating world: close (unpause) first, then run.
		const bool bWasOpen = bQuickMenuOpen;
		if (bWasOpen)
		{
			CloseQuickMenu();
		}
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ConsoleCommand(TEXT("afPhysicsLab"), /*bWriteToLog=*/ true);
		}
	}
	else if (ButtonId == FName(TEXT("LabLog")))
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ConsoleCommand(TEXT("afPhysicsLog"), /*bWriteToLog=*/ true);
		}
	}
	else if (ButtonId == FName(TEXT("RestoreDefaults")))
	{
		RestoreDefaultBindings();
	}
	else if (ButtonId.ToString().StartsWith(TEXT("Capture:")))
	{
		ControlsCaptureAction = FName(*ButtonId.ToString().RightChop(8));
	}
}

void AAfterfallCharacter::GetBindingRows(TArray<FActionBindingRow>& Out) const
{
	Out.Reset();
	FActionBindingRow MoveRow;
	MoveRow.ActionName = FName(TEXT("Move"));
	MoveRow.KeyName = TEXT("WASD (fixed)");
	MoveRow.bRemappable = false;
	Out.Add(MoveRow);
	FActionBindingRow LookRow;
	LookRow.ActionName = FName(TEXT("Look"));
	LookRow.KeyName = TEXT("Mouse (fixed)");
	LookRow.bRemappable = false;
	Out.Add(LookRow);
	for (const TPair<FName, FKey>& K : CurrentKeys)
	{
		FActionBindingRow R;
		R.ActionName = K.Key;
		R.KeyName = K.Value.ToString();
		R.bRemappable = true;
		Out.Add(R);
	}
	Out.Sort([](const FActionBindingRow& A, const FActionBindingRow& B)
	{
		return A.ActionName.LexicalLess(B.ActionName);
	});
}

bool AAfterfallCharacter::RemapAction(FName ActionName, const FKey& NewKey)
{
	UInputAction** Slot = nullptr;
	if (ActionName == FName(TEXT("Jump"))) Slot = &IA_Jump;
	else if (ActionName == FName(TEXT("Sprint"))) Slot = &IA_Sprint;
	else if (ActionName == FName(TEXT("Interact"))) Slot = &IA_Interact;
	else if (ActionName == FName(TEXT("Attack"))) Slot = &IA_Attack;
	else if (ActionName == FName(TEXT("Crouch"))) Slot = &IA_Crouch;
	else if (ActionName == FName(TEXT("Throw"))) Slot = &IA_Throw;
	else if (ActionName == FName(TEXT("Drop"))) Slot = &IA_Drop;
	else if (ActionName == FName(TEXT("Mat"))) Slot = &IA_Mat;
	else if (ActionName == FName(TEXT("Fire"))) Slot = &IA_Fire;
	else if (ActionName == FName(TEXT("Reload"))) Slot = &IA_Reload;
	else if (ActionName == FName(TEXT("Pause"))) Slot = &IA_Pause;
	else if (ActionName == FName(TEXT("GrabHold"))) Slot = &IA_GrabHold;
	if (!Slot || !*Slot || !SliceIMC)
	{
		return false;
	}
	// Reject conflicts: the new key must not already drive another action.
	for (const TPair<FName, FKey>& K : CurrentKeys)
	{
		if (K.Key != ActionName && K.Value == NewKey)
		{
			return false;
		}
	}
	const FKey OldKey = CurrentKeys.FindRef(ActionName);
	if (OldKey.IsValid())
	{
		SliceIMC->UnmapKey(*Slot, OldKey);
	}
	const FKeyRecipe R = BindingRecipes.FindRef(ActionName);
	MapKey(SliceIMC, *Slot, NewKey, R.bNegX, R.bNegY, R.bSwizzle);
	SliceIMC->Modify();
	CurrentKeys.Add(ActionName, NewKey);
	ControlsCaptureAction = NAME_None;
	SaveBindingOverrides();
	return true;
}

void AAfterfallCharacter::RestoreDefaultBindings()
{
	static const TMap<FName, FKey> Defaults = {
		{ FName(TEXT("Jump")), EKeys::SpaceBar }, { FName(TEXT("Sprint")), EKeys::LeftShift },
		{ FName(TEXT("Interact")), EKeys::E }, { FName(TEXT("Attack")), EKeys::LeftMouseButton },
		{ FName(TEXT("Crouch")), EKeys::C }, { FName(TEXT("Throw")), EKeys::Q },
		{ FName(TEXT("Drop")), EKeys::G }, { FName(TEXT("Mat")), EKeys::B },
		{ FName(TEXT("Fire")), EKeys::F }, { FName(TEXT("Reload")), EKeys::R },
		{ FName(TEXT("Pause")), EKeys::Escape }, { FName(TEXT("GrabHold")), EKeys::RightMouseButton }
	};
	for (const TPair<FName, FKey>& D : Defaults)
	{
		UInputAction** Slot = nullptr;
		if (D.Key == FName(TEXT("Jump"))) Slot = &IA_Jump;
		else if (D.Key == FName(TEXT("Sprint"))) Slot = &IA_Sprint;
		else if (D.Key == FName(TEXT("Interact"))) Slot = &IA_Interact;
		else if (D.Key == FName(TEXT("Attack"))) Slot = &IA_Attack;
		else if (D.Key == FName(TEXT("Crouch"))) Slot = &IA_Crouch;
		else if (D.Key == FName(TEXT("Throw"))) Slot = &IA_Throw;
		else if (D.Key == FName(TEXT("Drop"))) Slot = &IA_Drop;
		else if (D.Key == FName(TEXT("Mat"))) Slot = &IA_Mat;
		else if (D.Key == FName(TEXT("Fire"))) Slot = &IA_Fire;
		else if (D.Key == FName(TEXT("Reload"))) Slot = &IA_Reload;
		else if (D.Key == FName(TEXT("Pause"))) Slot = &IA_Pause;
		else if (D.Key == FName(TEXT("GrabHold"))) Slot = &IA_GrabHold;
		if (!Slot || !*Slot || !SliceIMC)
		{
		continue;
		}
		const FKey OldKey = CurrentKeys.FindRef(D.Key);
		if (OldKey.IsValid() && OldKey != D.Value)
		{
			SliceIMC->UnmapKey(*Slot, OldKey);
			const FKeyRecipe R = BindingRecipes.FindRef(D.Key);
			MapKey(SliceIMC, *Slot, D.Value, R.bNegX, R.bNegY, R.bSwizzle);
		}
		CurrentKeys.Add(D.Key, D.Value);
	}
	ControlsCaptureAction = NAME_None;
	SaveBindingOverrides();
}

static const TCHAR* BindingFileName()
{
	return TEXT("AfterfallBindings.ini");
}

void AAfterfallCharacter::LoadBindingOverrides()
{
	const FString Path = FPaths::ProjectSavedDir() / BindingFileName();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (const FString& L : Lines)
	{
		FString A, K;
		if (!L.Split(TEXT("="), &A, &K))
		{
			continue;
		}
		const FName Action(*A);
		const FKey NewKey(*K);
		if (!BindingRecipes.Contains(Action) || !NewKey.IsValid())
		{
			continue;
		}
		// Only apply conflict-free overrides (file could be hand-edited).
		bool bConflict = false;
		for (const TPair<FName, FKey>& E : CurrentKeys)
		{
			if (E.Key != Action && E.Value == NewKey)
			{
				bConflict = true;
				break;
			}
		}
		if (bConflict)
		{
			continue;
		}
		UInputAction** Slot = nullptr;
		if (Action == FName(TEXT("Jump"))) Slot = &IA_Jump;
		else if (Action == FName(TEXT("Sprint"))) Slot = &IA_Sprint;
		else if (Action == FName(TEXT("Interact"))) Slot = &IA_Interact;
		else if (Action == FName(TEXT("Attack"))) Slot = &IA_Attack;
		else if (Action == FName(TEXT("Crouch"))) Slot = &IA_Crouch;
		else if (Action == FName(TEXT("Throw"))) Slot = &IA_Throw;
		else if (Action == FName(TEXT("Drop"))) Slot = &IA_Drop;
		else if (Action == FName(TEXT("Mat"))) Slot = &IA_Mat;
		else if (Action == FName(TEXT("Fire"))) Slot = &IA_Fire;
		else if (Action == FName(TEXT("Reload"))) Slot = &IA_Reload;
		else if (Action == FName(TEXT("Pause"))) Slot = &IA_Pause;
		if (!Slot || !*Slot || !SliceIMC)
		{
			continue;
		}
		const FKey OldKey = CurrentKeys.FindRef(Action);
		if (OldKey.IsValid() && OldKey != NewKey && SliceIMC)
		{
			SliceIMC->UnmapKey(*Slot, OldKey);
			const FKeyRecipe R = BindingRecipes.FindRef(Action);
			MapKey(SliceIMC, *Slot, NewKey, R.bNegX, R.bNegY, R.bSwizzle);
			CurrentKeys.Add(Action, NewKey);
		}
	}
}

void AAfterfallCharacter::SaveBindingOverrides() const
{
	FString Text;
	for (const TPair<FName, FKey>& K : CurrentKeys)
	{
		Text += K.Key.ToString() + TEXT("=") + K.Value.ToString() + TEXT("\n");
	}
	FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / BindingFileName()));
}

// ---------------------------------------------------------------- RPCs ---

bool AAfterfallCharacter::ServerInteractRequest_Validate(AActor* Target)
{
	return Target != nullptr;
}

void AAfterfallCharacter::ServerInteractRequest_Implementation(AActor* Target)
{
	if (!HasAuthority() || !Target || !Interact)
	{
		return;
	}
	if (!Interact->ServerValidateTarget(GetController(), Target))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Interact rejected: validation failed"));
		return;
	}
	IInteractable::Execute_ServerInteract(Target, GetController());
}

bool AAfterfallCharacter::ServerRequestBodyGrab_Validate(AActor* Target, bool bGrab)
{
	return Target != nullptr;
}

void AAfterfallCharacter::ServerRequestBodyGrab_Implementation(AActor* Target, bool bGrab)
{
	if (!HasAuthority() || !Target || (Health && Health->bIsDead) || !Interact)
	{
		return;
	}
	// ONE validation for every grabbable class: range, facing, target state.
	if (!Interact->ServerValidateTarget(GetController(), Target))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("GRAB rejected: validation failed for %s"),
			*Target->GetName());
		return;
	}
	// Objects (items): the carry slot's own validated pickup. RMB release
	// deliberately does NOT drop it - Q/G own drop/throw, so releasing the
	// button can never launch a held item.
	if (APhysicalItem* Item = Cast<APhysicalItem>(Target))
	{
		if (bGrab && Carry)
		{
			Carry->ServerPickup(Item);
		}
		return;
	}
	// Bodies: the ArticulatedBody owns the constraint (physics handle on the
	// ragdoll torso; release preserves momentum). ServerGrab refuses a
	// non-ragdoll body, so a living zombie can never be puppeted. Attack
	// state is untouched: LMB swings neither cancel nor disturb a hold.
	AZombiePrototype* Z = Cast<AZombiePrototype>(Target);
	if (!Z || !Z->Body)
	{
		UE_LOG(LogAfterfall, Warning,
			TEXT("GRAB rejected: unsupported target %s (bodies and items only)"),
			*Target->GetName());
		return;
	}
	if (bGrab)
	{
		if (Z->Body->IsRagdoll() && !Z->Body->IsGrabbed())
		{
			Z->Body->ServerGrab(GetController());
		}
		else if (!Z->Body->IsRagdoll())
		{
			UE_LOG(LogAfterfall, Warning, TEXT("GRAB rejected: %s is not a grabbable body"),
				*Target->GetName());
		}
	}
	else if (Z->Body->IsGrabbed())
	{
		Z->Body->ServerRelease();
	}
}

bool AAfterfallCharacter::ServerCraftCombine_Validate(AActor* Target, FGuid TransactionId)
{
	return Target != nullptr && TransactionId.IsValid();
}

void AAfterfallCharacter::ServerCraftCombine_Implementation(AActor* Target, FGuid TransactionId)
{
	if (!HasAuthority() || !Inventory || !Interact || !Target)
	{
		return;
	}
	// Context gate: the wheel only offers Craft on a storage mat, so the
	// server demands the same station context — a client cannot turn this
	// into a context-free craft-anywhere request.
	if (!Target->IsA<APhysicalStorageMat>())
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Craft rejected: not a crafting station"));
		return;
	}
	// Same range + facing gate as every other interaction: no reach exploits.
	if (!Interact->ServerValidateTarget(GetController(), Target))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Craft rejected: validation failed"));
		return;
	}
	FString Error;
	if (!Inventory->ServerCombineFirstAvailable(TransactionId, Error))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Craft rejected: %s"), *Error);
	}
}

void AAfterfallCharacter::OnGrapplePressed()
{
	// Toggle through the server: attach when idle, release when attached.
	// Server RPCs execute directly on authority, so one path covers both.
	if (Grapple && Grapple->IsAttached())
	{
		ServerReleaseGrapple();
	}
	else
	{
		ServerRequestGrapple();
	}
}

bool AAfterfallCharacter::ServerRequestGrapple_Validate()
{
	return Grapple != nullptr && !HealthIsDead();
}

void AAfterfallCharacter::ServerRequestGrapple_Implementation()
{
	if (!HasAuthority() || !Grapple || (Health && Health->bIsDead))
	{
		return;
	}
	Grapple->TryGrapple();
}

bool AAfterfallCharacter::ServerReleaseGrapple_Validate()
{
	return Grapple != nullptr;
}

void AAfterfallCharacter::ServerReleaseGrapple_Implementation()
{
	if (!HasAuthority() || !Grapple)
	{
		return;
	}
	Grapple->Release();
}

bool AAfterfallCharacter::ServerAttack_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerAttack_Implementation()
{
	if (!HasAuthority() || (Health && Health->bIsDead) || IsMatInventoryOpen())
	{
		return;
	}
	AController* PC = GetController();
	if (!PC)
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector End = CamLoc + CamRot.Vector() * MeleeRangeCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFAttack), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params))
	{
		return; // honest whiff
	}
	if (AZombiePrototype* Z = Cast<AZombiePrototype>(Hit.GetActor()))
	{
		EBodyPart Part = EBodyPart::Torso;
		AZombiePrototype::PartFromComponent(Hit.GetComponent(), Part);
		Z->ServerHitPart(Part, PC);
		ConfirmedHits++;
	}
	else if (ABoxWallStructure* W = Cast<ABoxWallStructure>(Hit.GetActor()))
	{
		// FR-013: graybox melee strike into the box wall. The wall's ledger
		// recomputes from its own authoritative state; GUID-deduped like the
		// StructuralActor path. Energy: graybox strike (m+v below) matches
		// the punch plan (strong hit ~300 J -> breaches drywall/wood).
		FBoxImpactInput Swing;
		Swing.EffectiveMassKg = 30.f;
		Swing.RelativeSpeedCmS = 450.f;
		W->ServerApplyBoxImpact(Hit.ImpactPoint, CamRot.Vector(), Swing,
			FGuid::NewGuid());
		ConfirmedHits++;
	}
	else if (AStructuralActor* S = Cast<AStructuralActor>(Hit.GetActor()))
	{
		// Attack initiator: melee contact runs directional propagation on the
		// structure. Server recomputes everything from its own trace.
		FDestructionEvent Swing;
		Swing.Origin = Hit.ImpactPoint;
		Swing.Direction = CamRot.Vector();
		Swing.InitialEnergy = UDestructionPropagationService::AttackEnergy();
		Swing.Initiator = EDestructionInitiator::Attack;
		Swing.MaterialId = UDestructionPropagationService::MaterialForPart(S->Spec.Part);
		if (S->ServerApplyImpact(Swing,
			UDestructionPropagationService::AttackDamagePerCell(), FGuid::NewGuid()) > 0)
		{
			ConfirmedHits++;
		}
	}
	else if (AProceduralObjectActor* Object = Cast<AProceduralObjectActor>(Hit.GetActor()))
	{
		if (ApplyProceduralObjectImpact(Object, Hit.ImpactPoint, Hit.ImpactNormal,
			CamRot.Vector(), UDestructionPropagationService::AttackEnergy()))
		{
			++ConfirmedHits;
		}
	}
	AmmoConsumed++;
}

bool AAfterfallCharacter::ServerAttackWithPrediction_Validate(FGuid PredictionId)
{
	return ServerAttack_Validate();
}

void AAfterfallCharacter::ServerAttackWithPrediction_Implementation(FGuid PredictionId)
{
	// Thin reconcile wrapper: the existing body stays the single authority
	// path. ConfirmedHits moves only on authoritative damage branches, so
	// the before/after delta IS the authoritative answer (whiffs and dead/
	// mat rejections leave it unchanged -> false).
	const int32 BeforeHits = ConfirmedHits;
	ServerAttack_Implementation();
	if (PredictionId.IsValid())
	{
		ClientResolveHitPrediction(PredictionId, ConfirmedHits != BeforeHits);
	}
}

// ---------------------------------------------------------------- Build (unchanged authority paths) ---

FPlacementRequest AAfterfallCharacter::BuildPreviewRequest(EStructuralPart Part, FVector HitLocation, FRotator ViewRotation) const
{
	// Canonical 50cm snap. Pure client-side preview helper — not authoritative.
	FPlacementRequest Req;
	Req.Part = Part;
	Req.GridLocation = UConstructionValidator::SnapToGrid(HitLocation);
	Req.Rotation = FRotator(0.f, FMath::GridSnap(ViewRotation.Yaw, 90.f), 0.f);
	return Req;
}

bool AAfterfallCharacter::ServerRequestPlacement_Validate(const FPlacementRequest& Request)
{
	return !Request.GridLocation.ContainsNaN();
}

static int32 GetRequesterPlayerId(const AAfterfallCharacter* Char)
{
	// Vertical-slice stub: PlayerState ID when present, else 0 (listen-server host).
	// Replaced by persistent account ID in Phase 6.
	if (Char && Char->GetPlayerState())
	{
		return Char->GetPlayerState()->GetPlayerId();
	}
	return 0;
}

void AAfterfallCharacter::ServerRequestPlacement_Implementation(const FPlacementRequest& Request)
{
	if (!HasAuthority() || !Inventory)
	{
		return;
	}

	// 1. Grid re-enforcement via shared validator (never trust client).
	FString Reason;
	const FVector Snapped = UConstructionValidator::SnapToGrid(Request.GridLocation);
	if (!UConstructionValidator::IsOnGrid(Request.GridLocation))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: off-grid"));
		return;
	}

	// 2. Support query: count structural parts near support socket (50cm below center).
	int32 NumSupporting = 0;
	{
		const FVector Socket = Snapped - FVector(0.f, 0.f, 25.f);
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UConstructionSupport), false);
		GetWorld()->OverlapMultiByChannel(
			Overlaps, Socket, FQuat::Identity, ECC_WorldStatic,
			FCollisionShape::MakeBox(FVector(30.f, 30.f, 30.f)), Params);
		for (const FOverlapResult& O : Overlaps)
		{
			if (Cast<AStructuralActor>(O.GetActor()))
			{
				NumSupporting++;
			}
		}
		// Foundations always count as self-anchored for bootstrapping.
		if (Request.Part == EStructuralPart::Foundation)
		{
			NumSupporting = FMath::Max(NumSupporting, 1);
		}
	}

	// 3. Ownership: reject when overlapping a part claimed by another player.
	bool bClaimedByOther = false;
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UConstructionOwnership), false);
		GetWorld()->OverlapMultiByChannel(
			Overlaps, Snapped, FQuat::Identity, ECC_WorldStatic,
			FCollisionShape::MakeBox(FVector(25.f, 25.f, 25.f)), Params);
		const int32 Me = GetRequesterPlayerId(this);
		for (const FOverlapResult& O : Overlaps)
		{
			if (AStructuralActor* Other = Cast<AStructuralActor>(O.GetActor()))
			{
				if (Other->bIsClaimed && Other->OwnerPlayerId != Me)
				{
					bClaimedByOther = true;
					break;
				}
			}
		}
	}

	// 4. Full shared validation (grid + support + ownership + cost).
	if (!UConstructionValidator::ValidatePlacement(
		Request.Part, Snapped, NumSupporting, bClaimedByOther, Inventory->GetWood(), Reason))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: %s"), *Reason);
		return;
	}

	// 5. Debit resources FIRST (server-authoritative wallet), then spawn.
	const int32 Cost = UConstructionValidator::GetMaterialCost(Request.Part);
	if (!Inventory->ServerTryConsume(Cost))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Placement rejected: consume failed"));
		return;
	}

	FStructuralSpec Spec;
	Spec.Part = Request.Part;
	Spec.MaterialCost = Cost;
	Spec.MaxDurability = UConstructionValidator::GetMaxDurability(Request.Part);
	Spec.bRequiresSupport = UConstructionValidator::RequiresSupport(Request.Part);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	if (AStructuralActor* NewPart = GetWorld()->SpawnActor<AStructuralActor>(AStructuralActor::StaticClass(), Snapped, Request.Rotation, SpawnParams))
	{
		NewPart->ServerInitialize(Spec, GetRequesterPlayerId(this));
		UE_LOG(LogAfterfall, Log, TEXT("Placement approved %d at %s (cost %d)"), (int32)Request.Part, *Snapped.ToString(), Cost);
	}
	else
	{
		// Refund on spawn collision failure so rejects never eat resources.
		Inventory->ServerAddResource(Cost);
		UE_LOG(LogAfterfall, Warning, TEXT("Placement failed: collision, refunded %d"), Cost);
	}
}

bool AAfterfallCharacter::ServerRequestRepair_Validate(AStructuralActor* Target)
{
	return Target != nullptr;
}

void AAfterfallCharacter::ServerRequestRepair_Implementation(AStructuralActor* Target)
{
	if (!HasAuthority() || !Target || !Inventory)
	{
		return;
	}
	const float Missing = Target->Spec.MaxDurability - Target->Durability;
	if (Missing <= 1.f)
	{
		return;
	}
	// Rule: 1 Wood per 50 missing durability, rounded up.
	const int32 Cost = FMath::Clamp(FMath::CeilToInt(Missing / 50.f), 1, 20);
	if (!Inventory->ServerTryConsume(Cost))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Repair rejected: need %d wood"), Cost);
		return;
	}
	Target->ServerRepair();
}

bool AAfterfallCharacter::ServerRequestUpgrade_Validate(AStructuralActor* Target)
{
	return Target != nullptr;
}

void AAfterfallCharacter::ServerRequestUpgrade_Implementation(AStructuralActor* Target)
{
	if (!HasAuthority() || !Target || !Inventory)
	{
		return;
	}
	if (Target->UpgradeTier != 0)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Upgrade rejected: already tier %d"), Target->UpgradeTier);
		return;
	}
	static constexpr int32 UpgradeCost = 2;
	if (!Inventory->ServerTryConsume(UpgradeCost))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("Upgrade rejected: need %d wood"), UpgradeCost);
		return;
	}
	Target->ServerUpgrade();
}

// ---------------------------------------------------------------- Death ---

void AAfterfallCharacter::DoRespawn()
{
	if (!HasAuthority())
	{
		return;
	}
	bRespawning = false;
	AController* PC = GetController();
	if (!PC || IsPendingKillPending())
	{
		return;
	}
	FVector Loc = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	FRotator Rot = FRotator::ZeroRotator;
	if (APlayerStart* Start = Cast<APlayerStart>(UGameplayStatics::GetActorOfClass(
		GetWorld(), APlayerStart::StaticClass())))
	{
		Loc = Start->GetActorLocation();
		Rot = Start->GetActorRotation();
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AAfterfallCharacter* Fresh = GetWorld()->SpawnActor<AAfterfallCharacter>(
		AAfterfallCharacter::StaticClass(), Loc, Rot, P))
	{
		PC->Possess(Fresh);
		UE_LOG(LogAfterfall, Log, TEXT("Player respawned at %s"), *Loc.ToString());
	}
	Destroy();
}

bool AAfterfallCharacter::ServerRequestRespawn_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerRequestRespawn_Implementation()
{
	// Manual respawn converges on the auto-respawn path: only the dead
	// respawn, and only one respawn ever fires (bRespawning latch).
	if (!HasAuthority() || !HealthIsDead())
	{
		return;
	}
	DoRespawn();
}

bool AAfterfallCharacter::ServerReload_Validate()
{
	return true;
}

void AAfterfallCharacter::ServerReload_Implementation()
{
	if (!HasAuthority() || HealthIsDead())
	{
		return;
	}
	if (AmmoInMag < MagSize)
	{
		AmmoInMag = MagSize;
		UE_LOG(LogAfterfall, Log, TEXT("Reloaded (%d)"), AmmoInMag);
	}
}

// ------------------------------------------------------------------ Voxel ---

bool AAfterfallCharacter::ServerRequestVoxelEdit_Validate(AVoxelChunkActor* Chunk,
	FIntVector Local, uint8 State)
{
	// Payload sanity only. Out-of-chunk coords are rejected (and logged) in
	// the implementation instead of failing validation, so a malformed request
	// cannot disconnect the client.
	return Chunk != nullptr
		&& FMath::Abs(Local.X) <= 4096 && FMath::Abs(Local.Y) <= 4096
		&& FMath::Abs(Local.Z) <= 4096
		&& State <= AfterfallVoxel::MaxMaterialId;
}

void AAfterfallCharacter::ServerRequestVoxelEdit_Implementation(AVoxelChunkActor* Chunk,
	FIntVector Local, uint8 State)
{
	if (!HasAuthority() || !Chunk)
	{
		return;
	}
	if (Health && Health->bIsDead)
	{
		return;
	}
	// The server owns the commit: coordinates/material are re-validated and
	// the edit sequence is assigned here, never by the client.
	Chunk->ServerApplyVoxelEdit(Local, State);
}

bool AAfterfallCharacter::ServerRequestVoxelResync_Validate(AVoxelChunkActor* Chunk)
{
	return Chunk != nullptr;
}

void AAfterfallCharacter::ServerRequestVoxelResync_Implementation(AVoxelChunkActor* Chunk)
{
	if (!HasAuthority() || !Chunk)
	{
		return;
	}
	Chunk->ServerSendSnapshotToAll();
}

void AAfterfallCharacter::HandleDeath()
{
	if (!HasAuthority() || bRespawning)
	{
		return;
	}
	// Dying packs the interaction up immediately (no timers survive death).
	if (MatInteract)
	{
		MatInteract->ForceClose();
	}
	bRespawning = true;
	UE_LOG(LogAfterfall, Log, TEXT("Player %s died — respawn in 3s"), *GetName());
	SetActorEnableCollision(false);
	GetCharacterMovement()->DisableMovement();

	// Auto-respawn converges on the same single path as the manual button
	// (one respawn ever: the timer is the only pending instance).
	FTimerHandle RespawnTimer;
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this,
		&AAfterfallCharacter::DoRespawn, 3.f, false);
}
