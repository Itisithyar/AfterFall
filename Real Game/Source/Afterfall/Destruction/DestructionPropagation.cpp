// Copyright (c) 2026 AFTERFALL. All rights reserved.
#include "DestructionPropagation.h"

FVector UDestructionPropagationService::GridPoint(const FVector& Center, float ExtentCm,
	int32 IX, int32 IY, int32 IZ)
{
	const int32 CX = FMath::Clamp(IX, 0, 2) - 1;
	const int32 CY = FMath::Clamp(IY, 0, 2) - 1;
	const int32 CZ = FMath::Clamp(IZ, 0, 2) - 1;
	return Center + FVector(CX * ExtentCm, CY * ExtentCm, CZ * ExtentCm);
}

float UDestructionPropagationService::MaterialResistancePerCm(FName MaterialId)
{
	// Placeholder linear rule, explicitly documented: energy debit per cm.
	// Real curves move to data (DT_Materials) once design locks values.
	static const TMap<FName, float> Table = {
		{ TEXT("Glass"), 0.15f },
		{ TEXT("Wood"), 0.40f },
		{ TEXT("Concrete"), 1.20f },
		{ TEXT("Metal"), 1.60f },
	};
	if (const float* F = Table.Find(MaterialId))
	{
		return *F;
	}
	return 0.60f; // unknown material: conservative default, logged by caller
}

float UDestructionPropagationService::InitiatorBreakpointCost(EDestructionInitiator Initiator)
{
	switch (Initiator)
	{
	case EDestructionInitiator::Bullet:          return 8.f;
	case EDestructionInitiator::Attack:          return 18.f;
	case EDestructionInitiator::MaterialFailure: return 12.f;
	}
	return 18.f;
}

float UDestructionPropagationService::InitiatorReachCm(EDestructionInitiator Initiator)
{
	switch (Initiator)
	{
	case EDestructionInitiator::Bullet:          return 300.f;
	case EDestructionInitiator::Attack:          return 120.f;
	case EDestructionInitiator::MaterialFailure: return 150.f;
	}
	return 120.f;
}

bool UDestructionPropagationService::NextBreakpoint(const FVector& Center, float ExtentCm,
	const FVector& CurrentPos, const FVector& Direction, const TArray<int32>& Visited,
	float ReachCm, int32& OutIndex, FVector& OutPos)
{
	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero() || ReachCm <= 0.f)
	{
		return false;
	}
	bool bFound = false;
	float BestCost = 0.f;
	int32 BestIdx = INDEX_NONE;
	FVector BestPos = FVector::ZeroVector;
	for (int32 IZ = 0; IZ < 3; ++IZ)
	{
		for (int32 IY = 0; IY < 3; ++IY)
		{
			for (int32 IX = 0; IX < 3; ++IX)
			{
				const int32 Idx = GridIndex(IX, IY, IZ);
				if (Visited.Contains(Idx))
				{
					continue;
				}
				const FVector P = GridPoint(Center, ExtentCm, IX, IY, IZ);
				const FVector To = P - CurrentPos;
				const float Proj = FVector::DotProduct(To, Dir);
				if (Proj <= 0.01f || Proj > ReachCm)
				{
					continue; // behind us or out of reach: not reachable
				}
				// Nearest forward point first: damage enters at the surface and
				// advances inward (directional maze, not radial, not deepest-first
				// — jumping to the far side would skip surface cells and deny
				// legitimate impacts that can only afford the near cells).
				const float Lateral = (To - Dir * Proj).Size();
				const float Cost = Proj + Lateral * 0.5f;
				if (!bFound || Cost < BestCost)
				{
					bFound = true;
					BestCost = Cost;
					BestIdx = Idx;
					BestPos = P;
				}
			}
		}
	}
	if (!bFound)
	{
		return false;
	}
	OutIndex = BestIdx;
	OutPos = BestPos;
	return true;
}

FPropagationResult UDestructionPropagationService::Propagate(const FDestructionEvent& Event,
	const FVector& Center, float ExtentCm, int32 MaxSteps, float MinEnergy)
{
	FPropagationResult R;
	const int32 Steps = FMath::Clamp(MaxSteps, 1, 32);
	const FVector Dir = Event.Direction.GetSafeNormal();
	if (Dir.IsNearlyZero() || Event.InitialEnergy <= 0.f)
	{
		R.bDenied = true;
		R.bExhausted = true;
		return R; // invalid initiators never detach
	}

	float Energy = Event.InitialEnergy;
	const float Resistance = MaterialResistancePerCm(Event.MaterialId);
	const float BreakCost = InitiatorBreakpointCost(Event.Initiator);
	const float Reach = InitiatorReachCm(Event.Initiator);
	FVector Cursor = Event.Origin;
	float Travelled = 0.f;
	TArray<int32> Visited;

	for (int32 i = 0; i < Steps; ++i)
	{
		int32 Idx = INDEX_NONE;
		FVector Next = FVector::ZeroVector;
		if (!NextBreakpoint(Center, ExtentCm, Cursor, Dir, Visited, Reach, Idx, Next))
		{
			break; // valid stop: no reachable breakpoint
		}
		const float SegLen = FVector::Dist(Cursor, Next);
		Travelled += SegLen;
		// Explicit linear distance rule: debit = resistance * cm + breakpoint cost.
		const float Cost = Resistance * SegLen + BreakCost;
		if (Cost > Energy)
		{
			FPropagationStep S;
			S.Position = Next;
			S.BreakpointIndex = Idx;
			S.EnergyRemaining = Energy;
			S.DistanceTraversedCm = Travelled;
			S.bDenied = true;
			R.Steps.Add(S);
			R.bDenied = true;
			R.bExhausted = true;
			break;
		}
		Energy -= Cost;
		Visited.Add(Idx);
		FPropagationStep S;
		S.Position = Next;
		S.BreakpointIndex = Idx;
		S.EnergyRemaining = Energy;
		S.DistanceTraversedCm = Travelled;
		S.bDetached = true;
		R.Steps.Add(S);
		R.DetachedBreakpoints.Add(Idx);
		Cursor = Next;
		if (Energy < MinEnergy)
		{
			R.bExhausted = true;
			break;
		}
	}
	R.EnergyRemaining = Energy;
	R.bExhausted = R.DetachedBreakpoints.Num() == 0 ? true : (Energy < MinEnergy || R.bDenied || R.DetachedBreakpoints.Num() < Steps);
	if (R.Steps.Num() == 0)
	{
		R.bExhausted = true;
	}
	return R;
}

FName UDestructionPropagationService::MaterialForPart(EStructuralPart Part)
{
	switch (Part)
	{
	case EStructuralPart::Foundation: return TEXT("Concrete");
	case EStructuralPart::Window:      return TEXT("Glass");
	case EStructuralPart::Lock:        return TEXT("Metal");
	default:                           return TEXT("Wood");
	}
}

bool UDestructionPropagationService::AddDetachedCell(TArray<int32>& InOutDetached, int32 Cell)
{
	if (Cell < 0 || Cell >= GridPointCount())
	{
		return false;
	}
	if (InOutDetached.Contains(Cell))
	{
		return false; // duplicate replay: no double commit
	}
	InOutDetached.Add(Cell);
	return true;
}

int32 UDestructionPropagationService::MergeDetached(TArray<int32>& InOutDetached, const TArray<int32>& NewCells)
{
	int32 Added = 0;
	for (int32 C : NewCells)
	{
		if (AddDetachedCell(InOutDetached, C))
		{
			++Added;
		}
	}
	return Added;
}
