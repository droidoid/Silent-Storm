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

// RPGMaterials columns used by the statics: Weight (density), UltimatePressure, UltimateMoment.
struct MaterialStatics {
	float weight;
	float ultimatePressure;
	float ultimateMoment;
};
MaterialStatics MaterialStaticsOf(MaterialId m);

// What the schema calls to destroy the piece of a joint (DamageSpot with 255 in the original).
// Returns true if the node was destroyed; it must then call Schema::OnNodeDestroyed.
class IBreaker {
public:
	virtual bool BreakNode(NodeId node, bool effects) = 0;

protected:
	~IBreaker() = default;
};

class Schema {
public:
	void Build(const Lattice& lattice);
	void Clear();
	bool Empty() const { return joints.empty(); }

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
	size_t JointCount() const { return joints.size(); }
	size_t RodCount() const { return rods.size(); }

private:
	struct Joint {
		NodeId node;
		int16_t x, y, z;
		int32_t rod[6];
		Stability stability;
		uint32_t flags;
		float weight;
		float limitPressure, limitMoment;
		float moment[4];
		float pressure;
		int32_t wave;
		int32_t waveStep;
		uint8_t waveDir;
		float supportSum;
		int32_t supportCount;
	};
	struct Rod {
		int32_t a, b;   // a: the lower end, else the one with the smaller y, else the smaller x
		uint8_t locks;
		bool destroyed;
	};
	struct Wave {
		int32_t source;
		float weight;
		uint8_t back;   // direction from the first joint back to the source
	};

	int32_t Other(int32_t rod, int32_t joint) const { return rods[size_t(rod)].a == joint ? rods[size_t(rod)].b : rods[size_t(rod)].a; }
	bool Alive(int32_t rod) const { return rod >= 0 && !rods[size_t(rod)].destroyed; }
	const Joint* JointOf(NodeId node) const;

	void ComputeStability();
	bool Shoot(int32_t joint, int direction);
	void FindFree();
	void ComputeMoments();
	bool Advance(int32_t wave, int32_t rod, int32_t from, int direction, int32_t step);
	void ProcessFront(int32_t joint, int32_t step);
	void AddMoment(int32_t source, float share, const Joint& mass, int firstStep);
	bool ApplyRules(IBreaker& breaker);
	bool DestroyJoint(int32_t joint, bool effects, IBreaker& breaker);
	void MarkDestroyed(int32_t joint);
	void DestroyRod(int32_t rod, bool checkVerticalChain);

	std::vector<Joint> joints;
	std::vector<Rod> rods;
	std::vector<int32_t> nodeToJoint;
	std::vector<int32_t> layerStart;   // joints of plane z: [layerStart[z], layerStart[z + 1])
	std::vector<Wave> waves;
	std::vector<std::pair<int32_t, int32_t>> records;   // (joint, wave) that reached the joint
	std::vector<int32_t> front, nextFront, stack, region;
	SRand rnd;
	bool advanced = false;
};

} // namespace ssproto
