// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — universal procedural shape generator.
//
// Replication model (dedicated-server safe, deterministic):
//   * ONLY the shape parameters replicate (never vertex data).
//   * Server and every client run GenerateShape32() locally from the same
//     parameters, so the mesh comes out identical everywhere.
//   * No FMath::Rand / no global random state: all noise derives from Seed
//     via a locally-seeded FRandomStream plus a seed-derived offset into
//     FMath::PerlinNoise3D. Same Seed => same mesh on every machine.
//   * Optional bUseRegionSeed reads RegionSeed (1337) from AAfterfallGameMode
//     on the authority; clients without a GameMode fall back to 1337, which
//     equals the GameMode default, so they still converge.
//   * This actor never mutates gameplay state: it only builds its own visual
//     + collision geometry from its parameters.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Procedural/ProceduralFractureBridge.h"
#include "ShapeGenerator32.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/** Which signed-distance shape to evaluate. */
UENUM(BlueprintType)
enum class EShapeType32 : uint8
{
	Sphere   UMETA(DisplayName = "Sphere"),
	Box      UMETA(DisplayName = "Box"),
	Cylinder UMETA(DisplayName = "Cylinder"),
	Capsule  UMETA(DisplayName = "Capsule")
};

/** Which polygonizer to run over the sampled field. */
UENUM(BlueprintType)
enum class ESurfaceMode32 : uint8
{
	Smooth_MarchingCubes UMETA(DisplayName = "Smooth (Marching Cubes)"),
	Blocky_Voxel         UMETA(DisplayName = "Blocky (Voxel)")
};

UCLASS()
class AFTERFALL_API AShapeGenerator32 : public AActor
{
	GENERATED_BODY()

public:
	AShapeGenerator32();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Procedural mesh: one section, collision created with the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shape32")
	UProceduralMeshComponent* ProcMesh = nullptr;

	// ---- replicated shape parameters (mesh is rebuilt locally everywhere) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	EShapeType32 ShapeType = EShapeType32::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	ESurfaceMode32 SurfaceMode = ESurfaceMode32::Smooth_MarchingCubes;

	/** Samples per axis. Clamped to 2..64 in code. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32", meta = (ClampMin = "2", ClampMax = "64"))
	int32 GridSize = 32;

	/** World units between samples. Must be > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32", meta = (ClampMin = "0.01"))
	float CellSize = 10.0f;

	/** Actor-local offset added to every vertex after polygonization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	FVector Center = FVector::ZeroVector;

	/** Deterministic seed: all noise derives from this (no global RNG). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	float NoiseStrength = 12.0f;

	/** Sphere radius / cylinder radius / capsule radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32", meta = (ClampMin = "0.0"))
	float Radius = 120.0f;

	/** Box half-extents (a, b, c). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32", meta = (ClampMin = "0.0"))
	FVector HalfExtents = FVector(60.0f, 60.0f, 60.0f);

	/** Cylinder / capsule half-height along Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32", meta = (ClampMin = "0.0"))
	float Height = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	float DeformStrength = 0.0f;

	/** Smooth falloff distance of the deform blob around Center. <=0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	float DeformRadius = 50.0f;

	/** Solid means field <= IsoLevel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	float IsoLevel = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	UMaterialInterface* Material = nullptr;

	/** When true, EffectiveSeed() reads RegionSeed from the GameMode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Rebuild, Category = "Shape32")
	bool bUseRegionSeed = false;

	/** Local-only: regenerate in OnConstruction (editor) and BeginPlay (game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape32")
	bool bGenerateOnBeginPlay = true;

	/** Build (or rebuild) the mesh from the current parameters. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Shape")
	void GenerateShape32();

	/** Build an authoritative fracture plan from a world impact. The result is a lattice plan; geometry mutation remains with the owning voxel/SDF implementation. */
	AfterfallProcedural::FImpactPlan BuildServerFracturePlan(const FVector& ImpactWorldPosition, const FVector& ImpactVelocityCmPerSec, float MassKg, EMaterialType InMaterial, int32 MaxDepth = 8) const;

	/** Last build stats (not replicated; local diagnostics only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shape32")
	int32 LastVertexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shape32")
	int32 LastTriangleCount = 0;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Rebuild();

private:
	/** Resolved seed: GameMode RegionSeed when bUseRegionSeed, else Seed. */
	int32 EffectiveSeed() const;

	/** Linear field index: x + N * (y + N * z). */
	static int32 FieldIndex(int32 N, int32 X, int32 Y, int32 Z) { return X + N * (Y + N * Z); }

	float EvaluateSDF(const FVector& Q) const;

	/** Transient deterministic noise offset, set at the start of GenerateShape32. */
	FVector CachedNoiseOffset = FVector::ZeroVector;
};
