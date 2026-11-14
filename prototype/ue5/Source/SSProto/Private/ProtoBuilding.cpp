#include "ProtoBuilding.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
// Colour of a piece by kind and material, so that the picture reads like a building.
FLinearColor PieceColor(ssproto::PieceKind Kind, ssproto::MaterialId Material)
{
	if (Kind == ssproto::PieceKind::Slab)
		return FLinearColor(0.42f, 0.40f, 0.37f);
	switch (Material)
	{
	case ssproto::MaterialId::Bricks: return FLinearColor(0.55f, 0.22f, 0.15f);
	case ssproto::MaterialId::Wood: return FLinearColor(0.45f, 0.32f, 0.20f);
	case ssproto::MaterialId::Concrete: return FLinearColor(0.50f, 0.50f, 0.50f);
	default: return FLinearColor(0.60f, 0.60f, 0.60f);
	}
}
} // namespace

AProtoBuilding::AProtoBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Static, as the instance components hung on it: the engine refuses to attach a static component to a movable one.
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void AProtoBuilding::Init(const ssproto::TestBuildingDesc& Desc, ssproto::CollapseMode Mode, int32 ChunkParts, int32 HideAbovePlane)
{
	Building = std::make_unique<ssproto::Building>(ssproto::GenerateTestBuilding(Desc, &BuildingInfo), Mode);
	Building->SetStaticsProfile(&Profile);
	const ssproto::Lattice& Lat = Building->GetLattice();
	HideAbove = HideAbovePlane;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	check(Cube && BaseMaterial);

	const int32 ChunksX = ChunkParts > 0 ? FMath::DivideAndRoundUp(Building->PartsX(), ChunkParts) : 1;
	const int32 ChunksY = ChunkParts > 0 ? FMath::DivideAndRoundUp(Building->PartsY(), ChunkParts) : 1;

	NodeComponent.Init(INDEX_NONE, int32(Lat.NodeCount()));
	NodeInstance.SetNum(int32(Lat.NodeCount()));
	TMap<uint32, int32> ComponentOfKey;
	TArray<TArray<FTransform>> Transforms;
	TArray<TArray<ssproto::NodeId>> Nodes;
	for (ssproto::NodeId N = 0; N < Lat.NodeCount(); ++N)
	{
		if (Lat.hp[N] == ssproto::kHpEmpty)
			continue;
		int X, Y, Z;
		Lat.Coords(N, X, Y, Z);
		if (HideAbove >= 0 && Z > HideAbove)
			continue;
		uint32 Chunk = 0;
		if (ChunkParts > 0)
			Chunk = uint32(((Z / 4) * ChunksY + (Y / 2) / ChunkParts) * ChunksX + (X / 2) / ChunkParts);
		const uint32 Key = (Chunk << 8) | (uint32(Lat.kind[N]) << 5) | uint32(Lat.material[N]);
		int32& Index = ComponentOfKey.FindOrAdd(Key, INDEX_NONE);
		if (Index == INDEX_NONE)
		{
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
			C->SetMobility(EComponentMobility::Static);
			C->SetStaticMesh(Cube);
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			Material->SetVectorParameterValue(TEXT("Color"), PieceColor(ssproto::PieceKind(Lat.kind[N]), ssproto::MaterialId(Lat.material[N])));
			C->SetMaterial(0, Material);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			C->SetupAttachment(RootComponent);
			C->RegisterComponent();
			Index = Components.Add(C);
			Transforms.AddDefaulted();
			Nodes.AddDefaulted();
		}
		// The engine cube is 100 cm wide and centred, so its scale is the box size in metres.
		const ssproto::Aabb Box = Lat.PieceBox(N);
		const FVector Size(Box.max.x - Box.min.x, Box.max.y - Box.min.y, Box.max.z - Box.min.z);
		Transforms[Index].Add(FTransform(FQuat::Identity, ToWorld(ssproto::Center(Box)), Size));
		Nodes[Index].Add(N);
	}
	PendingRemoval.SetNum(Components.Num());
	for (int32 C = 0; C < Components.Num(); ++C)
	{
		const TArray<FPrimitiveInstanceId> Ids = Components[C]->AddInstancesById(Transforms[C], /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
		for (int32 K = 0; K < Ids.Num(); ++K)
		{
			NodeComponent[int32(Nodes[C][K])] = C;
			NodeInstance[int32(Nodes[C][K])] = Ids[K];
		}
		Instances += Ids.Num();
	}
}

FBox AProtoBuilding::WorldBounds() const
{
	const ssproto::Aabb B = Building->GetLattice().Bounds();
	return FBox(ToWorld(B.min), ToWorld(B.max));
}

void AProtoBuilding::RemoveFromView(const std::vector<ssproto::NodeId>& Nodes)
{
	for (const ssproto::NodeId N : Nodes)
	{
		const int32 C = NodeComponent[int32(N)];
		if (C == INDEX_NONE)
			continue;
		PendingRemoval[C].Add(NodeInstance[int32(N)]);
		NodeComponent[int32(N)] = INDEX_NONE;
	}
	for (int32 C = 0; C < PendingRemoval.Num(); ++C)
	{
		if (PendingRemoval[C].IsEmpty())
			continue;
		Components[C]->RemoveInstancesById(PendingRemoval[C], /*bUpdateNavigation*/ false);
		Instances -= PendingRemoval[C].Num();
		PendingRemoval[C].Reset();
	}
}

void AProtoBuilding::Step(FProtoStepStats& Stats, FProtoStepRow& Row)
{
	double Before[ssproto::kPhaseCount];
	FMemory::Memcpy(Before, Profile.ms, sizeof Before);
	const double T0 = FPlatformTime::Seconds();
	Building->EndStep(LastStep);
	const double T1 = FPlatformTime::Seconds();
	RemoveFromView(LastStep.destroyed);
	RemoveFromView(LastStep.broken);
	RemoveFromView(LastStep.detached);
	const double T2 = FPlatformTime::Seconds();

	Row.CoreMs = (T1 - T0) * 1000.0;
	for (int32 I = 0; I < ssproto::kPhaseCount; ++I)
		Row.PhaseMs[I] = Profile.ms[I] - Before[I];
	Row.ApplyMs = (T2 - T1) * 1000.0;
	Row.Destroyed = int32(LastStep.destroyed.size());
	Row.Broken = int32(LastStep.broken.size());
	Row.Detached = int32(LastStep.detached.size());
	Row.bBusy = LastStep.busy;
	Row.PiecesLeft = LastStep.piecesLeft;

	++Stats.Steps;
	Stats.CoreMs += Row.CoreMs;
	Stats.ApplyMs += Row.ApplyMs;
	Stats.Destroyed += Row.Destroyed;
	Stats.Broken += Row.Broken;
	Stats.Detached += Row.Detached;
	Stats.bBusy = Row.bBusy;
}
