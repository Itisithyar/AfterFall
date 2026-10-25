#pragma once
#include "CoreMinimal.h"
#include "DestructionTypes.h"

namespace AfterfallFracture
{
	// Stable identity for a rhombohedral lattice vertex.
	struct FVertexId
	{
		FIntVector Coord;
		bool operator==(const FVertexId& O) const { return Coord == O.Coord; }
	};

	// Deterministic weak-point attributes derived from vertex coord + seed.
	struct FVertexWeakness
	{
		float Weakness = 0.5f; // 0..1
		float Resistance = 0.5f;
		bool bPotential = false;
	};

	FVertexWeakness ComputeVertexWeakness(const FIntVector& Coord, uint32 Seed, EMaterialType Material);

	// ------------------------------------------------------------------------
	// Quadratic severity response (D(x) = K * x^2). DESIGNED response curve,
	// NOT a physical fracture law — the distinction is a repo rule (see
	// Docs/FALLAFTER_EQUATION_REGISTRY.md, quadratic contract section).
	//
	// Variables and units (equation contract):
	//   x = EnergyJ / ReferenceEnergyJ  — normalized impact severity, dimensionless
	//   D(x) = K * x^2                  — severity score, dimensionless (K gain)
	//   EnergyJ                         — impact energy in joules, already
	//                                     computed by the caller's 0.5*m*v^2
	//                                     (E5). This curve is applied ONCE,
	//                                     here, and never re-applies engine
	//                                     gravity or impulse (contract rule 4).
	//
	// Assumptions: monotone quadratic response; weak impacts stay below the
	// initiation threshold (dent), strong ones propagate fracture. Defaults
	// calibrate to the 80 kg throw table: initiation solves 100x^2 = 64 ->
	// x = 0.8 -> E = 800 J -> 80 kg at v = sqrt(2E/m) ~= 4.47 m/s, i.e.
	// between the table's 4 m/s (640 J, D = 40.96, dent) and 6 m/s
	// (1440 J, D = 207.36, fracture) rows.
	struct FFractureSeverityResponse
	{
		/** Severity x = 1.0 at this energy (joules). */
		float ReferenceEnergyJ = 1000.f;
		/** Response gain K: D(x) = K * x^2. */
		float K = 100.f;
		/** D below this value = dent only, no fracture initiation. */
		float InitiationThreshold = 64.f;
	};

	/** D(x) = K * x^2 for a raw energy (pure; no world state). */
	float SeverityD(const FFractureSeverityResponse& R, float EnergyJ);

	/** Threshold solve x* = sqrt(D_thr / K): the normalized severity at which
	 *  fracture initiates (documented example: sqrt(64/100) = 0.8). */
	float InitiationSeverityX(const FFractureSeverityResponse& R);

	/** True when the impact reaches the initiation threshold. */
	bool InitiatesFracture(const FFractureSeverityResponse& R, float EnergyJ);

	/** Quadratic propagation budget: E_eff = E^2 / E_ref (= E_ref * x^2).
	 *  Identity at E_ref; shrinks quadratically below, grows above. Applied
	 *  exactly once per event, at Solve entry. */
	float EffectivePropagationEnergy(const FFractureSeverityResponse& R, float EnergyJ);

	// ---- Failure utilization (energy demand-to-capacity, dimensionless) ----
	// Inspection finding: the engine has NO stress model anywhere in Source
	// (no sigma/tau, no elastic modulus, no K_IC), so the supported failure
	// criterion is the one the box path already uses: impact energy (demand)
	// vs per-cube break energy (capacity). U = demand / capacity is a
	// demand-to-capacity ratio, NOT a probability of fracture. U >= 1 means
	// the configured capacity is reached or exceeded.
	// Guards: non-finite or non-positive demand -> 0; non-finite or
	// non-positive capacity -> 0 (a zero-strength misconfiguration refuses
	// to initiate instead of shattering everything); result clamped to 1e6
	// against overflow; extreme inputs stay finite and deterministic.
	float FailureUtilization(float DemandJ, float CapacityJ);

	/** Capacity adapter: voxel-path EMaterialType -> the EXISTING box-path
	 *  break-energy table (UBoxBuildingLib::BreakEnergyJ, joules per 50 cm
	 *  cube). Documented cross-map: Stone->Brick, Plaster->Drywall; the
	 *  other three match 1:1. Invents no strength values of its own.
	 *  Unknown material returns 0 -> utilization 0 -> never initiates. */
	float MaterialStrengthJ(EMaterialType Material);

	/** Single tunable severity instance (lab page reads it, Set tunes it). */
	const FFractureSeverityResponse& GetSeverityResponse();
	/** Tuning entry point (lab/console). Non-finite or out-of-range fields
	 *  fall back to the documented defaults, so tuning cannot wedge Solve. */
	void SetSeverityResponse(const FFractureSeverityResponse& R);

	// Fracture propagation result.
	struct FFractureResult
	{
		TArray<FIntVector> InitiationPoints;
		TArray<FIntVector> Path;
		float EnergyConsumed = 0.0f;
		float RemainingEnergy = 0.0f;
		bool bHole = false;
		bool bDisconnected = false;
		int32 Depth = 0;
		/** False when the severity gate or the material-capacity check refused
		 *  initiation: Path is empty, nothing may be removed. */
		bool bInitiated = false;
		/** Demand-to-capacity ratio evaluated for this call (0 when the input
		 *  was invalid or refused). Reported, never used as a probability. */
		float Utilization = 0.f;
	};

	// Solver operating on lattice coords; energy budget.
	FFractureResult Solve(const FIntVector& Start, uint32 Seed, EMaterialType Material,
		float InitialEnergy, int32 MaxDepth, const FVector& LoadDir);
}
