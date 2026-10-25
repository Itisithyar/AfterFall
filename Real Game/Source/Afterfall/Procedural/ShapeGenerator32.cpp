// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
// Owner: Tech (Procedural) — universal procedural shape generator.
//
// Marching-cubes tables live in Procedural/MarchingCubesTables.h (shared
// with the terrain extractor; values copied verbatim from the UE 5.8 engine
// FMarchingCubes::EdgeTable / TriTable / EdgeIndices, UE corner ordering,
// packed TriTable encoding). Numerically identical to Paul Bourke's tables.
// API differences found in 5.8 vs older docs:
//   * UProceduralMeshComponent::CreateMeshSection FColor overload is marked
//     DeprecatedFunction for Blueprints; the LinearColor overloads are the
//     current path (this file uses CreateMeshSection_LinearColor like the
//     existing VoxelChunkActor).
//   * FMath::PerlinNoise3D takes ONLY a position (no frequency/seed args),
//     so determinism comes from a seed-derived offset + fixed 0.02 frequency.
//   * ProceduralMeshComponent plugin ships with 5.8 (EnabledByDefault, not
//     deprecated); GeometryScripting coexists as a separate plugin.
#include "Procedural/ShapeGenerator32.h"
#include "ProceduralMeshComponent.h"
#include "AfterfallGameMode.h"
#include "Afterfall.h"
#include "Net/UnrealNetwork.h"
#include "Procedural/MarchingCubesTables.h"
#include "Procedural/ProceduralSdf.h"

namespace
{
	// UE 5.8 corner ordering (see file header comment).



	// Verbatim from FMarchingCubes::TriTable (UE 5.8, packed: low 12 bits
	// first, 4 bits per edge index, zero nibbles terminate).
}

AShapeGenerator32::AShapeGenerator32()
{
	bReplicates = true;
	// Always relevant like the voxel chunk actor: a debug/visual shape must
	// reach every client on the dedicated server without distance culling.
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	RootComponent = ProcMesh;
	ProcMesh->SetMobility(EComponentMobility::Movable);
	// Collision is correct the same frame the mesh is rebuilt (matches the
	// voxel chunk actor: synchronous cooking, complex-as-simple).
	ProcMesh->bUseAsyncCooking = false;
	ProcMesh->bUseComplexAsSimpleCollision = true;
	ProcMesh->SetCollisionProfileName(TEXT("BlockAll"));
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProcMesh->SetGenerateOverlapEvents(false);
}

void AShapeGenerator32::BeginPlay()
{
	Super::BeginPlay();
	if (bGenerateOnBeginPlay)
	{
		GenerateShape32();
	}
}

void AShapeGenerator32::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bGenerateOnBeginPlay)
	{
		GenerateShape32();
	}
}

void AShapeGenerator32::OnRep_Rebuild()
{
	// Replicated parameters changed: rebuild locally from the same inputs,
	// so server and clients converge without any vertex data on the wire.
	GenerateShape32();
}

void AShapeGenerator32::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShapeGenerator32, ShapeType);
	DOREPLIFETIME(AShapeGenerator32, SurfaceMode);
	DOREPLIFETIME(AShapeGenerator32, GridSize);
	DOREPLIFETIME(AShapeGenerator32, CellSize);
	DOREPLIFETIME(AShapeGenerator32, Center);
	DOREPLIFETIME(AShapeGenerator32, Seed);
	DOREPLIFETIME(AShapeGenerator32, NoiseStrength);
	DOREPLIFETIME(AShapeGenerator32, Radius);
	DOREPLIFETIME(AShapeGenerator32, HalfExtents);
	DOREPLIFETIME(AShapeGenerator32, Height);
	DOREPLIFETIME(AShapeGenerator32, DeformStrength);
	DOREPLIFETIME(AShapeGenerator32, DeformRadius);
	DOREPLIFETIME(AShapeGenerator32, IsoLevel);
	DOREPLIFETIME(AShapeGenerator32, Material);
	DOREPLIFETIME(AShapeGenerator32, bUseRegionSeed);
	// NOTE: bGenerateOnBeginPlay is intentionally local-only.
}

int32 AShapeGenerator32::EffectiveSeed() const
{
	if (!bUseRegionSeed)
	{
		return Seed;
	}
	if (const UWorld* World = GetWorld())
	{
		// GetAuthGameMode is only valid on the authority / in PIE; everywhere
		// else we fall back to the GameMode default so clients converge.
		if (const AAfterfallGameMode* GM = World->GetAuthGameMode<AAfterfallGameMode>())
		{
			return GM->RegionSeed;
		}
	}
	return 1337; // == AAfterfallGameMode::RegionSeed default
}

AfterfallProcedural::FImpactPlan AShapeGenerator32::BuildServerFracturePlan(
	const FVector& ImpactWorldPosition, const FVector& ImpactVelocityCmPerSec,
	float MassKg, EMaterialType InMaterial, int32 MaxDepth) const
{
	AfterfallProcedural::FImpactPlan Empty;
	if (!HasAuthority() || !FMath::IsFinite(CellSize) || CellSize <= 0.0f
		|| !FMath::IsFinite(ImpactWorldPosition.X)
		|| !FMath::IsFinite(ImpactWorldPosition.Y)
		|| !FMath::IsFinite(ImpactWorldPosition.Z))
	{
		return Empty;
	}

	const int32 N = FMath::Clamp(GridSize, 2, 64);
	const float Half = (N - 1) * 0.5f;
	const FVector LocalSamplePosition = GetActorTransform().InverseTransformPosition(ImpactWorldPosition) - Center;
	const float GridHalfExtent = Half * CellSize;
	if (FMath::Abs(LocalSamplePosition.X) > GridHalfExtent
		|| FMath::Abs(LocalSamplePosition.Y) > GridHalfExtent
		|| FMath::Abs(LocalSamplePosition.Z) > GridHalfExtent)
	{
		return Empty;
	}

	AfterfallProcedural::FImpactRequest Request;
	Request.StartCell = FIntVector(
		FMath::Clamp(FMath::RoundToInt(LocalSamplePosition.X / CellSize + Half), 0, N - 1),
		FMath::Clamp(FMath::RoundToInt(LocalSamplePosition.Y / CellSize + Half), 0, N - 1),
		FMath::Clamp(FMath::RoundToInt(LocalSamplePosition.Z / CellSize + Half), 0, N - 1));
	Request.Seed = static_cast<uint32>(EffectiveSeed());
	Request.Material = InMaterial;
	Request.MassKg = MassKg;
	// Unreal rigid-body velocities are centimeters per second; the registered
	// impact-energy equation is SI, so convert exactly once at this boundary.
	Request.ImpactVelocityMps = ImpactVelocityCmPerSec * 0.01;
	Request.MaxDepth = MaxDepth;
	return AfterfallProcedural::BuildImpactPlan(Request);
}

float AShapeGenerator32::EvaluateSDF(const FVector& Q) const
{
	// Shared equations (ProceduralSdf.h): one bounded warp with the legacy
	// fixed direction, then the primitive, then deterministic noise.
	// Numerically identical to the previous inline implementation.
	AfterfallSdf::FSdfParams P;
	P.Shape = ShapeType;
	P.HalfExtents = HalfExtents;
	P.Radius = Radius;
	P.Height = Height;
	P.NoiseStrength = NoiseStrength;
	P.NoiseOffset = CachedNoiseOffset;
	AfterfallSdf::FWarpOp W;
	W.Center = Center;
	W.Direction = FVector(1.0f, -1.0f, 0.0f);
	W.Radius = DeformRadius;
	W.Strength = DeformStrength;
	TArray<AfterfallSdf::FWarpOp> Warps;
	Warps.Add(W);
	return AfterfallSdf::EvalField(P, Q, Warps);
}

namespace
{
	struct FShapeBuildBuffers
	{
		TArray<FVector> Positions;
		TArray<int32> Indices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
	};

	/** Quad emit with automatic winding fix (same pattern as VoxelMesher). */
	void AddFixedQuad(FShapeBuildBuffers& Out, const FVector& P0, const FVector& P1,
		const FVector& P2, const FVector& P3, const FVector& Outward)
	{
		FVector N = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
		const bool bFlipped = FVector::DotProduct(N, Outward) < 0.0f;
		if (bFlipped)
		{
			N = -N;
		}
		const int32 Base = Out.Positions.Num();
		Out.Positions.Add(P0);
		Out.Positions.Add(bFlipped ? P3 : P1);
		Out.Positions.Add(P2);
		Out.Positions.Add(bFlipped ? P1 : P3);
		for (int32 i = 0; i < 4; ++i)
		{
			Out.Normals.Add(N);
			Out.Colors.Add(FLinearColor::White);
		}
		Out.UVs.Add(FVector2D(0.0f, 0.0f));
		Out.UVs.Add(FVector2D(1.0f, 0.0f));
		Out.UVs.Add(FVector2D(1.0f, 1.0f));
		Out.UVs.Add(FVector2D(0.0f, 1.0f));
		Out.Indices.Add(Base + 0);
		Out.Indices.Add(Base + 1);
		Out.Indices.Add(Base + 2);
		Out.Indices.Add(Base + 0);
		Out.Indices.Add(Base + 2);
		Out.Indices.Add(Base + 3);
	}

	void BuildBlocky(const TArray<float>& Field, int32 N, float CellSize, float IsoLevel,
		const FVector& Center, FShapeBuildBuffers& Out)
	{
		const float H = CellSize * 0.5f;
		const float Half = (N - 1) * 0.5f;
		auto At = [&](int32 X, int32 Y, int32 Z) -> float
		{
			if (X < 0 || Y < 0 || Z < 0 || X >= N || Y >= N || Z >= N)
			{
				return IsoLevel + 1.0f; // out of bounds counts as non-solid
			}
			return Field[X + N * (Y + N * Z)];
		};
		for (int32 Z = 0; Z < N; ++Z)
		{
			for (int32 Y = 0; Y < N; ++Y)
			{
				for (int32 X = 0; X < N; ++X)
				{
					if (At(X, Y, Z) > IsoLevel)
					{
						continue;
					}
					const FVector Q((X - Half) * CellSize, (Y - Half) * CellSize, (Z - Half) * CellSize);
					const FVector C = Q + Center;
					if (At(X + 1, Y, Z) > IsoLevel)
					{
						AddFixedQuad(Out, C + FVector(H, -H, -H), C + FVector(H, -H, H),
							C + FVector(H, H, H), C + FVector(H, H, -H), FVector(1, 0, 0));
					}
					if (At(X - 1, Y, Z) > IsoLevel)
					{
						AddFixedQuad(Out, C + FVector(-H, -H, -H), C + FVector(-H, H, -H),
							C + FVector(-H, H, H), C + FVector(-H, -H, H), FVector(-1, 0, 0));
					}
					if (At(X, Y + 1, Z) > IsoLevel)
					{
						AddFixedQuad(Out, C + FVector(-H, H, -H), C + FVector(H, H, -H),
							C + FVector(H, H, H), C + FVector(-H, H, H), FVector(0, 1, 0));
					}
					if (At(X, Y - 1, Z) > IsoLevel)
					{
						AddFixedQuad(Out, C + FVector(-H, -H, -H), C + FVector(-H, -H, H),
							C + FVector(H, -H, H), C + FVector(H, -H, -H), FVector(0, -1, 0));
					}
					if (At(X, Y, Z + 1) > IsoLevel)
					{
						AddFixedQuad(Out, C + FVector(-H, -H, H), C + FVector(-H, H, H),
							C + FVector(H, H, H), C + FVector(H, -H, H), FVector(0, 0, 1));
					}
					if (At(X, Y, Z - 1) > IsoLevel)
					{
						AddFixedQuad(Out, C + FVector(-H, -H, -H), C + FVector(H, -H, -H),
							C + FVector(H, H, -H), C + FVector(-H, H, -H), FVector(0, 0, -1));
					}
				}
			}
		}
	}

	FVector FieldGradient(const TArray<float>& Field, int32 N, float CellSize, int32 X, int32 Y, int32 Z)
	{
		auto At = [&](int32 IX, int32 IY, int32 IZ) -> float
		{
			IX = FMath::Clamp(IX, 0, N - 1);
			IY = FMath::Clamp(IY, 0, N - 1);
			IZ = FMath::Clamp(IZ, 0, N - 1);
			return Field[IX + N * (IY + N * IZ)];
		};
		const float Inv2H = 1.0f / (2.0f * CellSize);
		return FVector(
			(At(X + 1, Y, Z) - At(X - 1, Y, Z)) * Inv2H,
			(At(X, Y + 1, Z) - At(X, Y - 1, Z)) * Inv2H,
			(At(X, Y, Z + 1) - At(X, Y, Z - 1)) * Inv2H);
	}

	// Returns number of skipped (invalid) triangles for diagnostics.
	int32 BuildMarchingCubes(const TArray<float>& Field, int32 N, float CellSize, float IsoLevel,
		const FVector& Center, FShapeBuildBuffers& Out)
	{
		int32 Skipped = 0;
		const float Half = (N - 1) * 0.5f;
		auto GridPos = [&](int32 X, int32 Y, int32 Z) -> FVector
		{
			return FVector((X - Half) * CellSize, (Y - Half) * CellSize, (Z - Half) * CellSize);
		};
		for (int32 Z = 0; Z < N - 1; ++Z)
		{
			for (int32 Y = 0; Y < N - 1; ++Y)
			{
				for (int32 X = 0; X < N - 1; ++X)
				{
					float F[8];
					FVector P[8];
					FVector G[8];
					for (int32 i = 0; i < 8; ++i)
					{
						const int32 GX = X + AfterfallMC::CornerDX[i];
						const int32 GY = Y + AfterfallMC::CornerDY[i];
						const int32 GZ = Z + AfterfallMC::CornerDZ[i];
						F[i] = Field[GX + N * (GY + N * GZ)];
						P[i] = GridPos(GX, GY, GZ);
						G[i] = FieldGradient(Field, N, CellSize, GX, GY, GZ);
					}
					int32 CubeIndex = 0;
					for (int32 i = 0; i < 8; ++i)
					{
						// Solid means F <= IsoLevel (matches blocky + UE's
						// inside convention of value < isovalue).
						if (F[i] <= IsoLevel)
						{
							CubeIndex |= (1 << i);
						}
					}
					const uint16 EdgeMask = AfterfallMC::EdgeTable[CubeIndex];
					if (EdgeMask == 0)
					{
						continue;
					}
					FVector VertPos[12];
					FVector VertNrm[12];
					for (int32 e = 0; e < 12; ++e)
					{
						if ((EdgeMask & (1u << e)) == 0)
						{
							continue;
						}
						const int32 a = AfterfallMC::EdgeCorners[e][0];
						const int32 b = AfterfallMC::EdgeCorners[e][1];
						const float Denom = F[b] - F[a];
						float T = 0.5f;
						if (!FMath::IsNearlyZero(Denom))
						{
							T = FMath::Clamp((IsoLevel - F[a]) / Denom, 0.0f, 1.0f);
						}
						VertPos[e] = P[a] + T * (P[b] - P[a]);
						FVector Nrm = G[a] + T * (G[b] - G[a]);
						// SDF grows outward, so the gradient IS the outward
						// normal; fall back to +Z only when degenerate.
						if (!Nrm.Normalize())
						{
							Nrm = FVector::UpVector;
						}
						VertNrm[e] = Nrm;
					}
					for (uint64 Packed = AfterfallMC::TriTable[CubeIndex]; Packed != 0; Packed >>= 12)
					{
						const int32 ea = int32(Packed & 0xFu);
						const int32 eb = int32((Packed >> 4) & 0xFu);
						const int32 ec = int32((Packed >> 8) & 0xFu);
						if (ea >= 12 || eb >= 12 || ec >= 12
							|| (EdgeMask & (1u << ea)) == 0
							|| (EdgeMask & (1u << eb)) == 0
							|| (EdgeMask & (1u << ec)) == 0)
						{
							++Skipped;
							continue;
						}
						const int32 Base = Out.Positions.Num();
						Out.Positions.Add(VertPos[ea] + Center);
						Out.Positions.Add(VertPos[eb] + Center);
						Out.Positions.Add(VertPos[ec] + Center);
						Out.Normals.Add(VertNrm[ea]);
						Out.Normals.Add(VertNrm[eb]);
						Out.Normals.Add(VertNrm[ec]);
						Out.Colors.Add(FLinearColor::White);
						Out.Colors.Add(FLinearColor::White);
						Out.Colors.Add(FLinearColor::White);
						// Cheap spatially-varying UVs (debug generator only).
						Out.UVs.Add(FVector2D((VertPos[ea].X + VertPos[ea].Y) * 0.01f, VertPos[ea].Z * 0.01f));
						Out.UVs.Add(FVector2D((VertPos[eb].X + VertPos[eb].Y) * 0.01f, VertPos[eb].Z * 0.01f));
						Out.UVs.Add(FVector2D((VertPos[ec].X + VertPos[ec].Y) * 0.01f, VertPos[ec].Z * 0.01f));
						Out.Indices.Add(Base + 0);
						Out.Indices.Add(Base + 1);
						Out.Indices.Add(Base + 2);
					}
				}
			}
		}
		return Skipped;
	}
}

void AShapeGenerator32::GenerateShape32()
{
	LastVertexCount = 0;
	LastTriangleCount = 0;

	const int32 N = FMath::Clamp(GridSize, 2, 64);
	if (CellSize <= 0.0f)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("SHAPEGEN32 rejected: CellSize must be > 0 (got %f)"), CellSize);
		if (ProcMesh)
		{
			ProcMesh->ClearAllMeshSections();
		}
		return;
	}
	if (!ProcMesh)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("SHAPEGEN32 rejected: ProcMesh is null"));
		return;
	}
	ProcMesh->ClearAllMeshSections();

	// Deterministic noise offset from the effective seed (no global RNG).
	{
		FRandomStream RS(EffectiveSeed());
		CachedNoiseOffset = FVector(RS.FRandRange(-512.0f, 512.0f),
			RS.FRandRange(-512.0f, 512.0f), RS.FRandRange(-512.0f, 512.0f));
	}

	TArray<float> Field;
	Field.SetNum(N * N * N);
	const float Half = (N - 1) * 0.5f;
	for (int32 Z = 0; Z < N; ++Z)
	{
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const FVector Q((X - Half) * CellSize, (Y - Half) * CellSize, (Z - Half) * CellSize);
				Field[FieldIndex(N, X, Y, Z)] = EvaluateSDF(Q);
			}
		}
	}

	FShapeBuildBuffers Buf;
	int32 SkippedTris = 0;
	if (SurfaceMode == ESurfaceMode32::Blocky_Voxel)
	{
		BuildBlocky(Field, N, CellSize, IsoLevel, Center, Buf);
	}
	else
	{
		SkippedTris = BuildMarchingCubes(Field, N, CellSize, IsoLevel, Center, Buf);
	}

	LastVertexCount = Buf.Positions.Num();
	LastTriangleCount = Buf.Indices.Num() / 3;

	if (Buf.Positions.Num() == 0 || Buf.Indices.Num() < 3)
	{
		UE_LOG(LogAfterfall, Warning,
			TEXT("SHAPEGEN32 empty mesh: shape=%d mode=%d seed=%d grid=%d (nothing solid in field)"),
			(int32)ShapeType, (int32)SurfaceMode, EffectiveSeed(), N);
		return;
	}

	// Post-build validation: index range + degenerate-triangle fraction.
	int32 BadIndices = 0;
	for (int32 Idx : Buf.Indices)
	{
		if (Idx < 0 || Idx >= Buf.Positions.Num())
		{
			++BadIndices;
		}
	}
	int32 Degenerate = 0;
	FBox Bounds(ForceInit);
	for (int32 t = 0; t + 2 < Buf.Indices.Num(); t += 3)
	{
		const int32 ia = Buf.Indices[t];
		const int32 ib = Buf.Indices[t + 1];
		const int32 ic = Buf.Indices[t + 2];
		if (ia < 0 || ib < 0 || ic < 0
			|| ia >= Buf.Positions.Num() || ib >= Buf.Positions.Num() || ic >= Buf.Positions.Num())
		{
			continue;
		}
		const FVector E1 = Buf.Positions[ib] - Buf.Positions[ia];
		const FVector E2 = Buf.Positions[ic] - Buf.Positions[ia];
		if (FVector::CrossProduct(E1, E2).Size() < 1e-4f)
		{
			++Degenerate;
		}
		Bounds += Buf.Positions[ia];
		Bounds += Buf.Positions[ib];
		Bounds += Buf.Positions[ic];
	}

	TArray<FProcMeshTangent> Tangents; // default tangents are fine
	ProcMesh->CreateMeshSection_LinearColor(0, Buf.Positions, Buf.Indices, Buf.Normals,
		Buf.UVs, Buf.Colors, Tangents, /*bCreateCollision=*/true);
	if (Material)
	{
		ProcMesh->SetMaterial(0, Material);
	}

	const float Extent = Bounds.GetExtent().GetMax();
	UE_LOG(LogAfterfall, Log,
		TEXT("SHAPEGEN32 built: shape=%d mode=%d seed=%d grid=%d verts=%d tris=%d extent=%.1f skipped=%d badidx=%d degenerate=%d"),
		(int32)ShapeType, (int32)SurfaceMode, EffectiveSeed(), N,
		LastVertexCount, LastTriangleCount, Extent, SkippedTris, BadIndices, Degenerate);
	if (BadIndices > 0 || (LastTriangleCount > 0 && Degenerate * 2 > LastTriangleCount))
	{
		UE_LOG(LogAfterfall, Warning,
			TEXT("SHAPEGEN32 table check FAILED: badidx=%d degenerate=%d/%d (shape=%d mode=%d)"),
			BadIndices, Degenerate, LastTriangleCount, (int32)ShapeType, (int32)SurfaceMode);
	}
}
