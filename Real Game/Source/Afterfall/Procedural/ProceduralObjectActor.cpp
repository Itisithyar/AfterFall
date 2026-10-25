// Copyright (c) 2026 AFTERFALL. All Rights Reserved.
#include "Procedural/ProceduralObjectActor.h"
#include "Procedural/ProceduralObjectPipeline.h"
#include "Procedural/ProceduralFractureBridge.h"
#include "Persistence/RegionStateSave.h"
#include "ProceduralMeshComponent.h"
#include "Afterfall.h"
#include "Net/UnrealNetwork.h"

AProceduralObjectActor::AProceduralObjectActor()
{
	bReplicates = true;
	// Normal relevancy (NOT always-relevant): per-object channel, snapshot
	// resync covers late joins. No unbounded actor-per-sample strategy.
	PrimaryActorTick.bCanEverTick = true;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	RootComponent = ProcMesh;
	// Runtime edits recook geometry/collision; movable mobility is required.
	ProcMesh->SetMobility(EComponentMobility::Movable);
	ProcMesh->bUseAsyncCooking = true;
	ProcMesh->bUseComplexAsSimpleCollision = true;
	ProcMesh->SetCollisionProfileName(TEXT("BlockAll"));
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProcMesh->SetGenerateOverlapEvents(false);
}

void AProceduralObjectActor::BeginPlay()
{
	Super::BeginPlay();
	MarkRebuildPending();
}

void AProceduralObjectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bRebuildPending)
	{
		bRebuildPending = false;
		RebuildNow();
	}
}

void AProceduralObjectActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProceduralObjectActor, Desc);
	DOREPLIFETIME(AProceduralObjectActor, DeformOps);
	DOREPLIFETIME(AProceduralObjectActor, FractureEvents);
	DOREPLIFETIME(AProceduralObjectActor, ServerSeq);
}

void AProceduralObjectActor::OnRep_Rebuild()
{
	MarkRebuildPending();
}

bool AProceduralObjectActor::HasOp(const FGuid& OpId) const
{
	for (const FProceduralDeformOp& Op : DeformOps)
	{
		if (Op.OpId == OpId)
		{
			return true;
		}
	}
	return false;
}

bool AProceduralObjectActor::HasEvent(const FGuid& EventId) const
{
	for (const FProceduralFractureEvent& Ev : FractureEvents)
	{
		if (Ev.EventId == EventId)
		{
			return true;
		}
	}
	return false;
}

bool AProceduralObjectActor::ServerAdoptDesc(const FProceduralObjectDesc& InDesc)
{
	if (!HasAuthority())
	{
		return false;
	}
	const int32 N = AfterfallProcObj::ClampGridSize(InDesc.GridSize);
	if (N != InDesc.GridSize || InDesc.CellSize <= 0.0f || !InDesc.ObjectId.IsValid())
	{
		return false;
	}
	Desc = InDesc;
	Desc.GridSize = N;
	Desc.SchemaVersion = AFTERFALL_PROC_OBJ_SCHEMA_VERSION;
	DeformOps.Reset();
	FractureEvents.Reset();
	++ServerSeq;
	MarkRebuildPending();
	return true;
}

bool AProceduralObjectActor::ServerApplyDeform(const FProceduralDeformOp& Op)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!Op.OpId.IsValid() || HasOp(Op.OpId))
	{
		return false; // duplicate replay: explicit no-op, never accumulates
	}
	if (!AfterfallProcObj::IsDeformOpValid(Desc, Op))
	{
		return false;
	}
	FProceduralDeformOp Committed = Op;
	Committed.Seq = ++ServerSeq;
	DeformOps.Add(Committed);
	MarkRebuildPending();
	return true;
}

bool AProceduralObjectActor::ServerApplyFracture(const FVector& LocalPos, const FVector& LocalDir,
	float EnergyJ, EMaterialType Material, uint32 Seed, const FGuid& EventId)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!EventId.IsValid() || HasEvent(EventId))
	{
		return false; // duplicate replay: explicit no-op
	}
	if (!AfterfallProcObj::IsFractureRequestValid(Desc, LocalPos, LocalDir, EnergyJ))
	{
		return false;
	}
	AfterfallProcedural::FImpactPlan Plan = AfterfallProcObj::PlanFractureEvent(
		Desc, LocalPos, LocalDir, EnergyJ, Material, Seed);
	if (!Plan.IsValidInput || !Plan.Fracture.bInitiated)
	{
		return false; // rejected event mutates nothing
	}
	FProceduralFractureEvent Ev;
	Ev.EventId = EventId;
	Ev.Seq = ++ServerSeq;
	Ev.LocalPos = LocalPos;
	Ev.LocalDir = LocalDir.GetSafeNormal();
	Ev.EnergyJ = EnergyJ;
	Ev.Material = Material;
	Ev.Seed = Seed;
	Ev.bInitiated = true;
	FractureEvents.Add(Ev);
	MarkRebuildPending();
	return true;
}

bool AProceduralObjectActor::ServerAdoptDelta(const FProceduralObjectDelta& Delta)
{
	if (!HasAuthority() || !Delta.ObjectId.IsValid())
	{
		return false;
	}
	if (Delta.SchemaVersion != AFTERFALL_PROC_OBJ_SCHEMA_VERSION)
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PROCOBJ adopt refused: schema %d != %d"),
			Delta.SchemaVersion, AFTERFALL_PROC_OBJ_SCHEMA_VERSION);
		return false;
	}
	FProceduralObjectDesc D;
	D.ObjectId = Delta.ObjectId;
	D.RegionKey = Delta.RegionKey;
	D.SchemaVersion = AFTERFALL_PROC_OBJ_SCHEMA_VERSION;
	D.Shape = static_cast<EShapeType32>(FMath::Clamp(Delta.ShapeName.IsEmpty() ? 0 : FCString::Atoi(*Delta.ShapeName), 0, 3));
	D.SurfaceMode = static_cast<ESurfaceMode32>(FMath::Clamp(Delta.SurfaceMode, 0, 1));
	D.GridSize = AfterfallProcObj::ClampGridSize(Delta.GridSize);
	D.CellSize = Delta.CellSize;
	D.Center = Delta.Center;
	D.Radius = Delta.Radius;
	D.HalfExtents = Delta.HalfExtents;
	D.Height = Delta.Height;
	D.NoiseStrength = Delta.NoiseStrength;
	D.Seed = Delta.Seed;
	D.IsoLevel = Delta.IsoLevel;
	D.MaterialProfile = static_cast<EMaterialType>(FMath::Clamp(Delta.MaterialProfile, 0, 4));
	D.FractureMaxDepth = FMath::Clamp(Delta.FractureMaxDepth, 1, 64);
	D.FractureCellCap = FMath::Max(0, Delta.FractureCellCap);
	D.CollisionMode = static_cast<EProcObjCollision>(FMath::Clamp(Delta.CollisionMode, 0, 2));
	if (D.CellSize <= 0.0f)
	{
		return false;
	}
	Desc = D;
	DeformOps.Reset();
	for (const FString& S : Delta.DeformOps)
	{
		FProceduralDeformOp Op;
		TArray<FString> F;
		S.ParseIntoArray(F, TEXT(","));
		if (F.Num() != 10 || !FGuid::Parse(F[0], Op.OpId))
		{
			continue;
		}
		Op.Seq = FCString::Atoi(*F[1]);
		Op.Center = FVector(FCString::Atof(*F[2]), FCString::Atof(*F[3]), FCString::Atof(*F[4]));
		Op.Direction = FVector(FCString::Atof(*F[5]), FCString::Atof(*F[6]), FCString::Atof(*F[7]));
		Op.Radius = FCString::Atof(*F[8]);
		Op.Strength = FCString::Atof(*F[9]);
		if (AfterfallProcObj::IsDeformOpValid(Desc, Op) && !HasOp(Op.OpId))
		{
			DeformOps.Add(Op);
		}
	}
	FractureEvents.Reset();
	for (const FString& S : Delta.FractureEvents)
	{
		FProceduralFractureEvent Ev;
		TArray<FString> F;
		S.ParseIntoArray(F, TEXT(","));
		if (F.Num() != 12 || !FGuid::Parse(F[0], Ev.EventId))
		{
			continue;
		}
		Ev.Seq = FCString::Atoi(*F[1]);
		Ev.LocalPos = FVector(FCString::Atof(*F[2]), FCString::Atof(*F[3]), FCString::Atof(*F[4]));
		Ev.LocalDir = FVector(FCString::Atof(*F[5]), FCString::Atof(*F[6]), FCString::Atof(*F[7]));
		Ev.EnergyJ = FCString::Atof(*F[8]);
		Ev.Material = static_cast<EMaterialType>(FMath::Clamp(FCString::Atoi(*F[9]), 0, 4));
		Ev.Seed = static_cast<uint32>(FCString::Atoi(*F[10]));
		Ev.bInitiated = F[11] == TEXT("1");
		if (Ev.bInitiated && AfterfallProcObj::IsFractureRequestValid(Desc, Ev.LocalPos, Ev.LocalDir, Ev.EnergyJ)
			&& !HasEvent(Ev.EventId))
		{
			FractureEvents.Add(Ev);
		}
	}
	DeformOps.Sort([](const FProceduralDeformOp& A, const FProceduralDeformOp& B) { return A.Seq < B.Seq; });
	FractureEvents.Sort([](const FProceduralFractureEvent& A, const FProceduralFractureEvent& B) { return A.Seq < B.Seq; });
	SetActorLocationAndRotation(Delta.Location, Delta.Rotation);
	++ServerSeq;
	bCollisionLive = false;
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkRebuildPending();
	return true;
}

bool AProceduralObjectActor::ServerRequestDeform_Validate(const FProceduralDeformOp& Op)
{
	return Op.OpId.IsValid() && AfterfallProcObj::IsDeformOpValid(Desc, Op) && !HasOp(Op.OpId);
}

void AProceduralObjectActor::ServerRequestDeform_Implementation(const FProceduralDeformOp& Op)
{
	ServerApplyDeform(Op);
}

bool AProceduralObjectActor::ServerRequestFracture_Validate(const FVector& LocalPos, const FVector& LocalDir,
	float EnergyJ, EMaterialType Material, uint32 Seed, const FGuid& EventId)
{
	return EventId.IsValid() && !HasEvent(EventId)
		&& AfterfallProcObj::IsFractureRequestValid(Desc, LocalPos, LocalDir, EnergyJ);
}

void AProceduralObjectActor::ServerRequestFracture_Implementation(const FVector& LocalPos, const FVector& LocalDir,
	float EnergyJ, EMaterialType Material, uint32 Seed, const FGuid& EventId)
{
	ServerApplyFracture(LocalPos, LocalDir, EnergyJ, Material, Seed, EventId);
}

FProceduralObjectDelta AProceduralObjectActor::GetDelta() const
{
	FProceduralObjectDelta D;
	D.ObjectId = Desc.ObjectId;
	D.RegionKey = Desc.RegionKey;
	D.SchemaVersion = AFTERFALL_PROC_OBJ_SCHEMA_VERSION;
	D.ShapeName = FString::FromInt(static_cast<int32>(Desc.Shape));
	D.GridSize = Desc.GridSize;
	D.CellSize = Desc.CellSize;
	D.Center = Desc.Center;
	D.Radius = Desc.Radius;
	D.HalfExtents = Desc.HalfExtents;
	D.Height = Desc.Height;
	D.NoiseStrength = Desc.NoiseStrength;
	D.Seed = Desc.Seed;
	D.IsoLevel = Desc.IsoLevel;
	D.SurfaceMode = static_cast<int32>(Desc.SurfaceMode);
	D.MaterialProfile = static_cast<int32>(Desc.MaterialProfile);
	D.FractureMaxDepth = Desc.FractureMaxDepth;
	D.FractureCellCap = Desc.FractureCellCap;
	D.CollisionMode = static_cast<int32>(Desc.CollisionMode);
	for (const FProceduralDeformOp& Op : DeformOps)
	{
		D.DeformOps.Add(FString::Printf(TEXT("%s,%d,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f"),
			*Op.OpId.ToString(), Op.Seq, Op.Center.X, Op.Center.Y, Op.Center.Z,
			Op.Direction.X, Op.Direction.Y, Op.Direction.Z, Op.Radius, Op.Strength));
	}
	for (const FProceduralFractureEvent& Ev : FractureEvents)
	{
		D.FractureEvents.Add(FString::Printf(TEXT("%s,%d,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%d,%u,%d"),
			*Ev.EventId.ToString(), Ev.Seq, Ev.LocalPos.X, Ev.LocalPos.Y, Ev.LocalPos.Z,
			Ev.LocalDir.X, Ev.LocalDir.Y, Ev.LocalDir.Z, Ev.EnergyJ,
			static_cast<int32>(Ev.Material), Ev.Seed, Ev.bInitiated ? 1 : 0));
	}
	D.Location = GetActorLocation();
	D.Rotation = GetActorRotation();
	D.Durability = 100.0f;
	D.bDestroyed = false;
	for (const FProceduralFractureEvent& Ev : FractureEvents)
	{
		D.RecentEventIds.Add(Ev.EventId);
		if (D.RecentEventIds.Num() >= 8)
		{
			break;
		}
	}
	return D;
}

void AProceduralObjectActor::RebuildNow()
{
	LastVertexCount = 0;
	LastTriangleCount = 0;
	LastCarvedSamples = 0;
	if (!ProcMesh)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	TArray<float> Field;
	AfterfallProcObj::EvaluateField(Desc, DeformOps, Field);
	if (Field.Num() == 0)
	{
		ProcMesh->ClearAllMeshSections();
		ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bCollisionLive = false;
		return;
	}
	// Replay committed fracture events in sequence order against the
	// already-deformed field: fracture consumes the deformed state.
	TArray<FProceduralFractureEvent> Ordered = FractureEvents;
	Ordered.Sort([](const FProceduralFractureEvent& A, const FProceduralFractureEvent& B) { return A.Seq < B.Seq; });
	for (const FProceduralFractureEvent& Ev : Ordered)
	{
		if (!Ev.bInitiated)
		{
			continue;
		}
		AfterfallProcedural::FImpactPlan Plan = AfterfallProcObj::PlanFractureEvent(
			Desc, Ev.LocalPos, Ev.LocalDir, Ev.EnergyJ, Ev.Material, Ev.Seed);
		LastCarvedSamples += AfterfallProcObj::ApplyFractureCarve(Field, Desc, Plan);
	}
	AfterfallProcObj::FProcMeshBuffers Buf;
	AfterfallProcObj::ExtractMesh(Field, Desc, Buf);
	FString Reason;
	if (!AfterfallProcObj::ValidateBuffers(Buf, &Reason))
	{
		UE_LOG(LogAfterfall, Warning, TEXT("PROCOBJ refused buffers: %s"), *Reason);
		ProcMesh->ClearAllMeshSections();
		ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bCollisionLive = false;
		return;
	}
	ProcMesh->ClearAllMeshSections();
	const bool bCollide = Desc.CollisionMode != EProcObjCollision::None;
	TArray<FProcMeshTangent> Tangents;
	ProcMesh->CreateMeshSection_LinearColor(0, Buf.Positions, Buf.Indices, Buf.Normals,
		Buf.UVs, Buf.Colors, Tangents, bCollide);
	LastVertexCount = Buf.Positions.Num();
	LastTriangleCount = Buf.Indices.Num() / 3;
	if (Desc.CollisionMode == EProcObjCollision::FragmentSimple)
	{
		// Cheap convex approximation: bounding-box hull instead of trimesh.
		ProcMesh->ClearCollisionConvexMeshes();
		TArray<FVector> Hull;
		const FBox& B = Buf.Bounds;
		for (int32 i = 0; i < 8; ++i)
		{
			Hull.Add(FVector((i & 1) ? B.Max.X : B.Min.X, (i & 2) ? B.Max.Y : B.Min.Y, (i & 4) ? B.Max.Z : B.Min.Z));
		}
		ProcMesh->AddCollisionConvexMesh(Hull);
		ProcMesh->bUseComplexAsSimpleCollision = false;
	}
	else
	{
		ProcMesh->bUseComplexAsSimpleCollision = true;
	}
	ProcMesh->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	bCollisionLive = bCollide;
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	UE_LOG(LogAfterfall, Log,
		TEXT("PROCOBJ rebuilt: id=%s grid=%d ops=%d events=%d verts=%d tris=%d carved=%d %.1fms"),
		*Desc.ObjectId.ToString(), Desc.GridSize, DeformOps.Num(), FractureEvents.Num(),
		LastVertexCount, LastTriangleCount, LastCarvedSamples, Ms);
}

