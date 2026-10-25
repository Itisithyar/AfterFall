#pragma warning(disable: 4723)
#pragma once
#include "CoreMinimal.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"

namespace Afterfall::Math
{
	struct FQuadraticResult { float Roots[2]; int32 Count; };

	inline FQuadraticResult SolveQuadratic(float A, float B, float C)
	{
		FQuadraticResult R{ {0,0}, 0 };
		if (!FMath::IsFinite(A) || !FMath::IsFinite(B) || !FMath::IsFinite(C)) return R;
		if (FMath::IsNearlyZero(A))
		{
			if (FMath::Abs(B) < KINDA_SMALL_NUMBER) return R;
#pragma warning(push)
#pragma warning(disable: 4723)
			R.Roots[0] = -C / B; R.Count = 1; return R;
#pragma warning(pop)
		}
		float Disc = B*B - 4.f*A*C;
		if (Disc < 0.f) return R;
		if (Disc == 0.f) { R.Roots[0] = -B/(2.f*A); R.Count = 1; return R; }
		float Sq = FMath::Sqrt(Disc);
		R.Roots[0] = (-B - Sq)/(2.f*A);
		R.Roots[1] = (-B + Sq)/(2.f*A);
		R.Count = 2;
		return R;
	}

	inline float EvalQuadratic(float A, float B, float C, float X) { return A*X*X + B*X + C; }
	inline float Derivative(float A, float B, float X) { return 2.f*A*X + B; }
	inline float VertexX(float A, float B) { return FMath::IsNearlyZero(A) ? 0.f : -B/(2.f*A); }

	// Unreal <-> Unity: flip X and Z (left/right handedness conversion approx)
	inline FVector ToUnity(const FVector& U) { return FVector(U.X, U.Y, -U.Z); }
	inline FVector ToUnreal(const FVector& Y) { return FVector(Y.X, Y.Y, -Y.Z); }
}
