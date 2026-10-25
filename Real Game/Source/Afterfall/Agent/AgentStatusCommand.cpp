// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Agent link) — dev-only caller surface for the v1 interface.
//
// Why this file exists: AgentGameLink.* stays pure (no UObject, no console
// registry, no world lookup) exactly as its header promises. This file is the
// *caller* — the smallest surface that makes the interface used rather than
// merely tested. It adds no transport, no socket, no auth and no thread; a
// future transport would call the same functions this file calls.
//
// Two entry points, one handler:
//   afAgentStatus [op]        console command (interactive dev use)
//   -afAgentStatus            command-line flag, fired at world init, then exit
//
// The flag path exists because `-ExecCmds` does not dispatch console commands in
// this project's editor-Cmd/-game runs (observed: the `Cmd:` line is logged and
// nothing executes). The flag + OnPostWorldInitialization shape is the one this
// project already proves works headlessly (PhysicsTestSubsystem, room self-test).
//
// Shipped games contain no command and no hook: the whole file compiles out
// under UE_BUILD_SHIPPING, matching the interface's own shipping gate.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Agent/AgentGameLink.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Afterfall.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

namespace
{
	/** One read-only status pass. Same calls a transport would make, in order. */
	void RunStatus(UWorld* World, const FString& Op)
	{
		// Validate first: an unknown op must be refused before any game state is read.
		const AgentGameLink::FValidation Validation = AgentGameLink::ValidateRequest(Op, FString());
		if (!Validation.bOk)
		{
			AgentGameLink::AuditLog(Op, Validation.Code);
			UE_LOG(LogAfterfall, Log, TEXT("AGENTSTATUS op=%s code=%s"), *Op, *Validation.Code);
			return;
		}

		const UAfterfallSubsystemRegistry* Registry =
			World ? World->GetSubsystem<UAfterfallSubsystemRegistry>() : nullptr;
		const AgentGameLink::FStatusResult Result = AgentGameLink::QueryStatus(Registry);
		AgentGameLink::AuditLog(Op, Result.Code);
		UE_LOG(LogAfterfall, Log,
			TEXT("AGENTSTATUS op=%s providers=%d code=%s registry_shutting_down=%d"),
			*Op, Result.Providers, *Result.Code, Result.bRegistryShuttingDown ? 1 : 0);
	}

	void HandleConsoleCommand(const TArray<FString>& Args, UWorld* World)
	{
		// Bare command means the v1 allowlisted op; an argument is validated, not trusted.
		RunStatus(World, Args.Num() > 0 ? Args[0] : TEXT("status"));
	}

	void PostWorldInitHook(UWorld* World, const UWorld::InitializationValues IVS)
	{
		if (!World || !FParse::Param(FCommandLine::Get(), TEXT("afAgentStatus")))
		{
			return;
		}
		// Deferred past world init: the registry is a world subsystem and must exist.
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateLambda([World]()
			{
				RunStatus(World, TEXT("status"));
				// Flag mode is a one-shot headless check, so it terminates the run.
				FPlatformMisc::RequestExit(false);
			}),
			1.0f, false);
	}

	FAutoConsoleCommandWithWorldAndArgs GAgentStatusCommand(
		TEXT("afAgentStatus"),
		TEXT("Read-only agent interface v1: prints provider count via AgentGameLink::QueryStatus. Dev builds only. Usage: afAgentStatus [status]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleConsoleCommand));

	struct FAgentStatusWorldHook
	{
		FAgentStatusWorldHook()
		{
			FWorldDelegates::OnPostWorldInitialization.AddStatic(&PostWorldInitHook);
		}
	} GAgentStatusWorldHook;
} // namespace

#endif // !UE_BUILD_SHIPPING
