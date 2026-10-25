#pragma once
#include "CoreMinimal.h"

namespace Afterfall
{
	/** Enforces N_candidate <= 3 at the candidate-generation boundary. */
	inline int32 ClampCandidateCount(int32 Requested)
	{
		if (Requested < 0) return 0;
		return FMath::Min(Requested, 3);
	}
}
