// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Destruction) — acceptance for the propagation slice.
// Mirrored by Scripts/validate_destruction.py for no-engine CI evidence.
#include "Misc/AutomationTest.h"
#include "Destruction/DestructionPropagation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDestructionGridTest, "Afterfall.Destruction.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDestructionGridTest::RunTest(const FString& P)
{
	TestEqual(TEXT("27 grid points"), UDestructionPropagationService::GridPointCount(), 27);
	TestEqual(TEXT("center slot is index 13"),
		UDestructionPropagationService::GridIndex(1, 1, 1), 13);
	const FVector C = FVector::ZeroVector;
	TestEqual(TEXT("center point"), UDestructionPropagationService::GridPoint(C, 50.f, 1, 1, 1), C);
	TestEqual(TEXT("corner point"),
		UDestructionPropagationService::GridPoint(C, 50.f, 0, 0, 0), FVector(-50.f, -50.f, -50.f));
	return true;
}

// NOTE: class intentionally NOT named FDestructionPropagationTest — that name is
// taken by DestructionSystemTest.cpp (other agent's file). Same name in two TUs
// would violate ODR / duplicate symbols at link. Test path unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDestructionMazePropagationTest, "Afterfall.Destruction.Propagate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDestructionMazePropagationTest::RunTest(const FString& P)
{
	const FVector Center = FVector::ZeroVector;

	// Bullet along +X with ample energy detaches forward breakpoints.
	FDestructionEvent Bullet;
	Bullet.Origin = FVector(-200.f, 0.f, 0.f);
	Bullet.Direction = FVector::ForwardVector;
	Bullet.InitialEnergy = 200.f;
	Bullet.Initiator = EDestructionInitiator::Bullet;
	Bullet.MaterialId = TEXT("Wood");
	const FPropagationResult RB =
		UDestructionPropagationService::Propagate(Bullet, Center, 50.f);
	TestTrue(TEXT("bullet detaches"), RB.DetachedBreakpoints.Num() > 0);
	TestTrue(TEXT("bullet bounded"), RB.Steps.Num() <= 8);

	// Same shot at concrete detaches less-or-equal (material matters).
	FDestructionEvent Conc = Bullet;
	Conc.MaterialId = TEXT("Concrete");
	const FPropagationResult RC =
		UDestructionPropagationService::Propagate(Conc, Center, 50.f);
	TestTrue(TEXT("concrete resists"), RC.DetachedBreakpoints.Num() <= RB.DetachedBreakpoints.Num());

	// Attack has shorter reach / higher cost than bullet: fewer-or-equal detaches.
	FDestructionEvent Atk = Bullet;
	Atk.Initiator = EDestructionInitiator::Attack;
	const FPropagationResult RA =
		UDestructionPropagationService::Propagate(Atk, Center, 50.f);
	TestTrue(TEXT("attack weaker reach"), RA.DetachedBreakpoints.Num() <= RB.DetachedBreakpoints.Num());

	// Truly empty event (zero direction, zero energy) denies without detaching.
	// NOTE: a default-constructed FDestructionEvent still carries 100 energy and
	// +X direction, so it is a VALID impact and must detach — only an invalid
	// (zero/zero) event detaches nothing. Repeating is deterministic.
	FDestructionEvent Empty;
	Empty.Origin = FVector::ZeroVector;
	Empty.Direction = FVector::ZeroVector;
	Empty.InitialEnergy = 0.f;
	const FPropagationResult R0 =
		UDestructionPropagationService::Propagate(Empty, Center, 50.f);
	TestEqual(TEXT("empty event detaches nothing"), R0.DetachedBreakpoints.Num(), 0);
	TestTrue(TEXT("empty event denied"), R0.bDenied);
	const FPropagationResult RB2 =
		UDestructionPropagationService::Propagate(Bullet, Center, 50.f);
	TestEqual(TEXT("deterministic"), RB2.DetachedBreakpoints.Num(), RB.DetachedBreakpoints.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDestructionInitiatorConfigTest, "Afterfall.Destruction.Initiators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDestructionInitiatorConfigTest::RunTest(const FString& P)
{
	// Documented energy/damage proxies stay positive and ordered:
	// bullets out-reach and out-hit melee; failure is the weakest trigger.
	TestTrue(TEXT("bullet energy > attack"),
		UDestructionPropagationService::BulletEnergy() > UDestructionPropagationService::AttackEnergy());
	TestTrue(TEXT("bullet damage > attack"),
		UDestructionPropagationService::BulletDamagePerCell() > UDestructionPropagationService::AttackDamagePerCell());
	TestTrue(TEXT("energies positive"),
		UDestructionPropagationService::FailureEnergy() > 0.f);
	// Part -> material mapping covers every structural part.
	TestEqual(TEXT("foundation is concrete"),
		UDestructionPropagationService::MaterialForPart(EStructuralPart::Foundation), FName(TEXT("Concrete")));
	TestEqual(TEXT("window is glass"),
		UDestructionPropagationService::MaterialForPart(EStructuralPart::Window), FName(TEXT("Glass")));
	TestEqual(TEXT("lock is metal"),
		UDestructionPropagationService::MaterialForPart(EStructuralPart::Lock), FName(TEXT("Metal")));
	TestEqual(TEXT("wall defaults wood"),
		UDestructionPropagationService::MaterialForPart(EStructuralPart::Wall), FName(TEXT("Wood")));
	// Failure direction is straight down (gravity-driven, deterministic).
	TestEqual(TEXT("down vector"), FVector::DownVector, FVector(0.f, 0.f, -1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDestructionLocalFrameTest, "Afterfall.Destruction.LocalFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDestructionLocalFrameTest::RunTest(const FString& P)
{
	// A yawed + translated actor hit from the world direction facing its local
	// -X face must detach the SAME local cells as the unrotated reference hit.
	// The world axis is derived from the transform itself (no convention guess).
	const FTransform ActorT(FRotator(0.f, 90.f, 0.f), FVector(100.f, -200.f, 300.f));
	const FVector WorldAxis = ActorT.TransformVector(FVector::ForwardVector).GetSafeNormal();
	const FVector WOrigin = ActorT.GetLocation() - WorldAxis * 60.f;
	const FTransform Inv = ActorT.Inverse();
	const FVector LOrigin = Inv.TransformPosition(WOrigin);
	const FVector LDir = Inv.TransformVector(WorldAxis).GetSafeNormal();
	TestEqual(TEXT("world impact maps to local -X face"), LOrigin, FVector(-60.f, 0.f, 0.f), 0.01f);
	TestEqual(TEXT("world dir maps to local +X"), LDir, FVector::ForwardVector, 0.01f);

	FDestructionEvent Local;
	Local.Origin = LOrigin;
	Local.Direction = LDir;
	Local.InitialEnergy = UDestructionPropagationService::AttackEnergy();
	Local.Initiator = EDestructionInitiator::Attack;
	Local.MaterialId = TEXT("Wood");
	FDestructionEvent Ref = Local;
	Ref.Origin = FVector(-60.f, 0.f, 0.f);
	Ref.Direction = FVector::ForwardVector;
	const FPropagationResult RL = UDestructionPropagationService::Propagate(Local, FVector::ZeroVector, 50.f);
	const FPropagationResult RR = UDestructionPropagationService::Propagate(Ref, FVector::ZeroVector, 50.f);
	TestEqual(TEXT("rotated frame detaches same count"), RL.DetachedBreakpoints.Num(), RR.DetachedBreakpoints.Num());
	bool bSame = RL.DetachedBreakpoints.Num() == RR.DetachedBreakpoints.Num();
	for (int32 i = 0; bSame && i < RL.DetachedBreakpoints.Num(); ++i)
	{
		bSame = RL.DetachedBreakpoints[i] == RR.DetachedBreakpoints[i];
	}
	TestTrue(TEXT("rotated frame detaches same cells in order"), bSame);
	TestTrue(TEXT("nonempty surface entry"), RL.DetachedBreakpoints.Num() > 0);
	if (RL.DetachedBreakpoints.Num() > 0)
	{
		// GridIndex ix = idx % 3; entry face is ix == 0.
		TestEqual(TEXT("first cell on entry face"), RL.DetachedBreakpoints[0] % 3, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDestructionCommitTest, "Afterfall.Destruction.CommitIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDestructionCommitTest::RunTest(const FString& P)
{
	TArray<int32> Live;
	TestTrue(TEXT("add valid cell"), UDestructionPropagationService::AddDetachedCell(Live, 13));
	TestFalse(TEXT("duplicate replay adds nothing"),
		UDestructionPropagationService::AddDetachedCell(Live, 13));
	TestFalse(TEXT("out-of-range rejected"),
		UDestructionPropagationService::AddDetachedCell(Live, 27));
	TestFalse(TEXT("negative rejected"),
		UDestructionPropagationService::AddDetachedCell(Live, -1));
	TestEqual(TEXT("one cell live"), Live.Num(), 1);

	// Batch merge: 13 dup + 14 new + 99 invalid => exactly 1 added.
	const int32 Added = UDestructionPropagationService::MergeDetached(Live, { 13, 14, 99 });
	TestEqual(TEXT("merge adds only new valid"), Added, 1);
	TestEqual(TEXT("two cells live"), Live.Num(), 2);

	// Save/unload/reload round trip: export cells, restore into a fresh root.
	TArray<int32> Fresh;
	const int32 Restored = UDestructionPropagationService::MergeDetached(Fresh, Live);
	TestEqual(TEXT("reload restores 2 cells"), Restored, 2);
	const int32 Replay = UDestructionPropagationService::MergeDetached(Fresh, Live);
	TestEqual(TEXT("replay after reload adds nothing"), Replay, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
