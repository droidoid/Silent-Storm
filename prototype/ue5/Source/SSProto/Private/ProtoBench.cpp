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

#include <algorithm>
#include <vector>

#include "ProtoBuilding.h"

namespace
{
constexpr double StepSeconds = 0.05;   // world step of the original
constexpr double WarmupSeconds = 5.0;
constexpr double MaxRunSeconds = 150.0;

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
	const ssproto::CollapseMode Mode = Scenario == TEXT("hundred") ? ssproto::CollapseMode::Connectivity : ssproto::CollapseMode::Original;
	Building = GetWorld()->SpawnActor<AProtoBuilding>();
	Building->Init(Desc, Mode, ChunkParts, HideAbove);
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

void AProtoBench::ExplodeAtRandomPiece()
{
	const ssproto::Lattice& Lat = Building->Core().GetLattice();
	for (int32 Attempt = 0; Attempt < 100000; ++Attempt)
	{
		const ssproto::NodeId N = ((uint32(Rnd.Next()) << 15) | uint32(Rnd.Next())) % Lat.NodeCount();
		int X, Y, Z;
		Lat.Coords(N, X, Y, Z);
		const bool bShown = Building->HiddenAbovePlane() < 0 || Z <= Building->HiddenAbovePlane();
		if (Lat.hp[N] != ssproto::kHpEmpty && Lat.hp[N] < ssproto::kHpCellar && bShown)
		{
			Building->Core().Explode(Lat.NodeCenter(N), 1000, 10.0f);   // a grenade-sized lattice explosion
			return;
		}
	}
}

void AProtoBench::DestroyNearest(int32 Count)
{
	const ssproto::Lattice& Lat = Building->Core().GetLattice();
	ssproto::NodeId Target = 0;
	bool bFound = false;
	for (int32 Attempt = 0; Attempt < 100000 && !bFound; ++Attempt)
	{
		Target = ((uint32(Rnd.Next()) << 15) | uint32(Rnd.Next())) % Lat.NodeCount();
		bFound = Lat.hp[Target] != ssproto::kHpEmpty && Lat.hp[Target] < ssproto::kHpCellar;
	}
	if (!bFound)
		return;
	const ssproto::Vec3 C = Lat.NodeCenter(Target);
	std::vector<ssproto::NodeId> Near;
	for (float Radius = 1.0f; Radius < 40.0f; Radius *= 1.3f)
	{
		Building->Core().OverlapSphere(C, Radius, Near);
		if (int32(Near.size()) >= Count + Count / 3)
			break;
	}
	std::sort(Near.begin(), Near.end(), [&](ssproto::NodeId A, ssproto::NodeId B) {
		return ssproto::LengthSq(Lat.NodeCenter(A) - C) < ssproto::LengthSq(Lat.NodeCenter(B) - C);
	});
	int32 Done = 0;
	for (const ssproto::NodeId N : Near)
	{
		if (Done == Count)
			break;
		if (Building->Core().DamageSpot(N, 255))
			++Done;
	}
}

void AProtoBench::CutHalf()
{
	// The same cut as in the core bench: a slice through the middle and the walls of plane 1 to its left.
	ssproto::Building& Core = Building->Core();
	const ssproto::Lattice& Lat = Core.GetLattice();
	const int Cut = Lat.nx / 2;
	for (int Z = 1; Z < Lat.nz; ++Z)
		for (int Y = 0; Y < Lat.ny; ++Y)
			Core.DamageSpot(Lat.Node(Cut, Y, Z), 255);
	for (int Y = 0; Y < Lat.ny; ++Y)
		for (int X = 0; X < Cut; ++X)
			Core.DamageSpot(Lat.Node(X, Y, 1), 255);
}

void AProtoBench::BeforeStep(double T)
{
	if (Scenario == TEXT("hundred") && T >= 2.0 && Building->Core().PiecesLeft() > 2000)
	{
		DestroyNearest(100);
		LastDamageTime = T;
	}
}

void AProtoBench::ScenarioTick(double T)
{
	auto Plan = [this](double At, const TCHAR* Name) { PlannedShots.Add(TPair<double, FString>(At, Name)); };
	const bool bSettled = !Building->Core().StaticsActive();
	if (Scenario == TEXT("idle"))
	{
		if (T >= 10.0)
			Finish(TEXT("idle time over"));
		return;
	}
	if (Scenario == TEXT("grenades"))
	{
		if (Grenades < 10 && T >= 2.0 + 3.0 * Grenades)
		{
			ExplodeAtRandomPiece();
			++Grenades;
			LastDamageTime = T;
			if (Grenades == 1)
				Plan(T + 0.5, TEXT("02_grenade1_0.5s"));
			if (Grenades == 5)
				Plan(T + 2.0, TEXT("03_grenade5_2s"));
		}
		if (Grenades == 10 && bSettled && T > LastDamageTime + 3.0)
			Finish(TEXT("all grenades settled"));
	}
	else if (Scenario == TEXT("half"))
	{
		if (!bCut && T >= 2.0)
		{
			CutHalf();
			bCut = true;
			LastDamageTime = T;
			Plan(T + 0.1, TEXT("02_cut_0.1s"));
			Plan(T + 0.5, TEXT("03_cut_0.5s"));
			Plan(T + 2.0, TEXT("04_cut_2s"));
		}
		if (bCut && bSettled && T > LastDamageTime + 3.0)
			Finish(TEXT("collapse settled"));
	}
	else if (Scenario == TEXT("hundred"))
	{
		if (T >= 2.5 && T < 2.55)
			Plan(T, TEXT("02_hundred_0.5s"));
		if (T >= 12.0 || (LastDamageTime >= 0.0 && Building->Core().PiecesLeft() <= 2000 && T > LastDamageTime + 1.0))
			Finish(TEXT("removal time over"));
	}
	else
	{
		Finish(TEXT("unknown scenario"));
	}
	if (T >= MaxRunSeconds)
		Finish(TEXT("time limit"));
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

	const double T = Now - RunStart;
	ScenarioTick(T);
	if (Phase == EPhase::Done)
		return;
	FProtoStepStats Stats;
	for (int32 Guard = 0; Now >= NextStep && Guard < 4; ++Guard)
	{
		BeforeStep(T);
		Building->Step(Stats);
		NextStep += StepSeconds;
	}
	if (Now > NextStep + 1.0)
		NextStep = Now;   // after a long hitch do not try to catch up

	for (int32 I = PlannedShots.Num() - 1; I >= 0; --I)
	{
		if (T >= PlannedShots[I].Key)
		{
			Shot(*PlannedShots[I].Value);
			PlannedShots.RemoveAt(I);
		}
	}

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
	Summary += FString::Printf(TEXT("core step max %.2f ms; instance removal max %.2f ms per frame; pieces removed %d, at most %d in one frame\n"),
		CoreMax, ApplyMax, RemovedTotal, RemovedMax);
	FFileHelper::SaveStringToFile(Summary, *FPaths::Combine(ResultsDir, TEXT("summary.txt")));
	UE_LOG(LogTemp, Display, TEXT("SSProto finished (%s):\n%s"), Reason, *Summary);

	// Leave a few frames for the last screenshot, then quit.
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, []() { FPlatformMisc::RequestExit(false, TEXT("SSProto bench finished")); }, 1.0f, false);
}
