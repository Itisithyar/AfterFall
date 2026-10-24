// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — authoritative chunk DATA model + pure edit/mesh helpers.
//
// Boundary of this slice (single-chunk replication proof):
//   * ONE 32^3 chunk. No world streaming, no region generation, no multi-chunk.
//   * Coordinates use the EXISTING rhombohedral lattice (Voxel/VoxelBasis.h,
//     S = 3.06). The lattice is NOT replaced with axis-aligned cubes; the
//     mesher emits the six parallelepiped faces of each rhombohedral cell.
//   * Occupancy + material id only. No per-voxel health/state yet.
//
// Everything in this header is world-free so the automation suite can test it
// headless without spawning actors.
#pragma once

#include "CoreMinimal.h"
#include "VoxelChunkTypes.generated.h"

namespace AfterfallVoxel
{
	/** Chunk edge in voxels (matches FVoxelBasis::VoxelToChunk / 32). */
	constexpr int32 ChunkSize = 32;
	constexpr int32 ChunkVoxelCount = ChunkSize * ChunkSize * ChunkSize;
	/** Highest accepted material id; 0 means empty. */
	constexpr uint8 MaxMaterialId = 4;
	/** Bounded server edit log used for resync recovery. */
	constexpr int32 EditLogCap = 512;

	/** 0 empty, 1 stone, 2 wood, 3 metal, 4 scrap. Deterministic palette. */
	FLinearColor MaterialColor(uint8 State);
}

/** Flat voxel array for one chunk. Index = X + 32*(Y + 32*Z). */
USTRUCT()
struct AFTERFALL_API FVoxelChunkData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Voxels;

	static int32 VoxelIndex(const FIntVector& Local);
	static bool IsValidCoord(const FIntVector& Local);

	/** Allocate an empty chunk. */
	void Reset();

	/**
	 * Deterministic initial content: the bottom 8 layers solid (stone).
	 * Same result on server and every client, no randomness.
	 */
	void InitializeDeterministic();

	/** False for out-of-range coords or material ids above MaxMaterialId. */
	bool Set(const FIntVector& Local, uint8 State);

	/** 0 for out-of-range coords (treated as empty). */
	uint8 Get(const FIntVector& Local) const;

	int32 CountSolid() const;
	int32 CountMaterial(uint8 State) const;
	bool IsValid() const { return Voxels.Num() == AfterfallVoxel::ChunkVoxelCount; }
};

/** One committed voxel change. Seq is the server's monotonic edit counter. */
USTRUCT()
struct AFTERFALL_API FVoxelEdit
{
	GENERATED_BODY()

	UPROPERTY()
	FIntVector Coord = FIntVector::ZeroValue;

	UPROPERTY()
	uint8 State = 0;

	UPROPERTY()
	int32 Seq = 0;
};

/** Full-chunk snapshot: the initial/late-join payload and resync answer. */
USTRUCT()
struct AFTERFALL_API FVoxelSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FVoxelChunkData Data;

	/** Server edit sequence this snapshot is current as of. */
	UPROPERTY()
	int32 Seq = 0;
};

namespace AfterfallVoxel
{
	/**
	 * Apply one edit if it is strictly newer than InOutLastSeq.
	 * Duplicate (Seq == last) and stale (Seq < last) edits change nothing, so
	 * reliable-RPC retries and replays cannot corrupt state. Returns true when
	 * the voxel data actually changed.
	 */
	bool ApplyEdit(FVoxelChunkData& Data, const FVoxelEdit& Edit, int32& InOutLastSeq);

	/**
	 * True when Seq is not the next expected value (a gap): the client should
	 * ask the server for a full snapshot. Reliable RPCs are ordered, so a gap
	 * means state was lost or the client joined mid-stream.
	 */
	bool IsGap(int32 Seq, int32 LastSeq);
}

namespace VoxelMesher
{
	/** Render-ready buffers for one material section. */
	struct AFTERFALL_API FMeshBuffer
	{
		TArray<FVector> Positions;
		TArray<int32> Indices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		int32 QuadCount = 0;

		void Reset();
		/** Editor/tests: count of emitted quads (6 faces max per solid voxel). */
		int32 NumTris() const { return Indices.Num() / 3; }
	};

	/**
	 * Build the chunk mesh: only faces between a solid voxel and an empty
	 * neighbour are emitted (no internal faces). Faces on the chunk border are
	 * emitted because a single chunk has no neighbours in this slice.
	 * Geometry is expressed in chunk-local space (lattice units); the actor
	 * supplies the chunk origin.
	 */
	void BuildChunkMesh(const FVoxelChunkData& Data, FMeshBuffer& Out);
}
