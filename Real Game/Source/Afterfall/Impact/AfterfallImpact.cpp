#include "Impact/AfterfallImpact.h"
#include "Math/AfterfallEquations.h"
#include <cmath>

namespace Afterfall
{
double KineticEnergyJ(double MassKg, double SpeedMps)
{
	if (!std::isfinite(MassKg) || !std::isfinite(SpeedMps) || MassKg <= 0.0)
	{
		return 0.0;
	}
	return MassKg * Eval(ImpactEnergy, SpeedMps);
}

FImpactResult CommitImpact(std::set<long long>& Committed, long long EventId,
	double NormalSpeed, double EffMass, double EtaTransfer, double EResistance)
{
	FImpactResult R{false, 0.0, 0.0};
	if (Committed.count(EventId) || !std::isfinite(EtaTransfer)
		|| !std::isfinite(EResistance) || EResistance <= 0.0
		|| !std::isfinite(NormalSpeed) || !std::isfinite(EffMass) || EffMass <= 0.0)
	{
		return R;
	}
	R.Energy = KineticEnergyJ(EffMass, NormalSpeed);
	const double SafeTransfer = EtaTransfer < 0.0 ? 0.0 : (EtaTransfer > 1.0 ? 1.0 : EtaTransfer);
	const double Available = R.Energy * SafeTransfer;
	R.Damage = Available / EResistance;
	Committed.insert(EventId);
	R.bApplied = true;
	return R;
}
}
