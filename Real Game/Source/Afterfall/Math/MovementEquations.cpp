#include "Math/MovementEquations.h"

namespace AfterfallMovementEquations
{
	FPortableEquation AccelerationFromLoad()
	{
		return Afterfall::Portable::Linear(-0.15f, 1.f);
	}

	FPortableEquation SpeedFromLoad()
	{
		return Afterfall::Portable::Linear(-0.08f, 1.f);
	}

	FPortableEquation BrakingFromLoad()
	{
		return Afterfall::Portable::Linear(-0.10f, 1.f);
	}
}
