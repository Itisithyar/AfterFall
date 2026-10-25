// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Subsystems) — world-scoped service registry.
//
// Stage B contract:
//   * A subsystem is registered once, while it is alive, and unregistered
//     before it dies. The registry never owns the object and never dereferences
//     it after Unregister.
//   * Lifecycle is an explicit state machine (AfterfallSubsystem::IsValidTransition).
//     Terminal states (Stopped/Failed) cannot be revived.
//   * Only Ready providers serve requests (AfterfallSubsystem::IsServable).
//   * ShutdownAll is terminal for the registry: it is idempotent, and during and
//     after it Register() refuses new subsystems and Find() refuses to hand out
//     providers, so callers cannot start work against a dying set.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AfterfallSubsystemRegistry.generated.h"

enum class ESubsystemState : uint8
{
	Uninitialized,
	Initializing,
	Ready,
	Stopping,
	Stopped,
	Failed
};

/** Typed service ids. Extend here when a second service is added. */
enum class EAfterfallServiceId : uint8
{
	VoxelCell
};

// Opaque declaration: the full definition lives in SubsystemService.h, so the
// registry does not depend on any service's payload types.
namespace AfterfallService
{
	enum class EError : uint8;
}

/**
 * Service-provider base. Implementations return a pointer to one of their
 * typed service interfaces (see SubsystemService.h) or null when unavailable.
 * The cast exists in exactly one place per service id, guarded by the id, so
 * no RTTI is needed (UE builds with RTTI off).
 */
class IAfterfallSubsystem
{
public:
	virtual ~IAfterfallSubsystem() = default;
	virtual ESubsystemState GetState() const = 0;
	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;
	virtual const TCHAR* GetName() const = 0;
	/** Typed service accessor; null when this subsystem provides no such service. */
	virtual void* GetService(EAfterfallServiceId /*Id*/) { return nullptr; }
};

namespace AfterfallSubsystem
{
	/** Same-state transitions are legal no-ops; terminal states are final. */
	AFTERFALL_API bool IsValidTransition(ESubsystemState From, ESubsystemState To);
	/** True only while the subsystem can answer requests (Ready). */
	AFTERFALL_API bool IsServable(ESubsystemState State);
}

UCLASS()
class AFTERFALL_API UAfterfallSubsystemRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	/**
	 * @return false for null, for an already-registered subsystem, or once the
	 *         registry is shutting down (new work must not start).
	 */
	bool Register(IAfterfallSubsystem* S);

	/** @return false for null or for a subsystem that was not registered. */
	bool Unregister(IAfterfallSubsystem* S);

	/** Null for unknown/null names and while shutting down. */
	IAfterfallSubsystem* Find(const TCHAR* Name) const;

	/**
	 * Typed resolution with a reason on failure. Never returns a provider that
	 * is not Ready, so callers cannot use a service mid-transition.
	 */
	IAfterfallSubsystem* Resolve(const TCHAR* Name, AfterfallService::EError& OutError) const;

	int32 Num() const { return Subsystems.Num(); }
	bool IsShuttingDown() const { return bShuttingDown; }

	/** Shut every provider down, then clear. Idempotent. @return count shut down. */
	int32 ShutdownAll();

private:
	TArray<IAfterfallSubsystem*> Subsystems;
	bool bShuttingDown = false;
};
