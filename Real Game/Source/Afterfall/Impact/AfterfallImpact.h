#pragma once
#include <set>

namespace Afterfall
{
struct FImpactResult { bool bApplied; double Energy; double Damage; };

/** Classical kinetic-energy model in SI units: kg * (m/s)^2 -> joules. */
double KineticEnergyJ(double MassKg, double SpeedMps);

/** Idempotent pure model helper; caller owns the committed-event ledger. */
FImpactResult CommitImpact(std::set<long long>& Committed, long long EventId,
    double NormalSpeed, double EffMass, double EtaTransfer, double EResistance);
} // namespace Afterfall
