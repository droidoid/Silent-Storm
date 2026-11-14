// Timings of the prototype core on the machine it runs on (CPU only, no engine). Plain text to
// stdout; the results kept in git are this output plus a machine header.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>
#include <vector>

#include "ssproto/Building.h"
#include "ssproto/Statics.h"
#include "ssproto/TestBuilding.h"

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
};

Settle StepUntilSettled(Building& b)
{
	Settle s;
	StepResult r;
	for (; s.steps < 1000; ++s.steps) {
		const auto a = Clock::now();
		b.EndStep(r);
		const double ms = Ms(Clock::now() - a);
		s.totalMs += ms;
		s.maxStepMs = std::max(s.maxStepMs, ms);
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

void StaticsCreate(const Lattice& lat)
{
	const auto a = Clock::now();
	Building b(lat);
	const double createMs = Ms(Clock::now() - a);
	Schema schema;
	const auto c = Clock::now();
	schema.Build(lat);
	const auto m = Clock::now();
	schema.RunPasses();
	const auto e = Clock::now();
	std::printf("   [statics] entering the world (schema + first iteration): %.2f ms; schema of the intact building: %zu joints, %zu rods, built in %.2f ms, passes 1-3 in %.2f ms\n",
		createMs, schema.JointCount(), schema.RodCount(), Ms(m - c), Ms(e - m));
}

void StaticsGrenades(const Lattice& lat)
{
	Building b(lat);
	SRand rnd(1);
	Stats steps, maxStep, total, broken, detached, goneInStep;
	for (int i = 0; i < 30; ++i) {
		NodeId target;
		if (!RandomPiece(rnd, b.GetLattice(), target))
			break;
		b.Explode(b.GetLattice().NodeCenter(target), 1000, 10.0f);
		const Settle s = StepUntilSettled(b);
		steps.Add(double(s.steps));
		maxStep.Add(s.maxStepMs);
		total.Add(s.totalMs);
		broken.Add(double(s.broken));
		detached.Add(double(s.detached));
		goneInStep.Add(double(s.maxGoneInStep));
	}
	std::printf("   [statics] grenades, each followed by steps until the statics settle, %zu grenades\n", steps.n);
	std::printf("     steps to settle avg %.1f, max %.0f; broken by overload avg %.1f, max %.0f; fell free avg %.1f, max %.0f\n",
		steps.Avg(), steps.max, broken.Avg(), broken.max, detached.Avg(), detached.max);
	std::printf("     the heaviest step (schema build or one iteration) avg %.3f ms, max %.3f ms; all steps of one grenade avg %.3f ms\n",
		maxStep.Avg(), maxStep.max, total.Avg());
	std::printf("     the most pieces gone in one step (destroyed + broken + fell): avg %.0f, max %.0f\n", goneInStep.Avg(), goneInStep.max);
	std::printf("     pieces left %u, of them without a chain to a support: %zu\n", b.PiecesLeft(), b.CountUnsupported());
}

void StaticsHalfCollapse(const Lattice& lat)
{
	Building b(lat);
	const Lattice& l = b.GetLattice();
	const int cut = l.nx / 2;
	for (int z = 1; z < l.nz; ++z)
		for (int y = 0; y < l.ny; ++y)
			b.DamageSpot(l.Node(cut, y, z), 255);
	for (int y = 0; y < l.ny; ++y)
		for (int x = 0; x < cut; ++x)
			b.DamageSpot(l.Node(x, y, 1), 255);
	const Settle s = StepUntilSettled(b);
	std::printf("   [statics] collapse of half the building: settled in %d steps; broken %zu, fell free %zu; heaviest step %.3f ms\n",
		s.steps, s.broken, s.detached, s.maxStepMs);
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

} // namespace

int main()
{
	std::printf("ssproto core bench (single thread)\n");
	TestBuildingDesc small;
	Run("small", small);
	TestBuildingDesc large;
	large.blocksX = 40;
	large.blocksY = 32;
	large.floors = 5;
	Run("large", large);
	return 0;
}
