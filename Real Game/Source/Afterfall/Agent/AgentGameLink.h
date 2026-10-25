// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Agent link) — game integration interface, part B.
//
// Narrow, versioned, read-only agent interface (v1).
//
// Design constraints (see AGENT-GAME-CONNECT-ARCHITECTURE.md):
//   * Pure functions only: no UObject, no Tick, no gameplay references.
//     Ordinary gameplay never calls this code and never depends on it.
//   * Allowlists, not blocklists: exactly one op ("status") is accepted.
//   * Failures are values (FValidation / FStatusResult), never silent and
//     never exceptions. Unknown/empty/oversize requests are rejected with
//     an explicit machine-readable Code.
//   * Shipping builds refuse everything (disabled-shipping): the shipped
//     game exposes zero agent surface. Tests run in Editor, never Shipping.
//   * World-thread only for QueryStatus (reads the world-scoped registry).
//     No async transport exists in this slice, so there is nothing to
//     time out or disconnect; that is documented, not emulated.
#pragma once

#include "CoreMinimal.h"

class UAfterfallSubsystemRegistry;

namespace AgentGameLink
{
	/** Maximum accepted payload length in TCHARs (DoS guard). */
	constexpr int32 MaxPayloadChars = 4096;

	/** v1 allowlist: read-only operations only. */
	AFTERFALL_API bool IsSupportedOp(const FString& Op);

	struct FValidation
	{
		bool bOk = false;
		/** ok | empty-op | unknown-op | payload-too-large | disabled-shipping */
		FString Code;
	};

	/**
	 * Validates an agent request without touching game state.
	 * Order: shipping gate -> empty op -> unknown op -> oversize payload.
	 */
	AFTERFALL_API FValidation ValidateRequest(const FString& Op, const FString& Payload);

	struct FStatusResult
	{
		bool bAccepted = false;
		/** ok | unavailable | shutting-down | disabled-shipping */
		FString Code;
		/** Registered provider count. Valid only when bAccepted. */
		int32 Providers = 0;
		bool bRegistryShuttingDown = false;
	};

	/**
	 * Read-only status answer. Never mutates the registry or any provider.
	 * Null registry and shutting-down registries are clean refusals.
	 */
	AFTERFALL_API FStatusResult QueryStatus(const UAfterfallSubsystemRegistry* Registry);

	/** Single audit-log line (LogAfterfall): op + outcome code, no payload. */
	AFTERFALL_API void AuditLog(const FString& Op, const FString& Code);
} // namespace AgentGameLink
