// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Character) — Phase 3b: full server validation wiring +
// Slice: runtime input, interaction/combat RPCs, death + auto-respawn.
#include "AfterfallCharacter.h"
#include "Structural/StructuralActor.h"
#include "Structural/ConstructionValidator.h"
#include "Destruction/DestructionPropagation.h"
#include "Inventory/InventoryComponent.h"
#include "Combat/HealthComponent.h"
#include "Interaction/InteractComponent.h"
#include "Interaction/CarryComponent.h"
#include "UI/RadialMenuComponent.h"
#include "Interaction/MatInteractionComponent.h"
#include "Simulation/SimulationScheduler.h"
#include "Interaction/Interactable.h"
#include "Movement/AfterfallMovementComponent.h"
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
#include "Voxel/VoxelChunkActor.h"
#include "Voxel/VoxelChunkTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Afterfall.h"
#include "Movement/AfterfallGrappleComponent.h"

AAfterfallCharacter::AAfterfallCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAfterfallMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Interact = CreateDefaultSubobject<UInteractComponent>(TEXT("Interact"));
	Carry = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
	MatInteract = CreateDefaultSubobject<UMatInteractionComponent>(TEXT("MatInteract"));
	RadialMenu = CreateDefaultSubobject<URadialMenuComponent>(TEXT("RadialMenu"));
	Grapple = CreateDefaultSubobject<UAfterfallGrappleComponent>(TEXT("Grapple"));

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
		MapKey(SliceIMC, IA_Interact, EKeys::E);
		MapKey(SliceIMC, IA_Attack, EKeys::LeftMouseButton);
		MapKey(SliceIMC, IA_Crouch, EKeys::C);
		MapKey(SliceIMC, IA_Throw, EKeys::Q);
		MapKey(SliceIMC, IA_Drop, EKeys::G);
		MapKey(SliceIMC, IA_Mat, EKeys::B);
		MapKey(SliceIMC, IA_Fire, EKeys::F);
		MapKey(SliceIMC, IA_Reload, EKeys::R);
		MapKey(SliceIMC, IA_Pause, EKeys::Escape);
		MapKey(SliceIMC, IA_Build, EKeys::V);
		MapKey(SliceIMC, IA_Grapple, EKeys::T);
		MapKey(SliceIMC, IA_Grapple, EKeys::Gamepad_DPad_Right);
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
		EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AAfterfallCharacter::OnAttackPressed);
		EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AAfterfallCharacter::OnCrouchPressed);
		EIC->BindAction(IA_Throw, ETriggerEvent::Started, this, &AAfterfallCharacter::OnThrowPressed);
		EIC->BindAction(IA_Drop, ETriggerEvent::Started, this, &AAfterfallCharacter::OnDropPressed);
		EIC->BindAction(IA_Mat, ETriggerEvent::Started, this, &AAfterfallCharacter::OnMatPressed);
		EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &AAfterfallCharacter::OnFirePressed);
		EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &AAfterfallCharacter::OnReloadPressed);
		EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &AAfterfallCharacter::OnPausePressed);
		EIC->BindAction(IA_Build, ETriggerEvent::Started, this, &AAfterfallCharacter::OnBuildPressed);
		EIC->BindAction(IA_Grapple, ETriggerEvent::Started, this, &AAfterfallCharacter::OnGrapplePressed);
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
	if (IsMatInventoryOpen())
	{
		return; // server sees no input and halts the pawn; no movement-code change
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
	if (IsMatInventoryOpen() || IsQuickMenuOpen())
	{
		return; // menu open: mouse drives the cursor, never the camera
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
	if (!IsLocallyControlled() || IsQuickMenuOpen())
	{
		return; // menu eats fire input: no weapon discharge through the UI
	}
	FVector CamLoc;
	FRotator CamRot;
	if (AController* PC = GetController())
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		ServerFireHitscan(CamRot.Vector());
	}
}

void AAfterfallCharacter::OnReloadPressed()
{
	if (IsLocallyControlled() && !IsQuickMenuOpen())
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
	if (!IsMatInventoryOpen())
	{
		ServerAttack();
	}
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

void AAfterfallCharacter::OnPausePressed()
{
	if (!IsLocallyControlled())
	{
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
		{ FName(TEXT("Pause")), EKeys::Escape }
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
	AmmoConsumed++;
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
