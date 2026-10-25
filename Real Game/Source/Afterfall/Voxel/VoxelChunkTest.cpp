// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Voxel) — headless tests for the chunk data model, the
// exposed-face mesher, and ordered edit application. No world required.
#include "Misc/AutomationTest.h"
#include "Voxel/VoxelChunkTypes.h"
#include "Voxel/VoxelBasis.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Count quads whose normal is within 1 degree of Dir. */
	int32 CountQuadsAlong(const VoxelMesher::FMeshBuffer& M, const FVector& Dir)
	{
		int32 N = 0;
		for (int32 i = 0; i + 3 < M.Positions.Num(); i += 4)
		{
			if (FVector::DotProduct(M.Normals[i], Dir) > 0.999f)
			{
				++N;
			}
		}
		return N;
	}

	int32 CountNonUnitNormals(const VoxelMesher::FMeshBuffer& M)
	{
		int32 N = 0;
		for (const FVector& V : M.Normals)
		{
			if (!FMath::IsNearlyEqual(V.Size(), 1.f, 0.001f))
			{
				++N;
			}
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelChunkInitTest, "Afterfall.Voxel.ChunkInit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoxelChunkInitTest::RunTest(const FString& P)
{
	FVoxelChunkData Data;
	TestFalse(TEXT("uninitialised chunk is invalid"), Data.IsValid());
	TestEqual(TEXT("uninitialised reads empty"), (int32)Data.Get(FIntVector(0, 0, 0)), 0);

	Data.InitializeDeterministic();
	TestTrue(TEXT("chunk holds 32^3 voxels"), Data.IsValid());
	TestEqual(TEXT("array size 32768"), Data.Voxels.Num(), AfterfallVoxel::ChunkVoxelCount);

	// Bottom 8 layers solid stone, everything above empty.
	TestEqual(TEXT("deterministic solid count"), Data.CountSolid(), 32 * 32 * 8);
	TestEqual(TEXT("all stone"), Data.CountMaterial(1), 32 * 32 * 8);
	TestEqual(TEXT("top layer empty"), (int32)Data.Get(FIntVector(5, 5, 8)), 0);
	TestEqual(TEXT("layer 7 solid"), (int32)Data.Get(FIntVector(5, 5, 7)), 1);
	TestEqual(TEXT("corner solid"), (int32)Data.Get(FIntVector(0, 0, 0)), 1);

	// Determinism: a second init reproduces the exact same array.
	FVoxelChunkData Again;
	Again.InitializeDeterministic();
	TestTrue(TEXT("init is deterministic"), Again.Voxels == Data.Voxels);

	// Coordinate validation.
	TestTrue(TEXT("origin valid"), FVoxelChunkData::IsValidCoord(FIntVector(0, 0, 0)));
	TestTrue(TEXT("max corner valid"), FVoxelChunkData::IsValidCoord(FIntVector(31, 31, 31)));
	TestFalse(TEXT("negative invalid"), FVoxelChunkData::IsValidCoord(FIntVector(-1, 0, 0)));
	TestFalse(TEXT("overflow invalid"), FVoxelChunkData::IsValidCoord(FIntVector(32, 0, 0)));
	TestFalse(TEXT("set negative rejected"), Data.Set(FIntVector(-1, 0, 0), 2));
	TestFalse(TEXT("set overflow rejected"), Data.Set(FIntVector(0, 0, 32), 2));
	TestFalse(TEXT("set bad material rejected"), Data.Set(FIntVector(0, 0, 32 - 32), 9));
	TestEqual(TEXT("rejected material did not write"), (int32)Data.Get(FIntVector(0, 0, 0)), 1);
	TestEqual(TEXT("out-of-chunk reads empty"), (int32)Data.Get(FIntVector(99, 99, 99)), 0);

	// Write + read back one cell.
	TestTrue(TEXT("set valid cell"), Data.Set(FIntVector(3, 4, 20), 3));
	TestEqual(TEXT("cell reads back metal"), (int32)Data.Get(FIntVector(3, 4, 20)), 3);
	TestEqual(TEXT("solid count +1"), Data.CountSolid(), 32 * 32 * 8 + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeshFacesTest, "Afterfall.Voxel.MeshFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoxelMeshFacesTest::RunTest(const FString& P)
{
	// --- Two adjacent voxels: 12 faces minus the 2 shared = 10 quads.
	{
		FVoxelChunkData Pair;
		Pair.Reset();
		Pair.Set(FIntVector(5, 5, 5), 1);
		Pair.Set(FIntVector(6, 5, 5), 1);
		VoxelMesher::FMeshBuffer M;
		VoxelMesher::BuildChunkMesh(Pair, M);
		TestEqual(TEXT("adjacent pair emits 10 quads"), M.QuadCount, 10);
		TestEqual(TEXT("20 triangles"), M.NumTris(), 20);
		TestEqual(TEXT("4 verts per quad"), M.Positions.Num(), 40);
		TestEqual(TEXT("one normal per vertex"), M.Normals.Num(), M.Positions.Num());
		TestEqual(TEXT("one uv per vertex"), M.UVs.Num(), M.Positions.Num());
		TestEqual(TEXT("one colour per vertex"), M.Colors.Num(), M.Positions.Num());
		TestEqual(TEXT("all normals unit"), CountNonUnitNormals(M), 0);
		// The shared +X/-X interface must be absent: no quad normal along the
		// lattice A direction on the seam side is impossible to distinguish
		// (outer faces share the axis), so assert the exact count instead.
		TestEqual(TEXT("no internal face (10 not 12)"), M.QuadCount != 12, true);
	}

	// --- Deterministic init slab: 3072 quads, 6144 tris.
	FVoxelChunkData Slab;
	Slab.InitializeDeterministic();
	VoxelMesher::FMeshBuffer A;
	VoxelMesher::BuildChunkMesh(Slab, A);
	VoxelMesher::FMeshBuffer B;
	VoxelMesher::BuildChunkMesh(Slab, B);

	TestEqual(TEXT("slab quads 3072"), A.QuadCount, 3072);
	TestEqual(TEXT("slab tris 6144"), A.NumTris(), 6144);
	TestTrue(TEXT("mesh generation deterministic"), A.Positions == B.Positions
		&& A.Indices == B.Indices && A.Normals == B.Normals);
	TestEqual(TEXT("all slab normals unit"), CountNonUnitNormals(A), 0);

	// Face orientation: top faces point along +C (cross(A,B)), bottom along -C.
	const FVector NC = FVector::CrossProduct(FVoxelBasis::A(), FVoxelBasis::B()).GetSafeNormal();
	TestEqual(TEXT("top faces 1024"), CountQuadsAlong(A, NC), 1024);
	TestEqual(TEXT("bottom faces 1024"), CountQuadsAlong(A, -NC), 1024);

	// No degenerate triangles (winding/parallelism sanity).
	int32 Degenerate = 0;
	for (int32 i = 0; i + 2 < A.Indices.Num(); i += 3)
	{
		const FVector& V0 = A.Positions[A.Indices[i]];
		const FVector& V1 = A.Positions[A.Indices[i + 1]];
		const FVector& V2 = A.Positions[A.Indices[i + 2]];
		if (FVector::CrossProduct(V1 - V0, V2 - V0).Size() < 0.001f)
		{
			++Degenerate;
		}
	}
	TestEqual(TEXT("no degenerate triangles"), Degenerate, 0);

	// Empty chunk emits nothing (collision comes from the mesh section).
	FVoxelChunkData Empty;
	Empty.Reset();
	VoxelMesher::FMeshBuffer E;
	VoxelMesher::BuildChunkMesh(Empty, E);
	TestEqual(TEXT("empty chunk has no geometry"), E.QuadCount, 0);
	TestEqual(TEXT("empty chunk has no indices"), E.Indices.Num(), 0);

	// Removing a voxel re-exposes its neighbours' faces.
	FVoxelChunkData Hole;
	Hole.InitializeDeterministic();
	const int32 Before = A.QuadCount;
	Hole.Set(FIntVector(4, 4, 4), 0);
	VoxelMesher::FMeshBuffer H;
	VoxelMesher::BuildChunkMesh(Hole, H);
	TestEqual(TEXT("carving one interior voxel exposes 6 faces"), H.QuadCount, Before + 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEditOrderTest, "Afterfall.Voxel.EditOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoxelEditOrderTest::RunTest(const FString& P)
{
	FVoxelChunkData Data;
	Data.InitializeDeterministic();
	int32 LastSeq = 0;

	FVoxelEdit Remove;
	Remove.Coord = FIntVector(10, 10, 3);
	Remove.State = 0;
	Remove.Seq = 1;
	TestTrue(TEXT("first edit applies"), AfterfallVoxel::ApplyEdit(Data, Remove, LastSeq));
	TestEqual(TEXT("seq advanced"), LastSeq, 1);
	TestEqual(TEXT("voxel removed"), (int32)Data.Get(FIntVector(10, 10, 3)), 0);

	// Duplicate (same seq) must not re-apply or rewind.
	TestFalse(TEXT("duplicate rejected"), AfterfallVoxel::ApplyEdit(Data, Remove, LastSeq));
	TestEqual(TEXT("seq unchanged by duplicate"), LastSeq, 1);

	FVoxelEdit Place;
	Place.Coord = FIntVector(20, 20, 20);
	Place.State = 2;
	Place.Seq = 2;
	TestTrue(TEXT("second edit applies"), AfterfallVoxel::ApplyEdit(Data, Place, LastSeq));
	TestEqual(TEXT("placed wood"), (int32)Data.Get(FIntVector(20, 20, 20)), 2);

	// Stale edit (older seq, different payload) cannot change anything.
	FVoxelEdit Stale;
	Stale.Coord = FIntVector(20, 20, 20);
	Stale.State = 0;
	Stale.Seq = 1;
	TestFalse(TEXT("stale rejected"), AfterfallVoxel::ApplyEdit(Data, Stale, LastSeq));
	TestEqual(TEXT("stale did not erase the voxel"), (int32)Data.Get(FIntVector(20, 20, 20)), 2);
	TestEqual(TEXT("seq unchanged by stale"), LastSeq, 2);

	// Same state again: applies the sequence but reports no data change.
	FVoxelEdit NoOp;
	NoOp.Coord = FIntVector(20, 20, 20);
	NoOp.State = 2;
	NoOp.Seq = 3;
	TestFalse(TEXT("no-op reports no change"), AfterfallVoxel::ApplyEdit(Data, NoOp, LastSeq));
	TestEqual(TEXT("no-op still advances seq"), LastSeq, 3);

	// Malformed edit advances the sequence (so later edits are not gaps) but
	// must not write out of range.
	FVoxelEdit Bad;
	Bad.Coord = FIntVector(99, 0, 0);
	Bad.State = 1;
	Bad.Seq = 4;
	TestFalse(TEXT("out-of-range edit changes nothing"), AfterfallVoxel::ApplyEdit(Data, Bad, LastSeq));
	TestEqual(TEXT("out-of-range still advances seq"), LastSeq, 4);

	// Gap detection drives the resync request.
	TestFalse(TEXT("next seq is not a gap"), AfterfallVoxel::IsGap(5, 4));
	TestTrue(TEXT("skipped seq is a gap"), AfterfallVoxel::IsGap(7, 4));
	TestFalse(TEXT("duplicate is not a gap"), AfterfallVoxel::IsGap(4, 4));
	TestFalse(TEXT("stale is not a gap"), AfterfallVoxel::IsGap(2, 4));
	return true;
}

#endif
