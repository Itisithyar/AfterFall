#include "Destruction/FractureSolver.h"
#include "Voxel/VoxelBasis.h"
#include "Structural/BoxBuilding.h"
#include "Math/AfterfallMath.h"
#include "Math/PortableEquation.h"
#include "Prediction/BoundedPredictiveMatrix.h"

namespace AfterfallFracture
{
	static float MatResistance(EMaterialType M)
	{
		switch (M)
		{
		case EMaterialType::Wood: return 0.3f;
		case EMaterialType::Plaster: return 0.2f;
		case EMaterialType::Stone: return 0.7f;
		case EMaterialType::Concrete: return 0.85f;
		case EMaterialType::Metal: return 0.95f;
		default: return 0.5f;
		}
	}

	FVertexWeakness ComputeVertexWeakness(const FIntVector& Coord, uint32 Seed, EMaterialType Material)
	{
		// Deterministic hash from coord + seed.
		uint32 h = Seed;
		h ^= (uint32)(Coord.X * 73856093);
		h ^= (uint32)(Coord.Y * 19349663);
		h ^= (uint32)(Coord.Z * 83492791);
		h = h * 2654435761u;
		const float r = (float)(h & 0xFFFF) / 65535.0f;
		FVertexWeakness W;
		W.Weakness = r;
		W.Resistance = MatResistance(Material) * (1.0f - 0.3f * r);
		W.bPotential = r > 0.65f;
		return W;
	}

	namespace
	{
		// Single tunable severity instance. Defaults are the documented
		// contract (E_ref = 1000 J, K = 100, D_thr = 64 -> x* = 0.8).
		FFractureSeverityResponse GSeverityResponse;

		// D(x) = K*x^2 exported as the SAME portable quadratic definition the
		// lab page, the automation tests and the Unity adapter read. One
		// definition per equation (repo rule), evaluated in ONE place.
		FPortableEquation PortableSeverity(const FFractureSeverityResponse& R)
		{
			return Afterfall::Portable::MakeQuadratic(R.K);
		}
	}

	const FFractureSeverityResponse& GetSeverityResponse()
	{
		return GSeverityResponse;
	}

	void SetSeverityResponse(const FFractureSeverityResponse& R)
	{
		const FFractureSeverityResponse Def;
		GSeverityResponse = R;
		if (!FMath::IsFinite(GSeverityResponse.ReferenceEnergyJ) || GSeverityResponse.ReferenceEnergyJ < 1.f)
		{
			GSeverityResponse.ReferenceEnergyJ = Def.ReferenceEnergyJ;
		}
		if (!FMath::IsFinite(GSeverityResponse.K) || GSeverityResponse.K <= 0.f)
		{
			GSeverityResponse.K = Def.K;
		}
		if (!FMath::IsFinite(GSeverityResponse.InitiationThreshold) || GSeverityResponse.InitiationThreshold < 0.f)
		{
			GSeverityResponse.InitiationThreshold = Def.InitiationThreshold;
		}
	}

	float FailureUtilization(float DemandJ, float CapacityJ)
	{
		// Reject invalid inputs explicitly (contract: no NaN/Inf into math).
		if (!FMath::IsFinite(DemandJ) || !FMath::IsFinite(CapacityJ))
		{
			return 0.f;
		}
		if (DemandJ <= 0.f || CapacityJ <= 0.f)
		{
			// Zero/negative demand -> no utilization. Zero/negative capacity
			// -> 0 as well: U >= 1 can never hold, so a misconfigured
			// zero-strength material refuses to fracture rather than always
			// fracturing (documented fail-safe direction).
			return 0.f;
		}
		const float U = DemandJ / CapacityJ;
		// Overflow guard: huge-but-finite demand against tiny capacity.
		return FMath::IsFinite(U) ? FMath::Min(U, 1.e6f) : 1.e6f;
	}

	float MaterialStrengthJ(EMaterialType Material)
	{
		// Cross-map onto the EXISTING box-path break-energy table
		// (BoxBuilding.cpp BoxDefaults::BreakEnergyJ, J per 50 cm cube).
		// No strength values are invented here; unknown -> 0 -> utilization
		// 0 -> initiation refused (report, don't guess).
		switch (Material)
		{
		case EMaterialType::Wood:     return UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Wood);
		case EMaterialType::Stone:    return UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Brick);
		case EMaterialType::Metal:    return UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Metal);
		case EMaterialType::Concrete: return UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Concrete);
		case EMaterialType::Plaster:  return UBoxBuildingLib::BreakEnergyJ(EBoxBlockMaterial::Drywall);
		default: break;
		}
		return 0.f;
	}

	float SeverityD(const FFractureSeverityResponse& R, float EnergyJ)
	{
		const float X = FMath::Max(0.f, EnergyJ) / FMath::Max(1.f, R.ReferenceEnergyJ);
		return Afterfall::Portable::Eval(PortableSeverity(R), X);
	}

	float InitiationSeverityX(const FFractureSeverityResponse& R)
	{
		// D(x) = D_thr  ->  K*x^2 - D_thr = 0, solved by the SHARED quadratic
		// solver (Math/AfterfallMath.h). x is a non-negative impact magnitude
		// by definition, so only a non-negative root is accepted.
		const Afterfall::Math::FQuadraticResult Sol =
			Afterfall::Math::SolveQuadratic(R.K, 0.f, -R.InitiationThreshold);
		for (int32 i = 0; i < Sol.Count; ++i)
		{
			if (Sol.Roots[i] >= 0.f && FMath::IsFinite(Sol.Roots[i]))
			{
				return Sol.Roots[i];
			}
		}
		// Degenerate tuning (the x^2 term vanishes, |K| below the solver's
		// zero threshold): keep the documented sqrt form so a near-zero gain
		// still resolves instead of wedging the gate at 0.
		return FMath::Sqrt(FMath::Max(0.f, R.InitiationThreshold) / FMath::Max(KINDA_SMALL_NUMBER, R.K));
	}

	bool InitiatesFracture(const FFractureSeverityResponse& R, float EnergyJ)
	{
		return SeverityD(R, EnergyJ) >= R.InitiationThreshold;
	}

	float EffectivePropagationEnergy(const FFractureSeverityResponse& R, float EnergyJ)
	{
		// E_eff = E_ref * x^2 == E^2 / E_ref. Quadratic response on the
		// already-computed impact energy; identity at the reference point.
		const float Ref = FMath::Max(1.f, R.ReferenceEnergyJ);
		const float E = FMath::Max(0.f, EnergyJ);
		return E * E / Ref;
	}

	FFractureResult Solve(const FIntVector& Start, uint32 Seed, EMaterialType Material,
		float InitialEnergy, int32 MaxDepth, const FVector& LoadDir)
	{
		FFractureResult R;
		R.RemainingEnergy = InitialEnergy;

		// ---- Milestone chain: material strength -> failure utilization ->
		// quadratic severity -> destruction output (one application point).
		//
		// 1. Demand: InitialEnergy (J), already computed by the caller's
		//    0.5*m*v^2 (Q1) — never re-derived here.
		// 2. Capacity: MaterialStrengthJ (existing box break-energy table).
		// 3. Utilization: U = demand/capacity, reported on the result.
		// 4. Severity: existing D(x) = K*x^2 gate (contract preserved).
		// 5. Initiation requires BOTH criteria as SEPARATE explicit checks —
		//    severity >= threshold AND capacity reached (U >= 1). They are not
		//    blended into a new formula, so neither double-counts the other.
		//    The budget walk below still scales per-step cost by MatResistance,
		//    so material affects output twice in different roles: gates
		//    initiation, prices propagation — never two gates on the same term.
		const FFractureSeverityResponse& Response = GetSeverityResponse();
		R.Utilization = FailureUtilization(InitialEnergy, MaterialStrengthJ(Material));
		if (!InitiatesFracture(Response, InitialEnergy) || R.Utilization < 1.f)
		{
			return R; // refused: dent only — empty path, no cell removal
		}
		const float Budget = EffectivePropagationEnergy(Response, InitialEnergy);

		// neighbor offsets in lattice coords
		const FIntVector Neigh[6] = {
			FIntVector(1,0,0), FIntVector(-1,0,0),
			FIntVector(0,1,0), FIntVector(0,-1,0),
			FIntVector(0,0,1), FIntVector(0,0,-1)
		};
		const int32 MaxCandidates = Afterfall::ClampCandidateCount(INT32_MAX);
		TSet<FIntVector> Visited;
		TArray<FIntVector> Frontier;
		Frontier.Add(Start);
		Visited.Add(Start);
		R.InitiationPoints.Add(Start);
		R.bInitiated = true;
		const float BaseResist = MatResistance(Material);
		float Energy = Budget;
		int32 Depth = 0;
		while (Frontier.Num() > 0 && Energy > 0 && Depth < MaxDepth)
		{
			TArray<FIntVector> Next;
			for (const FIntVector& Cur : Frontier)
			{
				for (int i = 0; i < 6; ++i)
				{
					FIntVector N = Cur + Neigh[i];
					if (Visited.Contains(N)) continue;
					// Bounded predictive matrix: at most N_candidate (=3) cells are
					// generated per prediction step, so the predicted spread and the
					// per-step solver work stay bounded (gameplay call site of
					// Prediction/BoundedPredictiveMatrix.h).
					if (Next.Num() >= MaxCandidates) continue;
					Visited.Add(N);
					const FVertexWeakness W = ComputeVertexWeakness(N, Seed, Material);
					// cost: resistance + directional bias
					FVector NWorld = FVoxelBasis::VoxelToWorld(N, FVector::ZeroVector);
					FVector CurWorld = FVoxelBasis::VoxelToWorld(Cur, FVector::ZeroVector);
					FVector Dir = (NWorld - CurWorld).GetSafeNormal();
					float Dot = FVector::DotProduct(Dir, LoadDir.GetSafeNormal());
					float Cost = W.Resistance * BaseResist * 10.0f * (1.0f - 0.3f * FMath::Max(0.0f, Dot));
					if (Cost > Energy) continue;
					Energy -= Cost;
					R.Path.Add(N);
					R.EnergyConsumed += Cost;
					Next.Add(N);
					// branching: weak points branch
					if (W.bPotential && Energy > Cost * 2.0f && Next.Num() < MaxCandidates)
					{
						// add lateral neighbor
						FIntVector L = N + Neigh[(i + 2) % 6];
						if (!Visited.Contains(L))
						{
							Visited.Add(L);
							Next.Add(L);
						}
					}
				}
			}
			Frontier = Next;
			++Depth;
		}
		R.RemainingEnergy = Energy;
		R.Depth = Depth;
		// hole/disconnect heuristic: path surrounds origin
		if (R.Path.Num() > 8) R.bHole = true;
		if (Depth >= MaxDepth && Energy <= 0) R.bDisconnected = true;
		return R;
	}
}
