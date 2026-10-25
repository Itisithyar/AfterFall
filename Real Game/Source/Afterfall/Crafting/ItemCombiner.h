// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Crafting) — data-driven combination engine (Phase 5).
// Pure functions: no world, no actors, no IO. The engine resolves a recipe
// against concrete item instances, plans exactly what each instance gives up,
// and mints the child instance with new identity + immediate-parent lineage +
// deterministic property inheritance. Transaction/commit safety lives in
// UInventoryComponent::TryCombine, which calls into this engine.
#pragma once

#include "CoreMinimal.h"
#include "Crafting/CraftingService.h"
#include "Inventory/ItemTypes.h"

/** One consumption decision: take Count units from Provided[index]. */
struct FCombineConsumeStep
{
	int32 ProvidedIndex = -1;
	int32 Count = 0;
};

/** Result of minting a child instance. Failure never mutates anything. */
struct FCombineOutcome
{
	bool bSuccess = false;
	FItemInstance Child;
	FString Error;
};

/**
 * Static-only engine (deliberately not a UObject: no state, fully testable
 * headless). All rules are explicit and deterministic:
 *
 *  - Consumption: provided instances are consumed in the order given until
 *    each recipe input is satisfied. Every provided instance must take part;
 *    an instance that ends up unused makes the request ambiguous and fails.
 *  - Identity: the child always gets a fresh InstanceId. Parents keep theirs;
 *    a partially consumed stack keeps its id with the remaining count.
 *  - Lineage: child records immediate parent instance ids + rule id/version +
 *    the caller's transaction id. That is immediate-parent provenance, not a
 *    full ancestor graph.
 *  - Condition inheritance: weakest-link (min of parent conditions, clamped
 *    0..1). Never averages — a child cannot be made of better material than
 *    its worst input.
 *  - Mass: output mass comes from the output definition, not from summing
 *    input mass. These recipes are an abstract gameplay transformation, not a
 *    physical simulation; no conservation identity is enforced (documented
 *    game rule, see DismantleReturn for the recycle direction).
 */
class AFTERFALL_API UItemCombiner
{
public:
	/**
	 * Resolve Recipe.Inputs against Provided instances.
	 * Fails (bSuccess=false + Error) when: an input is not covered, an id is
	 * unknown/duplicated within the request, or a provided instance is not
	 * needed by the recipe. Never mutates Provided.
	 */
	static bool PlanConsumption(const FCraftRecipe& Recipe,
		const TArray<FItemInstance>& Provided,
		TArray<FCombineConsumeStep>& OutPlan, FString& OutError);

	/** Weakest-link condition of the consumed parents (0..1, default 1). */
	static float InheritCondition(const TArray<FItemInstance>& Parents);

	/**
	 * Mint the child: new InstanceId, output id/count, inherited Condition,
	 * lineage (parents, rule id + version, transaction id), provenance in
	 * CustomData. Deterministic except the fresh InstanceId.
	 */
	static FCombineOutcome BuildChild(const FCraftRecipe& Recipe,
		const TArray<FItemInstance>& Parents, const FGuid& TransactionId);
};
