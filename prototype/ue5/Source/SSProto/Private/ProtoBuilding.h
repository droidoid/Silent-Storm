#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstanceDataTypes.h"

#include <memory>
#include <vector>

#include "ssproto/Building.h"
#include "ssproto/TestBuilding.h"

#include "ProtoBuilding.generated.h"

class UInstancedStaticMeshComponent;

// What happened to the building during the world steps of one frame.
struct FProtoStepStats
{
	int32 Steps = 0;        // world steps (50 ms each) run in this frame
	double CoreMs = 0.0;    // time in the core (EndStep: collapse logic)
	double ApplyMs = 0.0;   // time spent removing instances from the view
	int32 Destroyed = 0;    // pieces destroyed by damage
	int32 Broken = 0;       // pieces broken by the statics (overload)
	int32 Detached = 0;     // pieces fallen free
	bool bBusy = false;     // the building is still settling
};

// One world step: where its time went and what it removed (steps.csv; the core bench writes the
// same columns for the same scenario, so the two compare one step to one step).
struct FProtoStepRow
{
	int32 Step = 0;
	float Time = 0.0f;                                // seconds since the scenario started
	double CoreMs = 0.0;                              // EndStep of the core
	double PhaseMs[ssproto::kPhaseCount] = {};        // of it, the phases of the statics
	double ApplyMs = 0.0;                             // removing instances from the view
	int32 Destroyed = 0, Broken = 0, Detached = 0;
	bool bBusy = false;
	uint32 PiecesLeft = 0;
};

// The destructible building: the core of the prototype plus its view. Every surviving piece is one
// instance of the engine's unit cube, scaled to the piece box. Instances are grouped into
// components by material and, optionally, by chunks of parts, so a change touches only some of
// them. Collisions stay in the core (its own lattice); the components have none.
UCLASS()
class AProtoBuilding : public AActor
{
	GENERATED_BODY()

public:
	AProtoBuilding();

	// ChunkParts: 0 - one component per material for the whole building; N - one per N x N parts of
	// one floor and per material. Pieces above HideAbovePlane are not shown (the original cuts the
	// floors above the one the player looks at); -1 shows everything.
	void Init(const ssproto::TestBuildingDesc& Desc, ssproto::CollapseMode Mode, int32 ChunkParts, int32 HideAbovePlane);

	// One world step of the core, applied to the view: added to the frame's Stats, described in Row.
	void Step(FProtoStepStats& Stats, FProtoStepRow& Row);

	ssproto::Building& Core() { return *Building; }
	const ssproto::StepResult& LastResult() const { return LastStep; }
	const ssproto::TestBuildingInfo& Info() const { return BuildingInfo; }
	static FVector ToWorld(const ssproto::Vec3& P) { return FVector(P.x, P.y, P.z) * 100.0; }
	FBox WorldBounds() const;
	int32 ComponentCount() const { return Components.Num(); }
	int32 InstanceCount() const { return Instances; }
	int32 HiddenAbovePlane() const { return HideAbove; }

private:
	void RemoveFromView(const std::vector<ssproto::NodeId>& Nodes);

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;

	std::unique_ptr<ssproto::Building> Building;
	ssproto::StaticsProfile Profile;                      // the statics add the time of every phase here
	ssproto::TestBuildingInfo BuildingInfo;
	ssproto::StepResult LastStep;
	TArray<int32> NodeComponent;                         // per lattice node: component, or INDEX_NONE
	TArray<FPrimitiveInstanceId> NodeInstance;           // per lattice node: its instance in that component
	TArray<TArray<FPrimitiveInstanceId>> PendingRemoval; // per component, reused every step
	int32 Instances = 0;
	int32 HideAbove = -1;
};
