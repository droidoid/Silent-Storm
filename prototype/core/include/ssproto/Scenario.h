// The measurement scenarios of the prototype, counted in world steps (50 ms each). The engine layer
// and the core bench run this same code, so a scenario destroys the same pieces in the same steps in
// every build and on every machine: the traces of its steps (Trace::Step) come out equal, and the
// time of every step can be compared one to one.
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "ssproto/Building.h"

namespace ssproto {

enum class ScenarioKind : uint8_t {
	Idle,       // 10 s without damage
	Grenades,   // 10 grenades, one every 3 s, with the original's statics, until the building settles
	Hundred,    // 100 pieces every step for 10 s, the simple collapse mode
	Half,       // half of the building cut off, with the original's statics, until it settles
};

constexpr int kScenarioFirstDamageStep = 40;   // 2 s of quiet first
constexpr int kGrenadeCount = 10;
constexpr int kGrenadeInterval = 60;           // steps between grenades: 3 s
constexpr int kScenarioMaxSteps = 3000;        // 150 s: every scenario ends by then

// "idle", "grenades", "hundred", "half"; false for anything else.
bool ParseScenario(const char* name, ScenarioKind& out);
const char* ScenarioName(ScenarioKind kind);
// The collapse mode a scenario measures: "hundred" the simple one, the others the original's statics.
CollapseMode ScenarioMode(ScenarioKind kind);

class Scenario {
public:
	Scenario() = default;
	// Damage never targets pieces above plane maxTargetPlane (the view hides them, so the camera
	// would not see it); -1: any plane.
	Scenario(ScenarioKind kind, int maxTargetPlane);

	ScenarioKind Kind() const { return kind; }
	// The damage of world step `step` (counted from 0), done before Building::EndStep of that step.
	void BeforeStep(Building& b, int step);
	// After Building::EndStep of `step`: true when the scenario is over.
	bool Finished(const Building& b, int step) const;
	// The last step that did damage; -1: none yet.
	int LastDamageStep() const { return lastDamage; }

private:
	bool RandomTarget(const Lattice& lat, NodeId& out);
	void DestroyNearest(Building& b, int count);
	static void CutHalf(Building& b);

	ScenarioKind kind = ScenarioKind::Idle;
	int maxPlane = -1;
	SRand rnd{ 1 };
	int grenades = 0;
	int lastDamage = -1;
	std::vector<NodeId> nearby;
	std::vector<std::pair<float, NodeId>> sorted;
};

} // namespace ssproto
