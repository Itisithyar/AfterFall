// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Integration (World) — Phase 1 sampling contract implementation.
// Pure math only: no actors, no streaming, no collision, no materials.
#include "World/TerrainSampling.h"

namespace
{
	constexpr int32 SaltRegion = 101;
	constexpr int32 SaltBroad = 202;
	constexpr int32 SaltHill = 303;
	constexpr int32 SaltDetail = 404;
	constexpr int32 SaltCave = 505;

	inline float SmoothT(float T) { return T * T * (3.0f - 2.0f * T); }
}

uint32 AfterfallTerrain::Hash2(int32 X, int32 Y, int32 Seed, int32 Salt)
{
	// Unsigned arithmetic: wraparound is well-defined, so this is identical
	// on every compiler/platform for the same inputs.
	uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u
		+ (uint32)Seed * 2246822519u + (uint32)Salt * 3266489917u;
	H = (H ^ (H >> 13)) * 1274126177u;
	return H ^ (H >> 16);
}

uint32 AfterfallTerrain::Hash3(int32 X, int32 Y, int32 Z, int32 Seed, int32 Salt)
{
	uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + (uint32)Z * 1440662683u
		+ (uint32)Seed * 2246822519u + (uint32)Salt * 3266489917u;
	H = (H ^ (H >> 13)) * 1274126177u;
	return H ^ (H >> 16);
}

float AfterfallTerrain::ValueNoise2D(float X, float Y, int32 Seed, int32 Salt)
{
	const double Dx = (double)X;
	const double Dy = (double)Y;
	const int32 Ix = FMath::FloorToInt(Dx);
	const int32 Iy = FMath::FloorToInt(Dy);
	const float Fx = (float)(Dx - (double)Ix);
	const float Fy = (float)(Dy - (double)Iy);
	const float Sx = SmoothT(FMath::Clamp(Fx, 0.0f, 1.0f));
	const float Sy = SmoothT(FMath::Clamp(Fy, 0.0f, 1.0f));
	const float A = UnitFromHash(Hash2(Ix, Iy, Seed, Salt));
	const float B = UnitFromHash(Hash2(Ix + 1, Iy, Seed, Salt));
	const float C = UnitFromHash(Hash2(Ix, Iy + 1, Seed, Salt));
	const float D = UnitFromHash(Hash2(Ix + 1, Iy + 1, Seed, Salt));
	return FMath::Lerp(FMath::Lerp(A, B, Sx), FMath::Lerp(C, D, Sx), Sy);
}

float AfterfallTerrain::ValueNoise3D(float X, float Y, float Z, int32 Seed, int32 Salt)
{
	const double Dx = (double)X;
	const double Dy = (double)Y;
	const double Dz = (double)Z;
	const int32 Ix = FMath::FloorToInt(Dx);
	const int32 Iy = FMath::FloorToInt(Dy);
	const int32 Iz = FMath::FloorToInt(Dz);
	const float Fx = FMath::Clamp((float)(Dx - (double)Ix), 0.0f, 1.0f);
	const float Fy = FMath::Clamp((float)(Dy - (double)Iy), 0.0f, 1.0f);
	const float Fz = FMath::Clamp((float)(Dz - (double)Iz), 0.0f, 1.0f);
	const float Sx = SmoothT(Fx);
	const float Sy = SmoothT(Fy);
	const float Sz = SmoothT(Fz);
	const float C000 = UnitFromHash(Hash3(Ix, Iy, Iz, Seed, Salt));
	const float C100 = UnitFromHash(Hash3(Ix + 1, Iy, Iz, Seed, Salt));
	const float C010 = UnitFromHash(Hash3(Ix, Iy + 1, Iz, Seed, Salt));
	const float C110 = UnitFromHash(Hash3(Ix + 1, Iy + 1, Iz, Seed, Salt));
	const float C001 = UnitFromHash(Hash3(Ix, Iy, Iz + 1, Seed, Salt));
	const float C101 = UnitFromHash(Hash3(Ix + 1, Iy, Iz + 1, Seed, Salt));
	const float C011 = UnitFromHash(Hash3(Ix, Iy + 1, Iz + 1, Seed, Salt));
	const float C111 = UnitFromHash(Hash3(Ix + 1, Iy + 1, Iz + 1, Seed, Salt));
	const float X00 = FMath::Lerp(C000, C100, Sx);
	const float X10 = FMath::Lerp(C010, C110, Sx);
	const float X01 = FMath::Lerp(C001, C101, Sx);
	const float X11 = FMath::Lerp(C011, C111, Sx);
	return FMath::Lerp(FMath::Lerp(X00, X10, Sy), FMath::Lerp(X01, X11, Sy), Sz);
}

float AfterfallTerrain::Fbm2(float X, float Y, int32 Seed, int32 Salt, int32 Octaves)
{
	const int32 N = FMath::Clamp(Octaves, 1, 4);
	float Sum = 0.0f;
	float Amp = 0.5f;
	float Freq = 1.0f;
	float Norm = 0.0f;
	for (int32 i = 0; i < N; ++i)
	{
		Sum += Amp * ValueNoise2D(X * Freq, Y * Freq, Seed + i * 7919, Salt);
		Norm += Amp;
		Amp *= 0.5f;
		Freq *= 2.0f;
	}
	return Norm > 0.0f ? Sum / Norm : 0.5f; // [0,1]
}

float AfterfallTerrain::Fbm3(float X, float Y, float Z, int32 Seed, int32 Salt, int32 Octaves)
{
	const int32 N = FMath::Clamp(Octaves, 1, 4);
	float Sum = 0.0f;
	float Amp = 0.5f;
	float Freq = 1.0f;
	float Norm = 0.0f;
	for (int32 i = 0; i < N; ++i)
	{
		Sum += Amp * ValueNoise3D(X * Freq, Y * Freq, Z * Freq, Seed + i * 7919, Salt);
		Norm += Amp;
		Amp *= 0.5f;
		Freq *= 2.0f;
	}
	return Norm > 0.0f ? Sum / Norm : 0.5f; // [0,1]
}

float AfterfallTerrain::RegionMountain(float WorldXcm, float WorldYcm, const FAfterfallTerrainConfig& Cfg)
{
	const float Cell = FMath::Max(1.0f, Cfg.RegionCellCm);
	// Value noise over the coarse region lattice is already C1, so region
	// type blends continuously across cell borders by construction.
	return ValueNoise2D(WorldXcm / Cell, WorldYcm / Cell, Cfg.Seed, SaltRegion);
}

float AfterfallTerrain::SampleHeightCm(float WorldXcm, float WorldYcm, const FAfterfallTerrainConfig& Cfg)
{
	const float M = RegionMountain(WorldXcm, WorldYcm, Cfg);
	const float BroadWave = FMath::Max(1.0f, Cfg.BroadWavelengthCm);
	const float HillWave = FMath::Max(1.0f, Cfg.HillWavelengthCm);
	const float DetailWave = FMath::Max(1.0f, Cfg.DetailWavelengthCm);

	// Broad layer: signed [-1,1] so valleys dip below base and peaks rise.
	const float BroadN = Fbm2(WorldXcm / BroadWave, WorldYcm / BroadWave, Cfg.Seed, SaltBroad, 3);
	const float BroadAmp = FMath::Lerp(Cfg.ValleyAmpCm, Cfg.MountainAmpCm, M);
	const float Broad = BroadAmp * (BroadN * 2.0f - 1.0f);

	// Hills + detail ride on top; detail is small enough to keep traversal smooth.
	const float Hills = Cfg.HillAmpCm * (ValueNoise2D(WorldXcm / HillWave, WorldYcm / HillWave, Cfg.Seed, SaltHill) * 2.0f - 1.0f);
	const float Detail = Cfg.DetailAmpCm * (ValueNoise2D(WorldXcm / DetailWave, WorldYcm / DetailWave, Cfg.Seed, SaltDetail) * 2.0f - 1.0f);

	return Cfg.BaseHeightCm + Broad + Hills + Detail;
}

float AfterfallTerrain::CaveField(float WorldXcm, float WorldYcm, float WorldZcm, const FAfterfallTerrainConfig& Cfg)
{
	const float Wave = FMath::Max(1.0f, Cfg.CaveWavelengthCm);
	return Fbm3(WorldXcm / Wave, WorldYcm / Wave, WorldZcm / Wave, Cfg.Seed, SaltCave, 3);
}

bool AfterfallTerrain::IsSolid(float WorldXcm, float WorldYcm, float WorldZcm, const FAfterfallTerrainConfig& Cfg)
{
	const float H = SampleHeightCm(WorldXcm, WorldYcm, Cfg);
	if (WorldZcm > H)
	{
		return false; // air above the heightfield
	}
	// Cave carve requires BOTH: enough ground cover to keep the surface
	// intact, and depth above the solid floor. Otherwise the heightfield
	// stands as sampled.
	const float Cover = H - WorldZcm;
	if (Cover < Cfg.MinGroundCoverCm)
	{
		return true;
	}
	if (WorldZcm < Cfg.CaveFloorCm)
	{
		return true;
	}
	return CaveField(WorldXcm, WorldYcm, WorldZcm, Cfg) <= Cfg.CaveThreshold;
}
