#pragma once
#include <set>
#include <cmath>

namespace Afterfall
{
struct FImpactResult { bool bApplied; double Energy; double Damage; };

inline FImpactResult CommitImpact(std::set<long long>& Committed, long long EventId,
    double NormalSpeed, double EffMass, double EtaTransfer, double EResistance)
{
    FImpactResult R{false,0,0};
    if (Committed.count(EventId)) return R;
    if (EResistance <= 0.0) return R;
    R.Energy = 0.5 * EffMass * NormalSpeed * NormalSpeed;
    double Available = R.Energy * EtaTransfer;
    R.Damage = Available / EResistance;
    Committed.insert(EventId);
    R.bApplied = true;
    return R;
}
} // namespace Afterfall