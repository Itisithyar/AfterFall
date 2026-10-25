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
#include "Movement/AfterfallGrappleComponent.h"
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
// Runtime input (no .uasset IA/IMC required — built in code, injected for the
// local player; the template BP bindings we cannot edit headless stay untouched).
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputModifiers.h"
#include "Afterfall.h"

AAfterfallCharacter::AAfterfallCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAfterfallMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	bReplicates = true;
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Interact = CreateDefaultSubobject<UInteractComponent>(TEXT("Interact"));
	Carry = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
	Grapple = CreateDefaultSubobject<UAfterfallGrappleComponent>(TEXT("Grapple"));
	MatInteract = CreateDefaultSubobject<UMatInteractionComponent>(TEXT("MatInteract"));
	RadialMenu = CreateDefaultSubobject<URadialMenuComponent>(TEXT("RadialMenu"));

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
		IA_Pause = MakeAction(this, TEXT("Slice_Pause"), EInputActionValueType::Boolean);
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
		MapKey(SliceIMC, IA_Pause, EKeys::Escape);
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
	PlayerInputComponent->BindKey(EKeys::ThumbMouseButton, IE_Pressed, this, &AAfterfallCharacter::OnGrapplePressed);
	PlayerInputComponent->BindKey(EKeys::ThumbMouseButton, IE_Released, this, &AAfterfallCharacter::OnGrappleReleased);
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
		EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &AAfterfallCharacter::OnPausePressed);
	}
}

bool AAfterfallCharacter::IsMatInventoryOpen() const
{
	return MatInteract && MatInteract->IsInteractionBlocking();
}

void AAfterfallCharacter::Jump()
{
	if (IsMatInventoryOpen())
	{
		return; // kneeling over the mat: no jumping until packed up
	}
	Super::Jump();
}

void AAfterfallCharacter::Move(const FInputActionValue& Value)
{
	if (IsMatInventoryOpen())
	{
		return; // server sees no input and halts the pawn; no movement-code change
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
	if (!IsLocallyControlled())
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
	if (IsLocallyControlled())
	{
		ServerDropHeld();
	}
}

void AAfterfallCharacter::OnMatPressed()
{
	if (!IsLocallyControlled() || !MatInteract)
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

void AAfterfallCharacter::OnFirePressed()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	if (AController* PC = GetController())
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		ServerFireHitscan(CamRot.Vector());
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
	AController* PC = GetController();
	if (!PC)
	{
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
	AmmoConsumed++;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + Dir * HitscanRangeCm,
		ECC_Visibility, Params))
	{
		return; // whiff: energy spent, nothing hit
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
	if (IsLocallyControlled() && Interact && Interact->FocusedActor.IsValid())
	{
		ServerInteractRequest(Interact->FocusedActor.Get());
	}
}

void AAfterfallCharacter::OnAttackPressed()
{
	if (IsLocallyControlled())
	{
		ServerAttack();
	}
}

void AAfterfallCharacter::OnCrouchPressed()
{
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
	UWorld* W = GetWorld();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!W || !PC)
	{
		return;
	}
	const bool bPaused = !UGameplayStatics::IsGamePaused(W);
	UGameplayStatics::SetGamePaused(W, bPaused);
	PC->bShowMouseCursor = bPaused;
	if (bPaused)
	{
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
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

	TWeakObjectPtr<AController> WeakPC(GetController());
	FTimerHandle RespawnTimer;
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, [this, WeakPC]()
	{
		bRespawning = false;
		AController* PC = WeakPC.Get();
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
	}, 3.f, false);
}

void AAfterfallCharacter::OnGrapplePressed(){ if(Grapple) Grapple->TryGrapple(); }
void AAfterfallCharacter::OnGrappleReleased(){ if(Grapple) Grapple->Release(); }
