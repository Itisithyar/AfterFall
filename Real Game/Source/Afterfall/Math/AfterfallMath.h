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

	struct FQuad { FVector V[4]; };

	inline bool CanMergeTriangles(const FVector& A0, const FVector& A1, const FVector& A2,
		const FVector& B0, const FVector& B1, const FVector& B2, float CoplanarTol = 0.01f)
	{
		TArray<FVector> TA = { A0, A1, A2 };
		TArray<FVector> TB = { B0, B1, B2 };
		int match1 = -1, match2 = -1;
		for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
			if (TA[i].Equals(TB[j], 0.001f)) { if (match1 == -1) match1 = i; else if (match2 == -1) match2 = i; }
		if (match1 < 0 || match2 < 0) return false;
		int apexA = 3 - match1 - match2; // since 0+1+2=3
		int apexB = -1;
		for (int j = 0; j < 3; j++)
		{
			bool shared = false;
			for (int i : { match1, match2 }) if (TA[i].Equals(TB[j], 0.001f)) shared = true;
			if (!shared) { apexB = j; break; }
		}
		if (apexB < 0) return false;
		FVector AApex = TA[apexA];
		FVector BApex = TB[apexB];
		FVector S0 = TA[match1];
		FVector S1 = TA[match2];
		FVector Edge = S1 - S0;
		FVector N = FVector::CrossProduct(Edge, AApex - S0);
		float Vol = FVector::DotProduct(BApex - S0, N);
		if (FMath::Abs(Vol) > CoplanarTol) return false;
		float SA = FVector::DotProduct(FVector::CrossProduct(Edge, AApex - S0), N);
		float SB = FVector::DotProduct(FVector::CrossProduct(Edge, BApex - S0), N);
		// Convexity: segment between apexes must cross the shared edge.
		auto Cross2D = [](const FVector& O, const FVector& A, const FVector& B) {
			return (A - O).X * (B - O).Y - (A - O).Y * (B - O).X;
		};
		float d1 = Cross2D(S0, S1, AApex);
		float d2 = Cross2D(S0, S1, BApex);
		float d3 = Cross2D(AApex, BApex, S0);
		float d4 = Cross2D(AApex, BApex, S1);
		if (!(d1 * d2 < 0 && d3 * d4 < 0)) return false;
		return true;
	}

	inline bool MergeTrianglesToQuad(const FVector& A0, const FVector& A1, const FVector& A2,
		const FVector& B0, const FVector& B1, const FVector& B2, FQuad& Out, float CoplanarTol = 0.01f)
	{
		if (!CanMergeTriangles(A0, A1, A2, B0, B1, B2, CoplanarTol)) return false;
		TArray<FVector> TA = { A0, A1, A2 };
		TArray<FVector> TB = { B0, B1, B2 };
		int match1 = -1, match2 = -1;
		for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
			if (TA[i].Equals(TB[j], 0.001f)) { if (match1 == -1) match1 = i; else if (match2 == -1) match2 = i; }
		int apexA = 3 - match1 - match2;
		int apexB = -1;
		for (int j = 0; j < 3; j++)
		{
			bool shared = false;
			for (int i : { match1, match2 }) if (TA[i].Equals(TB[j], 0.001f)) shared = true;
			if (!shared) { apexB = j; break; }
		}
		Out.V[0] = TA[match1];
		Out.V[1] = TA[apexA];
		Out.V[2] = TA[match2];
		Out.V[3] = TB[apexB];
		return true;
	}

	inline bool IsRhombus(const FQuad& Q, float Tol = 0.001f)
	{
		float s0 = (Q.V[1] - Q.V[0]).Size();
		float s1 = (Q.V[2] - Q.V[1]).Size();
		float s2 = (Q.V[3] - Q.V[2]).Size();
		float s3 = (Q.V[0] - Q.V[3]).Size();
		return FMath::IsNearlyEqual(s0, s1, Tol) && FMath::IsNearlyEqual(s1, s2, Tol) && FMath::IsNearlyEqual(s2, s3, Tol);
	}

	// Unreal <-> Unity: flip X and Z (left/right handedness conversion approx)
	inline FVector ToUnity(const FVector& U) { return FVector(U.X, U.Y, -U.Z); }
	inline FVector ToUnreal(const FVector& Y) { return FVector(Y.X, Y.Y, -Y.Z); }
}
