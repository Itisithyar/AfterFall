#include "Misc/AutomationTest.h"
#include "Voxel/VoxelBasis.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBasisTest, "Afterfall.Voxel.Basis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBasisTest::RunTest(const FString& Parameters)
{
	// Basis vector lengths
	TestTrue("A length", FMath::IsNearlyEqual(FVoxelBasis::A().Size(), 3.06f, 0.001f));
	TestTrue("B length", FMath::IsNearlyEqual(FVoxelBasis::B().Size(), 3.06f, 0.001f));
	TestTrue("C length", FMath::IsNearlyEqual(FVoxelBasis::C().Size(), 3.06f, 0.001f));
	// Pairwise angles 60
	float cosAB = FVector::DotProduct(FVoxelBasis::A(), FVoxelBasis::B()) / (3.06f*3.06f);
	TestTrue("AB angle", FMath::IsNearlyEqual(cosAB, 0.5f, 0.001f));
	// Round trip
	FVector origin = FVector::ZeroVector;
	FIntVector v(1,2,3);
	FVector w = FVoxelBasis::VoxelToWorld(v, origin);
	FIntVector v2 = FVoxelBasis::WorldToVoxel(w, origin);
	TestEqual("roundtrip X", v2.X, v.X);
	TestEqual("roundtrip Y", v2.Y, v.Y);
	TestEqual("roundtrip Z", v2.Z, v.Z);
	// Negative floor
	FIntVector vn = FVoxelBasis::WorldToVoxel(FVector(-1,-1,-1), origin);
	TestTrue("negative floor", vn.X <= 0 && vn.Y <= 0 && vn.Z <= 0);
	// Chunk
	FIntVector chunk = FVoxelBasis::VoxelToChunk(FIntVector(33, -1, 0));
	TestEqual("chunk X", chunk.X, 1);
	TestEqual("chunk Y", chunk.Y, -1);
	FIntVector loc = FVoxelBasis::ChunkLocal(FIntVector(33, -1, 0));
	TestEqual("local X", loc.X, 1);
	TestEqual("local Y", loc.Y, 31);
	return true;
}