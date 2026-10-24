// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Interaction) — physical inventory mat open/close flow.
//
// Forest-inspired: B toggles a server-authoritative interaction. Opening
// crouches the pawn, (re)deploys the mat in front, then reveals the UI;
// closing packs the mat away and restores control. Movement code and
// collision are untouched — restriction is input gating on the character
// plus native replicated crouch. Animation is scaffolding: if UnrollMontage
// is assigned in-editor, an AnimNotify calling NotifyMatRevealed() ends the
// Opening phase early; otherwise UnrollDurationSec drives the transition.
// No physics changes; the mat never simulates.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MatInteractionComponent.generated.h"

class AAfterfallCharacter;
class APhysicalStorageMat;
class UAnimMontage;

/** Interaction lifecycle. Replicated; visuals/UI follow OnRep. */
UENUM(BlueprintType)
enum class EMatInteractionState : uint8
{
	Closed  UMETA(DisplayName = "Closed"),
	Opening UMETA(DisplayName = "Opening (unrolling)"),
	Open    UMETA(DisplayName = "Open (inventory visible)"),
	Closing UMETA(DisplayName = "Closing (packing up)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatInteractionChanged,
	EMatInteractionState, NewState, APhysicalStorageMat*, Mat);

/**
 * Server-authoritative open/close state for one character's inventory mat.
 * Repeated input is safe: requests are only honored from the states that
 * allow them (see CanTransition); every other combination is a no-op.
 */
UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UMatInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatInteractionComponent();

	/** Current lifecycle state (server writes, clients observe). */
	UPROPERTY(ReplicatedUsing = OnRep_InteractionState, VisibleAnywhere, Category = "Afterfall|MatInteract")
	EMatInteractionState InteractionState = EMatInteractionState::Closed;

	/** Fallback unroll time when no montage/notify is assigned (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|MatInteract")
	float UnrollDurationSec = 1.2f;

	/** Pack-up time before control is restored (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|MatInteract")
	float PackDurationSec = 0.8f;

	/**
	 * Optional unroll montage, assigned in-editor (see Docs/MAT-INTERACTION.md).
	 * Null is valid: the timer above drives the transition instead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|MatInteract")
	UAnimMontage* UnrollMontage = nullptr;

	/** UI/UMG binds here; fires on every replicated transition. */
	UPROPERTY(BlueprintAssignable, Category = "Afterfall|MatInteract")
	FOnMatInteractionChanged OnInteractionChanged;

	/** Client -> server: begin the open flow (from Closed only). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|MatInteract")
	void ServerRequestMatOpen();

	/** Client -> server: begin the close flow (from Open only). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Afterfall|MatInteract")
	void ServerRequestMatClose();

	/**
	 * AnimNotify hook (server): call from a notify named MatRevealed in
	 * UnrollMontage to end Opening immediately. Safe to call any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Afterfall|MatInteract")
	void NotifyMatRevealed();

	/** True while locomotion input must be gated (Opening/Open/Closing). */
	bool IsInteractionBlocking() const;

	/** True only when the inventory UI should be visible. */
	bool IsInventoryUiVisible() const { return InteractionState == EMatInteractionState::Open; }

	/** Server: abort to Closed immediately (death path). Safe any time. */
	void ForceClose();

	/** Pure transition table (unit-tested, no world needed). */
	static bool CanTransition(EMatInteractionState From, EMatInteractionState To);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_InteractionState();

private:
	FTimerHandle TransitionTimer;
	/** Mat the current interaction is showing (replicated so client UI can
	 * bind Slots; null until first deploy). Raw pointer + UPROPERTY keeps
	 * it GC-tracked; TWeakObjectPtr would not replicate. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Afterfall|MatInteract")
	APhysicalStorageMat* ActiveMat = nullptr;

	AAfterfallCharacter* OwnerCharacter() const;
	APhysicalStorageMat* EnsureMat();
	void CompleteOpening();
	void CompleteClosing();
	void ClearTransitionTimer();
};
