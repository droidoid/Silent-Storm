// One destructible building of the prototype core: damage, queries, collapse.
//
// Engine-agnostic. The engine layer reads StepResult after every world step (50 ms in the
// original) and only redraws: removes destroyed pieces (with dust and sound), turns detached
// pieces into falling debris, rebuilds the dirty parts if it draws merged meshes.
//
// Collapse rule of the prototype: a piece falls when no chain of face-adjacent pieces links it to
// a support (a node of the ground plane or an indestructible node). This is pass 2 ("FindFree") of
// the original statics (spec "Мир 2", section 9); passes 1 and 3 (spans held between opposite
// supports, weight flowing to supports, breaking overloaded joints) are not implemented yet.
#pragma once

#include <cstdint>
#include <vector>

#include "ssproto/Lattice.h"

namespace ssproto {

// Lattice explosion does nothing at this power or below (MIN_ADDITIVE_DMG of the original).
constexpr int kMinAdditiveDamage = 50;

struct RayHit {
	NodeId node;
	float tEnter;   // metres from the ray origin
	float tExit;
};

struct StepResult {
	std::vector<NodeId> destroyed;     // HP reached 0 from damage since the last step (loud: dust, sound)
	std::vector<NodeId> detached;      // lost every path to a support in this step (quiet; falls as debris)
	std::vector<uint32_t> dirtyParts;  // redraw units touched since the last step, ascending
	uint32_t piecesLeft = 0;
};

class Building {
public:
	explicit Building(Lattice lattice);

	const Lattice& GetLattice() const { return lat; }
	uint32_t PiecesLeft() const { return alive; }

	// Redraw units of the original ("SPart"): one floor (4 planes) over 2 x 2 nodes.
	int PartsX() const { return (lat.nx + 1) / 2; }
	int PartsY() const { return (lat.ny + 1) / 2; }
	int PartsZ() const { return (lat.nz - 1) / 4 + 1; }
	uint32_t PartOf(NodeId n) const;

	// The single place where HP goes down (DamageSpot of the original, spec "Мир 2" 6.2): empty,
	// cellar and indestructible nodes ignore damage; HP = max(0, HP - damage). Returns true when
	// the node is destroyed by this call.
	bool DamageSpot(NodeId node, int damage);

	// Lattice explosion with the original formula (spec "Мир 2" 6.3): nothing at power <= 50;
	// only nodes within radius / 2 are hit; damage = floor(power * min(1, 1 / d^2)), d in metres.
	// Simplified sampling: d is measured to the node centre (the original samples points shifted
	// from the nodes and skips the last row and column of its box).
	void Explode(Vec3 center, int power, float radius);

	// Every piece the ray passes through, in order, with entry and exit distances (the shot model
	// needs all intersections, not the first one). dir need not be normalised.
	void Raycast(Vec3 origin, Vec3 dir, float maxDist, std::vector<RayHit>& hits) const;

	// Pieces whose box intersects the sphere.
	void OverlapSphere(Vec3 center, float radius, std::vector<NodeId>& nodes) const;

	// Ends a world step: finds pieces that lost support (if anything was destroyed), removes them
	// and reports everything that changed since the previous call.
	void EndStep(StepResult& out);

private:
	void MarkPartsAround(NodeId n);
	void FindDetached(std::vector<NodeId>& out);

	Lattice lat;
	uint32_t alive = 0;
	bool supportCheckNeeded = false;
	std::vector<NodeId> pendingDestroyed;
	std::vector<uint8_t> partDirty;
	std::vector<uint32_t> dirtyList;
	std::vector<uint32_t> visitStamp;
	uint32_t stamp = 0;
	std::vector<NodeId> queue;
};

} // namespace ssproto
