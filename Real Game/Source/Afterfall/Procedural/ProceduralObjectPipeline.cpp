// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
#include "Procedural/ProceduralObjectPipeline.h"
#include "Procedural/ProceduralSdf.h"
#include "Procedural/MarchingCubesTables.h"

namespace AfterfallProcObj
{
	namespace
	{
		inline int32 FIdx(int32 N, int32 X, int32 Y, int32 Z) { return X + N * (Y + N * Z); }

		void ToSdfParams(const FProceduralObjectDesc& Desc, AfterfallSdf::FSdfParams& P)
		{
			P.Shape = Desc.Shape;
			P.HalfExtents = Desc.HalfExtents;
			P.Radius = Desc.Radius;
			P.Height = Desc.Height;
			P.NoiseStrength = Desc.NoiseStrength;
			P.NoiseOffset = AfterfallSdf::MakeNoiseOffset(Desc.Seed);
		}

		void ToWarp(const FProceduralDeformOp& Op, AfterfallSdf::FWarpOp& W)
		{
			W.Center = Op.Center;
			W.Direction = Op.Direction.GetSafeNormal();
			W.Radius = Op.Radius;
			W.Strength = Op.Strength;
		}

		FVector FieldGrad(const TArray<float>& Field, int32 N, float Cell, int32 X, int32 Y, int32 Z)
		{
			auto At = [&](int32 IX, int32 IY, int32 IZ) -> float
			{
				IX = FMath::Clamp(IX, 0, N - 1);
				IY = FMath::Clamp(IY, 0, N - 1);
				IZ = FMath::Clamp(IZ, 0, N - 1);
				return Field[FIdx(N, IX, IY, IZ)];
			};
			const float Inv2H = 1.0f / (2.0f * Cell);
			return FVector(
				(At(X + 1, Y, Z) - At(X - 1, Y, Z)) * Inv2H,
				(At(X, Y + 1, Z) - At(X, Y - 1, Z)) * Inv2H,
				(At(X, Y, Z + 1) - At(X, Y, Z - 1)) * Inv2H);
		}

		void EmitQuad(FProcMeshBuffers& Out, const FVector& P0, const FVector& P1,
			const FVector& P2, const FVector& P3, const FVector& Outward)
		{
			FVector Nrm = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
			const bool bFlipped = FVector::DotProduct(Nrm, Outward) < 0.0f;
			if (bFlipped)
			{
				Nrm = -Nrm;
			}
			const int32 Base = Out.Positions.Num();
			Out.Positions.Add(P0);
			Out.Positions.Add(bFlipped ? P3 : P1);
			Out.Positions.Add(P2);
			Out.Positions.Add(bFlipped ? P3 : P1);
			for (int32 i = 0; i < 4; ++i)
			{
				Out.Normals.Add(Nrm);
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

		void ExtractBlocky(const TArray<float>& Field, int32 N, float Cell, float Iso,
			const FVector& Center, FProcMeshBuffers& Out)
		{
			const float H = Cell * 0.5f;
			const float Half = (N - 1) * 0.5f;
			auto At = [&](int32 X, int32 Y, int32 Z) -> float
			{
				if (X < 0 || Y < 0 || Z < 0 || X >= N || Y >= N || Z >= N)
				{
					return Iso + 1.0f;
				}
				return Field[FIdx(N, X, Y, Z)];
			};
			for (int32 Z = 0; Z < N; ++Z)
				for (int32 Y = 0; Y < N; ++Y)
					for (int32 X = 0; X < N; ++X)
					{
						if (At(X, Y, Z) > Iso)
						{
							continue;
						}
						const FVector C = FVector((X - Half) * Cell, (Y - Half) * Cell, (Z - Half) * Cell) + Center;
						if (At(X + 1, Y, Z) > Iso) EmitQuad(Out, C + FVector(H, -H, -H), C + FVector(H, -H, H), C + FVector(H, H, H), C + FVector(H, H, -H), FVector(1, 0, 0));
						if (At(X - 1, Y, Z) > Iso) EmitQuad(Out, C + FVector(-H, -H, -H), C + FVector(-H, H, -H), C + FVector(-H, H, H), C + FVector(-H, -H, H), FVector(-1, 0, 0));
						if (At(X, Y + 1, Z) > Iso) EmitQuad(Out, C + FVector(-H, H, -H), C + FVector(H, H, -H), C + FVector(H, H, H), C + FVector(-H, H, H), FVector(0, 1, 0));
						if (At(X, Y - 1, Z) > Iso) EmitQuad(Out, C + FVector(-H, -H, -H), C + FVector(-H, -H, H), C + FVector(H, -H, H), C + FVector(H, -H, -H), FVector(0, -1, 0));
						if (At(X, Y, Z + 1) > Iso) EmitQuad(Out, C + FVector(-H, -H, H), C + FVector(-H, H, H), C + FVector(H, H, H), C + FVector(H, -H, H), FVector(0, 0, 1));
						if (At(X, Y, Z - 1) > Iso) EmitQuad(Out, C + FVector(-H, -H, -H), C + FVector(H, -H, -H), C + FVector(H, H, -H), C + FVector(-H, H, -H), FVector(0, 0, -1));
					}
		}

		void ExtractMC(const TArray<float>& Field, int32 N, float Cell, float Iso,
			const FVector& Center, FProcMeshBuffers& Out)
		{
			const float Half = (N - 1) * 0.5f;
			auto GridPos = [&](int32 X, int32 Y, int32 Z) -> FVector
			{
				return FVector((X - Half) * Cell, (Y - Half) * Cell, (Z - Half) * Cell);
			};
			for (int32 Z = 0; Z < N - 1; ++Z)
				for (int32 Y = 0; Y < N - 1; ++Y)
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
							F[i] = Field[FIdx(N, GX, GY, GZ)];
							P[i] = GridPos(GX, GY, GZ);
							G[i] = FieldGrad(Field, N, Cell, GX, GY, GZ);
						}
						int32 CubeIndex = 0;
						for (int32 i = 0; i < 8; ++i)
						{
							if (F[i] <= Iso)
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
								T = FMath::Clamp((Iso - F[a]) / Denom, 0.0f, 1.0f);
							}
							VertPos[e] = P[a] + T * (P[b] - P[a]);
							FVector Nrm = G[a] + T * (G[b] - G[a]);
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
								++Out.SkippedTris;
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

	void EvaluateField(const FProceduralObjectDesc& Desc,
		const TArray<FProceduralDeformOp>& DeformOps, TArray<float>& OutField)
	{
		OutField.Reset();
		if (Desc.CellSize <= 0.0f || !FMath::IsFinite(Desc.CellSize))
		{
			return;
		}
		const int32 N = ClampGridSize(Desc.GridSize);
		AfterfallSdf::FSdfParams P;
		ToSdfParams(Desc, P);
		TArray<AfterfallSdf::FWarpOp> Warps;
		Warps.Reserve(DeformOps.Num());
		for (const FProceduralDeformOp& Op : DeformOps)
		{
			if (Op.Strength == 0.0f || Op.Radius <= 0.0f)
			{
				continue;
			}
			AfterfallSdf::FWarpOp W;
			ToWarp(Op, W);
			Warps.Add(W);
		}
		OutField.SetNum(N * N * N);
		const float Half = (N - 1) * 0.5f;
		for (int32 Z = 0; Z < N; ++Z)
			for (int32 Y = 0; Y < N; ++Y)
				for (int32 X = 0; X < N; ++X)
				{
					const FVector Q((X - Half) * Desc.CellSize, (Y - Half) * Desc.CellSize, (Z - Half) * Desc.CellSize);
					OutField[FIdx(N, X, Y, Z)] = AfterfallSdf::EvalField(P, Q, Warps);
				}
	}

	void ExtractMesh(const TArray<float>& Field, const FProceduralObjectDesc& Desc,
		FProcMeshBuffers& OutBuffers)
	{
		OutBuffers = FProcMeshBuffers();
		const int32 N = ClampGridSize(Desc.GridSize);
		if (Field.Num() != N * N * N)
		{
			return;
		}
		if (Desc.SurfaceMode == ESurfaceMode32::Blocky_Voxel)
		{
			ExtractBlocky(Field, N, Desc.CellSize, Desc.IsoLevel, Desc.Center, OutBuffers);
		}
		else
		{
			ExtractMC(Field, N, Desc.CellSize, Desc.IsoLevel, Desc.Center, OutBuffers);
		}
		if (OutBuffers.Positions.Num() == 0 || OutBuffers.Indices.Num() < 3)
		{
			return;
		}
		for (int32 Idx : OutBuffers.Indices)
		{
			if (Idx < 0 || Idx >= OutBuffers.Positions.Num())
			{
				++OutBuffers.BadIndices;
			}
		}
		for (int32 t = 0; t + 2 < OutBuffers.Indices.Num(); t += 3)
		{
			const int32 ia = OutBuffers.Indices[t];
			const int32 ib = OutBuffers.Indices[t + 1];
			const int32 ic = OutBuffers.Indices[t + 2];
			if (ia < 0 || ib < 0 || ic < 0
				|| ia >= OutBuffers.Positions.Num() || ib >= OutBuffers.Positions.Num() || ic >= OutBuffers.Positions.Num())
			{
				continue;
			}
			const FVector E1 = OutBuffers.Positions[ib] - OutBuffers.Positions[ia];
			const FVector E2 = OutBuffers.Positions[ic] - OutBuffers.Positions[ia];
			if (FVector::CrossProduct(E1, E2).Size() < 1e-4f)
			{
				++OutBuffers.DegenerateTris;
			}
			OutBuffers.Bounds += OutBuffers.Positions[ia];
			OutBuffers.Bounds += OutBuffers.Positions[ib];
			OutBuffers.Bounds += OutBuffers.Positions[ic];
		}
	}

	bool ValidateBuffers(const FProcMeshBuffers& Buf, FString* OutReason)
	{
		auto Fail = [&](const FString& R) -> bool
		{
			if (OutReason)
			{
				*OutReason = R;
			}
			return false;
		};
		if (Buf.Positions.Num() == 0 || Buf.Indices.Num() < 3)
		{
			return Fail(TEXT("empty mesh"));
		}
		if (Buf.Indices.Num() % 3 != 0)
		{
			return Fail(TEXT("index count not a multiple of 3"));
		}
		if (Buf.Normals.Num() != Buf.Positions.Num() || Buf.UVs.Num() != Buf.Positions.Num())
		{
			return Fail(TEXT("attribute count mismatch"));
		}
		for (int32 Idx : Buf.Indices)
		{
			if (Idx < 0 || Idx >= Buf.Positions.Num())
			{
				return Fail(TEXT("index out of range"));
			}
		}
		for (const FVector& V : Buf.Positions)
		{
			if (!FMath::IsFinite(V.X) || !FMath::IsFinite(V.Y) || !FMath::IsFinite(V.Z))
			{
				return Fail(TEXT("non-finite position"));
			}
		}
		for (const FVector& Nr : Buf.Normals)
		{
			if (!FMath::IsFinite(Nr.X) || !FMath::IsFinite(Nr.Y) || !FMath::IsFinite(Nr.Z))
			{
				return Fail(TEXT("non-finite normal"));
			}
		}
		const int32 TriCount = Buf.Indices.Num() / 3;
		if (TriCount > 0 && Buf.DegenerateTris * 2 > TriCount)
		{
			return Fail(TEXT("degenerate majority"));
		}
		return true;
	}

	uint64 HashField(const TArray<float>& Field)
	{
		// FNV-1a over the raw bits: deterministic across runs/machines.
		uint64 H = 1469598103934665603ULL;
		for (float V : Field)
		{
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &V, sizeof(Bits));
			H ^= Bits;
			H *= 1099511628211ULL;
		}
		return H;
	}

	AfterfallProcedural::FImpactPlan PlanFractureEvent(
		const FProceduralObjectDesc& Desc, const FVector& LocalPos,
		const FVector& LocalDir, float EnergyJ, EMaterialType Material, uint32 Seed)
	{
		AfterfallProcedural::FImpactPlan Refused;
		if (!IsFractureRequestValid(Desc, LocalPos, LocalDir, EnergyJ))
		{
			return Refused;
		}
		const int32 N = ClampGridSize(Desc.GridSize);
		const float HalfExtent = (N - 1) * 0.5f * Desc.CellSize;
		AfterfallProcedural::FImpactRequest Request;
		Request.StartCell = FIntVector(
			FMath::Clamp(FMath::RoundToInt(LocalPos.X / Desc.CellSize + (N - 1) * 0.5f), 0, N - 1),
			FMath::Clamp(FMath::RoundToInt(LocalPos.Y / Desc.CellSize + (N - 1) * 0.5f), 0, N - 1),
			FMath::Clamp(FMath::RoundToInt(LocalPos.Z / Desc.CellSize + (N - 1) * 0.5f), 0, N - 1));
		Request.Seed = Seed;
		Request.Material = Material;
		// Bridge expects SI velocity; here the caller supplies joules
		// directly, so recover the equivalent speed for a 1 kg mass:
		// E = 0.5 * v^2  ->  v = sqrt(2E). Mass cancels in the bridge
		// (E = m * 0.5*v^2 with m=1, v from E).
		Request.MassKg = 1.0f;
		const float SpeedMps = FMath::Sqrt(FMath::Max(0.0f, EnergyJ) * 2.0f);
		const FVector DirN = LocalDir.GetSafeNormal();
		Request.ImpactVelocityMps = DirN * SpeedMps;
		Request.MaxDepth = FMath::Clamp(Desc.FractureMaxDepth, 1, 64);
		AfterfallProcedural::FImpactPlan Plan = AfterfallProcedural::BuildImpactPlan(Request);
		// Guard tiny FP drift: the bridge must reproduce the caller energy.
		if (Plan.IsValidInput && FMath::Abs(Plan.ImpactEnergyJ - EnergyJ) > FMath::Max(1.0f, EnergyJ * 0.001f))
		{
			Plan.IsValidInput = false;
			Plan.Fracture.bInitiated = false;
		}
		(void)HalfExtent;
		return Plan;
	}

	int32 ApplyFractureCarve(TArray<float>& Field, const FProceduralObjectDesc& Desc,
		const AfterfallProcedural::FImpactPlan& Plan)
	{
		if (!Plan.IsValidInput || !Plan.Fracture.bInitiated || Plan.Fracture.Path.Num() == 0)
		{
			return 0;
		}
		const int32 N = ClampGridSize(Desc.GridSize);
		if (Field.Num() != N * N * N)
		{
			return 0;
		}
		// Map lattice cells (0..N-1) to field sample coords 1:1, then clear
		// a bounded sphere per path cell. Cap total carved samples.
		const float CarveRadius = Desc.CellSize * 1.5f;
		const float Half = (N - 1) * 0.5f;
		const int32 Cap = FMath::Max(0, Desc.FractureCellCap);
		int32 Carved = 0;
		for (const FIntVector& Cell : Plan.Fracture.Path)
		{
			if (Carved >= Cap)
			{
				break;
			}
			const FVector CellCenter(
				(FMath::Clamp(Cell.X, 0, N - 1) - Half) * Desc.CellSize,
				(FMath::Clamp(Cell.Y, 0, N - 1) - Half) * Desc.CellSize,
				(FMath::Clamp(Cell.Z, 0, N - 1) - Half) * Desc.CellSize);
			const int32 R = FMath::Max(1, FMath::RoundToInt(CarveRadius / Desc.CellSize));
			const int32 CX = FMath::Clamp(Cell.X, 0, N - 1);
			const int32 CY = FMath::Clamp(Cell.Y, 0, N - 1);
			const int32 CZ = FMath::Clamp(Cell.Z, 0, N - 1);
			for (int32 Z = CZ - R; Z <= CZ + R && Carved < Cap; ++Z)
				for (int32 Y = CY - R; Y <= CY + R && Carved < Cap; ++Y)
					for (int32 X = CX - R; X <= CX + R && Carved < Cap; ++X)
					{
						if (X < 0 || Y < 0 || Z < 0 || X >= N || Y >= N || Z >= N)
						{
							continue;
						}
						const FVector Q((X - Half) * Desc.CellSize, (Y - Half) * Desc.CellSize, (Z - Half) * Desc.CellSize);
						if (FVector::Dist(Q, CellCenter) <= CarveRadius)
						{
							float& F = Field[FIdx(N, X, Y, Z)];
							if (F <= Desc.IsoLevel)
							{
								F = Desc.IsoLevel + Desc.CellSize;
								++Carved;
							}
						}
					}
		}
		return Carved;
	}

	bool IsDeformOpValid(const FProceduralObjectDesc& Desc, const FProceduralDeformOp& Op)
	{
		const int32 N = ClampGridSize(Desc.GridSize);
		const float Extent = (N - 1) * 0.5f * Desc.CellSize + Op.Radius;
		return Op.OpId.IsValid()
			&& FMath::IsFinite(Op.Strength) && Op.Strength != 0.0f && FMath::Abs(Op.Strength) <= 100000.0f
			&& FMath::IsFinite(Op.Radius) && Op.Radius > 0.0f && Op.Radius <= 100000.0f
			&& FMath::IsFinite(Op.Center.X) && FMath::IsFinite(Op.Center.Y) && FMath::IsFinite(Op.Center.Z)
			&& Op.Center.Size() <= Extent + 100000.0f
			&& FMath::IsFinite(Op.Direction.X) && FMath::IsFinite(Op.Direction.Y) && FMath::IsFinite(Op.Direction.Z)
			&& Op.Direction.SizeSquared() > 1e-8f;
	}

	bool IsFractureRequestValid(const FProceduralObjectDesc& Desc, const FVector& LocalPos,
		const FVector& LocalDir, float EnergyJ)
	{
		const int32 N = ClampGridSize(Desc.GridSize);
		const float Extent = (N - 1) * 0.5f * Desc.CellSize;
		return Desc.CellSize > 0.0f && FMath::IsFinite(Desc.CellSize)
			&& FMath::IsFinite(EnergyJ) && EnergyJ > 0.0f && EnergyJ <= 1e9f
			&& FMath::IsFinite(LocalPos.X) && FMath::IsFinite(LocalPos.Y) && FMath::IsFinite(LocalPos.Z)
			&& FMath::Abs(LocalPos.X) <= Extent && FMath::Abs(LocalPos.Y) <= Extent && FMath::Abs(LocalPos.Z) <= Extent
			&& FMath::IsFinite(LocalDir.X) && FMath::IsFinite(LocalDir.Y) && FMath::IsFinite(LocalDir.Z)
			&& LocalDir.SizeSquared() > 1e-8f;
	}
}
