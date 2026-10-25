// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "BodyRegionPhysics.h"

namespace
{
// 80 kg reference adult; shares sum to 1.0. Values are tunable starting
// points from standard body-segment parameters, NOT hardcoded combat law:
// combat/destruction code must read them through this table.
constexpr float kMassShare[6] = { 0.08f, 0.50f, 0.05f, 0.05f, 0.16f, 0.16f };
constexpr float kGlassBreak[6] = { 60.f, 140.f, 45.f, 45.f, 90.f, 90.f };
}

const TArray<FBodyRegionPhysicsProfile>& UBodyRegionPhysicsLib::GetProfiles()
{
	static const TArray<FBodyRegionPhysicsProfile> P = []()
	{
		TArray<FBodyRegionPhysicsProfile> A;
		const TCHAR* Tags[6] = { TEXT("Head"), TEXT("Spine"), TEXT("HandL"),
			TEXT("HandR"), TEXT("FootL"), TEXT("FootR") };
		// Socket names are placeholders for the skeletal art pass; the tag
		// order matches EBodyPart so PartFromComponent keeps working.
		for (int32 i = 0; i < 6; ++i)
		{
			FBodyRegionPhysicsProfile R;
			R.MassShare = kMassShare[i];
			R.SocketName = Tags[i];
			R.GrabStiffness = (i == 2 || i == 3) ? 1.3f : 1.f;
			R.ImpulseResponse = 1.f;
			R.GlassBreakEnergy = kGlassBreak[i];
			A.Add(R);
		}
		return A;
	}();
	return P;
}

const FBodyRegionPhysicsProfile* UBodyRegionPhysicsLib::FindProfile(EBodyPart Part)
{
	// EBodyPart order (Head=0 .. LegR=5) matches the profile table order.
	const TArray<FBodyRegionPhysicsProfile>& P = GetProfiles();
	const int32 I = static_cast<int32>(Part);
	return P.IsValidIndex(I) ? &P[I] : nullptr;
}

float UBodyRegionPhysicsLib::RegionMassKg(EBodyPart Part, float TotalMassKg)
{
	const FBodyRegionPhysicsProfile* R = FindProfile(Part);
	if (!R || TotalMassKg <= 0.f)
	{
		return 0.f;
	}
	return R->MassShare * TotalMassKg;
}

float UBodyRegionPhysicsLib::ImpulseVelocity(float ImpulseMag, EBodyPart Part, float TotalMassKg)
{
	const float M = RegionMassKg(Part, TotalMassKg);
	if (M <= 0.f)
	{
		return 0.f;
	}
	const FBodyRegionPhysicsProfile* R = FindProfile(Part);
	return ImpulseMag / M * (R ? R->ImpulseResponse : 1.f);
}

bool UBodyRegionPhysicsLib::CanBreakGlass(EBodyPart Part, float TotalMassKg, float SpeedCmS)
{
	// Proxy energy: 1/2 * m * v^2 with v in m/s.
	const float M = RegionMassKg(Part, TotalMassKg);
	if (M <= 0.f || SpeedCmS <= 0.f)
	{
		return false;
	}
	const float V = SpeedCmS / 100.f;
	const float E = 0.5f * M * V * V;
	const FBodyRegionPhysicsProfile* R = FindProfile(Part);
	return R && E >= R->GlassBreakEnergy;
}
