#include "ProtoBench.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "DynamicRHI.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"
#include "RenderTimer.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
constexpr double StepSeconds = 0.05;   // world step of the original
constexpr double WarmupSeconds = 5.0;
constexpr double MaxRunSeconds = 150.0;

const TCHAR* const PhaseNames[ssproto::kPhaseCount] = { TEXT("build"), TEXT("pass 1"), TEXT("pass 2"), TEXT("pass 3"), TEXT("rules"), TEXT("reset") };

double CyclesToMs(uint32 Cycles) { return FPlatformTime::ToMilliseconds(Cycles); }

// p-th percentile (0..100) of a copy of the values.
float Percentile(TArray<float> Values, float P)
{
	if (Values.IsEmpty())
		return 0.0f;
	Values.Sort();
	const int32 Index = FMath::Clamp(int32(FMath::RoundToInt(P / 100.0f * float(Values.Num() - 1))), 0, Values.Num() - 1);
	return Values[Index];
}
} // namespace

AProtoBench::AProtoBench()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void AProtoBench::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("SSScenario="), Scenario);
	FParse::Value(Cmd, TEXT("SSBuilding="), BuildingSize);
	FParse::Value(Cmd, TEXT("SSChunk="), ChunkParts);
	FParse::Value(Cmd, TEXT("SSSunLux="), SunLux);
	FParse::Value(Cmd, TEXT("SSLampLumens="), LampLumens);
	bManualExposure = FParse::Value(Cmd, TEXT("SSExposure="), ExposureEV);

	ssproto::TestBuildingDesc Desc;
	if (BuildingSize == TEXT("large"))
	{
		Desc.blocksX = 40;
		Desc.blocksY = 32;
		Desc.floors = 5;
	}
	int32 HideAbove = 4 * Desc.floors - 1;   // hides the roof: the camera looks into the top floor
	FParse::Value(Cmd, TEXT("SSHideAbove="), HideAbove);

	if (!FParse::Value(Cmd, TEXT("SSResults="), ResultsDir))
		ResultsDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SSResults"),
			FString::Printf(TEXT("%s_%s_%s_c%d"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), *Scenario, *BuildingSize, ChunkParts));
	IFileManager::Get().MakeDirectory(*ResultsDir, true);

	// "hundred" measures steady removal without the statics; the others use the original's statics.
	ssproto::ScenarioKind Kind = ssproto::ScenarioKind::Idle;
	bKnownScenario = ssproto::ParseScenario(StringCast<ANSICHAR>(*Scenario).Get(), Kind);
	Building = GetWorld()->SpawnActor<AProtoBuilding>();
	Building->Init(Desc, ssproto::ScenarioMode(Kind), ChunkParts, HideAbove);
	// Damage only where the view shows it, so the camera sees every hit.
	Plan = ssproto::Scenario(Kind, HideAbove);
	PlanShots(Kind);
	if (bKnownScenario)
		ReplayBackToBack(Desc, Kind, HideAbove);
	SetupEnvironment();
	SetupCamera();
	PhaseStart = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("SSProto: scenario %s, building %s, %d instances in %d components; results in %s"), *Scenario,
		*BuildingSize, Building->InstanceCount(), Building->ComponentCount(), *ResultsDir);
}

void AProtoBench::SetupEnvironment()
{
	UWorld* World = GetWorld();
	if (SunLux > 0.0f)
	{
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-35.0, 40.0, 0.0));
		UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
		SunLight->SetMobility(EComponentMobility::Movable);
		SunLight->SetIntensity(SunLux);
		SunLight->SetAtmosphereSunLight(true);
	}
	World->SpawnActor<ASkyAtmosphere>();
	ASkyLight* Sky = World->SpawnActor<ASkyLight>();
	USkyLightComponent* SkyLight = Sky->GetLightComponent();
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetRealTimeCapture(true);

	if (bManualExposure)
	{
		APostProcessVolume* Post = World->SpawnActor<APostProcessVolume>();
		Post->bUnbound = true;
		Post->Settings.bOverride_AutoExposureMethod = true;
		Post->Settings.AutoExposureMethod = AEM_Manual;
		Post->Settings.bOverride_AutoExposureBias = true;
		Post->Settings.AutoExposureBias = ExposureEV;
	}

	// A lamp in every room of the floors that are shown, casting shadows: the case of the prototype.
	const int32 HideAbove = Building->HiddenAbovePlane();
	for (const ssproto::LightSpot& Spot : Building->Info().lights)
	{
		if (HideAbove >= 0 && 4 * Spot.floor > HideAbove)
			continue;
		APointLight* Lamp = World->SpawnActor<APointLight>(AProtoBuilding::ToWorld(Spot.position), FRotator::ZeroRotator);
		UPointLightComponent* Light = Lamp->PointLightComponent;
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(LampLumens);
		Light->SetAttenuationRadius(Spot.radius * 100.0f);
		Light->SetLightColor(FLinearColor(1.0f, 0.85f, 0.65f));
		Light->SetCastShadows(true);
	}
}

void AProtoBench::SetupCamera()
{
	const FBox Bounds = Building->WorldBounds();
	const FVector Center = Bounds.GetCenter();
	const FVector Size = Bounds.GetSize();
	// A tactical view from above and from a corner, as in the original.
	const FVector Eye = Center + FVector(-0.75 * Size.X, -0.95 * Size.Y, 0.9 * Size.X);
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Center - Eye).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(55.0f);
	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Player->bAutoManageActiveCameraTarget = false;
		Player->SetViewTarget(Camera);
	}
}

void AProtoBench::PlanShots(ssproto::ScenarioKind Kind)
{
	// Screenshots after the world step with this number (a step is 50 ms).
	const int32 First = ssproto::kScenarioFirstDamageStep;
	switch (Kind)
	{
	case ssproto::ScenarioKind::Grenades:
		PlannedShots.Emplace(First + 10, TEXT("02_grenade1_0.5s"));
		PlannedShots.Emplace(First + 4 * ssproto::kGrenadeInterval + 40, TEXT("03_grenade5_2s"));
		break;
	case ssproto::ScenarioKind::Half:
		PlannedShots.Emplace(First + 2, TEXT("02_cut_0.1s"));
		PlannedShots.Emplace(First + 10, TEXT("03_cut_0.5s"));
		PlannedShots.Emplace(First + 40, TEXT("04_cut_2s"));
		break;
	case ssproto::ScenarioKind::Hundred:
		PlannedShots.Emplace(First + 10, TEXT("02_hundred_0.5s"));
		break;
	default:
		break;
	}
}

void AProtoBench::ReplayBackToBack(const ssproto::TestBuildingDesc& Desc, ssproto::ScenarioKind Kind, int32 HideAbove)
{
	// The whole scenario once more on a copy of the building, every step right after the previous
	// one: what the steps cost with a busy CPU and warm caches. In the game the CPU may idle between
	// the 50 ms steps, and on some machines a step then takes several times longer (prototype/README.md).
	ssproto::Building Twin(ssproto::GenerateTestBuilding(Desc), ssproto::ScenarioMode(Kind));
	ssproto::Scenario TwinPlan(Kind, HideAbove);
	ssproto::Trace Hash;
	ssproto::StepResult Result;
	for (int32 S = 0;; ++S)
	{
		TwinPlan.BeforeStep(Twin, S);
		const double T0 = FPlatformTime::Seconds();
		Twin.EndStep(Result);
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		Hash.Step(Result);
		TwinTotalMs += Ms;
		if (Ms > TwinHeaviestMs)
		{
			TwinHeaviestMs = Ms;
			TwinHeaviestStep = S;
		}
		if (TwinPlan.Finished(Twin, S))
		{
			TwinSteps = S + 1;
			break;
		}
	}
	TwinTrace = Hash.h;
}

bool AProtoBench::ShadersBusy() const
{
#if WITH_EDITOR
	return GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
#else
	return false;
#endif
}

void AProtoBench::Shot(const TCHAR* Name)
{
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(ResultsDir, FString(Name) + TEXT(".png")), false, false);
}

void AProtoBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = FPlatformTime::Seconds();
	switch (Phase)
	{
	case EPhase::WaitShaders:
		if (!ShadersBusy())
		{
			Phase = EPhase::Warmup;
			PhaseStart = Now;
		}
		return;
	case EPhase::Warmup:
		if (Now - PhaseStart >= WarmupSeconds)
		{
			Phase = EPhase::Run;
			RunStart = Now;
			NextStep = Now;
			Shot(TEXT("01_before"));
		}
		return;
	case EPhase::Done:
		return;
	default:
		break;
	}

	if (!bKnownScenario)
	{
		Finish(TEXT("unknown scenario"));
		return;
	}
	const double T = Now - RunStart;
	FProtoStepStats Stats;
	bool bOver = false;
	for (int32 Guard = 0; Now >= NextStep && Guard < 4 && !bOver; ++Guard)
	{
		Plan.BeforeStep(Building->Core(), StepIndex);
		FProtoStepRow& StepRow = StepRows.AddDefaulted_GetRef();
		StepRow.Step = StepIndex;
		StepRow.Time = float(T);
		Building->Step(Stats, StepRow);
		StepTrace.Step(Building->LastResult());
		for (int32 I = PlannedShots.Num() - 1; I >= 0; --I)
		{
			if (PlannedShots[I].Key <= StepIndex)
			{
				Shot(*PlannedShots[I].Value);
				PlannedShots.RemoveAt(I);
			}
		}
		bOver = Plan.Finished(Building->Core(), StepIndex);
		++StepIndex;
		NextStep += StepSeconds;
	}
	if (Now > NextStep + 1.0)
		NextStep = Now;   // after a long hitch do not try to catch up

	FFrameRow Row;
	Row.Time = float(T);
	Row.FrameMs = DeltaSeconds * 1000.0f;
	Row.GameMs = float(CyclesToMs(GGameThreadTime));
	Row.RenderMs = float(CyclesToMs(GRenderThreadTime));
	Row.RhiMs = float(CyclesToMs(GRHIThreadTime));
	Row.GpuMs = float(CyclesToMs(RHIGetGPUFrameCycles(0)));
	Row.CoreMs = float(Stats.CoreMs);
	Row.ApplyMs = float(Stats.ApplyMs);
	Row.Steps = Stats.Steps;
	Row.Destroyed = Stats.Destroyed;
	Row.Broken = Stats.Broken;
	Row.Detached = Stats.Detached;
	Row.Instances = Building->InstanceCount();
	Row.bBusy = Stats.bBusy;
	Frames.Add(Row);

	if (bOver)
		Finish(TEXT("scenario over"));
	else if (T >= MaxRunSeconds)
		Finish(TEXT("time limit"));
}

FString AProtoBench::StepsReport() const
{
	FString Csv = TEXT("step,time_s,core_ms,build_ms,pass1_ms,pass2_ms,pass3_ms,rules_ms,reset_ms,apply_ms,destroyed,broken,detached,busy,pieces_left\n");
	double TotalMs = 0.0, WorkMs = 0.0;
	int32 WorkSteps = 0;
	const FProtoStepRow* Heaviest = nullptr;
	for (const FProtoStepRow& S : StepRows)
	{
		Csv += FString::Printf(TEXT("%d,%.4f,%.4f"), S.Step, S.Time, S.CoreMs);
		for (int32 I = 0; I < ssproto::kPhaseCount; ++I)
			Csv += FString::Printf(TEXT(",%.4f"), S.PhaseMs[I]);
		Csv += FString::Printf(TEXT(",%.4f,%d,%d,%d,%d,%u\n"), S.ApplyMs, S.Destroyed, S.Broken, S.Detached, S.bBusy ? 1 : 0, S.PiecesLeft);
		TotalMs += S.CoreMs;
		if (S.CoreMs > 0.1)
		{
			WorkMs += S.CoreMs;
			++WorkSteps;
		}
		if (!Heaviest || S.CoreMs > Heaviest->CoreMs)
			Heaviest = &S;
	}
	FFileHelper::SaveStringToFile(Csv, *FPaths::Combine(ResultsDir, TEXT("steps.csv")));

	// The same two lines as ssproto_bench scenarios prints for the same scenario.
	FString Report = FString::Printf(TEXT("world steps %d, all steps %.1f ms; steps over 0.1 ms: %d, mean %.3f ms; pieces left %u; trace %016llx\n"),
		StepRows.Num(), TotalMs, WorkSteps, WorkSteps ? WorkMs / double(WorkSteps) : 0.0, Building->Core().PiecesLeft(),
		(unsigned long long)StepTrace.h);
	if (Heaviest)
	{
		Report += FString::Printf(TEXT("the heaviest step (%d) %.3f ms:"), Heaviest->Step, Heaviest->CoreMs);
		double InSchema = 0.0;
		for (int32 I = 0; I < ssproto::kPhaseCount; ++I)
		{
			Report += FString::Printf(TEXT("%s %s %.3f"), I ? TEXT(",") : TEXT(""), PhaseNames[I], Heaviest->PhaseMs[I]);
			InSchema += Heaviest->PhaseMs[I];
		}
		Report += FString::Printf(TEXT(", outside the schema %.3f\n"), Heaviest->CoreMs - InSchema);
	}
	if (TwinSteps > 0)
	{
		const bool bSame = TwinTrace == StepTrace.h && TwinSteps == StepRows.Num();
		Report += FString::Printf(TEXT("the same steps back to back before the run: all steps %.1f ms, the heaviest %.3f ms (step %d); trace %016llx (%s)\n"),
			TwinTotalMs, TwinHeaviestMs, TwinHeaviestStep, (unsigned long long)TwinTrace, bSame ? TEXT("equal") : TEXT("DIFFERENT"));
	}
	return Report;
}

void AProtoBench::Finish(const TCHAR* Reason)
{
	if (Phase == EPhase::Done)
		return;
	Phase = EPhase::Done;
	Shot(TEXT("05_end"));

	FString Csv = TEXT("time_s,frame_ms,game_ms,render_ms,rhi_ms,gpu_ms,core_ms,apply_ms,steps,destroyed,broken,detached,instances,busy\n");
	TArray<float> Frame, Game, Render, Gpu, CollapseFrame, CollapseGpu;
	double CoreMax = 0.0, ApplyMax = 0.0;
	int32 RemovedMax = 0, RemovedTotal = 0;
	for (const FFrameRow& R : Frames)
	{
		Csv += FString::Printf(TEXT("%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d\n"), R.Time, R.FrameMs, R.GameMs, R.RenderMs, R.RhiMs,
			R.GpuMs, R.CoreMs, R.ApplyMs, R.Steps, R.Destroyed, R.Broken, R.Detached, R.Instances, R.bBusy ? 1 : 0);
		Frame.Add(R.FrameMs);
		Game.Add(R.GameMs);
		Render.Add(R.RenderMs);
		Gpu.Add(R.GpuMs);
		const int32 Removed = R.Destroyed + R.Broken + R.Detached;
		if (R.bBusy || Removed > 0)
		{
			CollapseFrame.Add(R.FrameMs);
			CollapseGpu.Add(R.GpuMs);
		}
		CoreMax = FMath::Max(CoreMax, double(R.CoreMs));
		ApplyMax = FMath::Max(ApplyMax, double(R.ApplyMs));
		RemovedMax = FMath::Max(RemovedMax, Removed);
		RemovedTotal += Removed;
	}
	FFileHelper::SaveStringToFile(Csv, *FPaths::Combine(ResultsDir, TEXT("frames.csv")));

	auto Line = [](const TCHAR* Name, const TArray<float>& V) {
		return FString::Printf(TEXT("%-28s p50 %7.2f  p95 %7.2f  p99 %7.2f  max %7.2f ms\n"), Name, Percentile(V, 50), Percentile(V, 95),
			Percentile(V, 99), Percentile(V, 100));
	};
	FString Summary;
	Summary += FString::Printf(TEXT("SSProto bench: scenario %s, building %s, chunk %d, finished: %s\n"), *Scenario, *BuildingSize, ChunkParts, Reason);
	Summary += FString::Printf(TEXT("CPU: %s\nGPU: %s (%s)\n"), *FPlatformMisc::GetCPUBrand().TrimStartAndEnd(), *GRHIAdapterName,
		GDynamicRHI ? GDynamicRHI->GetName() : TEXT("no RHI"));
	Summary += FString::Printf(TEXT("components %d, instances at the end %d; frames %d over %.1f s\n"), Building->ComponentCount(),
		Building->InstanceCount(), Frames.Num(), Frames.IsEmpty() ? 0.0f : Frames.Last().Time);
	Summary += Line(TEXT("frame, all"), Frame);
	Summary += Line(TEXT("game thread, all"), Game);
	Summary += Line(TEXT("render thread, all"), Render);
	Summary += Line(TEXT("GPU, all"), Gpu);
	Summary += Line(TEXT("frame, while collapsing"), CollapseFrame);
	Summary += Line(TEXT("GPU, while collapsing"), CollapseGpu);
	Summary += FString::Printf(TEXT("core max %.2f ms per frame; instance removal max %.2f ms per frame; pieces removed %d, at most %d in one frame\n"),
		CoreMax, ApplyMax, RemovedTotal, RemovedMax);
	Summary += StepsReport();
	FFileHelper::SaveStringToFile(Summary, *FPaths::Combine(ResultsDir, TEXT("summary.txt")));
	UE_LOG(LogTemp, Display, TEXT("SSProto finished (%s):\n%s"), Reason, *Summary);

	// Leave a few frames for the last screenshot, then quit.
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, []() { FPlatformMisc::RequestExit(false, TEXT("SSProto bench finished")); }, 1.0f, false);
}
