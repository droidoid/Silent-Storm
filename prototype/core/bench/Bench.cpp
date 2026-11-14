// Timings of the prototype core on the machine it runs on (CPU only, no engine). Plain text to
// stdout; the results kept in git are this output plus a machine header.
//
// The statics scenarios also print traces: FNV-1a hashes of everything they produce. A change that
// only makes the code faster must leave every trace as it was.
//
//   ssproto_bench                     the core's own scenarios (the results in git)
//   ssproto_bench scenarios [options] the scenarios of the engine layer (ssproto/Scenario.h), step by
//                                     step, to compare with a run in the engine one step to one step:
//       --paced MS                    sleep MS before every step (a game steps the world every 50 ms)
//       --spin                        with --paced: wait busy instead of sleeping (the CPU never idles)
//       --csv DIR                     write every step to DIR/<building>_<scenario>.csv
//       --building small|large        only this building
//       --scenario NAME               only this scenario
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ssproto/Building.h"
#include "ssproto/Scenario.h"
#include "ssproto/Statics.h"
#include "ssproto/TestBuilding.h"
#include "ssproto/Trace.h"

using namespace ssproto;

namespace {

using Clock = std::chrono::steady_clock;

double Ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

struct Stats {
	double sum = 0, max = 0;
	size_t n = 0;
	void Add(double v)
	{
		sum += v;
		max = std::max(max, v);
		++n;
	}
	double Avg() const { return n ? sum / double(n) : 0.0; }
};

// Traces of all statics scenarios of the run, printed at the end.
Trace g_stepsTrace, g_passesTrace;

// Passes 1-3 on a fresh schema of the lattice: stability, pressure and moments of every piece.
void HashPasses(const Lattice& lat, Trace& t)
{
	Schema s;
	s.Build(lat);
	s.RunPasses();
	t.U32(uint32_t(s.JointCount()));
	t.U32(uint32_t(s.RodCount()));
	for (NodeId n = 0; n < lat.NodeCount(); ++n) {
		if (lat.hp[n] == kHpEmpty)
			continue;
		t.U32(n);
		t.Byte(uint8_t(s.StabilityOfNode(n)));
		t.F32(s.WeightOfNode(n));
		t.F32(s.PressureOfNode(n));
		for (int d = kXP; d <= kYM; ++d)
			t.F32(s.MomentOfNode(n, d));
	}
}

const char* const kPhaseNames[kPhaseCount] = { "build", "pass 1", "pass 2", "pass 3", "rules", "reset" };

void PrintPhases(const StaticsProfile& p)
{
	std::printf("     phases, total ms (runs, longest run ms):");
	for (int i = 0; i < kPhaseCount; ++i)
		std::printf("%s %s %.2f (%llu, %.3f)", i ? "," : "", kPhaseNames[i], p.ms[i], (unsigned long long)p.runs[i], p.maxMs[i]);
	std::printf("\n");
}

uint32_t Rand30(SRand& rnd) { return (uint32_t(rnd.Next()) << 15) | uint32_t(rnd.Next()); }

// A random piece that damage can destroy.
bool RandomPiece(SRand& rnd, const Lattice& lat, NodeId& out)
{
	for (int attempt = 0; attempt < 100000; ++attempt) {
		const NodeId n = Rand30(rnd) % lat.NodeCount();
		if (lat.hp[n] != kHpEmpty && lat.hp[n] < kHpCellar) {
			out = n;
			return true;
		}
	}
	return false;
}

void Grenades(const Lattice& lat)
{
	Building b(lat, CollapseMode::Connectivity);
	SRand rnd(1);
	StepResult r;
	Stats explodeMs, stepMs, destroyed, detached, dirty;
	for (int i = 0; i < 100; ++i) {
		NodeId target;
		if (!RandomPiece(rnd, b.GetLattice(), target))
			break;
		const Vec3 c = b.GetLattice().NodeCenter(target);
		const auto a = Clock::now();
		b.Explode(c, 1000, 10.0f);
		const auto m = Clock::now();
		b.EndStep(r);
		const auto e = Clock::now();
		explodeMs.Add(Ms(m - a));
		stepMs.Add(Ms(e - m));
		destroyed.Add(double(r.destroyed.size()));
		detached.Add(double(r.detached.size()));
		dirty.Add(double(r.dirtyParts.size()));
	}
	std::printf("   [connectivity] grenades: lattice explosion P = 1000, R = 10 m at a random piece, one per step, %zu steps\n", stepMs.n);
	std::printf("     destroyed per step avg %.0f, max %.0f; detached avg %.0f, max %.0f; dirty parts avg %.0f, max %.0f\n",
		destroyed.Avg(), destroyed.max, detached.Avg(), detached.max, dirty.Avg(), dirty.max);
	std::printf("     Explode avg %.3f ms, max %.3f ms; EndStep (collapse check) avg %.3f ms, max %.3f ms; pieces left %u\n",
		explodeMs.Avg(), explodeMs.max, stepMs.Avg(), stepMs.max, b.PiecesLeft());
}

void HundredPerStep(const Lattice& lat)
{
	Building b(lat, CollapseMode::Connectivity);
	SRand rnd(2);
	StepResult r;
	Stats damageMs, stepMs, detached;
	std::vector<NodeId> nearby;
	std::vector<std::pair<float, NodeId>> sorted;
	for (int i = 0; i < 100; ++i) {
		NodeId target;
		if (!RandomPiece(rnd, b.GetLattice(), target))
			break;
		const Vec3 c = b.GetLattice().NodeCenter(target);
		for (float radius = 1.0f;; radius *= 1.3f) {
			b.OverlapSphere(c, radius, nearby);
			if (nearby.size() >= 130 || radius > 40.0f)
				break;
		}
		sorted.clear();
		for (const NodeId n : nearby)
			sorted.push_back({ LengthSq(b.GetLattice().NodeCenter(n) - c), n });
		std::sort(sorted.begin(), sorted.end());
		const auto a = Clock::now();
		int count = 0;
		for (const auto& item : sorted) {
			if (count == 100)
				break;
			if (b.DamageSpot(item.second, 255))
				++count;
		}
		const auto m = Clock::now();
		b.EndStep(r);
		const auto e = Clock::now();
		damageMs.Add(Ms(m - a));
		stepMs.Add(Ms(e - m));
		detached.Add(double(r.detached.size()));
	}
	std::printf("   [connectivity] 100 pieces per step (the nearest to a random piece), %zu steps\n", stepMs.n);
	std::printf("     DamageSpot x 100 avg %.3f ms; EndStep avg %.3f ms, max %.3f ms; detached avg %.0f, max %.0f; pieces left %u\n",
		damageMs.Avg(), stepMs.Avg(), stepMs.max, detached.Avg(), detached.max, b.PiecesLeft());
}

void HalfCollapse(const Lattice& lat)
{
	Building b(lat, CollapseMode::Connectivity);
	const Lattice& l = b.GetLattice();
	const int cut = l.nx / 2;
	StepResult r;
	const auto a = Clock::now();
	for (int z = 1; z < l.nz; ++z)
		for (int y = 0; y < l.ny; ++y)
			b.DamageSpot(l.Node(cut, y, z), 255);
	for (int y = 0; y < l.ny; ++y)
		for (int x = 0; x < cut; ++x)
			b.DamageSpot(l.Node(x, y, 1), 255);
	const auto m = Clock::now();
	b.EndStep(r);
	const auto e = Clock::now();
	std::printf("   [connectivity] collapse of half the building (cut through x = %d and the walls of plane 1 to its left)\n", cut);
	std::printf("     destroyed %zu, detached %zu, dirty parts %zu; damage %.3f ms, EndStep %.3f ms\n", r.destroyed.size(),
		r.detached.size(), r.dirtyParts.size(), Ms(m - a), Ms(e - m));
}

// Damages the building with the original's rhythm and steps until the statics settle.
struct Settle {
	int steps = 0;
	size_t broken = 0, detached = 0;
	size_t maxGoneInStep = 0;   // destroyed + broken + detached in one step: what the engine removes at once
	double totalMs = 0, maxStepMs = 0;
	double maxStepPhases[kPhaseCount] = {};   // where the time of the heaviest step went
};

// One world step of a scenario: its time and where it went.
struct StepTime {
	double ms;
	double phases[kPhaseCount];
};

// The building must have the profile attached (Building::SetStaticsProfile). Every step is appended
// to times.
Settle StepUntilSettled(Building& b, const StaticsProfile& profile, Trace& trace, std::vector<StepTime>& times)
{
	Settle s;
	StepResult r;
	for (; s.steps < 1000; ++s.steps) {
		double before[kPhaseCount];
		std::memcpy(before, profile.ms, sizeof before);
		const auto a = Clock::now();
		b.EndStep(r);
		const double ms = Ms(Clock::now() - a);
		trace.Step(r);
		StepTime t;
		t.ms = ms;
		for (int i = 0; i < kPhaseCount; ++i)
			t.phases[i] = profile.ms[i] - before[i];
		times.push_back(t);
		s.totalMs += ms;
		if (ms > s.maxStepMs) {
			s.maxStepMs = ms;
			std::memcpy(s.maxStepPhases, t.phases, sizeof t.phases);
		}
		s.broken += r.broken.size();
		s.detached += r.detached.size();
		s.maxGoneInStep = std::max(s.maxGoneInStep, r.destroyed.size() + r.broken.size() + r.detached.size());
		if (!b.StaticsActive()) {
			++s.steps;
			break;
		}
	}
	return s;
}

void PrintStepPhases(const char* what, double stepMs, const double* phases)
{
	std::printf("     %s %.3f ms:", what, stepMs);
	double sum = 0;
	for (int i = 0; i < kPhaseCount; ++i) {
		std::printf("%s %s %.3f", i ? "," : "", kPhaseNames[i], phases[i]);
		sum += phases[i];
	}
	std::printf(", outside the schema %.3f\n", stepMs - sum);
}

// A statics scenario runs this many times. The steps are the same every time (the traces must be
// equal); every step counts with its fastest run, so other work on the machine shows less.
constexpr int kRuns = 3;

struct ScenarioRun {
	std::vector<StepTime> steps;
	uint64_t trace = 0;
};

void PrintFastest(const std::vector<ScenarioRun>& runs, size_t scenarios)
{
	bool same = true;
	for (const ScenarioRun& run : runs)
		same = same && run.trace == runs[0].trace && run.steps.size() == runs[0].steps.size();
	if (!same) {
		std::printf("     the %zu runs differ: no fastest-of-runs timing\n", runs.size());
		return;
	}
	double heaviest = 0, total = 0;
	const StepTime* heaviestStep = nullptr;
	for (size_t i = 0; i < runs[0].steps.size(); ++i) {
		const StepTime* fastest = &runs[0].steps[i];
		for (const ScenarioRun& run : runs)
			if (run.steps[i].ms < fastest->ms)
				fastest = &run.steps[i];
		total += fastest->ms;
		if (fastest->ms > heaviest) {
			heaviest = fastest->ms;
			heaviestStep = fastest;
		}
	}
	std::printf("     every step its fastest of %zu runs (the traces of the runs are equal): all steps %.3f ms", runs.size(), total);
	if (scenarios > 1)
		std::printf(", of one grenade avg %.3f ms", total / double(scenarios));
	std::printf("\n");
	if (heaviestStep)
		PrintStepPhases("the heaviest step", heaviest, heaviestStep->phases);
}

void StaticsCreate(const Lattice& lat)
{
	const auto a = Clock::now();
	Building b(lat);
	const double createMs = Ms(Clock::now() - a);
	Trace created;
	created.U32(b.PiecesLeft());
	Schema schema;
	StaticsProfile profile;
	schema.SetProfile(&profile);
	const auto c = Clock::now();
	schema.Build(lat);
	const auto m = Clock::now();
	schema.RunPasses();
	const auto e = Clock::now();
	std::printf("   [statics] entering the world (schema + first iteration): %.2f ms; schema of the intact building: %zu joints, %zu rods, built in %.2f ms, passes 1-3 in %.2f ms\n",
		createMs, schema.JointCount(), schema.RodCount(), Ms(m - c), Ms(e - m));
	std::printf("     intact building: pass 1 %.3f ms, pass 2 %.3f ms, pass 3 %.3f ms\n", profile.ms[kPhasePass1],
		profile.ms[kPhasePass2], profile.ms[kPhasePass3]);
	Trace passes;
	HashPasses(lat, passes);
	std::printf("     trace: entering the world %016llx, passes of the intact building %016llx\n", (unsigned long long)created.h,
		(unsigned long long)passes.h);
	g_stepsTrace.U64(created.h);
	g_passesTrace.U64(passes.h);
	// Fastest of kRuns: entering the world, the schema built into a new Schema, and into one built
	// before (a building keeps its schema's memory between collapses).
	double enterMs = createMs, newMs = 1e30, againMs = 1e30;
	for (int r = 0; r < kRuns; ++r) {
		if (r > 0) {
			const auto t = Clock::now();
			const Building again(lat);
			enterMs = std::min(enterMs, Ms(Clock::now() - t));
		}
		const auto t0 = Clock::now();
		{
			Schema fresh;
			fresh.Build(lat);
			newMs = std::min(newMs, Ms(Clock::now() - t0));
		}
		const auto t1 = Clock::now();
		schema.Build(lat);
		againMs = std::min(againMs, Ms(Clock::now() - t1));
	}
	std::printf("     fastest of %d runs: entering the world %.2f ms; schema built into a new schema %.2f ms, into one built before %.2f ms\n",
		kRuns, enterMs, newMs, againMs);
}

// The first run prints the details and also hashes passes 1-3 of a fresh schema after every grenade.
ScenarioRun GrenadesRun(const Lattice& lat, bool first, size_t& grenades)
{
	Building b(lat);
	StaticsProfile profile;
	b.SetStaticsProfile(&profile);
	Trace trace, passes;
	ScenarioRun run;
	SRand rnd(1);
	Stats steps, maxStep, total, broken, detached, goneInStep;
	double heaviestMs = 0;
	double heaviestPhases[kPhaseCount] = {};
	for (int i = 0; i < 30; ++i) {
		NodeId target;
		if (!RandomPiece(rnd, b.GetLattice(), target))
			break;
		b.Explode(b.GetLattice().NodeCenter(target), 1000, 10.0f);
		if (first)
			HashPasses(b.GetLattice(), passes);   // not timed: the schema of the building is built later
		const Settle s = StepUntilSettled(b, profile, trace, run.steps);
		steps.Add(double(s.steps));
		maxStep.Add(s.maxStepMs);
		total.Add(s.totalMs);
		broken.Add(double(s.broken));
		detached.Add(double(s.detached));
		goneInStep.Add(double(s.maxGoneInStep));
		if (s.maxStepMs > heaviestMs) {
			heaviestMs = s.maxStepMs;
			std::memcpy(heaviestPhases, s.maxStepPhases, sizeof heaviestPhases);
		}
	}
	run.trace = trace.h;
	grenades = steps.n;
	if (!first)
		return run;
	std::printf("   [statics] grenades, each followed by steps until the statics settle, %zu grenades\n", steps.n);
	std::printf("     steps to settle avg %.1f, max %.0f; broken by overload avg %.1f, max %.0f; fell free avg %.1f, max %.0f\n",
		steps.Avg(), steps.max, broken.Avg(), broken.max, detached.Avg(), detached.max);
	std::printf("     the heaviest step (schema build or one iteration) avg %.3f ms, max %.3f ms; all steps of one grenade avg %.3f ms\n",
		maxStep.Avg(), maxStep.max, total.Avg());
	std::printf("     the most pieces gone in one step (destroyed + broken + fell): avg %.0f, max %.0f\n", goneInStep.Avg(), goneInStep.max);
	std::printf("     pieces left %u, of them without a chain to a support: %zu\n", b.PiecesLeft(), b.CountUnsupported());
	PrintPhases(profile);
	PrintStepPhases("the heaviest step", heaviestMs, heaviestPhases);
	std::printf("     trace: steps %016llx, passes after every grenade %016llx\n", (unsigned long long)trace.h,
		(unsigned long long)passes.h);
	g_stepsTrace.U64(trace.h);
	g_passesTrace.U64(passes.h);
	return run;
}

void StaticsGrenades(const Lattice& lat)
{
	std::vector<ScenarioRun> runs;
	size_t grenades = 0;
	for (int r = 0; r < kRuns; ++r)
		runs.push_back(GrenadesRun(lat, r == 0, grenades));
	PrintFastest(runs, grenades);
}

ScenarioRun HalfCollapseRun(const Lattice& lat, bool first)
{
	Building b(lat);
	StaticsProfile profile;
	b.SetStaticsProfile(&profile);
	Trace trace, passes;
	ScenarioRun run;
	const Lattice& l = b.GetLattice();
	const int cut = l.nx / 2;
	for (int z = 1; z < l.nz; ++z)
		for (int y = 0; y < l.ny; ++y)
			b.DamageSpot(l.Node(cut, y, z), 255);
	for (int y = 0; y < l.ny; ++y)
		for (int x = 0; x < cut; ++x)
			b.DamageSpot(l.Node(x, y, 1), 255);
	if (first)
		HashPasses(l, passes);
	const Settle s = StepUntilSettled(b, profile, trace, run.steps);
	run.trace = trace.h;
	if (!first)
		return run;
	std::printf("   [statics] collapse of half the building: settled in %d steps; broken %zu, fell free %zu; heaviest step %.3f ms\n",
		s.steps, s.broken, s.detached, s.maxStepMs);
	PrintPhases(profile);
	PrintStepPhases("the heaviest step", s.maxStepMs, s.maxStepPhases);
	std::printf("     trace: steps %016llx, passes after the cut %016llx\n", (unsigned long long)trace.h, (unsigned long long)passes.h);
	g_stepsTrace.U64(trace.h);
	g_passesTrace.U64(passes.h);
	return run;
}

void StaticsHalfCollapse(const Lattice& lat)
{
	std::vector<ScenarioRun> runs;
	for (int r = 0; r < kRuns; ++r)
		runs.push_back(HalfCollapseRun(lat, r == 0));
	PrintFastest(runs, 1);
}

void Rays(const Lattice& lat)
{
	Building b(lat, CollapseMode::Connectivity);
	SRand rnd(4);
	const Aabb bounds = b.GetLattice().Bounds();
	const int count = 100000;
	std::vector<std::pair<Vec3, Vec3>> rays;
	rays.reserve(count);
	for (int i = 0; i < count; ++i) {
		const Vec3 o = { rnd.GetFloat(bounds.min.x - 20, bounds.max.x + 20), rnd.GetFloat(bounds.min.y - 20, bounds.max.y + 20),
			rnd.GetFloat(bounds.min.z, bounds.max.z + 10) };
		const Vec3 t = { rnd.GetFloat(bounds.min.x, bounds.max.x), rnd.GetFloat(bounds.min.y, bounds.max.y),
			rnd.GetFloat(bounds.min.z, bounds.max.z) };
		rays.push_back({ o, t - o });
	}
	std::vector<RayHit> hits;
	size_t total = 0;
	const auto a = Clock::now();
	for (const auto& ray : rays) {
		b.Raycast(ray.first, ray.second, 200.0f, hits);
		total += hits.size();
	}
	const auto e = Clock::now();
	std::printf("   rays: %d rays from outside through the building, all hits each: %.3f us per ray, %.1f pieces per ray\n",
		count, Ms(e - a) * 1000.0 / double(count), double(total) / double(count));
}

void Run(const char* name, const TestBuildingDesc& desc)
{
	TestBuildingInfo info;
	const auto a = Clock::now();
	const Lattice lat = GenerateTestBuilding(desc, &info);
	const double genMs = Ms(Clock::now() - a);
	std::printf("== %s: %d x %d blocks (%.1f x %.1f m), %d floors; lattice %d x %d x %d = %u nodes\n", name, desc.blocksX,
		desc.blocksY, double(desc.blocksX) * 1.25, double(desc.blocksY) * 1.25, desc.floors, lat.nx, lat.ny, lat.nz,
		lat.NodeCount());
	std::printf("   pieces %zu (walls %zu, slabs %zu); windows %zu, door openings %zu, rooms (lights) %zu; generated in %.2f ms\n",
		info.wallPieces + info.slabPieces, info.wallPieces, info.slabPieces, info.windows, info.doors, info.lights.size(), genMs);
	Grenades(lat);
	HundredPerStep(lat);
	HalfCollapse(lat);
	StaticsCreate(lat);
	StaticsGrenades(lat);
	StaticsHalfCollapse(lat);
	Rays(lat);
}

// The buildings of the engine layer (AProtoBench): the same two.
TestBuildingDesc SmallBuilding() { return TestBuildingDesc(); }
TestBuildingDesc LargeBuilding()
{
	TestBuildingDesc d;
	d.blocksX = 40;
	d.blocksY = 32;
	d.floors = 5;
	return d;
}

struct ScenarioOptions {
	int pacedMs = 0;
	bool spin = false;
	const char* csvDir = nullptr;
	const char* building = nullptr;
	const char* scenario = nullptr;
};

// One scenario of the engine layer, step by step, as the engine runs it: damage, then EndStep.
void RunEngineScenario(const char* buildingName, const TestBuildingDesc& desc, ScenarioKind kind, const ScenarioOptions& opt)
{
	Building b(GenerateTestBuilding(desc), ScenarioMode(kind));
	StaticsProfile profile;
	b.SetStaticsProfile(&profile);
	Scenario plan(kind, 4 * desc.floors - 1);   // the view of the engine layer hides the roof by default
	Trace trace;
	StepResult r;
	std::ofstream csv;
	if (opt.csvDir) {
		const std::string path = std::string(opt.csvDir) + "/" + buildingName + "_" + ScenarioName(kind) + ".csv";
		csv.open(path);
		if (csv)
			csv << "step,core_ms,build_ms,pass1_ms,pass2_ms,pass3_ms,rules_ms,reset_ms,destroyed,broken,detached,busy,pieces_left\n";
		else
			std::printf("   cannot write %s\n", path.c_str());
	}
	double totalMs = 0, maxMs = 0, workMs = 0;
	double maxPhases[kPhaseCount] = {};
	int maxStep = 0, workSteps = 0, step = 0;
	for (;; ++step) {
		if (opt.pacedMs > 0 && opt.spin) {
			const auto until = Clock::now() + std::chrono::milliseconds(opt.pacedMs);
			while (Clock::now() < until) {
			}
		} else if (opt.pacedMs > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(opt.pacedMs));
		}
		plan.BeforeStep(b, step);
		double before[kPhaseCount];
		std::memcpy(before, profile.ms, sizeof before);
		const auto a = Clock::now();
		b.EndStep(r);
		const double ms = Ms(Clock::now() - a);
		trace.Step(r);
		double phases[kPhaseCount];
		for (int i = 0; i < kPhaseCount; ++i)
			phases[i] = profile.ms[i] - before[i];
		totalMs += ms;
		if (ms > 0.1) {
			workMs += ms;
			++workSteps;
		}
		if (ms > maxMs) {
			maxMs = ms;
			maxStep = step;
			std::memcpy(maxPhases, phases, sizeof phases);
		}
		if (csv.is_open()) {
			char line[256];
			int len = std::snprintf(line, sizeof line, "%d,%.4f", step, ms);
			for (int i = 0; i < kPhaseCount; ++i)
				len += std::snprintf(line + len, sizeof line - size_t(len), ",%.4f", phases[i]);
			std::snprintf(line + len, sizeof line - size_t(len), ",%zu,%zu,%zu,%d,%u\n", r.destroyed.size(), r.broken.size(),
				r.detached.size(), r.busy ? 1 : 0, r.piecesLeft);
			csv << line;
		}
		if (plan.Finished(b, step))
			break;
	}
	std::printf("   %s %s: %d steps, all steps %.1f ms; steps over 0.1 ms: %d, mean %.3f ms; pieces left %u; trace %016llx\n",
		buildingName, ScenarioName(kind), step + 1, totalMs, workSteps, workSteps ? workMs / double(workSteps) : 0.0, b.PiecesLeft(),
		(unsigned long long)trace.h);
	char what[64];
	std::snprintf(what, sizeof what, "the heaviest step (%d)", maxStep);
	PrintStepPhases(what, maxMs, maxPhases);
}

int RunEngineScenarios(int argc, char** argv)
{
	ScenarioOptions opt;
	for (int i = 0; i < argc; ++i) {
		const bool hasValue = i + 1 < argc;
		if (std::strcmp(argv[i], "--paced") == 0 && hasValue)
			opt.pacedMs = std::atoi(argv[++i]);
		else if (std::strcmp(argv[i], "--spin") == 0)
			opt.spin = true;
		else if (std::strcmp(argv[i], "--csv") == 0 && hasValue)
			opt.csvDir = argv[++i];
		else if (std::strcmp(argv[i], "--building") == 0 && hasValue)
			opt.building = argv[++i];
		else if (std::strcmp(argv[i], "--scenario") == 0 && hasValue)
			opt.scenario = argv[++i];
		else {
			std::printf("unknown option %s\n", argv[i]);
			return 2;
		}
	}
	std::printf("ssproto core bench: the scenarios of the engine layer, step by step (single thread)");
	if (opt.pacedMs > 0)
		std::printf(", a pause of %d ms before every step%s", opt.pacedMs, opt.spin ? " (busy wait)" : "");
	std::printf("\n");
	const std::pair<const char*, TestBuildingDesc> buildings[] = { { "small", SmallBuilding() }, { "large", LargeBuilding() } };
	for (const auto& building : buildings) {
		if (opt.building && std::strcmp(opt.building, building.first) != 0)
			continue;
		for (const ScenarioKind kind : { ScenarioKind::Idle, ScenarioKind::Grenades, ScenarioKind::Hundred, ScenarioKind::Half }) {
			if (opt.scenario && std::strcmp(opt.scenario, ScenarioName(kind)) != 0)
				continue;
			RunEngineScenario(building.first, building.second, kind, opt);
		}
	}
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc > 1 && std::strcmp(argv[1], "scenarios") == 0)
		return RunEngineScenarios(argc - 2, argv + 2);
	std::printf("ssproto core bench (single thread)\n");
	Run("small", SmallBuilding());
	Run("large", LargeBuilding());
	std::printf("statics trace of the run: steps %016llx, passes %016llx\n", (unsigned long long)g_stepsTrace.h,
		(unsigned long long)g_passesTrace.h);
	return 0;
}
