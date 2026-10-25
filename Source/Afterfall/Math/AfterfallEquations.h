// AfterfallEquations.h - engine-independent equation math (no UE types).
#pragma once
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <vector>

namespace Afterfall
{
constexpr double G = 9.81;
constexpr double DT = 1.0 / 60.0;
constexpr double RHO = 1.0;
constexpr double LOSS = 1.0;
constexpr double E_IN = 100.0;
constexpr double V0_BLOOD = 5000.0;
constexpr double BLEED = 10.0;
constexpr double K_GRAB = 50.0;
constexpr double REST_LEN = 1.0;
constexpr double K_ROPE = 200.0;

struct FQuad { double A, B, C; };
inline double Eval(const FQuad& Q, double T) { return Q.A * T * T + Q.B * T + Q.C; }

constexpr FQuad TerrainHeight{0, 0, 1};
constexpr FQuad FragmentImpulse{-1, 0, 1};
constexpr FQuad RhombusShear{0, 0.15, 0};
constexpr FQuad LootWeight{0, 1, 0};
constexpr FQuad RhombusFaces{0, -1, 2};
constexpr FQuad RhombusEdges{0, -1, 5};
constexpr FQuad RhombusVerts{0, 0, 4};
constexpr FQuad RhombusEuler{0, 0, 1};
constexpr FQuad RhombusSideError{0.0225, 0, 0};
constexpr FQuad ImpactEnergy{0.5, 0, 0};
constexpr FQuad ImpactSpeedSqFromFall{0, 2 * G, 0};
constexpr FQuad DebrisHeight{-G / 2, 0, 0};
constexpr FQuad DebrisVelocity{0, -G, 0};
constexpr FQuad NormalizedDamage{0, 1, 0};
constexpr FQuad VoxelMass{0, RHO, 0};
constexpr FQuad EnergyBudget{0, -LOSS, E_IN};
constexpr FQuad JointDamage{DT, 0, 0};
constexpr FQuad GrabSpringForce{0, K_GRAB, 0};
constexpr FQuad RopeStretchError{0, 1, -REST_LEN};
constexpr FQuad RopeTension{0, K_ROPE, 0};
constexpr FQuad BloodRemaining{0, -BLEED, V0_BLOOD};
constexpr FQuad PositionIntegration{0, 1, 0};

inline int ApplyEvent(std::set<long long>& Committed, long long EventId)
{
    return Committed.insert(EventId).second ? 1 : 0;
}

inline std::array<double, 3> ClampMagnitude(std::array<double, 3> V, double FMax)
{
    const double N = std::sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
    if (N <= FMax || N == 0.0) return V;
    const double S = FMax / N;
    return {V[0] * S, V[1] * S, V[2] * S};
}

inline bool EulerClosedOk(int V, int E, int F) { return V - E + F == 2; }

inline std::vector<int> MergeFaces(const std::vector<int>& F1, const std::vector<int>& F2)
{
    std::vector<int> Q(F1.size());
    for (size_t i = 0; i < F1.size(); ++i) Q[i] = F1[i] ^ F2[i];
    return Q;
}

inline std::vector<double> SolveLinear(std::vector<std::vector<double>> K, std::vector<double> f)
{
    const size_t n = f.size();
    for (size_t i = 0; i < n; ++i) K[i].push_back(f[i]);
    for (size_t i = 0; i < n; ++i)
    {
        size_t p = i;
        for (size_t r = i + 1; r < n; ++r)
            if (std::fabs(K[r][i]) > std::fabs(K[p][i])) p = r;
        if (std::fabs(K[p][i]) < 1e-12) throw std::runtime_error("singular");
        std::swap(K[i], K[p]);
        for (size_t r = i + 1; r < n; ++r)
        {
            const double k = K[r][i] / K[i][i];
            for (size_t c = i; c <= n; ++c) K[r][c] -= k * K[i][c];
        }
    }
    std::vector<double> u(n, 0.0);
    for (size_t ii = n; ii-- > 0;)
    {
        double s = K[ii][n];
        for (size_t j = ii + 1; j < n; ++j) s -= K[ii][j] * u[j];
        u[ii] = s / K[ii][ii];
    }
    return u;
}
} // namespace Afterfall