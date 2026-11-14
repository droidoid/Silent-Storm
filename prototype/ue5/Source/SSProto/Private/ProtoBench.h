#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ssproto/Math.h"

#include "ProtoBench.generated.h"

class AProtoBuilding;

// Runs one measurement scenario and quits. Everything is set from the command line:
//   -SSScenario=idle|grenades|hundred|half   what happens to the building (default grenades)
//   -SSBuilding=small|large                  the test building (default small)
//   -SSChunk=N                               0: one component per material; N: per N x N parts (default 0)
//   -SSHideAbove=P                           hide planes above P; default hides the roof, -1 shows all
//   -SSSunLux=L -SSLampLumens=L -SSExposure=EV   light (an exposure value switches to manual exposure)
//   -SSResults=Dir                           where to write frames.csv, summary.txt and screenshots
// The timeline starts when shaders are compiled, then 5 s of warm-up. World steps run at 20 Hz
// (50 ms, as in the original); the core decides what breaks, the view only removes instances.
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
	void BeforeStep(double T);            // scenario actions that happen in step with the world
	void ScenarioTick(double T);          // scenario actions on the clock
	void ExplodeAtRandomPiece();
	void DestroyNearest(int32 Count);
	void CutHalf();
	void Shot(const TCHAR* Name);
	void Finish(const TCHAR* Reason);
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
	double LastDamageTime = -1.0;
	int32 Grenades = 0;
	bool bCut = false;
	TArray<TPair<double, FString>> PlannedShots;
	TArray<FFrameRow> Frames;
	ssproto::SRand Rnd{ 1 };
};
