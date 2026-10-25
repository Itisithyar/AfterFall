#pragma once

#include "Math/PortableEquation.h"

namespace AfterfallMovementEquations
{
	/** Dimensionless response curves. Clamp ranges are owned by the caller. */
	FPortableEquation AccelerationFromLoad();
	FPortableEquation SpeedFromLoad();
	FPortableEquation BrakingFromLoad();
}
