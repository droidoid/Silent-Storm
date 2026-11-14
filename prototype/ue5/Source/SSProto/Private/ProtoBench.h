#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ssproto/Scenario.h"
#include "ssproto/Trace.h"

#include "ProtoBuilding.h"

#include "ProtoBench.generated.h"

// Runs one measurement scenario and quits. Everything is set from the command line:
//   -SSScenario=idle|grenades|hundred|half   what happens to the building (default grenades)
//   -SSBuilding=small|large                  the test building (default small)
//   -SSChunk=N                               0: one component per material; N: per N x N parts (default 0)
//   -SSHideAbove=P                           hide planes above P; default hides the roof, -1 shows all
//   -SSSunLux=L -SSLampLumens=L -SSExposure=EV   light (an exposure value switches to manual exposure)
//   -SSResults=Dir                           where to write frames.csv, steps.csv, summary.txt and screenshots
// The timeline starts when shaders are compiled, then 5 s of warm-up. World steps run at 20 Hz
// (50 ms, as in the original); the core decides what breaks, the view only removes instances. The
// scenario (ssproto/Scenario.h) damages the building at fixed world steps, so every machine and the
// core bench (ssproto_bench scenarios) run the same collapse: the trace in summary.txt shows it.
UCLASS()
class AProtoBench : public AActor
{
	GENERATED_BODY()

public:
	AProtoBench();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EPhase : uint8 { WaitShaders, Warmup, Run, Done };

	struct FFrameRow
	{
		float Time, FrameMs, GameMs, RenderMs, RhiMs, GpuMs, CoreMs, ApplyMs;
		int32 Steps, Destroyed, Broken, Detached, Instances;
		bool bBusy;
	};

	void SetupEnvironment();
	void SetupCamera();
	void PlanShots(ssproto::ScenarioKind Kind);
	void ReplayBackToBack(const ssproto::TestBuildingDesc& Desc, ssproto::ScenarioKind Kind, int32 HideAbove);
	void Shot(const TCHAR* Name);
	void Finish(const TCHAR* Reason);
	FString StepsReport() const;   // writes steps.csv; returns the lines of summary.txt about the steps
	bool ShadersBusy() const;

	UPROPERTY()
	TObjectPtr<AProtoBuilding> Building;

	FString Scenario = TEXT("grenades");
	FString BuildingSize = TEXT("small");
	FString ResultsDir;
	int32 ChunkParts = 0;
	float SunLux = 3.0f;
	float LampLumens = 1500.0f;
	float ExposureEV = 0.0f;
	bool bManualExposure = false;

	EPhase Phase = EPhase::WaitShaders;
	double PhaseStart = 0.0;
	double RunStart = 0.0;
	double NextStep = 0.0;
	ssproto::Scenario Plan;
	bool bKnownScenario = true;
	int32 StepIndex = 0;
	ssproto::Trace StepTrace;
	TArray<TPair<int32, FString>> PlannedShots;   // (world step, name)
	TArray<FFrameRow> Frames;
	TArray<FProtoStepRow> StepRows;
	// The same scenario on a copy of the building, every step right after the previous one.
	double TwinTotalMs = 0.0;
	double TwinHeaviestMs = 0.0;
	int32 TwinHeaviestStep = 0;
	int32 TwinSteps = 0;
	uint64 TwinTrace = 0;
};
