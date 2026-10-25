// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Structural (Destruction) — local damage presentation for one structure.
// Renders the 3x3x3 cell grid as instanced graybox cubes; detached cells are
// REMOVED (the hole IS the detached set, not a decal). Rebuild() is idempotent:
// same DetachedCells in -> same instances out, safe to call from OnRep, init,
// and ImportDelta replays. Collision follows instances (BlockAll profile).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralDestructionComponent.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(ClassGroup = (Afterfall), meta = (BlueprintSpawnableComponent))
class AFTERFALL_API UStructuralDestructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStructuralDestructionComponent();

	/** Half-spacing of the 3x3x3 lattice (grid spans -Extent..+Extent). */
	UPROPERTY(EditDefaultsOnly, Category = "Afterfall|Destruction")
	float CellExtentCm = 50.f;

	/**
	 * Rebuild instances from authoritative detached cells. Idempotent:
	 * Clear + re-add intact cells, so repeats converge to the same state.
	 */
	void Rebuild(const TArray<int32>& DetachedCells);

	/** Instances currently shown (27 - detached count). */
	int32 VisibleCellCount() const;

	/** Last committed detached set (for sync checks / diagnostics). */
	const TArray<int32>& GetBuiltDetached() const { return BuiltDetached; }

	/** True when the built visual matches the given authoritative set. */
	bool IsInSync(const TArray<int32>& DetachedCells) const;

protected:
	virtual void OnRegister() override;

private:
	UPROPERTY()
	UInstancedStaticMeshComponent* CellMesh = nullptr;

	TArray<int32> BuiltDetached;

	static bool SameSet(const TArray<int32>& A, const TArray<int32>& B);
};
