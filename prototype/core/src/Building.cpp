#include "ssproto/Building.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ssproto {

namespace {

int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int CellIndex(float coord, float origin, float step) { return int(std::floor((coord - origin) / step)); }

// Rhythm constants of the original (spec "Мир 2", section 11).
constexpr int kStaticsDelay = 5;        // steps before the first statics; also the redraw period while collapsing
constexpr int kHoldAfterSettle = 30;    // steps the world stays held after the building settles
constexpr int kIterationsOnCreate = 100;

} // namespace

Building::Building(Lattice lattice, CollapseMode collapseMode)
	: lat(std::move(lattice))
	, mode(collapseMode)
{
	const uint32_t count = lat.NodeCount();
	for (uint32_t n = 0; n < count; ++n)
		if (lat.hp[n] != kHpEmpty)
			++alive;
	partDirty.assign(size_t(PartsX()) * size_t(PartsY()) * size_t(PartsZ()), 0);
	visitStamp.assign(count, 0u);
	// The original settles a building when it enters the world: the schema and up to 99 iterations
	// at once; if it still moves, the per-step loop takes over.
	if (mode == CollapseMode::Original) {
		if (UpdateStability(kIterationsOnCreate))
			Wake();
		rebuildPending = !pendingBroken.empty() || !pendingDetached.empty();
	}
}

uint32_t Building::PartOf(NodeId n) const
{
	int x, y, z;
	lat.Coords(n, x, y, z);
	return (uint32_t(z / 4) * uint32_t(PartsY()) + uint32_t(y / 2)) * uint32_t(PartsX()) + uint32_t(x / 2);
}

void Building::MarkPartsAround(NodeId n)
{
	// The original marks the 27 parts around a destroyed node (spec "Мир 2" 6.2): neighbouring
	// parts may need new wall clipping after the hole appears.
	int x, y, z;
	lat.Coords(n, x, y, z);
	const int px = x / 2, py = y / 2, pz = z / 4;
	const int sx = PartsX(), sy = PartsY(), sz = PartsZ();
	for (int dz = -1; dz <= 1; ++dz)
		for (int dy = -1; dy <= 1; ++dy)
			for (int dx = -1; dx <= 1; ++dx) {
				const int qx = px + dx, qy = py + dy, qz = pz + dz;
				if (qx < 0 || qy < 0 || qz < 0 || qx >= sx || qy >= sy || qz >= sz)
					continue;
				const uint32_t id = (uint32_t(qz) * uint32_t(sy) + uint32_t(qy)) * uint32_t(sx) + uint32_t(qx);
				if (!partDirty[id]) {
					partDirty[id] = 1;
					dirtyList.push_back(id);
				}
			}
}

bool Building::DestroyNode(NodeId node, int damage)
{
	if (node >= lat.NodeCount() || damage <= 0)
		return false;
	const uint8_t h = lat.hp[node];
	if (h == kHpEmpty || h >= kHpCellar)
		return false;
	if (int(h) > damage) {
		lat.hp[node] = uint8_t(int(h) - damage);
		return false;
	}
	lat.hp[node] = kHpEmpty;
	--alive;
	MarkPartsAround(node);
	if (!schema.Empty())
		schema.OnNodeDestroyed(node);
	supportCheckNeeded = true;
	return true;
}

void Building::Wake()
{
	segmentCounter = 0;
	busy = true;
	holdCounter = -1;
}

bool Building::DamageSpot(NodeId node, int damage)
{
	if (!DestroyNode(node, damage))
		return false;
	pendingDestroyed.push_back(node);
	if (mode == CollapseMode::Original)
		Wake();
	return true;
}

bool Building::BreakNode(NodeId node, bool effects)
{
	if (!DestroyNode(node, 255))
		return false;
	(effects ? pendingBroken : pendingDetached).push_back(node);
	return true;
}

void Building::Explode(Vec3 center, int power, float radius)
{
	if (power <= kMinAdditiveDamage || !(radius > 0.0f))
		return;
	const float r = radius * 0.5f;
	const float r2 = r * r;
	const float s = lat.step;
	const int x0 = ClampInt(CellIndex(center.x - r, lat.origin.x, s), 0, lat.nx - 1);
	const int x1 = ClampInt(CellIndex(center.x + r, lat.origin.x, s), 0, lat.nx - 1);
	const int y0 = ClampInt(CellIndex(center.y - r, lat.origin.y, s), 0, lat.ny - 1);
	const int y1 = ClampInt(CellIndex(center.y + r, lat.origin.y, s), 0, lat.ny - 1);
	const int z0 = ClampInt(CellIndex(center.z - r, lat.origin.z, s), 0, lat.nz - 1);
	const int z1 = ClampInt(CellIndex(center.z + r, lat.origin.z, s), 0, lat.nz - 1);
	for (int z = z0; z <= z1; ++z)
		for (int y = y0; y <= y1; ++y)
			for (int x = x0; x <= x1; ++x) {
				const NodeId n = lat.Node(x, y, z);
				const uint8_t h = lat.hp[n];
				if (h == kHpEmpty || h >= kHpCellar)
					continue;
				const Vec3 c = { lat.origin.x + (float(x) + 0.5f) * s, lat.origin.y + (float(y) + 0.5f) * s,
					lat.origin.z + (float(z) + 0.5f) * s };
				const float d2 = LengthSq(c - center);
				if (d2 > r2)
					continue;
				const float k = d2 < 1.0f ? 1.0f : 1.0f / d2;
				if (DestroyNode(n, int(float(power) * k)))
					pendingDestroyed.push_back(n);
			}
	// The original redraws at once and starts the update loop after every lattice explosion.
	if (mode == CollapseMode::Original)
		Wake();
}

bool Building::UpdateStability(int iterations)
{
	// UpdateBuildingStability of the original: building the schema takes one iteration of the budget.
	int iteration = -1;
	const int limit = iterations - 1;
	if (schema.Empty()) {
		schema.Build(lat);
		// The bottom of every column becomes invulnerable at every build (the foundation stays).
		const uint32_t layer = lat.LayerSize();
		for (NodeId n = 0; n < layer; ++n)
			if (lat.hp[n] != kHpEmpty && lat.hp[n] < kHpCellar)
				lat.hp[n] = kHpCellar;
		if (++iteration == limit)
			return true;
	}
	bool changed = true;
	while (changed) {
		changed = schema.Recalc(*this);
		if (changed)
			schema.Reset();
		if (++iteration == limit)
			break;
	}
	if (!changed)
		schema.Clear();
	return changed;
}

void Building::Raycast(Vec3 origin, Vec3 dir, float maxDist, std::vector<RayHit>& hits) const
{
	hits.clear();
	const float len = Length(dir);
	if (!(len > 0.0f) || !(maxDist > 0.0f))
		return;
	const Vec3 d = dir * (1.0f / len);
	float tStart, tEnd;
	if (!RayBox(origin, d, lat.Bounds(), 0.0f, maxDist, tStart, tEnd))
		return;

	// Walk the cells along the ray (3D DDA). Every piece lies inside its cell, so the hits come
	// out ordered by distance without sorting.
	const float s = lat.step;
	const Vec3 p = origin + d * tStart;
	int ix = ClampInt(CellIndex(p.x, lat.origin.x, s), 0, lat.nx - 1);
	int iy = ClampInt(CellIndex(p.y, lat.origin.y, s), 0, lat.ny - 1);
	int iz = ClampInt(CellIndex(p.z, lat.origin.z, s), 0, lat.nz - 1);
	const int stepX = d.x > 0.0f ? 1 : (d.x < 0.0f ? -1 : 0);
	const int stepY = d.y > 0.0f ? 1 : (d.y < 0.0f ? -1 : 0);
	const int stepZ = d.z > 0.0f ? 1 : (d.z < 0.0f ? -1 : 0);
	const float inf = std::numeric_limits<float>::infinity();
	auto firstCrossing = [&](float o, float dc, int i, float org, int st) {
		if (st == 0)
			return inf;
		const float edge = org + float(i + (st > 0 ? 1 : 0)) * s;
		return (edge - o) / dc;
	};
	float tMaxX = firstCrossing(origin.x, d.x, ix, lat.origin.x, stepX);
	float tMaxY = firstCrossing(origin.y, d.y, iy, lat.origin.y, stepY);
	float tMaxZ = firstCrossing(origin.z, d.z, iz, lat.origin.z, stepZ);
	const float tDeltaX = stepX != 0 ? s / std::fabs(d.x) : inf;
	const float tDeltaY = stepY != 0 ? s / std::fabs(d.y) : inf;
	const float tDeltaZ = stepZ != 0 ? s / std::fabs(d.z) : inf;

	for (;;) {
		const NodeId n = lat.Node(ix, iy, iz);
		if (lat.hp[n] != kHpEmpty) {
			float te, tx;
			if (RayBox(origin, d, lat.PieceBox(n), 0.0f, maxDist, te, tx))
				hits.push_back({ n, te, tx });
		}
		if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
			if (tMaxX > tEnd)
				break;
			ix += stepX;
			if (ix < 0 || ix >= lat.nx)
				break;
			tMaxX += tDeltaX;
		} else if (tMaxY <= tMaxZ) {
			if (tMaxY > tEnd)
				break;
			iy += stepY;
			if (iy < 0 || iy >= lat.ny)
				break;
			tMaxY += tDeltaY;
		} else {
			if (tMaxZ > tEnd)
				break;
			iz += stepZ;
			if (iz < 0 || iz >= lat.nz)
				break;
			tMaxZ += tDeltaZ;
		}
	}
}

void Building::OverlapSphere(Vec3 center, float radius, std::vector<NodeId>& nodes) const
{
	nodes.clear();
	if (!(radius >= 0.0f))
		return;
	const float s = lat.step;
	const int x0 = CellIndex(center.x - radius, lat.origin.x, s), x1 = CellIndex(center.x + radius, lat.origin.x, s);
	const int y0 = CellIndex(center.y - radius, lat.origin.y, s), y1 = CellIndex(center.y + radius, lat.origin.y, s);
	const int z0 = CellIndex(center.z - radius, lat.origin.z, s), z1 = CellIndex(center.z + radius, lat.origin.z, s);
	if (x1 < 0 || y1 < 0 || z1 < 0 || x0 >= lat.nx || y0 >= lat.ny || z0 >= lat.nz)
		return;
	const float r2 = radius * radius;
	for (int z = std::max(z0, 0); z <= std::min(z1, lat.nz - 1); ++z)
		for (int y = std::max(y0, 0); y <= std::min(y1, lat.ny - 1); ++y)
			for (int x = std::max(x0, 0); x <= std::min(x1, lat.nx - 1); ++x) {
				const NodeId n = lat.Node(x, y, z);
				if (lat.hp[n] != kHpEmpty && DistanceSq(center, lat.PieceBox(n)) <= r2)
					nodes.push_back(n);
			}
}

void Building::FindUnsupported(std::vector<NodeId>& out) const
{
	const uint32_t count = lat.NodeCount();
	if (++stamp == 0) {
		std::fill(visitStamp.begin(), visitStamp.end(), 0u);
		stamp = 1;
	}
	queue.clear();
	// Supports: every piece of the ground plane and every node that can never break.
	const uint32_t layer = lat.LayerSize();
	for (uint32_t n = 0; n < count; ++n) {
		const uint8_t h = lat.hp[n];
		if (h == kHpEmpty)
			continue;
		if (n < layer || h >= kHpCellar) {
			visitStamp[n] = stamp;
			queue.push_back(n);
		}
	}
	const uint32_t rowSize = uint32_t(lat.nx);
	auto visit = [&](NodeId m) {
		if (lat.hp[m] != kHpEmpty && visitStamp[m] != stamp) {
			visitStamp[m] = stamp;
			queue.push_back(m);
		}
	};
	for (size_t head = 0; head < queue.size(); ++head) {
		const NodeId n = queue[head];
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (x > 0)
			visit(n - 1);
		if (x + 1 < lat.nx)
			visit(n + 1);
		if (y > 0)
			visit(n - rowSize);
		if (y + 1 < lat.ny)
			visit(n + rowSize);
		if (z > 0)
			visit(n - layer);
		if (z + 1 < lat.nz)
			visit(n + layer);
	}
	for (uint32_t n = 0; n < count; ++n)
		if (lat.hp[n] != kHpEmpty && visitStamp[n] != stamp)
			out.push_back(n);
}

size_t Building::CountUnsupported() const
{
	std::vector<NodeId> nodes;
	FindUnsupported(nodes);
	return nodes.size();
}

void Building::EndStep(StepResult& out)
{
	bool rebuild = rebuildPending;
	rebuildPending = false;
	if (mode == CollapseMode::Connectivity) {
		if (supportCheckNeeded) {
			std::vector<NodeId> unsupported;
			FindUnsupported(unsupported);
			for (const NodeId n : unsupported) {
				lat.hp[n] = kHpEmpty;
				--alive;
				MarkPartsAround(n);
				pendingDetached.push_back(n);
			}
		}
		rebuild = rebuild || !dirtyList.empty();
	} else {
		// One segment of the building's update loop in the original.
		if (segmentCounter == 0)
			rebuild = true;   // the damage becomes visible at once
		if (segmentCounter >= 0 && ++segmentCounter == kStaticsDelay) {
			segmentCounter = -1;
			staticsRunning = false;
			holdCounter = 0;
			if (UpdateStability(1)) {
				segmentCounter = 0;
				staticsRunning = true;
				holdCounter = -1;
			} else {
				rebuild = true;
			}
		} else if (segmentCounter >= 0 && staticsRunning) {
			if (!UpdateStability(1)) {
				segmentCounter = -1;
				staticsRunning = false;
				holdCounter = 0;
				rebuild = true;
			}
		}
		if (holdCounter >= 0 && ++holdCounter == kHoldAfterSettle) {
			busy = false;
			holdCounter = -1;
		}
	}
	supportCheckNeeded = false;

	out.destroyed.clear();
	out.broken.clear();
	out.detached.clear();
	out.dirtyParts.clear();
	out.destroyed.swap(pendingDestroyed);
	out.broken.swap(pendingBroken);
	out.detached.swap(pendingDetached);
	out.rebuild = rebuild;
	if (rebuild) {
		std::sort(dirtyList.begin(), dirtyList.end());
		for (const uint32_t id : dirtyList)
			partDirty[id] = 0;
		out.dirtyParts.swap(dirtyList);
		dirtyList.clear();
	}
	out.busy = busy;
	out.piecesLeft = alive;
}

} // namespace ssproto
