// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Agent link) — validation + read-only status contract.
//
// Isolation: uses a LOCAL registry instance (outer = the editor world), so
// these tests never touch the live providers of the running world and never
// mutate gameplay state. Asserts exact machine-readable refusal codes.
#include "Misc/AutomationTest.h"
#include "Agent/AgentGameLink.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Subsystem/SubsystemService.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FLinkTestProvider : public IAfterfallSubsystem
	{
		ESubsystemState State = ESubsystemState::Ready;
		FString Name = TEXT("LinkProbe");

		virtual ESubsystemState GetState() const override { return State; }
		virtual bool Initialize() override { State = ESubsystemState::Ready; return true; }
		virtual void Shutdown() override { State = ESubsystemState::Stopped; }
		virtual const TCHAR* GetName() const override { return *Name; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentGameLinkTest, "Afterfall.Agent.Link",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAgentGameLinkTest::RunTest(const FString& Parameters)
{
	using namespace AgentGameLink;

	// ---- request validation: exact refusal codes, no game state touched
	{
		const FValidation Empty = ValidateRequest(TEXT(""), TEXT(""));
		TestFalse(TEXT("empty op refused"), Empty.bOk);
		TestEqual(TEXT("empty op code"), Empty.Code, FString(TEXT("empty-op")));
	}
	{
		const FValidation Unknown = ValidateRequest(TEXT("spawn-zombie"), TEXT("{}"));
		TestFalse(TEXT("unknown op refused"), Unknown.bOk);
		TestEqual(TEXT("unknown op code"), Unknown.Code, FString(TEXT("unknown-op")));
	}
	{
		// Prefix of a valid op must NOT pass: allowlist is exact match only.
		const FValidation Prefix = ValidateRequest(TEXT("statu"), TEXT(""));
		TestFalse(TEXT("op prefix refused"), Prefix.bOk);
		TestEqual(TEXT("op prefix code"), Prefix.Code, FString(TEXT("unknown-op")));
	}
	{
		FString Big;
		for (int32 i = 0; i < MaxPayloadChars + 1; ++i) { Big += TEXT("x"); }
		const FValidation Huge = ValidateRequest(TEXT("status"), Big);
		TestFalse(TEXT("oversize payload refused"), Huge.bOk);
		TestEqual(TEXT("oversize code"), Huge.Code, FString(TEXT("payload-too-large")));
	}
	{
		const FValidation Good = ValidateRequest(TEXT("status"), TEXT("{}"));
		TestTrue(TEXT("status op accepted"), Good.bOk);
		TestEqual(TEXT("status code ok"), Good.Code, FString(TEXT("ok")));
		AuditLog(TEXT("status"), Good.Code); // must not crash; payload never logged
	}

	// ---- status query against an isolated local registry
	UWorld* W = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr;
	if (!TestNotNull(TEXT("world context"), W))
	{
		return false;
	}
	TStrongObjectPtr<UAfterfallSubsystemRegistry> Local(NewObject<UAfterfallSubsystemRegistry>(W));
	UAfterfallSubsystemRegistry* Reg = Local.Get();
	if (!TestNotNull(TEXT("local registry"), Reg))
	{
		return false;
	}
	{
		const FStatusResult NoReg = QueryStatus(nullptr);
		TestFalse(TEXT("null registry refused"), NoReg.bAccepted);
		TestEqual(TEXT("null registry code"), NoReg.Code, FString(TEXT("unavailable")));
	}
	{
		const FStatusResult Empty = QueryStatus(Reg);
		TestTrue(TEXT("empty registry accepted"), Empty.bAccepted);
		TestEqual(TEXT("empty registry providers"), Empty.Providers, 0);
		TestFalse(TEXT("not shutting down"), Empty.bRegistryShuttingDown);
	}
	const int32 Before = Reg->Num();
	{
		FLinkTestProvider P;
		TestTrue(TEXT("probe registered"), Reg->Register(&P));
		const FStatusResult One = QueryStatus(Reg);
		TestTrue(TEXT("status accepted"), One.bAccepted);
		TestEqual(TEXT("one provider counted"), One.Providers, 1);
		TestEqual(TEXT("registry unchanged by query"), Reg->Num(), Before + 1);
		TestTrue(TEXT("probe unregistered"), Reg->Unregister(&P));
		TestEqual(TEXT("registry restored"), Reg->Num(), Before);
	}
	// ---- shutting-down registry: clean refusal, never a read through a dying set
	{
		Reg->ShutdownAll();
		TestTrue(TEXT("registry reports shutting down"), Reg->IsShuttingDown());
		const FStatusResult Dead = QueryStatus(Reg);
		TestFalse(TEXT("shutting-down registry refused"), Dead.bAccepted);
		TestEqual(TEXT("shutting-down code"), Dead.Code, FString(TEXT("shutting-down")));
		TestTrue(TEXT("shutting-down flag reported"), Dead.bRegistryShuttingDown);
		TestEqual(TEXT("no providers reported when refused"), Dead.Providers, 0);
		// Positive marker: proves this block executed, not merely that the test passed.
		AddInfo(FString::Printf(TEXT("shutting-down refusal exercised: code=%s"), *Dead.Code));
		// Positive marker: a passing run alone cannot prove this block executed.
		AddInfo(FString::Printf(TEXT("shutting-down refusal exercised: code=%s"), *Dead.Code));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
