// Structural statics of the original (spec "Мир 2", sections 7-11).
//
// A graph of joints and rods is built from the surviving pieces. One iteration (Recalc) runs three
// passes and then the breaking rules:
//   pass 1 - stability layer by layer from the bottom: a joint is stable if it stands on a stable
//            joint, or if "arrows" shot along continuous rods from stable joints reach it from two
//            opposite sides (spans, lintels and beams hold between two supports);
//   pass 2 - every unstable region either touches a stable joint (hangs but holds) or is free;
//   pass 3 - the weight of every hanging joint flows to the stable joints along random walks
//            (fixed seed 1), adding pressure and bending moments to them;
//   rules  - free joints are removed quietly; a stable joint breaks when its pressure exceeds the
//            material's limit; a rod breaks (taking the piece at its far end) when the moment in
//            its direction exceeds the material's limit.
//
// Prototype model: one joint per surviving piece (every joint is "filled"), a rod between every
// two face-adjacent pieces. The original also has joints without a piece (connection points) and
// takes the joints of a piece from its geometry. Ground joints are the pieces of plane 0 and
// indestructible nodes.
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "ssproto/Lattice.h"

namespace ssproto {

enum class Stability : uint8_t { Unknown, Stable, Unstable, Free, UnstableNotFree, UnknownMoment };

// Directions of rods and arrows; the number is the bit of the arrow.
enum Direction : int { kXP = 0, kXM = 1, kYP = 2, kYM = 3, kUP = 4, kDN = 5, kXPYP = 7, kXMYP = 8, kXMYM = 9, kXPYM = 10 };

// Phases of the statics, for timing (bench and diagnostics).
enum StaticsPhase : int { kPhaseBuild, kPhasePass1, kPhasePass2, kPhasePass3, kPhaseRules, kPhaseReset, kPhaseCount };

// Time spent in every phase. A schema given a profile adds each phase it runs to it; without a
// profile nothing is measured.
struct StaticsProfile {
	double ms[kPhaseCount] = {};      // total
	double maxMs[kPhaseCount] = {};   // the longest single run
	uint64_t runs[kPhaseCount] = {};
};

// RPGMaterials columns used by the statics: Weight (density), UltimatePressure, UltimateMoment.
struct MaterialStatics {
	float weight;
	float ultimatePressure;
	float ultimateMoment;
};
MaterialStatics MaterialStaticsOf(MaterialId m);

// What the schema calls to destroy the piece of a joint (DamageSpot with 255 in the original).
// Returns true if the node was destroyed; it must then call Schema::OnNodeDestroyed. Like
// DamageSpot, it does nothing for a node already destroyed, so the schema does not ask again about
// a node it was told is gone.
class IBreaker {
public:
	virtual bool BreakNode(NodeId node, bool effects) = 0;

protected:
	// Virtual, so that a class implementing it has a virtual destructor (warning C4265 in Unreal).
	virtual ~IBreaker() = default;
};

class Schema {
public:
	void Build(const Lattice& lattice);
	void Clear();
	bool Empty() const { return info.empty(); }

	// One iteration: passes 1-3 and the breaking rules. Returns true if anything broke.
	bool Recalc(IBreaker& breaker);
	// After an iteration that broke something (the original's reset of the schema).
	void Reset();
	// The lattice lost this node: its joint and all its rods are destroyed.
	void OnNodeDestroyed(NodeId node);

	// Passes 1-3 without the rules (tests and debugging).
	void RunPasses();
	Stability StabilityOfNode(NodeId node) const;
	float PressureOfNode(NodeId node) const;
	float MomentOfNode(NodeId node, int direction) const;   // kXP, kXM, kYP, kYM
	float WeightOfNode(NodeId node) const;
	size_t JointCount() const { return info.size(); }
	size_t RodCount() const { return rodCount; }

	// Timing of the phases (nullptr: none). Survives Clear and Build.
	void SetProfile(StaticsProfile* p) { profile = p; }

private:
	// Layout: one entry per joint in every per-joint array below (joints are ordered by node, so by
	// plane, then y, then x). The arrays are separate so that every pass reads only what it needs.
	// Rods have no array of their own: a rod is known from either end by its direction (links,
	// alive), and both ends keep a copy of its lock counter (Walker::locks).
	struct JointInfo {   // fixed after Build
		NodeId node;
		float limitPressure, limitMoment;
	};
	struct Load {        // what the hanging joints put on a stable joint (pass 3)
		float pressure;
		float moment[5];   // XP, XM, YP, YM; the fifth takes the shares of waves sent up or down (never read)
	};
	// Everything the waves of pass 3 touch in a joint, in one cache line.
	struct alignas(64) Walker {
		int32_t link[6];    // a copy of the joint's links, the joint itself where there is no rod (fixed after Build)
		int32_t wave;       // the wave that reached the joint last (valid while the joint is a front) ...
		int32_t step;       // ... at this step
		float support;      // the sum of the weights of the waves the joint recorded; 0: none (weights are > 0)
		float weight;       // fixed after Build
		int16_t x, y;       // fixed after Build: lattice coordinates, the levers of the moments
		uint8_t dir;        // the direction the wave came in
		uint8_t locks[6];   // how many times waves passed the rod in direction d (both ends count)
		uint8_t listed;     // the joint is in the list Reset cleans
		uint8_t unused[12]; // to 64 bytes
	};
	struct Wave {
		int32_t source;
		float weight;
		uint8_t back;   // direction from the first joint back to the source
	};

	int32_t JointOf(NodeId node) const;
	int32_t Link(int32_t joint, int direction) const { return links[6 * size_t(joint) + size_t(direction)]; }

	void ComputeStability(bool pull);
	void Shoot(int32_t joint, int direction, int32_t& unstable);
	void PullArrows(int32_t begin, int32_t end, int32_t& unstable);
	void FindFree();
	void ComputeMoments(bool tracked);
	void Emit(int32_t source);
	void Advance(int32_t wave, int32_t from, int direction, int32_t step);
	size_t Walk();
	static void AddMoment(Load& load, const Walker& source, float share, const Walker& mass, int firstStep);
	bool ApplyRules(IBreaker& breaker);
	bool DestroyJoint(int32_t joint, bool effects, IBreaker& breaker);
	void MarkDestroyed(int32_t joint);
	void DestroyRod(int32_t joint, int direction, bool checkVerticalChain);

	std::vector<JointInfo> info;
	std::vector<int32_t> links;          // 6 per joint: the joint at the other end of the rod in direction d; -1: no rod
	std::vector<uint8_t> flags;          // kGround, kDestroyed, kGone, kLoaded, kChecked, kFront (see Statics.cpp)
	std::vector<uint8_t> alive;          // bit d: the rod in direction d is alive
	std::vector<Stability> stability;
	std::vector<uint16_t> arrows;        // pass 1: bit d is the arrow d
	std::vector<Load> loads;
	std::vector<Walker> walkers;
	std::vector<uint64_t> sources;       // bit per joint: stable and next to a hanging joint (pass 2 for pass 3)
	std::vector<int32_t> nodeToJoint;
	std::vector<int32_t> layerStart;     // joints of plane z: [layerStart[z], layerStart[z + 1])
	std::vector<Wave> waves;
	std::vector<std::pair<int32_t, int32_t>> records;   // pass 3: (joint, wave) that reached the joint; grows only
	std::vector<int32_t> front, nextFront;   // pass 3: the joints of the wave fronts; grow only (see Walk)
	size_t nextCount = 0;                    // entries in nextFront
	std::vector<int32_t> work;
	std::vector<int32_t> region, hanging;   // pass 2: one region; the hanging joints (room for all joints)
	size_t hangingCount = 0;
	std::vector<int32_t> walked;         // joints whose locks or support pass 3 changed since Reset ...
	size_t walkedCount = 0;              // ... the first walkedCount entries
	size_t rodCount = 0;
	bool advanced = false;
	// The joints are as Build or Reset left them (no arrows, nothing checked): pass 1 can pull the
	// arrows, pass 2 can collect the sources and the hanging joints for pass 3.
	bool fresh = false;
	StaticsProfile* profile = nullptr;
};

} // namespace ssproto
