// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Interaction) — camera-based focus for the local player.
// Prompt is display-only; execution always goes through the server RPC,
// which re-validates range and target state (no trust in the client trace).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractComponent.generated.h"

class AController;

/**
 * Ticks at 10Hz (timer, not per-frame) from the owning character's camera.
 * FocusedActor is client-local; the server never trusts it.
 */
UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractComponent();

	/** Max use distance in cm. Slice: 300 (matches node harvest rule). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Interact")
	float InteractRangeCm = 300.f;

	/** Currently focused interactable (client-local, may be stale). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Interact")
	TWeakObjectPtr<AActor> FocusedActor;

	/** Prompt for the focused actor, empty when nothing valid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Afterfall|Interact")
	FText FocusedPrompt;

	/** Server-side validation helper: target alive, in range, facing. */
	bool ServerValidateTarget(AController* Instigator, AActor* Target) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	FTimerHandle FocusTimer;

	void UpdateFocus();
};
