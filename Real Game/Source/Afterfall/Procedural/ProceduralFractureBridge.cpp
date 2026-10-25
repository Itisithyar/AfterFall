// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "Procedural/ProceduralFractureBridge.h"
#include "Math/AfterfallEquations.h"

namespace AfterfallProcedural
{
	FImpactPlan BuildImpactPlan(const FImpactRequest& Request)
	{
		FImpactPlan Plan;
		if (!FMath::IsFinite(Request.MassKg) || Request.MassKg <= 0.0f
			|| !FMath::IsFinite(Request.ImpactVelocityMps.X)
			|| !FMath::IsFinite(Request.ImpactVelocityMps.Y)
			|| !FMath::IsFinite(Request.ImpactVelocityMps.Z)
			|| Request.MaxDepth <= 0)
		{
			return Plan;
		}

		const double SpeedMps = Request.ImpactVelocityMps.Size();
		const double EnergyJ = Request.MassKg
			* Afterfall::Eval(Afterfall::ImpactEnergy, SpeedMps);
		if (!std::isfinite(EnergyJ) || EnergyJ <= 0.0
			|| EnergyJ > static_cast<double>(MAX_flt))
		{
			return Plan;
		}

		Plan.IsValidInput = true;
		Plan.ImpactEnergyJ = static_cast<float>(EnergyJ);
		Plan.Fracture = AfterfallFracture::Solve(
			Request.StartCell,
			Request.Seed,
			Request.Material,
			Plan.ImpactEnergyJ,
			FMath::Clamp(Request.MaxDepth, 1, 64),
			Request.ImpactVelocityMps.GetSafeNormal());
		return Plan;
	}
}
