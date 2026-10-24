// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Character) — first-person pawn. Placement preview is
// client-side ONLY; ServerRequestPlacement is the single authority path.
//
// Slice role: this C++ pawn is possessed at runtime by the slice subsystem
// (the template map pins template BPs we cannot edit headless), bringing
// camera + movement + health + interaction + combat to ANY map.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Structural/StructuralTypes.h"
#include "AfterfallCharacter.generated.h"

class UInventoryComponent;
class UHealthComponent;
class UInteractComponent;
class UCarryComponent;
class UAfterfallGrappleComponent;
class URadialMenuComponent;
class UMatInteractionComponent;
class UAfterfallMovementComponent;
class APhysicalStorageMat;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class AStructuralActor;
class AVoxelChunkActor;

/** Quick-menu page. Local UI state only; never replicated. */
UENUM(BlueprintType)
enum class EQuickMenuPage : uint8
{
	Main     UMETA(DisplayName = "Main"),
	Map      UMETA(DisplayName = "Map"),
	Controls UMETA(DisplayName = "Controls"),
	Lab      UMETA(DisplayName = "Lab")
};

/** One remappable binding row (keyboard only; Move/Look are fixed). */
USTRUCT(BlueprintType)
struct FActionBindingRow
{
	GENERATED_BODY()
	UPROPERTY() FName ActionName = NAME_None;
	UPROPERTY() FString KeyName;
	UPROPERTY() bool bRemappable = true;
};

UCLASS()
class AFTERFALL_API AAfterfallCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAfterfallCharacter(const FObjectInitializer& ObjectInitializer);

	// Custom momentum model (5s simulation-time sprint, carry cap).
	virtual UAfterfallMovementComponent* GetAfterfallMovement() const;

	// Server-authoritative wallet. Created as default subobject; replicated.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Inventory")
	UInventoryComponent* Inventory;

	// Server-authoritative health. Death snakes to HandleDeath (auto-respawn).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Health")
	UHealthComponent* Health;

	// Rope/grapple traversal (server-authoritative attach; momentum preserved).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Traversal")
	UAfterfallGrappleComponent* Grapple;

	// Single held-object slot (pickup/drop/throw transaction owner).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Carry")
	UCarryComponent* Carry;

	// Physical inventory mat open/close flow (B toggles; server-owned).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Mat")
	UMatInteractionComponent* MatInteract;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|UI")
	class URadialMenuComponent* RadialMenu;

	// Backpack mat; deployed copy lives in the world as a separate storage.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Storage")
	APhysicalStorageMat* DeployedMat;

	// Client-local camera focus (prompt display). Server never trusts it.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Interact")
	UInteractComponent* Interact;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Camera")
	UCameraComponent* FirstPersonCamera;

	// Slice movement tuning (applies unless a child BP overrides).
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move")
	float WalkSpeed = 400.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Move")
	float SprintSpeed = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Combat")
	float MeleeRangeCm = 350.f;
	/** Max build-trace distance from the camera (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Build")
	float BuildRangeCm = 500.f;

	// Client asks to use the focused interactable. Server re-validates.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Interact")
	void ServerInteractRequest(AActor* Target);

	// Melee swing. Server traces from the controller camera, resolves
	// the zombie body part actually hit, and applies server-side damage.
	// A structural hit instead runs attack propagation on that structure.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Combat")
	void ServerAttack();

	// Graybox hitscan (test-fire). Server traces from the camera along the
	// validated direction; a structural hit runs bullet propagation on it.
	// Clients never supply cells or damage — only aim.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Combat")
	void ServerFireHitscan(const FVector& Direction);

	// Throw the held object: server recomputes release velocity from the
	// validated direction and the current pawn velocity (no client trust).
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Carry")
	void ServerThrowHeld(const FVector& Direction);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Carry")
	void ServerDropHeld();

	// Unroll the carried mat at the pawn's feet (one deployed mat per player).
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Storage")
	void ServerDeployMat();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Combat")
	int32 AmmoConsumed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Combat")
	int32 ConfirmedHits = 0;

	// Client builds preview from Spec table; ghost material only, no spawn.
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Build")
	FPlacementRequest BuildPreviewRequest(EStructuralPart Part, FVector HitLocation, FRotator ViewRotation) const;

	// Single server-authoritative placement entry. Server validates grid,
	// support, cost, ownership — then spawns AStructuralActor.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Build")
	void ServerRequestPlacement(const FPlacementRequest& Request);

	// Repair target to full durability. Cost deducted server-side from Inventory.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Build")
	void ServerRequestRepair(AStructuralActor* Target);

	// Upgrade tier 0 -> 1 (2 Wood). Server validates + deducts.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Build")
	void ServerRequestUpgrade(AStructuralActor* Target);

	// Pause toggle (Esc / gamepad Start). Public for the headless
	// physics/controls fixture; still locally-controlled only.
	void OnPausePressed();

	// ---- Quick menu (Part A): compact top-left panel, Canvas-drawn ----
	/** Local-only open flag. Drives HUD panel + input gating + pause. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Menu")
	bool bQuickMenuOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Menu")
	EQuickMenuPage QuickMenuPage = EQuickMenuPage::Main;
	/** Real-time menu-open stamp (drives the open/close animation). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Menu")
	double MenuAnimStart = 0.0;
	/** Action awaiting a replacement key (Controls tab capture mode). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Menu")
	FName ControlsCaptureAction = NAME_None;
	/** Gamepad/keyboard selection index into the current page's buttons. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Menu")
	int32 MenuSelectedIndex = 0;

	bool IsQuickMenuOpen() const { return bQuickMenuOpen; }
	bool HealthIsDead() const;
	void ToggleQuickMenu();
	void CloseQuickMenu();
	/** Execute a menu button id (Resume/Map/Controls/Lab/Respawn/Close/...). */
	void QuickMenuAction(const FName& ButtonId);
	/** Pure page router (unit-tested, no world): Back always -> Main. */
	static EQuickMenuPage QuickMenuNextPage(EQuickMenuPage From, const FName& ButtonId);
	/** Current binding rows for the Controls tab. */
	void GetBindingRows(TArray<FActionBindingRow>& Out) const;
	/** Remap one keyboard action; persists to Saved/AfterfallBindings.ini. */
	bool RemapAction(FName ActionName, const FKey& NewKey);
	void RestoreDefaultBindings();

	// ---- Test weapon (Part: firing loop) ----
	/** Rounds in the magazine (server-authoritative, replicated). */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Weapon")
	int32 AmmoInMag = 12;
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Weapon")
	int32 MagSize = 12;
	/** Minimum seconds between shots. */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Weapon")
	float FireIntervalSec = 0.25f;
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Weapon")
	void ServerReload();
	/** Immediate respawn while dead (menu button; same path as auto-respawn). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Respawn")
	void ServerRequestRespawn();

	// ---- Voxel chunk (single-chunk replication proof) ----
	/**
	 * Client -> server voxel edit request. The chunk actor has no owning
	 * connection, so client requests are relayed over the pawn like every
	 * other gameplay action. The server validates coord/material and assigns
	 * the sequence number; the client's values are never trusted as state.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Voxel")
	void ServerRequestVoxelEdit(AVoxelChunkActor* Chunk, FIntVector Local, uint8 State);

	/** Client -> server: full chunk snapshot after a sequence gap/late join. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Voxel")
	void ServerRequestVoxelResync(AVoxelChunkActor* Chunk);

	/** True while the mat inventory is gating locomotion (Opening/Open/Closing). */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|Mat")
	bool IsMatInventoryOpen() const;

	virtual void Jump() override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void BeginPlay() override;

private:
	UPROPERTY() UInputMappingContext* SliceIMC = nullptr;
	UPROPERTY() UInputAction* IA_Move = nullptr;
	UPROPERTY() UInputAction* IA_Look = nullptr;
	UPROPERTY() UInputAction* IA_Jump = nullptr;
	UPROPERTY() UInputAction* IA_Sprint = nullptr;
	UPROPERTY() UInputAction* IA_Interact = nullptr;
	UPROPERTY() UInputAction* IA_Attack = nullptr;
	UPROPERTY() UInputAction* IA_Crouch = nullptr;
	UPROPERTY() UInputAction* IA_Throw = nullptr;
	UPROPERTY() UInputAction* IA_Drop = nullptr;
	UPROPERTY() UInputAction* IA_Mat = nullptr;
	UPROPERTY() UInputAction* IA_Fire = nullptr;
	UPROPERTY() UInputAction* IA_Reload = nullptr;
	UPROPERTY() UInputAction* IA_Pause = nullptr;
	UPROPERTY() UInputAction* IA_Build = nullptr;
	UPROPERTY() UInputAction* IA_Grapple = nullptr;
	bool bSprintHeld = false;
	bool bRespawning = false;

	void InjectSliceInput();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnSprintStarted();
	void OnSprintReleased();
	void OnInteractPressed();
	void OnAttackPressed();
	void OnCrouchPressed();
	void OnThrowPressed();
	void OnDropPressed();
	void OnMatPressed();
	void OnFirePressed();
	void OnReloadPressed();
	/**
	 * Gameplay entry point for construction (Part A of the runtime repair):
	 * the ONLY production path that creates an AStructuralActor had no caller,
	 * so no destructible structure could ever exist in play. Aims from the
	 * camera, then defers to the existing server-authoritative placement RPC.
	 */
	void OnBuildPressed();
	/** Grapple toggle (traversal): attaches when idle, releases when attached. */
	void OnGrapplePressed();
	// Grapple attach/release requests. Server validates owner state; the
	// component itself refuses non-authority callers. Momentum is preserved.
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Traversal")
	void ServerRequestGrapple();
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|Traversal")
	void ServerReleaseGrapple();
	/** Shared respawn body (auto timer + manual button converge here). */
	void DoRespawn();
	void LoadBindingOverrides();
	void SaveBindingOverrides() const;
	// Modifier recipe for one keyboard mapping (reused on remap).
	struct FKeyRecipe { bool bNegX = false; bool bNegY = false; bool bSwizzle = false; };
	TMap<FName, FKeyRecipe> BindingRecipes;
	TMap<FName, FKey> CurrentKeys;
	double LastFireTime = -10.0;
	double LastMenuNavTime = -10.0;

	UFUNCTION()
	void HandleDeath();
	void Respawn();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
