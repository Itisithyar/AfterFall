// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Agent link) — game integration interface, part B.
#include "Agent/AgentGameLink.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Afterfall.h"

namespace AgentGameLink
{
	bool IsSupportedOp(const FString& Op)
	{
		// v1: exactly one read-only op. Extend here with a new literal and a
		// dedicated test; never accept prefixes or patterns.
		return Op == TEXT("status");
	}

	FValidation ValidateRequest(const FString& Op, const FString& Payload)
	{
		FValidation V;
#if UE_BUILD_SHIPPING
		// The shipped game exposes zero agent surface by construction.
		V.bOk = false;
		V.Code = TEXT("disabled-shipping");
		return V;
#else
		if (Op.IsEmpty())
		{
			V.bOk = false;
			V.Code = TEXT("empty-op");
			return V;
		}
		if (!IsSupportedOp(Op))
		{
			V.bOk = false;
			V.Code = TEXT("unknown-op");
			return V;
		}
		if (Payload.Len() > MaxPayloadChars)
		{
			V.bOk = false;
			V.Code = TEXT("payload-too-large");
			return V;
		}
		V.bOk = true;
		V.Code = TEXT("ok");
		return V;
#endif
	}

	FStatusResult QueryStatus(const UAfterfallSubsystemRegistry* Registry)
	{
		FStatusResult R;
#if UE_BUILD_SHIPPING
		R.bAccepted = false;
		R.Code = TEXT("disabled-shipping");
		return R;
#else
		if (!Registry)
		{
			R.bAccepted = false;
			R.Code = TEXT("unavailable");
			return R;
		}
		if (Registry->IsShuttingDown())
		{
			R.bAccepted = false;
			R.Code = TEXT("shutting-down");
			R.bRegistryShuttingDown = true;
			return R;
		}
		R.bAccepted = true;
		R.Code = TEXT("ok");
		R.Providers = Registry->Num();
		R.bRegistryShuttingDown = false;
		return R;
#endif
	}

	void AuditLog(const FString& Op, const FString& Code)
	{
		// Payloads are never logged: they may carry caller-side data.
		// LogAfterfall keeps this inside the game's existing log category.
		UE_LOG(LogAfterfall, Log, TEXT("AGENTLINK op=%s result=%s"), *Op, *Code);
	}
} // namespace AgentGameLink
