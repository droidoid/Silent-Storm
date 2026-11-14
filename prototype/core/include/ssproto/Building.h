// One destructible building of the prototype core: damage, queries, collapse.
//
// Engine-agnostic. The engine layer calls EndStep once per world step (50 ms in the original) and
// only redraws: removes destroyed and broken pieces (with dust and sound), lets detached pieces
// fall as debris, rebuilds the dirty parts when told to if it draws merged meshes.
#pragma once

#include <cstdint>
#include <vector>

#include "ssproto/Lattice.h"
#include "ssproto/Statics.h"

namespace ssproto {

// Lattice explosion does nothing at this power or below (MIN_ADDITIVE_DMG of the original).
constexpr int kMinAdditiveDamage = 50;

enum class CollapseMode : uint8_t {
	// The original's statics with its rhythm (spec "Мир 2", sections 7-11): the schema is built 5
	// steps after the damage, then one iteration per step; spans hold between supports, hanging
	// parts load their supports, overloaded joints and rods break, free parts fall.
	Original,
	// Immediate and simple: every piece without a chain of neighbours to a support falls in the same
	// step; nothing breaks from overload. For stress tests and as a cheap fallback.
	Connectivity,
};

struct RayHit {
	NodeId node;
	float tEnter;   // metres from the ray origin
	float tExit;
};

struct StepResult {
	std::vector<NodeId> destroyed;     // by damage (hits, explosions) since the previous step
	std::vector<NodeId> broken;        // by the statics with effects (overload): dust and sound
	std::vector<NodeId> detached;      // by the statics quietly (free pieces): may fall as debris
	bool rebuild = false;              // the original redraws the building now
	std::vector<uint32_t> dirtyParts;  // with rebuild: parts touched since the previous rebuild, ascending
	bool busy = false;                 // still settling: the original holds the world (no turn passes)
	uint32_t piecesLeft = 0;
};

class Building : private IBreaker {
public:
	explicit Building(Lattice lattice, CollapseMode mode = CollapseMode::Original);

	const Lattice& GetLattice() const { return lat; }
	CollapseMode Mode() const { return mode; }
	uint32_t PiecesLeft() const { return alive; }
	// The statics are counting down or iterating (Original mode).
	bool StaticsActive() const { return segmentCounter >= 0 || staticsRunning; }
	// The current schema: empty while the building is at rest.
	const Schema& CurrentSchema() const { return schema; }

	// Redraw units of the original ("SPart"): one floor (4 planes) over 2 x 2 nodes.
	int PartsX() const { return (lat.nx + 1) / 2; }
	int PartsY() const { return (lat.ny + 1) / 2; }
	int PartsZ() const { return (lat.nz - 1) / 4 + 1; }
	uint32_t PartOf(NodeId n) const;

	// The single place where HP goes down (DamageSpot of the original, spec "Мир 2" 6.2): empty,
	// cellar and indestructible nodes ignore damage; HP = max(0, HP - damage). Returns true when
	// the node is destroyed by this call; then the building wakes up (Original mode).
	bool DamageSpot(NodeId node, int damage);

	// Lattice explosion with the original formula (spec "Мир 2" 6.3): nothing at power <= 50;
	// only nodes within radius / 2 are hit; damage = floor(power * min(1, 1 / d^2)), d in metres.
	// Simplified sampling: d is measured to the node centre (the original samples points shifted
	// from the nodes and skips the last row and column of its box). Wakes the building up.
	void Explode(Vec3 center, int power, float radius);

	// Every piece the ray passes through, in order, with entry and exit distances (the shot model
	// needs all intersections, not the first one). dir need not be normalised.
	void Raycast(Vec3 origin, Vec3 dir, float maxDist, std::vector<RayHit>& hits) const;

	// Pieces whose box intersects the sphere.
	void OverlapSphere(Vec3 center, float radius, std::vector<NodeId>& nodes) const;

	// One world step: runs the collapse logic of the mode and reports what changed.
	void EndStep(StepResult& out);

	// Pieces with no chain of neighbours to a support (diagnostics; 0 after the statics settle).
	size_t CountUnsupported() const;

private:
	bool BreakNode(NodeId node, bool effects) override;
	bool DestroyNode(NodeId node, int damage);
	void Wake();
	bool UpdateStability(int iterations);
	void MarkPartsAround(NodeId n);
	void FindUnsupported(std::vector<NodeId>& out) const;

	Lattice lat;
	CollapseMode mode;
	uint32_t alive = 0;
	Schema schema;
	// Rhythm of the original (spec "Мир 2", section 11).
	int segmentCounter = -1;
	bool staticsRunning = false;
	int holdCounter = -1;
	bool busy = false;
	bool rebuildPending = false;
	bool supportCheckNeeded = false;   // Connectivity mode
	std::vector<NodeId> pendingDestroyed, pendingBroken, pendingDetached;
	std::vector<uint8_t> partDirty;
	std::vector<uint32_t> dirtyList;
	mutable std::vector<uint32_t> visitStamp;
	mutable uint32_t stamp = 0;
	mutable std::vector<NodeId> queue;
};

} // namespace ssproto
