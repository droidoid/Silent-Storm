#include "ssproto/Scenario.h"

#include <algorithm>
#include <cstring>

namespace ssproto {

namespace {

constexpr int kIdleSteps = 200;                                      // 10 s
constexpr int kHundredUntilStep = kScenarioFirstDamageStep + 200;   // 10 s of removal ...
constexpr uint32_t kHundredFloor = 2000;                             // ... or until this few pieces are left
constexpr int kQuietAfterDamage = 60;                                // settled and 3 s after the last damage

uint32_t Rand30(SRand& rnd) { return (uint32_t(rnd.Next()) << 15) | uint32_t(rnd.Next()); }

} // namespace

bool ParseScenario(const char* name, ScenarioKind& out)
{
	for (const ScenarioKind kind : { ScenarioKind::Idle, ScenarioKind::Grenades, ScenarioKind::Hundred, ScenarioKind::Half })
		if (std::strcmp(name, ScenarioName(kind)) == 0) {
			out = kind;
			return true;
		}
	return false;
}

const char* ScenarioName(ScenarioKind kind)
{
	switch (kind) {
	case ScenarioKind::Idle: return "idle";
	case ScenarioKind::Grenades: return "grenades";
	case ScenarioKind::Hundred: return "hundred";
	case ScenarioKind::Half: return "half";
	}
	return "?";
}

CollapseMode ScenarioMode(ScenarioKind kind)
{
	return kind == ScenarioKind::Hundred ? CollapseMode::Connectivity : CollapseMode::Original;
}

Scenario::Scenario(ScenarioKind scenarioKind, int maxTargetPlane)
	: kind(scenarioKind)
	, maxPlane(maxTargetPlane)
{
}

bool Scenario::RandomTarget(const Lattice& lat, NodeId& out)
{
	for (int attempt = 0; attempt < 100000; ++attempt) {
		const NodeId n = Rand30(rnd) % lat.NodeCount();
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (lat.hp[n] != kHpEmpty && lat.hp[n] < kHpCellar && (maxPlane < 0 || z <= maxPlane)) {
			out = n;
			return true;
		}
	}
	return false;
}

void Scenario::DestroyNearest(Building& b, int count)
{
	const Lattice& lat = b.GetLattice();
	NodeId target;
	if (!RandomTarget(lat, target))
		return;
	const Vec3 c = lat.NodeCenter(target);
	for (float radius = 1.0f; radius < 40.0f; radius *= 1.3f) {
		b.OverlapSphere(c, radius, nearby);
		if (nearby.size() >= size_t(count + count / 3))
			break;
	}
	sorted.clear();
	for (const NodeId n : nearby)
		sorted.push_back({ LengthSq(lat.NodeCenter(n) - c), n });
	std::sort(sorted.begin(), sorted.end());
	int done = 0;
	for (const auto& item : sorted) {
		if (done == count)
			break;
		if (b.DamageSpot(item.second, 255))
			++done;
	}
}

void Scenario::CutHalf(Building& b)
{
	// A slice through the middle and the walls of plane 1 to its left.
	const Lattice& lat = b.GetLattice();
	const int cut = lat.nx / 2;
	for (int z = 1; z < lat.nz; ++z)
		for (int y = 0; y < lat.ny; ++y)
			b.DamageSpot(lat.Node(cut, y, z), 255);
	for (int y = 0; y < lat.ny; ++y)
		for (int x = 0; x < cut; ++x)
			b.DamageSpot(lat.Node(x, y, 1), 255);
}

void Scenario::BeforeStep(Building& b, int step)
{
	switch (kind) {
	case ScenarioKind::Idle:
		break;
	case ScenarioKind::Grenades:
		if (grenades < kGrenadeCount && step >= kScenarioFirstDamageStep + kGrenadeInterval * grenades) {
			NodeId target;
			if (RandomTarget(b.GetLattice(), target))
				b.Explode(b.GetLattice().NodeCenter(target), 1000, 10.0f);   // a grenade-sized lattice explosion
			++grenades;
			lastDamage = step;
		}
		break;
	case ScenarioKind::Hundred:
		if (step >= kScenarioFirstDamageStep && step < kHundredUntilStep && b.PiecesLeft() > kHundredFloor) {
			DestroyNearest(b, 100);
			lastDamage = step;
		}
		break;
	case ScenarioKind::Half:
		if (lastDamage < 0 && step >= kScenarioFirstDamageStep) {
			CutHalf(b);
			lastDamage = step;
		}
		break;
	}
}

bool Scenario::Finished(const Building& b, int step) const
{
	if (step + 1 >= kScenarioMaxSteps)
		return true;
	const bool quiet = lastDamage >= 0 && !b.StaticsActive() && step > lastDamage + kQuietAfterDamage;
	switch (kind) {
	case ScenarioKind::Idle:
		return step + 1 >= kIdleSteps;
	case ScenarioKind::Grenades:
		return grenades == kGrenadeCount && quiet;
	case ScenarioKind::Hundred:
		return step + 1 >= kHundredUntilStep || (lastDamage >= 0 && b.PiecesLeft() <= kHundredFloor && step > lastDamage + 20);
	case ScenarioKind::Half:
		return quiet;
	}
	return true;
}

} // namespace ssproto
