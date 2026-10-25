// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Combat) — FR-004 client prediction ledger.
// Pure, engine-independent bounded pending set with TTL expiry and
// idempotent resolution. No world, no actors, no HP: the server stays the
// only damage authority. This ledger tracks the CLIENT's optimistic
// expectation so the authoritative answer can confirm or retract it.
#pragma once

#include "CoreMinimal.h"
#include "Prediction/BoundedPredictiveMatrix.h"

namespace Afterfall
{
	enum class EHitPredictionOutcome : uint8
	{
		Confirmed,         // predicted hit, server confirmed hit
		Retracted,         // predicted hit, server authoritative miss
		AuthoritativeOnly, // no predicted hit, server confirmed hit
		AgreedNoEvent,     // predicted miss, server confirmed miss
		UnknownId,         // id never registered (or evicted while bounded)
		AlreadyResolved,   // duplicate reconcile for an already-closed id
		Expired            // entry aged past TTL before the answer arrived
	};

	struct FHitPredictionEntry
	{
		FGuid Id;
		FName PredictedTarget = NAME_None;
		double ClientTimeSec = 0.0;
		bool bPredictedHit = false;
	};

	/** Bounded client prediction ledger. Pending entries are capped with the
	 *  shared candidate clamp (the same N_candidate <= 3 bound the fracture
	 *  solver uses at its candidate-generation boundary). */
	class FHitPredictionLedger
	{
	public:
		static constexpr double TTLSeconds = 2.0;
		static constexpr int32 MaxResolvedIds = 8;

		int32 NumPending() const { return Pending.Num(); }

		FGuid Add(const FName& Target, bool bPredictedHit, double NowSec)
		{
			PurgeExpired(NowSec);
			const FGuid Id = FGuid::NewGuid();
			// Bounded predictive set: at most 3 pending predictions are kept;
			// the oldest is evicted first (gameplay call site of
			// Prediction/BoundedPredictiveMatrix.h).
			const int32 Cap = Afterfall::ClampCandidateCount(3);
			while (Pending.Num() >= Cap)
			{
				Pending.RemoveAt(0);
			}
			FHitPredictionEntry Entry;
			Entry.Id = Id;
			Entry.PredictedTarget = Target;
			Entry.ClientTimeSec = NowSec;
			Entry.bPredictedHit = bPredictedHit;
			Pending.Add(Entry);
			return Id;
		}

		EHitPredictionOutcome Resolve(const FGuid& Id, bool bAuthoritativeHit, double NowSec)
		{
			if (!Id.IsValid())
			{
				return EHitPredictionOutcome::UnknownId;
			}
			int32 Index = INDEX_NONE;
			for (int32 i = 0; i < Pending.Num(); ++i)
			{
				if (Pending[i].Id == Id)
				{
					Index = i;
					break;
				}
			}
			if (Index == INDEX_NONE)
			{
				return ResolvedIds.Contains(Id)
					? EHitPredictionOutcome::AlreadyResolved
					: EHitPredictionOutcome::UnknownId;
			}
			const FHitPredictionEntry Entry = Pending[Index];
			Pending.RemoveAt(Index);
			ResolvedIds.Add(Id);
			if (ResolvedIds.Num() > MaxResolvedIds)
			{
				ResolvedIds.RemoveAt(0, ResolvedIds.Num() - MaxResolvedIds);
			}
			if (NowSec - Entry.ClientTimeSec > TTLSeconds)
			{
				return EHitPredictionOutcome::Expired;
			}
			if (Entry.bPredictedHit)
			{
				return bAuthoritativeHit
					? EHitPredictionOutcome::Confirmed
					: EHitPredictionOutcome::Retracted;
			}
			return bAuthoritativeHit
				? EHitPredictionOutcome::AuthoritativeOnly
				: EHitPredictionOutcome::AgreedNoEvent;
		}

		void PurgeExpired(double NowSec)
		{
			for (int32 i = Pending.Num() - 1; i >= 0; --i)
			{
				if (NowSec - Pending[i].ClientTimeSec > TTLSeconds)
				{
					Pending.RemoveAt(i);
				}
			}
		}

	private:
		TArray<FHitPredictionEntry> Pending;
		TArray<FGuid> ResolvedIds;
	};
}
