// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Interaction) — reusable contract for anything the player
// can use: resource nodes, doors, containers, loot, crafting stations.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Server is the only authority that executes interaction. Prompt/validity
 * are display aids; the server re-validates range + state on every call.
 */
class AFTERFALL_API IInteractable
{
	GENERATED_BODY()

public:
	/** Short prompt shown when focused, e.g. "Harvest Wood (E)". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Afterfall|Interact")
	FText GetInteractPrompt() const;

	/** Cheap client-side pre-check for prompt visibility. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Afterfall|Interact")
	bool CanInteract(AController* Instigator) const;

	/** Server-only execution. Must re-validate everything. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Afterfall|Interact")
	void ServerInteract(AController* Caller);
};
