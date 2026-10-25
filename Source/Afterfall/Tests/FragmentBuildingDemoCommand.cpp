// Copyright (c) 2026 AFTERFALL. All rights reserved.
// Owner: Gameplay (Building) — dev-only PIE harness for the FragmentBuilding
// blast. This is not an automation test: it is a console command
// (afFragmentDemo) that spawns an AProceduralBuilding ahead of the player,
// fires FragmentBuilding() 1s later, and captures before/during/after
// screenshots plus per-section displacement logs, so the blast physics can be
// verified visually in a -game run. Guarded by WITH_DEV_AUTOMATION_TESTS so
// it never ships.
#include "CoreMinimal.h"
#include "Afterfall.h"
#include "Building/ProceduralBuilding.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	void FragDemoShot(UWorld* W, const TCHAR* Stage)
	{
		if (!W)
		{
			return;
		}
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
		IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		const FString Path = Dir / FString::Printf(TEXT("FragmentDemo_%s.png"), Stage);
		FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI=*/false, /*bAddFilenameSuffix=*/false);
		UE_LOG(LogAfterfall, Log, TEXT("[FRAGDEMO] screenshot requested -> %s"), *Path);
	}

	// Stage 2+: blast at t=1.0, in-flight shot at t=1.8, settle + evidence + quit at t=4.0.
	void FragDemoSchedule(UWorld* W, AProceduralBuilding* Building)
	{
		TWeakObjectPtr<UWorld> WeakW(W);
		TWeakObjectPtr<AProceduralBuilding> WeakB(Building);

		// Remember section world positions at rest so we can log displacement.
		TMap<UActorComponent*, TArray<FVector>> Initial;
		TArray<UActorComponent*> Comps;
		Building->GetComponents(Comps);
		for (UActorComponent* C : Comps)
		{
			if (UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C))
			{
				Initial.Add(C, { PMC->GetComponentLocation() });
			}
		}
		UE_LOG(LogAfterfall, Log, TEXT("[FRAGDEMO] %d sections at rest"), Initial.Num());

		FTimerHandle H1, H2, H3;
		// t=1.0s: last frame before the blast, then fire it.
		W->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WeakW, WeakB]()
		{
			FragDemoShot(WeakW.Get(), TEXT("2_JustBeforeBlast"));
			if (AProceduralBuilding* B = WeakB.Get())
			{
				B->FragmentBuilding();
				UE_LOG(LogAfterfall, Log, TEXT("[FRAGDEMO] FragmentBuilding fired (strength %.0f, falloff %.0f, jitter %.2f)"),
					B->FragmentStrength, B->FragmentFalloffRadius, B->FragmentJitter);
				// Physics diagnostics: is each section actually simulating, and how heavy is it?
				int32 N = 0, Sim = 0, NoCol = 0;
				double MinMass = TNumericLimits<double>::Max(), MaxMass = 0.0;
				TArray<UActorComponent*> Cs;
				B->GetComponents(Cs);
				for (UActorComponent* C : Cs)
				{
					UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C);
					if (!PMC)
					{
						continue;
					}
					++N;
					if (PMC->IsSimulatingPhysics()) { ++Sim; }
					if (PMC->GetCollisionEnabled() == ECollisionEnabled::NoCollision) { ++NoCol; }
					const double M = PMC->GetMass();
					MinMass = FMath::Min(MinMass, M);
					MaxMass = FMath::Max(MaxMass, M);
				}
				UE_LOG(LogAfterfall, Log, TEXT("[FRAGDEMO] post-blast sections=%d simulating=%d noCollision=%d mass=%.1f..%.1fkg gravityZ=%.1f"),
					N, Sim, NoCol, MinMass, MaxMass, WeakW.IsValid() ? WeakW->GetGravityZ() : 0.f);
			}
		}), 1.0f, false);
		// t=1.8s: debris in flight.
		W->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WeakW]()
		{
			FragDemoShot(WeakW.Get(), TEXT("3_DuringBlast"));
		}), 1.8f, false);
		// t=4.0s: settled + numeric evidence, then quit.
		W->GetTimerManager().SetTimer(H3, FTimerDelegate::CreateLambda([WeakW, WeakB, Initial]()
		{
			UWorld* W2 = WeakW.Get();
			FragDemoShot(W2, TEXT("4_AfterSettle"));
			int32 Moved = 0, Seen = 0;
			double MaxDisp = 0.0, MaxSpeed = 0.0;
			if (AProceduralBuilding* B = WeakB.Get())
			{
				TArray<UActorComponent*> Comps;
				B->GetComponents(Comps);
				for (UActorComponent* C : Comps)
				{
					const TArray<FVector>* Start = Initial.Find(C);
					UProceduralMeshComponent* PMC = Cast<UProceduralMeshComponent>(C);
					if (!Start || (*Start).Num() == 0 || !PMC)
					{
						continue;
					}
					++Seen;
					const double Disp = FVector::Dist((*Start)[0], PMC->GetComponentLocation());
					MaxDisp = FMath::Max(MaxDisp, Disp);
					if (Disp > 1.0)
					{
						++Moved;
					}
					MaxSpeed = FMath::Max(MaxSpeed, (double)PMC->GetPhysicsLinearVelocity().Size());
				}
			}
			UE_LOG(LogAfterfall, Log, TEXT("[FRAGDEMO] RESULT sections=%d moved=%d maxDisplacement=%.1fcm maxSpeed=%.1fcm/s (fragmented=%d)"),
				Seen, Moved, MaxDisp, MaxSpeed, (WeakB.IsValid() && WeakB->bFragmented) ? 1 : 0);
			if (W2 && GEngine)
			{
				GEngine->Exec(W2, TEXT("Quit"));
			}
		}), 4.0f, false);
	}

	void FragDemoRun(UWorld* W)
	{
		// Viewpoint: where the player is looking; fall back to a fixed spot.
		FVector Loc(0.f, 0.f, 200.f);
		FRotator Rot = FRotator::ZeroRotator;
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			PC->GetPlayerViewPoint(Loc, Rot);
		}
		const FVector Fwd = Rot.Vector();
		FVector Flat(Fwd.X, Fwd.Y, 0.f);
		if (Flat.IsNearlyZero(0.01f))
		{
			Flat = FVector(1.f, 0.f, 0.f);
		}
		Flat.Normalize();
		// Footprint is 900x900 up to ~900 tall: 1800 units out fits the frame.
		const FVector SpawnAt(Loc.X + Flat.X * 1800.f, Loc.Y + Flat.Y * 1800.f, 0.f);

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AProceduralBuilding* B = W->SpawnActor<AProceduralBuilding>(SpawnAt, FRotator::ZeroRotator, P);
		if (!B)
		{
			UE_LOG(LogAfterfall, Error, TEXT("[FRAGDEMO] spawn failed at %s"), *SpawnAt.ToString());
			return;
		}
		UE_LOG(LogAfterfall, Log, TEXT("[FRAGDEMO] spawned %s at %s (view from %s, pawn=%d)"),
			*B->GetName(), *SpawnAt.ToString(), *Loc.ToString(),
			W->GetFirstPlayerController() && W->GetFirstPlayerController()->GetPawn() ? 1 : 0);
		FragDemoShot(W, TEXT("1_BeforeBlast"));
		FragDemoSchedule(W, B);
	}
}

static FAutoConsoleCommandWithWorld GFragDemoCmd(
	TEXT("afFragmentDemo"),
	TEXT("Spawn a procedural building ahead of the player, blast it, capture before/during/after screenshots."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
	{
		if (!W || !W->IsGameWorld())
		{
			UE_LOG(LogAfterfall, Error, TEXT("[FRAGDEMO] no game world — run from a -game/PIE session"));
			return;
		}
		FragDemoRun(W);
	}));

#endif
