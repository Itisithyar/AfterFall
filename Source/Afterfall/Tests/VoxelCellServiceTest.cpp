// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Tech (Subsystems) — typed service path: success + every failure mode.
#include "Misc/AutomationTest.h"
#include "Subsystem/AfterfallSubsystemRegistry.h"
#include "Subsystem/SubsystemService.h"
#include "Voxel/VoxelBasis.h"
#include "Voxel/VoxelSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Provider used to drive each lifecycle state deterministically. */
	struct FTestVoxelProvider : public IAfterfallSubsystem, public IVoxelCellService
	{
		ESubsystemState State = ESubsystemState::Ready;
		bool bAdvertiseService = true;
		int32 Served = 0;

		virtual ESubsystemState GetState() const override { return State; }
		virtual bool Initialize() override { State = ESubsystemState::Ready; return true; }
		virtual void Shutdown() override { State = ESubsystemState::Stopped; }
		virtual const TCHAR* GetName() const override { return TEXT("VoxelTest"); }
		virtual void* GetService(EAfterfallServiceId Id) override
		{
			if (Id != EAfterfallServiceId::VoxelCell || !bAdvertiseService)
			{
				return nullptr;
			}
			return static_cast<IVoxelCellService*>(this);
		}
		virtual FVoxelCellResponse QueryCell(const FVoxelCellRequest& Request) const override
		{
			FVoxelCellResponse R;
			switch (State)
			{
			case ESubsystemState::Ready: break;
			case ESubsystemState::Failed:
				R.Error = AfterfallService::EError::Failed;
				return R;
			case ESubsystemState::Stopping:
			case ESubsystemState::Stopped:
				R.Error = AfterfallService::EError::Unavailable;
				return R;
			default:
				R.Error = AfterfallService::EError::NotReady;
				return R;
			}
			if (Request.WorldPoint.ContainsNaN())
			{
				R.Error = AfterfallService::EError::InvalidRequest;
				return R;
			}
			R.Cell = FVoxelBasis::WorldToVoxel(Request.WorldPoint, Request.Origin);
			R.Error = AfterfallService::EError::None;
			const_cast<FTestVoxelProvider*>(this)->Served++;
			return R;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCellServiceTest, "Afterfall.Voxel.CellService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoxelCellServiceTest::RunTest(const FString& Parameters)
{
	using AfterfallService::EError;

	// ---- 1. Registry unreachable (null): a value, not a crash.
	{
		FVoxelCellRequest Req;
		const FVoxelCellResponse R = QueryVoxelCell(nullptr, Req);
		TestFalse(TEXT("null registry fails"), R.IsSuccess());
		TestTrue(TEXT("null registry -> Unavailable"), R.Error == EError::Unavailable);
	}

	UWorld* W = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr;
	if (!TestNotNull(TEXT("world context"), W))
	{
		return false;
	}
	TStrongObjectPtr<UAfterfallSubsystemRegistry> Local(NewObject<UAfterfallSubsystemRegistry>(W));
	UAfterfallSubsystemRegistry* Reg = Local.Get();
	TestNotNull(TEXT("local registry"), Reg);

	// ---- 2. No provider registered under that name.
	{
		FVoxelCellRequest Req;
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		TestTrue(TEXT("missing provider -> Unavailable"), R.Error == EError::Unavailable);
	}

	// ---- 3. Ready provider serves a real answer through the lattice math.
	FTestVoxelProvider Provider;
	Reg->Register(&Provider);
	FVector Point(3.06f * 4.f + 1.f, 1.f, 1.f);
	FVoxelCellRequest Req;
	Req.WorldPoint = Point;
	Req.Origin = FVector::ZeroVector;
	{
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		if (TestTrue(TEXT("ready provider succeeds"), R.IsSuccess()))
		{
			const FIntVector Expected = FVoxelBasis::WorldToVoxel(Point, FVector::ZeroVector);
			TestEqual(TEXT("cell X"), R.Cell.X, Expected.X);
			TestEqual(TEXT("cell Y"), R.Cell.Y, Expected.Y);
			TestEqual(TEXT("cell Z"), R.Cell.Z, Expected.Z);
		}
		TestEqual(TEXT("provider served once"), Provider.Served, 1);
	}

	// ---- 4. Provider exists but is not ready.
	Provider.State = ESubsystemState::Initializing;
	{
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		TestTrue(TEXT("initializing -> NotReady"), R.Error == EError::NotReady);
	}
	Provider.State = ESubsystemState::Failed;
	{
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		TestTrue(TEXT("failed -> Failed"), R.Error == EError::Failed);
	}
	Provider.State = ESubsystemState::Stopping;
	{
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		TestTrue(TEXT("stopping -> NotReady"), R.Error == EError::NotReady);
	}
	Provider.State = ESubsystemState::Ready;

	// ---- 5. Malformed payload is rejected and not served.
	{
		FVoxelCellRequest Bad;
		// Non-finite: the floor-to-cell conversion is undefined for it.
		Bad.WorldPoint = FVector(FMath::Pow(10.f, 40.f), 0.f, 0.f); // +inf at runtime
		const int32 Before = Provider.Served;
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Bad, TEXT("VoxelTest"));
		TestTrue(TEXT("non-finite -> InvalidRequest"), R.Error == EError::InvalidRequest);
		TestEqual(TEXT("invalid payload not served"), Provider.Served, Before);
	}

	// ---- 6. Provider is ready but does not advertise the service.
	Provider.bAdvertiseService = false;
	{
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		TestTrue(TEXT("no service advertised -> Unavailable"), R.Error == EError::Unavailable);
	}
	Provider.bAdvertiseService = true;

	// ---- 7. Shutting down the registry refuses new work with a reason.
	Reg->ShutdownAll();
	{
		const FVoxelCellResponse R = QueryVoxelCell(Reg, Req, TEXT("VoxelTest"));
		TestTrue(TEXT("registry shutdown -> ShuttingDown"), R.Error == EError::ShuttingDown);
	}
	TestTrue(TEXT("shutdown stopped the provider"), Provider.State == ESubsystemState::Stopped);

	// ---- 8. The shipped provider answers through the same typed path when the
	//         editor world has it registered and ready (skipped otherwise).
	if (UAfterfallSubsystemRegistry* WorldReg = W->GetSubsystem<UAfterfallSubsystemRegistry>())
	{
		if (UVoxelSubsystem* Voxel = W->GetSubsystem<UVoxelSubsystem>())
		{
			if (Voxel->GetState() == ESubsystemState::Ready)
			{
				const FVoxelCellResponse R = QueryVoxelCell(WorldReg, Req);
				TestTrue(TEXT("shipped provider succeeds"), R.IsSuccess());
				TestEqual(TEXT("shipped provider counts the request"),
					Voxel->GetServedCount() > 0, true);
			}
			else
			{
				AddInfo(TEXT("shipped Voxel provider is not Ready; live path skipped"));
			}
		}
		else
		{
			AddInfo(TEXT("no Voxel provider registered in this world"));
		}
	}
	return true;
}

#endif
