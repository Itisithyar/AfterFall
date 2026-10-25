// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: FLINT (GLM session) — see BoxBuilding.h. Pure implementations.
#include "BoxBuilding.h"
#include "Math/UnrealMathUtility.h"

namespace BoxDefaults
{
	static const float DensityKgM3[] =
	{
		800.f,  // Drywall (plaster/panel composite)
		600.f,  // Wood (framing/board)
		1800.f, // Brick/masonry
		2300.f, // Concrete
		2500.f, // Glass
		7800.f  // Metal structural
	};

	// Break energy (J) per ONE 50cm cube. Weak vs strong behavior anchor:
	// a body at ~3 m/s (~338 J for 75kg) punches through drywall/wood/glass,
	// dents brick or metal, and never breaks a concrete cube in one hit.
	static const float BreakEnergyJ[] =
	{
		120.f,  // Drywall
		250.f,  // Wood
		700.f,  // Brick
		2500.f, // Concrete
		60.f,   // Glass
		900.f   // Metal
	};

	static const float SplashFraction[] =
	{
		0.10f, // Drywall (brittle but neighbors dent, don't shred)
		0.25f, // Wood (stiff frame transfers a little)
		0.20f, // Brick (mortar transfers some)
		0.10f, // Concrete (very local)
		0.40f, // Glass (window pane shatters wide)
		0.15f  // Metal (bends locally)
	};
}

FName UBoxBuildingLib::MaterialId(EBoxBlockMaterial M)
{
	static const FName Ids[] = { TEXT("Drywall"), TEXT("Wood"), TEXT("Brick"),
		TEXT("Concrete"), TEXT("Glass"), TEXT("Metal") };
	return Ids[static_cast<int32>(M)];
}

float UBoxBuildingLib::DensityKgM3(EBoxBlockMaterial M)
{
	return BoxDefaults::DensityKgM3[static_cast<int32>(M)];
}

float UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial M)
{
	return BoxDefaults::BreakEnergyJ[static_cast<int32>(M)];
}

float UBoxBuildingLib::SplashFraction(EBoxBlockMaterial M)
{
	return BoxDefaults::SplashFraction[static_cast<int32>(M)];
}

namespace BoxInternal
{
	struct FFaceAxes
	{
		// For face FI, map face (a,b) axes to grid axes: AxisA/AxisB, fixed axis + value func
		int32 AxisA = 0, AxisB = 2, FixedAxis = 1;
		int32 FixedValue(int32 A, int32 V) const { return V; }
	};

	// Returns true if grid coord belongs to face FI (fixed axis at its face value's FIRST/LAST).
	static bool OnFace(const FBoxBuildingProfile& P, int32 FaceIndex, const FIntVector& G)
	{
		const int32 H = P.CubesAcrossFace, V = P.CubesVertical, D = P.CubesDepth;
		const bool bFaceEdge = [&](int32 a, bool bFirst) {
			return bFirst ? (a == 0) : (a == INT32_MAX == 0 ? false : a == -1);
		}(0, true);
		(void)bFaceEdge; (void)H; (void)V; (void)D; (void)G;
		return false; // superseded by lambda version in BuildLayout
	}
}

void UBoxBuildingLib::BuildLayout(const FBoxBuildingProfile& P, TArray<FBoxBlock>& OutBlocks)
{
	OutBlocks.Reset();
	const int32 H = P.CubesAcrossFace; // x extent for Front/Back faces
	const int32 D = P.CubesDepth;      // y extent
	const int32 V = P.CubesVertical;   // z extent
	auto FaceOpeningContains = [&](int32 FaceIndex, FIntPoint FaceAB)
	{
		for (const FBoxOpening& O : P.Openings)
		{
			if (O.FaceIndex == FaceIndex &&
				FaceAB.X >= O.MinCube.X && FaceAB.X <= O.MaxCube.X &&
				FaceAB.Y >= O.MinCube.Y && FaceAB.Y <= O.MaxCube.Y)
			{
				return true;
			}
		}
		return false;
	};

	// Dedup corners: only emit a cube once per grid coordinate, checking
	// every candidate face; a block on multiple faces is ONE shared block.
	auto IsOnFace = [&](int32 FaceIndex, const FIntVector& G, FIntPoint& OutAB)
	{
		switch (FaceIndex)
		{
		case 0: // Front: y = 0
			if (G.Y != 0) return false;
			OutAB = FIntPoint(G.X, G.Z); return true;
		case 1: // Back: y = D-1
			if (G.Y != D - 1) return false;
			OutAB = FIntPoint(G.X, G.Z); return true;
		case 2: // Left: x = 0
			if (G.X != 0) return false;
			OutAB = FIntPoint(G.Y, G.Z); return true;
		case 3: // Right: x = H-1
			if (G.X != H - 1) return false;
			OutAB = FIntPoint(G.Y, G.Z); return true;
		case 4: // Floor: z = 0
			if (G.Z != 0) return false;
			OutAB = FIntPoint(G.X, G.Y); return true;
		case 5: // Ceiling: z = V-1
			if (G.Z != V - 1) return false;
			OutAB = FIntPoint(G.X, G.Y); return true;
		}
		return false;
	};

	for (int32 z = 0; z < V; ++z)
	{
		for (int32 y = 0; y < D; ++y)
		{
			for (int32 x = 0; x < H; ++x)
			{
				FIntVector G(x, y, z);
				auto HollowCheck = [&](FIntVector Test)
				{
					return (Test.X != 0 && Test.X != H - 1) &&
					       (Test.Y != 0 && Test.Y != D - 1) &&
					       (Test.Z != 0 && Test.Z != V - 1);
				};
				if (HollowCheck(G)) continue; // interior air: hollow box
				bool bOpened = false;
				for (int32 F = 0; F < 6 && !bOpened; ++F)
				{
					FIntPoint AB;
					if (IsOnFace(F, G, AB)) bOpened = FaceOpeningContains(F, AB);
				}
				if (bOpened) continue; // door/window opening subtracts the block

				FBoxBlock B;
				B.Grid = G;
				B.Id = x + y * H + z * H * D + 1;
				B.Material = P.Material;
				B.State = EBoxBlockState::Intact;
				B.LocalCenter = FVector((x + 0.5) * P.CubeSizeCm,
					(y + 0.5) * P.CubeSizeCm, (z + 0.5) * P.CubeSizeCm);
				OutBlocks.Add(B);
			}
		}
	}
}

bool UBoxBuildingLib::ValidateLayout(const FBoxBuildingProfile& P, const TArray<FBoxBlock>& Blocks)
{
	if (P.CubesAcrossFace < 2 || P.CubesVertical < 1 || P.CubesDepth < 2 || Blocks.Num() < 4)
	{
		return false;
	}
	const int32 H = P.CubesAcrossFace, D = P.CubesDepth, V = P.CubesVertical;
	for (const FBoxBlock& B : Blocks)
	{
		if (B.Grid.X < 0 || B.Grid.X >= H || B.Grid.Y < 0 || B.Grid.Y >= D ||
			B.Grid.Z < 0 || B.Grid.Z >= V) return false; // outside box
		const bool bSurfaceBlock =
			(B.Grid.X == 0 || B.Grid.X == H - 1) ||
			(B.Grid.Y == 0 || B.Grid.Y == D - 1) ||
			(B.Grid.Z == 0 || B.Grid.Z == V - 1);
		if (!bSurfaceBlock) return false; // floating interior block
	}
	// Opposing faces span the same extents at the same mesh of cube SIZE:
	// guaranteed by construction; openings must stay inside their face.
	for (const FBoxOpening& O : P.Openings)
	{
		if (O.FaceIndex < 0 || O.FaceIndex > 5) return false;
		const auto InRange = [](int32 Min_, int32 Max_, int32 FaceSize) {
			return Min_ >= 0 && Max_ >= Min_ && Max_ < FaceSize;
		};
		// a/b extents per face: Front/Back use (H, V); Left/Right use (D, V);
		// Floor/Ceiling use (H, D).
		switch (O.FaceIndex)
		{
		case 0: case 1:
			if (!InRange(O.MinCube.X, O.MaxCube.X, H) || !InRange(O.MinCube.Y, O.MaxCube.Y, V)) return false;
			break;
		case 2: case 3:
			if (!InRange(O.MinCube.X, O.MaxCube.X, D) || !InRange(O.MinCube.Y, O.MaxCube.Y, V)) return false;
			break;
		case 4: case 5:
			if (!InRange(O.MinCube.X, O.MaxCube.X, H) || !InRange(O.MinCube.Y, O.MaxCube.Y, D)) return false;
			break;
		}
	}
	return true;
}

float UBoxBuildingLib::MassFactor(EBoxBlockMaterial M)
{
	static const float MassFactorTable[] =
	{
		0.25f, // Drywall panel shell
		0.35f, // Wood framing pocket
		0.60f, // Brick masonry
		0.80f, // Concrete wall core
		0.04f, // Glass pane (very thin)
		0.30f  // Metal structural skin
	};
	return MassFactorTable[static_cast<int32>(M)];
}

float UBoxBuildingLib::BlockMassKg(const FBoxBuildingProfile& P, const FBoxBlock& B)
{
	const float VolM3 = FMath::Cube(P.CubeSizeCm / 100.f);
	return DensityKgM3(B.Material) * VolM3 * MassFactor(B.Material);
}

int32 UBoxBuildingLib::NearestBlock(const TArray<FBoxBlock>& Blocks, const FVector& LocalPos)
{
	int32 Best = INDEX_NONE;
	float BestDistSq = BIG_NUMBER;
	for (int32 i = 0; i < Blocks.Num(); ++i)
	{
		if (Blocks[i].State == EBoxBlockState::Detached ||
			Blocks[i].State == EBoxBlockState::Destroyed) continue;
		const float DistSq = FVector::DistSquared(Blocks[i].LocalCenter, LocalPos);
		if (DistSq < BestDistSq) { BestDistSq = DistSq; Best = i; }
	}
	return Best;
}

FBoxImpactResult UBoxBuildingLib::EvaluateImpact(const FBoxBuildingProfile& P,
	const TArray<FBoxBlock>& BlocksIn, const FBoxImpactInput& I)
{
	FBoxImpactResult R;
	TArray<FBoxBlock> Blocks = BlocksIn; // local working copy; caller re-commits
	const int32 CenterIdx = NearestBlock(Blocks, I.ImpactLocalCenter);
	if (CenterIdx == INDEX_NONE)
	{
		R.bDenied = true;
		return R;
	}
	const float SpeedCmS = FMath::Max(0.f, I.RelativeSpeedCmS);
	const float Energy = 0.5f * FMath::Max(0.f, I.EffectiveMassKg) * FMath::Square(SpeedCmS / 100.f);
	R.ImpactEnergyJ = Energy;
	const float Durability = BreakEnergyJ(I.Material);
	if (Energy < Durability * 0.5f)
	{
		R.bDenied = true;
		R.UpdatedBlocks = Blocks; // unchanged state returned for symmetry
		return R; // under-threshold: no state change at all
	}

	// Center block takes full impact energy as damage.
	Blocks[CenterIdx].AccDamage += Energy;
	if (Blocks[CenterIdx].AccDamage >= Durability)
	{
		Blocks[CenterIdx].State = EBoxBlockState::Detached;
		R.RemovedBlocks.Add(Blocks[CenterIdx].Id);
	}
	else
	{
		Blocks[CenterIdx].State = EBoxBlockState::Damaged;
		R.DamagedBlocks.Add(Blocks[CenterIdx].Id);
	}
	R.CenterBlockId = Blocks[CenterIdx].Id;

	// Splash to adjacent (<= 1 cube away, Chebyshev) intact blocks.
	const float Splash = Energy * SplashFraction(I.Material);
	const int32 CBX = Blocks[CenterIdx].Grid.X, CBY = Blocks[CenterIdx].Grid.Y, CBZ = Blocks[CenterIdx].Grid.Z;
	for (int32 i = 0; i < Blocks.Num(); ++i)
	{
		if (i == CenterIdx) continue;
		if (Blocks[i].State != EBoxBlockState::Intact && Blocks[i].State != EBoxBlockState::Damaged) continue;
		if (FMath::Abs(Blocks[i].Grid.X - CBX) > 1 || FMath::Abs(Blocks[i].Grid.Y - CBY) > 1 ||
			FMath::Abs(Blocks[i].Grid.Z - CBZ) > 1) continue;
		const float D2 = Durability;
		Blocks[i].AccDamage += Splash;
		if (Blocks[i].AccDamage >= D2)
		{
			Blocks[i].State = EBoxBlockState::Detached;
			R.RemovedBlocks.Add(Blocks[i].Id);
		}
		else
		{
			Blocks[i].State = EBoxBlockState::Damaged;
			R.DamagedBlocks.Add(Blocks[i].Id);
		}
	}

	// Support collapse after removals.
	for (int32 Id : R.RemovedBlocks)
	{
		SupportCollapse(P, Blocks, Id, R.CollapsedBlocks);
	}
	// FIX-A (ledger commit): fallen blocks are Detached in the authoritative
	// post-impact state, so later NearestBlock/evaluations never reselect
	// physically-gone blocks. Ids stay stable; iteration order deterministic.
	for (FBoxBlock& B : Blocks)
	{
		if (R.CollapsedBlocks.Contains(B.Id))
		{
			B.State = EBoxBlockState::Detached;
		}
	}
	// Out lists are deterministic because iteration order was by index.
	R.UpdatedBlocks = Blocks;
	return R;
}

void UBoxBuildingLib::SupportCollapse(const FBoxBuildingProfile& P,
	const TArray<FBoxBlock>& Blocks, int32 RemovedId, TArray<int32>& OutCollapse)
{
	// Iterative flood: a block's column support is the block directly below
	// (same x, y; z-1). If that is detached/destroyed, the block falls, and
	// its above-neighbors re-evaluated. Anchors are floor-face blocks (z=0).
	TMap<int32, int32> ById;
	for (int32 i = 0; i < Blocks.Num(); ++i) ById.Add(Blocks[i].Id, i);

	TArray<bool> FallingGrace; // matched Blocks by index
	FallingGrace.Init(false, Blocks.Num());

	// Seed: blocks directly above the removed cube (x,y, z+1).
	int32 SeedIdx = INDEX_NONE;
	if (int32* Found = ById.Find(RemovedId)) SeedIdx = *Found;
	else return;

	auto IndexAt = [&](int32 X, int32 Y, int32 Z) -> int32
	{
		const int32 Id = X + Y * P.CubesAcrossFace + Z * P.CubesAcrossFace * P.CubesDepth + 1;
		if (!ById.Contains(Id)) return INDEX_NONE;
		const int32 i = ById[Id];
		if (Blocks[i].State == EBoxBlockState::Detached || Blocks[i].State == EBoxBlockState::Destroyed)
			return INDEX_NONE;
		return i;
	};

	TArray<bool> InOut; InOut.Init(true, Blocks.Num()); // queued check
	for (int32 Iter = 0; Iter < Blocks.Num(); ++Iter)   // bounded propagation
	{
		bool bChanged = false;
		for (int32 i = 0; i < Blocks.Num(); ++i)
		{
			if (Blocks[i].State == EBoxBlockState::Detached ||
				Blocks[i].State == EBoxBlockState::Destroyed) continue;
			if (Blocks[i].Grid.Z == 0) continue; // floor anchor: never falls
			const int32 Below = IndexAt(Blocks[i].Grid.X, Blocks[i].Grid.Y, Blocks[i].Grid.Z - 1);
			const bool bIsCornerSupported = (Blocks[i].Grid.X == 0 || Blocks[i].Grid.X == P.CubesAcrossFace - 1) &&
				(Blocks[i].Grid.Y == 0 || Blocks[i].Grid.Y == P.CubesDepth - 1);
			const bool bSupportGone = (Below == INDEX_NONE);
			if (bIsCornerSupported && bSupportGone)
			{
				// Strawman rule: corner blocks joined to two ortogonal faces
				// may not fall, but a below-support loss without a lateral
				// anchor lets them detach too once their support chain dies.
			}
			if (bSupportGone && !FallingGrace[i])
			{
				FallingGrace[i] = true;
				if (!OutCollapse.Contains(Blocks[i].Id))
				{
					OutCollapse.Add(Blocks[i].Id);
					bChanged = true;
				}
				// Also mark neighbor-above links to re-check next iteration.
			}
		}
		if (!bChanged) break;
	}
}
