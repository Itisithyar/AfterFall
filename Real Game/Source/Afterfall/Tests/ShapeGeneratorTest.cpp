// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Tests) — acceptance for the universal shape generator.
// Run in editor: Session Frontend > Automation > Afterfall.ShapeGenerator.
// Headless: UnrealEditor-Cmd.exe RealGame.uproject -ExecCmds="Automation RunTests Afterfall.ShapeGenerator" -unattended -nopause -nullrhi -log
// Pattern matches PlayablePawnTest: throwaway Game world, SpawnActor, explicit
// GenerateShape32, DestroyWorld. No map, no net driver, no gameplay state.
#include "Misc/AutomationTest.h"
#include "Procedural/ShapeGenerator32.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Validates the live render buffers, not just the counters: every index
	// in range, every position finite, triangle count consistent.
	bool ValidateSectionBuffers(FAutomationTestBase* Test, const FString& Label, AShapeGenerator32* Gen)
	{
		if (!Test->TestNotNull(FString(Label) + TEXT(" procmesh"), Gen ? Gen->ProcMesh : nullptr))
		{
			return false;
		}
		if (!Test->TestTrue(FString(Label) + TEXT(" has section"), Gen->ProcMesh->GetNumSections() >= 1))
		{
			return false;
		}
		const FProcMeshSection* Sec = Gen->ProcMesh->GetProcMeshSection(0);
		if (!Test->TestNotNull(FString(Label) + TEXT(" section ptr"), Sec))
		{
			return false;
		}
		const int32 NumVerts = Sec->ProcVertexBuffer.Num();
		Test->TestTrue(FString(Label) + TEXT(" section non-empty"), NumVerts > 0);
		Test->TestTrue(FString(Label) + TEXT(" index count multiple of 3"), Sec->ProcIndexBuffer.Num() % 3 == 0);
		bool bIndicesOk = true;
		for (uint32 Idx : Sec->ProcIndexBuffer)
		{
			if ((int32)Idx >= NumVerts)
			{
				bIndicesOk = false;
				break;
			}
		}
		Test->TestTrue(FString(Label) + TEXT(" all indices in range"), bIndicesOk);
		bool bFinite = true;
		for (const FProcMeshVertex& V : Sec->ProcVertexBuffer)
		{
			if (!FMath::IsFinite(V.Position.X) || !FMath::IsFinite(V.Position.Y) || !FMath::IsFinite(V.Position.Z))
			{
				bFinite = false;
				break;
			}
		}
		Test->TestTrue(FString(Label) + TEXT(" all positions finite"), bFinite);
		return NumVerts > 0 && bIndicesOk && bFinite;
	}

	AShapeGenerator32* SpawnTestGenerator(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AShapeGenerator32* Gen = World->SpawnActor<AShapeGenerator32>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		if (Gen)
		{
			// Explicit generation only; BeginPlay has not run in this world.
			Gen->bGenerateOnBeginPlay = false;
		}
		return Gen;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeGeneratorSphereBothModesTest, "Afterfall.ShapeGenerator.SphereBothModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShapeGeneratorSphereBothModesTest::RunTest(const FString& P)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world created"), World))
	{
		return false;
	}
	AShapeGenerator32* Gen = SpawnTestGenerator(World);
	if (!TestNotNull(TEXT("generator spawned"), Gen))
	{
		World->DestroyWorld(false);
		return false;
	}

	// Deterministic pure sphere: no noise, radius fits inside the grid
	// (grid extent is (32-1)/2*10 = 155, radius 120 -> full sphere visible).
	Gen->ShapeType = EShapeType32::Sphere;
	Gen->GridSize = 32;
	Gen->CellSize = 10.0f;
	Gen->Radius = 120.0f;
	Gen->NoiseStrength = 0.0f;
	Gen->DeformStrength = 0.0f;
	Gen->Center = FVector::ZeroVector;

	Gen->SurfaceMode = ESurfaceMode32::Smooth_MarchingCubes;
	Gen->GenerateShape32();
	TestTrue(TEXT("smooth sphere non-empty verts"), Gen->LastVertexCount > 0);
	TestTrue(TEXT("smooth sphere non-empty tris"), Gen->LastTriangleCount > 0);
	ValidateSectionBuffers(this, TEXT("smooth"), Gen);
	const float SmoothExtent = Gen->ProcMesh ? Gen->ProcMesh->CalcLocalBounds().GetBox().GetExtent().GetMax() : 0.0f;
	TestTrue(TEXT("smooth bounds within radius+2 cells"), SmoothExtent > 100.0f && SmoothExtent <= 140.0f);

	Gen->SurfaceMode = ESurfaceMode32::Blocky_Voxel;
	Gen->GenerateShape32();
	TestTrue(TEXT("blocky sphere non-empty verts"), Gen->LastVertexCount > 0);
	TestTrue(TEXT("blocky sphere non-empty tris"), Gen->LastTriangleCount > 0);
	ValidateSectionBuffers(this, TEXT("blocky"), Gen);
	const float BlockyExtent = Gen->ProcMesh ? Gen->ProcMesh->CalcLocalBounds().GetBox().GetExtent().GetMax() : 0.0f;
	TestTrue(TEXT("blocky bounds within radius+2 cells"), BlockyExtent > 100.0f && BlockyExtent <= 140.0f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShapeGeneratorDeterminismTest, "Afterfall.ShapeGenerator.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShapeGeneratorDeterminismTest::RunTest(const FString& P)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world created"), World))
	{
		return false;
	}
	AShapeGenerator32* Gen = SpawnTestGenerator(World);
	if (!TestNotNull(TEXT("generator spawned"), Gen))
	{
		World->DestroyWorld(false);
		return false;
	}	Gen->ShapeType = EShapeType32::Sphere;
	Gen->SurfaceMode = ESurfaceMode32::Smooth_MarchingCubes;
	Gen->GridSize = 32;
	Gen->CellSize = 10.0f;
	Gen->Radius = 120.0f;
	Gen->NoiseStrength = 12.0f;
	Gen->Seed = 1337;
	Gen->GenerateShape32();
	const int32 VertsA = Gen->LastVertexCount;
	const int32 TrisA = Gen->LastTriangleCount;
	TestTrue(TEXT("seeded sphere non-empty"), VertsA > 0 && TrisA > 0);

	// Same seed again: identical mesh stats (no global RNG anywhere).
	Gen->GenerateShape32();
	TestEqual(TEXT("same seed same verts"), Gen->LastVertexCount, VertsA);
	TestEqual(TEXT("same seed same tris"), Gen->LastTriangleCount, TrisA);

	// Different seed still builds (seed path is live), guard rails hold.
	Gen->Seed = 1338;
	Gen->GenerateShape32();
	TestTrue(TEXT("other seed non-empty"), Gen->LastVertexCount > 0 && Gen->LastTriangleCount > 0);

	// Invalid inputs are rejected without crashing.
	Gen->GridSize = 1; // clamps to 2
	Gen->GenerateShape32();
	Gen->GridSize = 32;
	Gen->CellSize = 0.0f; // rejected: section cleared, counts reset
	Gen->GenerateShape32();
	TestEqual(TEXT("zero cellsize clears mesh"), Gen->LastVertexCount, 0);

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
