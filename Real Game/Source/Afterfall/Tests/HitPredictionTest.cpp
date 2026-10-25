// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Combat|Tests) — FR-004 client prediction ledger.
// Pure ledger checks: confirm/retract/agree paths, bounded pending set,
// TTL expiry, idempotent resolution. No world, no actors, no HP.
#include "Misc/AutomationTest.h"
#include "Combat/HitPrediction.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitPredictionConfirmTest, "Afterfall.Prediction.HitPrediction.Confirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitPredictionConfirmTest::RunTest(const FString& P)
{
	Afterfall::FHitPredictionLedger Ledger;
	const FGuid Id = Ledger.Add(TEXT("Zombie_0"), true, 1.0);
	TestEqual(TEXT("one pending"), Ledger.NumPending(), 1);
	TestTrue(TEXT("predicted hit + authoritative hit -> Confirmed"),
		Ledger.Resolve(Id, true, 1.1) == Afterfall::EHitPredictionOutcome::Confirmed);
	TestEqual(TEXT("drained after resolve"), Ledger.NumPending(), 0);
	// Duplicate reconcile is a no-op, never double-counted.
	TestTrue(TEXT("second resolve -> AlreadyResolved"),
		Ledger.Resolve(Id, true, 1.2) == Afterfall::EHitPredictionOutcome::AlreadyResolved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitPredictionRetractTest, "Afterfall.Prediction.HitPrediction.Retract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitPredictionRetractTest::RunTest(const FString& P)
{
	Afterfall::FHitPredictionLedger Ledger;
	// Predicted hit, server says whiff: local feedback must be retracted.
	const FGuid Id = Ledger.Add(TEXT("Wall_3"), true, 5.0);
	TestTrue(TEXT("predicted hit + authoritative miss -> Retracted"),
		Ledger.Resolve(Id, false, 5.2) == Afterfall::EHitPredictionOutcome::Retracted);
	// Predicted miss, server confirms hit anyway: authoritative-only.
	const FGuid Surprise = Ledger.Add(NAME_None, false, 6.0);
	TestTrue(TEXT("predicted miss + authoritative hit -> AuthoritativeOnly"),
		Ledger.Resolve(Surprise, true, 6.1) == Afterfall::EHitPredictionOutcome::AuthoritativeOnly);
	// Both sides agree on a miss: no feedback either way.
	const FGuid Clean = Ledger.Add(NAME_None, false, 7.0);
	TestTrue(TEXT("predicted miss + authoritative miss -> AgreedNoEvent"),
		Ledger.Resolve(Clean, false, 7.1) == Afterfall::EHitPredictionOutcome::AgreedNoEvent);
	// Unknown ids (never registered) are reported, never crash.
	TestTrue(TEXT("unknown id -> UnknownId"),
		Ledger.Resolve(FGuid::NewGuid(), true, 7.2) == Afterfall::EHitPredictionOutcome::UnknownId);
	TestTrue(TEXT("invalid id -> UnknownId"),
		Ledger.Resolve(FGuid(), true, 7.2) == Afterfall::EHitPredictionOutcome::UnknownId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitPredictionBoundTest, "Afterfall.Prediction.HitPrediction.Bounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitPredictionBoundTest::RunTest(const FString& P)
{
	Afterfall::FHitPredictionLedger Ledger;
	// Five rapid swings: the bounded set keeps the newest 3 (shared clamp).
	FGuid Ids[5];
	for (int32 i = 0; i < 5; ++i)
	{
		Ids[i] = Ledger.Add(*FString::Printf(TEXT("Target_%d"), i), true, 10.0);
	}
	TestEqual(TEXT("pending capped at 3"), Ledger.NumPending(), 3);
	// The two oldest were evicted, not resolved: unknown, not already-resolved.
	TestTrue(TEXT("evicted oldest -> UnknownId"),
		Ledger.Resolve(Ids[0], true, 10.1) == Afterfall::EHitPredictionOutcome::UnknownId);
	TestTrue(TEXT("evicted second -> UnknownId"),
		Ledger.Resolve(Ids[1], true, 10.1) == Afterfall::EHitPredictionOutcome::UnknownId);
	// The newest three still reconcile normally.
	TestTrue(TEXT("newest resolves"),
		Ledger.Resolve(Ids[4], true, 10.1) == Afterfall::EHitPredictionOutcome::Confirmed);
	TestEqual(TEXT("two remain"), Ledger.NumPending(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitPredictionExpiryTest, "Afterfall.Prediction.HitPrediction.Expiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitPredictionExpiryTest::RunTest(const FString& P)
{
	Afterfall::FHitPredictionLedger Ledger;
	const FGuid Id = Ledger.Add(TEXT("Zombie_1"), true, 20.0);
	// Answer arrives after the 2 s TTL: expired, not confirmed.
	TestTrue(TEXT("late answer -> Expired"),
		Ledger.Resolve(Id, true, 25.0) == Afterfall::EHitPredictionOutcome::Expired);
	// ...and stays closed afterwards.
	TestTrue(TEXT("expired id re-resolve -> AlreadyResolved"),
		Ledger.Resolve(Id, true, 25.1) == Afterfall::EHitPredictionOutcome::AlreadyResolved);
	// Stale entries are purged on the next registration.
	const FGuid Fresh = Ledger.Add(TEXT("Zombie_2"), true, 30.0);
	Ledger.PurgeExpired(40.0);
	TestEqual(TEXT("stale purged"), Ledger.NumPending(), 0);
	TestTrue(TEXT("purged id -> UnknownId"),
		Ledger.Resolve(Fresh, true, 40.1) == Afterfall::EHitPredictionOutcome::UnknownId);
	(void)Fresh;
	return true;
}

#endif
