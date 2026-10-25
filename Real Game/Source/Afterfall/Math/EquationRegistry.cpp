#include "Math/EquationRegistry.h"

void UAfterfallEquationRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FAfterfallEquation Terrain; Terrain.ID = TEXT("TerrainHeight"); Terrain.A=0; Terrain.B=0; Terrain.C=1; Equations.Add(Terrain.ID, Terrain);
	FAfterfallEquation Frag; Frag.ID = TEXT("FragmentImpulse"); Frag.A=-1; Frag.B=0; Frag.C=1; Equations.Add(Frag.ID, Frag);
	FAfterfallEquation Shear; Shear.ID = TEXT("RhombusShear"); Shear.A=0; Shear.B=0.15f; Shear.C=0; Equations.Add(Shear.ID, Shear);
	FAfterfallEquation Loot; Loot.ID = TEXT("LootWeight"); Loot.A=0; Loot.B=1; Loot.C=0; Equations.Add(Loot.ID, Loot);
	auto AddEq = [&](const TCHAR* Id, float A, float B, float C) { FAfterfallEquation E; E.ID = Id; E.A=A; E.B=B; E.C=C; Equations.Add(E.ID, E); };
	AddEq(TEXT("RhombusFaces"), 0, -1, 2);
	AddEq(TEXT("RhombusEdges"), 0, -1, 5);
	AddEq(TEXT("RhombusVerts"), 0, 0, 4);
	AddEq(TEXT("RhombusEuler"), 0, 0, 1);
	AddEq(TEXT("RhombusSideError"), 0.0225f, 0, 0);
	AddEq(TEXT("ImpactEnergy"), 0.5f, 0, 0);
	AddEq(TEXT("ImpactSpeedSqFromFall"), 0, 2*9.81f, 0);
	AddEq(TEXT("DebrisHeight"), -9.81f/2.f, 0, 0);
	AddEq(TEXT("DebrisVelocity"), 0, -9.81f, 0);
	AddEq(TEXT("NormalizedDamage"), 0, 1, 0);
	AddEq(TEXT("VoxelMass"), 0, 1.0f, 0);
	AddEq(TEXT("EnergyBudget"), 0, -1.0f, 100.0f);
	AddEq(TEXT("JointDamage"), 0.01667f, 0, 0);
	AddEq(TEXT("GrabSpringForce"), 0, 50.0f, 0);
	AddEq(TEXT("RopeStretchError"), 0, 1, -1.0f);
	AddEq(TEXT("RopeTension"), 0, 200.0f, 0);
	AddEq(TEXT("BloodRemaining"), 0, -10.0f, 5000.0f);
	AddEq(TEXT("PositionIntegration"), 0, 1, 0);
}

FAfterfallEquation UAfterfallEquationRegistry::GetEquation(FName ID) const
{
	if (const FAfterfallEquation* P = Equations.Find(ID)) return *P;
	return FAfterfallEquation();
}

float UAfterfallEquationRegistry::EvalEquation(FName ID, float T) const
{
	return GetEquation(ID).Eval(T);
}
