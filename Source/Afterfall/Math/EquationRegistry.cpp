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
