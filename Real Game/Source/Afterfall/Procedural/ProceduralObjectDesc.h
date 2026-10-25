// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — serializable universal procedural-object
// description. Compact generation parameters + ordered edit deltas; never
// vertex buffers. Regeneration from this description is deterministic.
#pragma once

#include "CoreMinimal.h"
#include "Procedural/ShapeGenerator32.h"
#include "Destruction/DestructionTypes.h"
#include "ProceduralObjectDesc.generated.h"

/** Current generator schema. Bump when field semantics change. */
#define AFTERFALL_PROC_OBJ_SCHEMA_VERSION 1

/** Collision policy for generated output. */
UENUM(BlueprintType)
enum class EProcObjCollision : uint8
{
	/** Static geometry: complex-as-simple, exact query collision. */
	StaticExact UMETA(DisplayName = "Static Exact"),
	/** Movable fragments: cheap convex/box approximation. */
	FragmentSimple UMETA(DisplayName = "Fragment Simple"),
	/** No collision (pure visual / distant LOD). */
	None UMETA(DisplayName = "None")
};

/**
 * One localized, bounded deformation. Actor-local coordinates: Center and
 * Direction are interpreted in the owning actor's local frame, Radius and
 * Strength in world units. Replays are idempotent by OpId: an OpId already
 * applied is skipped, never accumulated twice.
 */
USTRUCT(BlueprintType)
struct FProceduralDeformOp
{
	GENERATED_BODY()
	/** Stable operation identity (network + save replay protection). */
	UPROPERTY() FGuid OpId;
	/** Actor-local blob center. */
	UPROPERTY() FVector Center = FVector::ZeroVector;
	/** Warp direction in actor-local space (normalized by the pipeline). */
	UPROPERTY() FVector Direction = FVector(1.0f, 0.0f, 0.0f);
	/** Falloff radius in world units. <= 0 refuses (no-op). */
	UPROPERTY() float Radius = 50.0f;
	/** Peak displacement in world units. 0 refuses (no-op). */
	UPROPERTY() float Strength = 0.0f;
	/** Authoritative sequence assigned by the server. */
	UPROPERTY() int32 Seq = 0;
};

/**
 * One committed fracture event. Planned through the existing severity /
 * material / lattice solver (ProceduralFractureBridge); rejected events
 * (below initiation threshold, invalid input) carry bInitiated=false and
 * must not mutate the field.
 */
USTRUCT(BlueprintType)
struct FProceduralFractureEvent
{
	GENERATED_BODY()
	/** Stable event identity (replay protection). */
	UPROPERTY() FGuid EventId;
	/** Authoritative sequence assigned by the server. */
	UPROPERTY() int32 Seq = 0;
	/** Actor-local impact point. */
	UPROPERTY() FVector LocalPos = FVector::ZeroVector;
	/** Actor-local impact direction. */
	UPROPERTY() FVector LocalDir = FVector(1.0f, 0.0f, 0.0f);
	/** Impact energy in joules (0.5*m*v^2, computed once by the caller). */
	UPROPERTY() float EnergyJ = 0.0f;
	UPROPERTY() EMaterialType Material = EMaterialType::Concrete;
	UPROPERTY() uint32 Seed = 1337;
	/** Solver verdict replicated for diagnostics; field carve only when true. */
	UPROPERTY() bool bInitiated = false;
};

/**
 * Serializable generation description: shape/source, dimensions, frame,
 * resolution, material profile, seed, schema. GridSize is SAMPLES per axis
 * (32 samples -> 31 cells); large objects must use explicit CellSize, never
 * silently reuse a prop-scale cell.
 */
USTRUCT(BlueprintType)
struct FProceduralObjectDesc
{
	GENERATED_BODY()
	/** Stable object identity across edits, fragments, save/load. */
	UPROPERTY() FGuid ObjectId;
	/** World/region key (region save + streaming identity). */
	UPROPERTY() FString RegionKey;
	UPROPERTY() int32 SchemaVersion = AFTERFALL_PROC_OBJ_SCHEMA_VERSION;
	UPROPERTY() EShapeType32 Shape = EShapeType32::Sphere;
	UPROPERTY() ESurfaceMode32 SurfaceMode = ESurfaceMode32::Smooth_MarchingCubes;
	/** Samples per axis, clamped 2..64 by the pipeline. */
	UPROPERTY() int32 GridSize = 32;
	/** World units between samples. Must be > 0. */
	UPROPERTY() float CellSize = 10.0f;
	/** Actor-local mesh offset added to every vertex. */
	UPROPERTY() FVector Center = FVector::ZeroVector;
	UPROPERTY() float Radius = 120.0f;
	UPROPERTY() FVector HalfExtents = FVector(60.0f, 60.0f, 60.0f);
	UPROPERTY() float Height = 60.0f;
	UPROPERTY() float NoiseStrength = 0.0f;
	UPROPERTY() int32 Seed = 1337;
	UPROPERTY() float IsoLevel = 0.0f;
	/** Material profile for fracture rules + section identity. */
	UPROPERTY() EMaterialType MaterialProfile = EMaterialType::Stone;
	/** Max lattice depth per fracture event (clamped 1..64). */
	UPROPERTY() int32 FractureMaxDepth = 8;
	/** Cap on carved cells applied per fracture event. */
	UPROPERTY() int32 FractureCellCap = 512;
	UPROPERTY() EProcObjCollision CollisionMode = EProcObjCollision::StaticExact;
};
