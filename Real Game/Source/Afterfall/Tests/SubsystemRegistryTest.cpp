// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Subsystems) — registry behavior + lifecycle contract.
//
// Uses a LOCAL registry instance (outer = the editor world) instead of the
// world's own registry, so these tests cannot stop or shut down the live
// providers of the running world.
#include "Misc/AutomationTest.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Subsystem/SubsystemService.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTestSubsystem : public IAfterfallSubsystem
	{
		ESubsystemState State = ESubsystemState::Uninitialized;
		FString Name = TEXT("TestSub");
		int32 InitCount = 0;
		int32 ShutdownCount = 0;
		UAfterfallSubsystemRegistry* SelfUnregisterFrom = nullptr;

		virtual ESubsystemState GetState() const override { return State; }
		virtual bool Initialize() override
		{
			++InitCount;
			State = ESubsystemState::Ready;
			return true;
		}
		virtual void Shutdown() override
		{
			++ShutdownCount;
			State = ESubsystemState::Stopped;
			// Mirrors UVoxelSubsystem: providers unregister themselves, which
			// used to invalidate the registry's ranged-for.
			if (SelfUnregisterFrom)
			{
				SelfUnregisterFrom->Unregister(this);
			}
		}
		virtual const TCHAR* GetName() const override { return *Name; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubsystemRegistryTest, "Afterfall.Subsystem.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSubsystemRegistryTest::RunTest(const FString& Parameters)
{
	UWorld* W = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr;
	if (!TestNotNull(TEXT("world context"), W))
	{
		return false;
	}
	// Local registry: isolated from the world's live providers.
	TStrongObjectPtr<UAfterfallSubsystemRegistry> Local(NewObject<UAfterfallSubsystemRegistry>(W));
	UAfterfallSubsystemRegistry* Reg = Local.Get();
	if (!TestNotNull(TEXT("registry"), Reg))
	{
		return false;
	}
	TestEqual(TEXT("empty registry"), Reg->Num(), 0);
	TestFalse(TEXT("not shutting down initially"), Reg->IsShuttingDown());

	// ---- registration + lookup
	FTestSubsystem A;
	A.Name = TEXT("A");
	TestTrue(TEXT("register A"), Reg->Register(&A));
	TestEqual(TEXT("one registered"), Reg->Num(), 1);
	TestEqual(TEXT("find A"), Reg->Find(TEXT("A")), static_cast<IAfterfallSubsystem*>(&A));
	TestNull(TEXT("missing name -> null"), Reg->Find(TEXT("Missing")));
	TestNull(TEXT("null name -> null"), Reg->Find(nullptr));
	TestEqual(TEXT("find is case-insensitive"), Reg->Find(TEXT("a")),
		static_cast<IAfterfallSubsystem*>(&A));

	// ---- duplicates are refused and never double-register
	TestFalse(TEXT("duplicate refused"), Reg->Register(&A));
	TestEqual(TEXT("still one"), Reg->Num(), 1);
	TestFalse(TEXT("null register refused"), Reg->Register(nullptr));
	TestEqual(TEXT("null register did not add"), Reg->Num(), 1);

	// ---- unregistration
	TestTrue(TEXT("unregister A"), Reg->Unregister(&A));
	TestNull(TEXT("find after unregister"), Reg->Find(TEXT("A")));
	TestFalse(TEXT("second unregister refused"), Reg->Unregister(&A));
	TestFalse(TEXT("null unregister refused"), Reg->Unregister(nullptr));

	// ---- lifecycle transitions
	using AfterfallSubsystem::IsValidTransition;
	using AfterfallSubsystem::IsServable;
	TestTrue(TEXT("U->Initializing"), IsValidTransition(ESubsystemState::Uninitialized, ESubsystemState::Initializing));
	TestTrue(TEXT("Initializing->Ready"), IsValidTransition(ESubsystemState::Initializing, ESubsystemState::Ready));
	TestTrue(TEXT("Ready->Stopping"), IsValidTransition(ESubsystemState::Ready, ESubsystemState::Stopping));
	TestTrue(TEXT("Stopping->Stopped"), IsValidTransition(ESubsystemState::Stopping, ESubsystemState::Stopped));
	TestTrue(TEXT("same-state is idempotent"), IsValidTransition(ESubsystemState::Ready, ESubsystemState::Ready));
	TestTrue(TEXT("U->Failed (init failure)"), IsValidTransition(ESubsystemState::Uninitialized, ESubsystemState::Failed));
	TestTrue(TEXT("Ready->Failed"), IsValidTransition(ESubsystemState::Ready, ESubsystemState::Failed));
	TestFalse(TEXT("Ready->Initializing illegal"), IsValidTransition(ESubsystemState::Ready, ESubsystemState::Initializing));
	TestFalse(TEXT("Stopped is terminal"), IsValidTransition(ESubsystemState::Stopped, ESubsystemState::Ready));
	TestFalse(TEXT("Failed is terminal"), IsValidTransition(ESubsystemState::Failed, ESubsystemState::Ready));
	TestFalse(TEXT("cannot skip to Stopped"), IsValidTransition(ESubsystemState::Ready, ESubsystemState::Stopped));
	TestTrue(TEXT("only Ready is servable"), IsServable(ESubsystemState::Ready));
	TestFalse(TEXT("Initializing not servable"), IsServable(ESubsystemState::Initializing));
	TestFalse(TEXT("Stopping not servable"), IsServable(ESubsystemState::Stopping));
	TestFalse(TEXT("Stopped not servable"), IsServable(ESubsystemState::Stopped));
	TestFalse(TEXT("Failed not servable"), IsServable(ESubsystemState::Failed));

	// ---- typed resolution reasons
	FTestSubsystem Ready;
	Ready.Name = TEXT("ReadySub");
	Ready.State = ESubsystemState::Ready;
	Reg->Register(&Ready);
	FTestSubsystem NotReady;
	NotReady.Name = TEXT("NotReadySub");
	NotReady.State = ESubsystemState::Initializing;
	Reg->Register(&NotReady);
	FTestSubsystem Failed;
	Failed.Name = TEXT("FailedSub");
	Failed.State = ESubsystemState::Failed;
	Reg->Register(&Failed);

	AfterfallService::EError Err = AfterfallService::EError::None;
	TestNotNull(TEXT("resolve ready"), Reg->Resolve(TEXT("ReadySub"), Err));
	TestTrue(TEXT("resolve ready -> None"), Err == AfterfallService::EError::None);
	TestNull(TEXT("resolve not-ready returns null"), Reg->Resolve(TEXT("NotReadySub"), Err));
	TestTrue(TEXT("resolve not-ready -> NotReady"), Err == AfterfallService::EError::NotReady);
	TestNull(TEXT("resolve failed returns null"), Reg->Resolve(TEXT("FailedSub"), Err));
	TestTrue(TEXT("resolve failed -> Failed"), Err == AfterfallService::EError::Failed);
	TestNull(TEXT("resolve missing returns null"), Reg->Resolve(TEXT("Nope"), Err));
	TestTrue(TEXT("resolve missing -> Unavailable"), Err == AfterfallService::EError::Unavailable);

	// ---- shutdown-all: providers unregister themselves while iterating
	FTestSubsystem SelfRemoving;
	SelfRemoving.Name = TEXT("SelfRemoving");
	SelfRemoving.State = ESubsystemState::Ready;
	SelfRemoving.SelfUnregisterFrom = Reg;
	Reg->Register(&SelfRemoving);
	// Already-stopped providers are not counted or shut down twice.
	FTestSubsystem AlreadyDone;
	AlreadyDone.Name = TEXT("AlreadyDone");
	AlreadyDone.State = ESubsystemState::Stopped;
	Reg->Register(&AlreadyDone);

	// Ready + NotReady + Failed + SelfRemoving are shut down; AlreadyDone is
	// already Stopped and must not be shut down a second time.
	const int32 Shut = Reg->ShutdownAll();
	TestEqual(TEXT("shutdown count excludes already-stopped"), Shut, 4);
	TestEqual(TEXT("Ready shut down once"), Ready.ShutdownCount, 1);
	TestEqual(TEXT("self-removing shut down once"), SelfRemoving.ShutdownCount, 1);
	TestEqual(TEXT("already stopped not re-shut"), AlreadyDone.ShutdownCount, 0);
	TestEqual(TEXT("registry empty after shutdown"), Reg->Num(), 0);
	TestTrue(TEXT("registry reports shutting down"), Reg->IsShuttingDown());

	// ---- repeated shutdown is a no-op
	TestEqual(TEXT("second shutdown returns 0"), Reg->ShutdownAll(), 0);
	TestEqual(TEXT("second shutdown changes nothing"), Ready.ShutdownCount, 1);

	// ---- no new work once shutting down
	FTestSubsystem Late;
	Late.Name = TEXT("Late");
	TestFalse(TEXT("register refused while shutting down"), Reg->Register(&Late));
	TestNull(TEXT("find refused while shutting down"), Reg->Find(TEXT("ReadySub")));
	TestNull(TEXT("resolve refused while shutting down"), Reg->Resolve(TEXT("ReadySub"), Err));
	TestTrue(TEXT("resolve -> ShuttingDown"), Err == AfterfallService::EError::ShuttingDown);
	return true;
}

#endif
