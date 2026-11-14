// Tests of the prototype core. No framework: CHECK counts failures, the exit code is their number.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ssproto/Building.h"
#include "ssproto/Scenario.h"
#include "ssproto/Statics.h"
#include "ssproto/TestBuilding.h"
#include "ssproto/Trace.h"

using namespace ssproto;

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                                    \
	do {                                                                               \
		++g_checks;                                                                    \
		if (!(cond)) {                                                                 \
			++g_failures;                                                              \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
		}                                                                              \
	} while (0)

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

bool Inside(const Aabb& inner, const Aabb& outer)
{
	const float e = 1e-5f;
	return inner.min.x >= outer.min.x - e && inner.min.y >= outer.min.y - e && inner.min.z >= outer.min.z - e &&
		inner.max.x <= outer.max.x + e && inner.max.y <= outer.max.y + e && inner.max.z <= outer.max.z + e;
}

Vec3 RandomPoint(SRand& rnd, const Aabb& b, float margin)
{
	return { rnd.GetFloat(b.min.x - margin, b.max.x + margin), rnd.GetFloat(b.min.y - margin, b.max.y + margin),
		rnd.GetFloat(b.min.z - margin, b.max.z + margin) };
}

void BruteRaycast(const Building& b, Vec3 o, Vec3 dir, float maxDist, std::vector<RayHit>& hits)
{
	hits.clear();
	const Lattice& lat = b.GetLattice();
	const Vec3 d = dir * (1.0f / Length(dir));
	for (NodeId n = 0; n < lat.NodeCount(); ++n) {
		if (lat.hp[n] == kHpEmpty)
			continue;
		float te, tx;
		if (RayBox(o, d, lat.PieceBox(n), 0.0f, maxDist, te, tx))
			hits.push_back({ n, te, tx });
	}
	std::sort(hits.begin(), hits.end(), [](const RayHit& a, const RayHit& c) {
		return a.tEnter < c.tEnter || (a.tEnter == c.tEnter && a.node < c.node);
	});
}

// Grazing hits (zero length inside a piece) depend on rounding; compare only real crossings.
void KeepSolid(std::vector<RayHit>& hits)
{
	hits.erase(std::remove_if(hits.begin(), hits.end(), [](const RayHit& h) { return h.tExit - h.tEnter <= 1e-4f; }),
		hits.end());
}

// A brick column on the ground at x = 0 (planes 0-3) and a beam on plane 4 from x = 0 to x = length.
// Every piece is a 0.25 x 0.25 x 0.625 m wall box: weight in the schema 0.1 * V * 1600 = 6.25.
Lattice ColumnWithBeam(int length)
{
	Lattice lat;
	lat.Resize(length + 3, 3, 6);
	auto put = [&](int x, int z) {
		const NodeId n = lat.Node(x, 1, z);
		lat.kind[n] = uint8_t(PieceKind::Wall);
		lat.material[n] = uint8_t(MaterialId::Bricks);
		lat.hp[n] = z == 0 ? kHpCellar : 100;
	};
	for (int z = 0; z <= 3; ++z)
		put(0, z);
	for (int x = 1; x <= length; ++x)
		put(x, 4);
	put(0, 4);
	return lat;
}

void TestRayBox()
{
	const Aabb box{ { 0, 0, 0 }, { 1, 1, 1 } };
	float te = 0, tx = 0;
	CHECK(RayBox({ -1, 0.5f, 0.5f }, { 1, 0, 0 }, box, 0.0f, 100.0f, te, tx) && Near(te, 1) && Near(tx, 2));
	CHECK(!RayBox({ -1, 1.5f, 0.5f }, { 1, 0, 0 }, box, 0.0f, 100.0f, te, tx));
	CHECK(!RayBox({ -1, 0.5f, 0.5f }, { 1, 0, 0 }, box, 0.0f, 0.5f, te, tx));      // too short
	CHECK(RayBox({ 0.5f, 0.5f, 0.5f }, { 0, 0, 1 }, box, 0.0f, 100.0f, te, tx) && te == 0.0f && Near(tx, 0.5f));
	CHECK(!RayBox({ 2, 0.5f, 0.5f }, { 0, 1, 0 }, box, 0.0f, 100.0f, te, tx));      // parallel, outside
}

void TestGeneration()
{
	TestBuildingInfo info;
	const Lattice lat = GenerateTestBuilding(TestBuildingDesc{}, &info);
	CHECK(lat.nx == 40 && lat.ny == 32 && lat.nz == 13);
	CHECK(info.lights.size() == 30);
	CHECK(info.windows > 0 && info.doors > 0);
	size_t pieces = 0;
	bool hpMatchesKind = true, insideCells = true, cellarOnGround = true, hpInRange = true;
	for (NodeId n = 0; n < lat.NodeCount(); ++n) {
		const bool occupied = PieceKind(lat.kind[n]) != PieceKind::None;
		if (occupied != (lat.hp[n] != kHpEmpty))
			hpMatchesKind = false;
		if (!occupied)
			continue;
		++pieces;
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (!Inside(lat.PieceBox(n), lat.CellBox(x, y, z)))
			insideCells = false;
		if (z == 0 && lat.hp[n] != kHpCellar)
			cellarOnGround = false;
		if (z > 0 && (lat.hp[n] < 1 || lat.hp[n] > kHpMax))
			hpInRange = false;
	}
	CHECK(hpMatchesKind);
	CHECK(insideCells);
	CHECK(cellarOnGround);
	CHECK(hpInRange);
	CHECK(pieces == info.wallPieces + info.slabPieces);
	CHECK(pieces > 5000);

	// A straight brick wall piece (top plane of floor 1, no window there): 0.625 x 0.25 x 0.625 m =
	// 0.09766 m^3, HP = ceil(2 * 650 * V) = 127.
	const NodeId wall = lat.Node(1, 0, 7);
	CHECK(PieceKind(lat.kind[wall]) == PieceKind::Wall && lat.hp[wall] == 127);

	// Entering the world runs the statics: the generated building must stand as it is.
	Building b(lat);
	StepResult r;
	b.EndStep(r);
	CHECK(r.destroyed.empty() && r.broken.empty() && r.detached.empty() && r.dirtyParts.empty() && !r.rebuild);
	CHECK(r.piecesLeft == pieces && !r.busy && b.CurrentSchema().Empty());
}

void TestRaycastMatchesBruteForce()
{
	Building b(GenerateTestBuilding(TestBuildingDesc{}));
	SRand rnd(7);
	const Aabb bounds = b.GetLattice().Bounds();
	for (int i = 0; i < 40; ++i)   // holes, so that empty cells are crossed too
		b.Explode(RandomPoint(rnd, bounds, 0.0f), 1000, 6.0f);
	StepResult r;
	b.EndStep(r);

	std::vector<RayHit> fast, slow;
	int mismatches = 0, disordered = 0;
	size_t total = 0;
	for (int i = 0; i < 3000; ++i) {
		const Vec3 o = RandomPoint(rnd, bounds, (i % 2) ? 6.0f : 0.0f);
		Vec3 dir = RandomPoint(rnd, bounds, 0.0f) - o;
		if (i % 7 == 0)
			dir = (i % 3 == 0) ? Vec3{ dir.x, 0, 0 } : ((i % 3 == 1) ? Vec3{ 0, dir.y, 0 } : Vec3{ 0, 0, dir.z });
		if (LengthSq(dir) < 1e-6f)
			continue;
		const float maxDist = rnd.GetFloat(2.0f, 80.0f);
		b.Raycast(o, dir, maxDist, fast);
		for (size_t k = 1; k < fast.size(); ++k)
			if (fast[k].tEnter < fast[k - 1].tEnter - 1e-5f)
				++disordered;
		BruteRaycast(b, o, dir, maxDist, slow);
		KeepSolid(fast);
		KeepSolid(slow);
		bool same = fast.size() == slow.size();
		for (size_t k = 0; same && k < fast.size(); ++k)
			same = fast[k].node == slow[k].node && Near(fast[k].tEnter, slow[k].tEnter) && Near(fast[k].tExit, slow[k].tExit);
		if (!same)
			++mismatches;
		total += slow.size();
	}
	CHECK(mismatches == 0);
	CHECK(disordered == 0);
	CHECK(total > 3000);
	std::printf("  raycast: 3000 rays, %zu solid hits, %d mismatches\n", total, mismatches);
}

void TestOverlapMatchesBruteForce()
{
	Building b(GenerateTestBuilding(TestBuildingDesc{}));
	const Lattice& lat = b.GetLattice();
	SRand rnd(11);
	std::vector<NodeId> fast, slow;
	int mismatches = 0;
	for (int i = 0; i < 1000; ++i) {
		const Vec3 c = RandomPoint(rnd, lat.Bounds(), 2.0f);
		const float radius = rnd.GetFloat(0.0f, 4.0f);
		b.OverlapSphere(c, radius, fast);
		slow.clear();
		for (NodeId n = 0; n < lat.NodeCount(); ++n)
			if (lat.hp[n] != kHpEmpty && DistanceSq(c, lat.PieceBox(n)) <= radius * radius)
				slow.push_back(n);
		std::sort(fast.begin(), fast.end());
		if (fast != slow)
			++mismatches;
	}
	CHECK(mismatches == 0);
}

void TestDamageSpot()
{
	Lattice lat;
	lat.Resize(3, 3, 3);
	const NodeId ground = lat.Node(1, 1, 0), mid = lat.Node(1, 1, 1), top = lat.Node(1, 1, 2);
	for (const NodeId n : { ground, mid, top })
		lat.kind[n] = uint8_t(PieceKind::Wall);
	lat.hp[ground] = kHpCellar;
	lat.hp[mid] = 100;
	lat.hp[top] = kHpIndestructible;
	Building b(lat);
	CHECK(!b.DamageSpot(ground, 1000));
	CHECK(!b.DamageSpot(top, 1000));
	CHECK(!b.DamageSpot(lat.Node(0, 0, 1), 1000));    // empty node
	CHECK(!b.DamageSpot(mid, 0));
	CHECK(!b.DamageSpot(mid, 40) && b.GetLattice().hp[mid] == 60);
	CHECK(!b.StaticsActive());                        // a hit that destroys nothing does not wake the building
	CHECK(b.DamageSpot(mid, 60));                     // exactly to zero
	CHECK(b.StaticsActive());
	CHECK(!b.DamageSpot(mid, 10));                    // already gone
	CHECK(!b.DamageSpot(1000000, 10));                // outside the lattice
	StepResult r;
	b.EndStep(r);
	CHECK(r.destroyed.size() == 1 && r.destroyed[0] == mid);
	CHECK(r.detached.empty() && r.broken.empty());
	CHECK(b.PiecesLeft() == 2);
}

void TestExplodeFormula()
{
	Lattice lat;
	lat.Resize(9, 9, 9);
	for (NodeId n = 0; n < lat.NodeCount(); ++n) {
		lat.kind[n] = uint8_t(PieceKind::Wall);
		lat.hp[n] = n < lat.LayerSize() ? kHpCellar : kHpMax;
	}
	Building b(lat);
	auto hp = [&](int x, int y, int z) { return int(b.GetLattice().hp[lat.Node(x, y, z)]); };
	const Vec3 c = lat.NodeCenter(lat.Node(4, 4, 4));
	b.Explode(c, 50, 10.0f);                          // at the threshold: nothing
	CHECK(hp(4, 4, 4) == 253);
	b.Explode(c, 200, 4.0f);                          // half radius 2 m
	CHECK(hp(4, 4, 4) == 53);                         // d = 0: full power
	CHECK(hp(5, 4, 4) == 53);                         // d = 0.625 < 1: full power
	CHECK(hp(6, 4, 4) == 253 - int(200.0f * (1.0f / 1.5625f)));    // d = 1.25: 200 / 1.5625 = 128
	CHECK(hp(4, 4, 7) == 253 - int(200.0f * (1.0f / 3.515625f)));  // d = 1.875: floor(56.9) = 56
	CHECK(hp(8, 4, 4) == 253);                        // d = 2.5 > 2: untouched
	b.Explode(lat.NodeCenter(lat.Node(4, 4, 1)), 1000, 4.0f);
	CHECK(hp(4, 4, 0) == kHpCellar);                  // cellar never damaged
	StepResult r;
	b.EndStep(r);
	CHECK(std::find(r.destroyed.begin(), r.destroyed.end(), lat.Node(4, 4, 1)) != r.destroyed.end());
}

void TestConnectivityColumn()
{
	Lattice lat;
	lat.Resize(6, 3, 8);
	auto put = [&](int x, int y, int z, uint8_t h) {
		const NodeId n = lat.Node(x, y, z);
		lat.kind[n] = uint8_t(PieceKind::Wall);
		lat.hp[n] = h;
	};
	for (int z = 0; z <= 6; ++z)
		put(1, 1, z, z == 0 ? kHpCellar : 100);
	for (int x = 2; x <= 4; ++x)
		put(x, 1, 6, 100);
	Building b(lat, CollapseMode::Connectivity);
	StepResult r;
	b.DamageSpot(lat.Node(4, 1, 6), 255);             // the tip of the cantilever
	b.EndStep(r);
	CHECK(r.destroyed.size() == 1 && r.detached.empty());
	b.DamageSpot(lat.Node(1, 1, 2), 255);             // cut the column
	b.EndStep(r);
	CHECK(r.destroyed.size() == 1);
	CHECK(r.detached.size() == 6);                    // column z = 3..6 and the cantilever x = 2..3
	CHECK(b.PiecesLeft() == 2);
	b.EndStep(r);                                     // a step without damage reports nothing
	CHECK(r.destroyed.empty() && r.detached.empty() && r.dirtyParts.empty() && !r.rebuild);
}

void CutHalf(Building& b)
{
	const Lattice& lat = b.GetLattice();
	const int cut = lat.nx / 2;
	for (int z = 1; z < lat.nz; ++z)
		for (int y = 0; y < lat.ny; ++y)
			b.DamageSpot(lat.Node(cut, y, z), 255);
	for (int y = 0; y < lat.ny; ++y)
		for (int x = 0; x < cut; ++x)
			b.DamageSpot(lat.Node(x, y, 1), 255);
}

size_t CountLeftAbove(const Lattice& lat)
{
	size_t count = 0;
	for (NodeId n = 0; n < lat.NodeCount(); ++n) {
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (lat.hp[n] != kHpEmpty && x < lat.nx / 2 && z >= 2)
			++count;
	}
	return count;
}

void TestConnectivitySlice()
{
	Building b(GenerateTestBuilding(TestBuildingDesc{}), CollapseMode::Connectivity);
	const Lattice& lat = b.GetLattice();
	const size_t leftAbove = CountLeftAbove(lat);
	CutHalf(b);
	StepResult r;
	b.EndStep(r);
	bool allLeftAbove = true;
	for (const NodeId n : r.detached) {
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (!(x < lat.nx / 2 && z >= 2))
			allLeftAbove = false;
	}
	CHECK(allLeftAbove);
	CHECK(r.detached.size() == leftAbove);
	CHECK(CountLeftAbove(lat) == 0);
	b.EndStep(r);
	CHECK(r.detached.empty());
}

void TestDirtyParts()
{
	Building b(GenerateTestBuilding(TestBuildingDesc{}));
	const Lattice& lat = b.GetLattice();
	const NodeId n = lat.Node(8, 5, 5);               // inner wall, floor 1
	CHECK(PieceKind(lat.kind[n]) == PieceKind::Wall);
	CHECK(b.DamageSpot(n, 255));
	StepResult r;
	b.EndStep(r);
	CHECK(r.rebuild);
	CHECK(r.detached.empty());
	CHECK(r.dirtyParts.size() == 27);
	bool expected = true;
	for (int pz = 0; pz <= 2; ++pz)
		for (int py = 1; py <= 3; ++py)
			for (int px = 3; px <= 5; ++px) {
				const uint32_t id = (uint32_t(pz) * uint32_t(b.PartsY()) + uint32_t(py)) * uint32_t(b.PartsX()) + uint32_t(px);
				if (!std::binary_search(r.dirtyParts.begin(), r.dirtyParts.end(), id))
					expected = false;
			}
	CHECK(expected);
	CHECK(b.PartOf(n) == (1u * uint32_t(b.PartsY()) + 2u) * uint32_t(b.PartsX()) + 4u);
}

// ---- statics ----

void TestStaticsGeneratedBuildingStands()
{
	TestBuildingDesc large;
	large.blocksX = 40;
	large.blocksY = 32;
	large.floors = 5;
	for (const TestBuildingDesc& desc : { TestBuildingDesc{}, large }) {
		const Lattice lat = GenerateTestBuilding(desc);
		Schema s;
		s.Build(lat);
		s.RunPasses();
		size_t notStable = 0;
		for (NodeId n = 0; n < lat.NodeCount(); ++n)
			if (lat.hp[n] != kHpEmpty && s.StabilityOfNode(n) != Stability::Stable)
				++notStable;
		CHECK(notStable == 0);
		CHECK(s.JointCount() > 5000 && s.RodCount() > s.JointCount());
	}
}

void TestStaticsSpanHolds()
{
	// Two columns 5 m apart and a beam between them: held by arrows from both sides.
	Lattice lat;
	lat.Resize(9, 3, 6);
	auto put = [&](int x, int z) {
		const NodeId n = lat.Node(x, 1, z);
		lat.kind[n] = uint8_t(PieceKind::Wall);
		lat.material[n] = uint8_t(MaterialId::Bricks);
		lat.hp[n] = z == 0 ? kHpCellar : 100;
	};
	for (int z = 0; z <= 3; ++z) {
		put(0, z);
		put(8, z);
	}
	for (int x = 0; x <= 8; ++x)
		put(x, 4);
	Schema s;
	s.Build(lat);
	s.RunPasses();
	bool allStable = true;
	for (NodeId n = 0; n < lat.NodeCount(); ++n)
		if (lat.hp[n] != kHpEmpty && s.StabilityOfNode(n) != Stability::Stable)
			allStable = false;
	CHECK(allStable);
	Building b(lat);
	CHECK(b.PiecesLeft() == 17);
}

void TestStaticsDiagonal()
{
	// A slab on two columns at opposite corners: the diagonal between them is held by diagonal arrows.
	Lattice lat;
	lat.Resize(5, 5, 2);
	for (int y = 0; y < 5; ++y)
		for (int x = 0; x < 5; ++x) {
			const NodeId n = lat.Node(x, y, 1);
			lat.kind[n] = uint8_t(PieceKind::Slab);
			lat.material[n] = uint8_t(MaterialId::Bricks);
			lat.hp[n] = 100;
		}
	for (const NodeId n : { lat.Node(0, 0, 0), lat.Node(4, 4, 0) }) {
		lat.kind[n] = uint8_t(PieceKind::Slab);
		lat.material[n] = uint8_t(MaterialId::Bricks);
		lat.hp[n] = kHpCellar;
	}
	Schema s;
	s.Build(lat);
	s.RunPasses();
	CHECK(s.StabilityOfNode(lat.Node(1, 1, 1)) == Stability::Stable);
	CHECK(s.StabilityOfNode(lat.Node(2, 2, 1)) == Stability::Stable);
	CHECK(s.StabilityOfNode(lat.Node(3, 3, 1)) == Stability::Stable);
}

void TestStaticsCantileverLoads()
{
	const Lattice lat = ColumnWithBeam(3);
	Schema s;
	s.Build(lat);
	s.RunPasses();
	const NodeId support = lat.Node(0, 1, 4);
	CHECK(s.StabilityOfNode(support) == Stability::Stable);
	for (int x = 1; x <= 3; ++x)
		CHECK(s.StabilityOfNode(lat.Node(x, 1, 4)) == Stability::UnstableNotFree);
	const float w = s.WeightOfNode(lat.Node(1, 1, 4));
	CHECK(Near(w, 6.25f));
	// One source, so every hanging joint gives it its whole weight: pressure = sum of weights,
	// moment along +x = sum of weight * lever (1 + 2 + 3 lattice steps).
	CHECK(Near(s.PressureOfNode(support), 3.0f * w));
	CHECK(Near(s.MomentOfNode(support, kXP), 6.0f * w));
	CHECK(s.PressureOfNode(lat.Node(0, 1, 3)) == 0.0f);
}

void TestStaticsMomentBreak()
{
	{
		// 6 pieces out: pressure 37.5 < 50, moment 6.25 * 21 = 131.25 < 140 - holds.
		Building b(ColumnWithBeam(6));
		StepResult r;
		b.EndStep(r);
		CHECK(r.broken.empty() && r.detached.empty() && b.PiecesLeft() == 11);
	}
	{
		// 7 out: pressure 43.75, moment 6.25 * 28 = 175 > 140 - the rod at the root breaks and takes
		// the first piece (with dust); the rest is free and falls quietly.
		const Lattice lat = ColumnWithBeam(7);
		Building b(lat);
		StepResult r;
		b.EndStep(r);
		CHECK(r.broken.size() == 1 && !r.broken.empty() && r.broken[0] == lat.Node(1, 1, 4));
		CHECK(r.detached.size() == 6);
		bool beamOnly = true;
		for (const NodeId n : r.detached) {
			int x, y, z;
			lat.Coords(n, x, y, z);
			if (!(z == 4 && x >= 2 && x <= 7))
				beamOnly = false;
		}
		CHECK(beamOnly);
		CHECK(b.PiecesLeft() == 5);
		CHECK(r.rebuild);
	}
}

void TestStaticsPressureBreak()
{
	// 10 out: pressure 62.5 > 50 at the support - the support itself breaks, then the beam is free.
	const Lattice lat = ColumnWithBeam(10);
	Building b(lat);
	StepResult r;
	b.EndStep(r);
	CHECK(r.broken.size() == 1 && !r.broken.empty() && r.broken[0] == lat.Node(0, 1, 4));
	CHECK(r.detached.size() == 10);
	CHECK(b.PiecesLeft() == 4);
}

void TestStaticsRhythm()
{
	const Lattice lat = ColumnWithBeam(6);
	Building b(lat);
	StepResult r;
	CHECK(b.DamageSpot(lat.Node(6, 1, 4), 255));      // the tip; 5 pieces out still hold
	b.EndStep(r);                                     // step 1: the hole is shown at once
	CHECK(r.rebuild && r.busy && r.destroyed.size() == 1 && b.StaticsActive() && b.CurrentSchema().Empty());
	for (int step = 2; step <= 4; ++step) {
		b.EndStep(r);
		CHECK(!r.rebuild && r.busy && b.CurrentSchema().Empty());
	}
	b.EndStep(r);                                     // step 5: the schema is built (the whole budget)
	CHECK(!r.rebuild && !b.CurrentSchema().Empty());
	b.EndStep(r);                                     // step 6: redraw, iteration 1 - nothing breaks
	CHECK(r.rebuild && r.broken.empty() && r.detached.empty());
	CHECK(b.CurrentSchema().Empty() && !b.StaticsActive() && r.busy);
	int releasedAt = 0;
	for (int step = 7; step <= 40; ++step) {
		b.EndStep(r);
		if (!r.busy && releasedAt == 0)
			releasedAt = step;
	}
	CHECK(releasedAt == 35);                          // the world is held 30 steps after settling
}

void TestStaticsSliceOriginal()
{
	Building b(GenerateTestBuilding(TestBuildingDesc{}));
	const Lattice& lat = b.GetLattice();
	CutHalf(b);
	StepResult r;
	size_t broken = 0, detached = 0;
	int steps = 0;
	for (; steps < 300; ++steps) {
		b.EndStep(r);
		broken += r.broken.size();
		detached += r.detached.size();
		if (!b.StaticsActive())
			break;
	}
	CHECK(!b.StaticsActive());
	CHECK(CountLeftAbove(lat) == 0);
	CHECK(b.CountUnsupported() == 0);
	CHECK(detached > 2000);
	std::printf("  statics, half collapse: settled after %d steps, broken %zu, detached %zu\n", steps + 1, broken, detached);
}

void TestDeterminism()
{
	auto run = [](std::vector<uint32_t>& log) {
		Building b(GenerateTestBuilding(TestBuildingDesc{}));
		SRand rnd(12345);
		StepResult r;
		for (int i = 0; i < 20; ++i) {
			b.Explode(RandomPoint(rnd, b.GetLattice().Bounds(), 0.0f), 1000, 8.0f);
			for (int step = 0; step < 8; ++step) {
				b.EndStep(r);
				log.push_back(uint32_t(r.destroyed.size()));
				log.push_back(uint32_t(r.broken.size()));
				log.push_back(uint32_t(r.detached.size()));
				log.push_back(uint32_t(r.dirtyParts.size()));
				log.push_back(r.piecesLeft);
				log.insert(log.end(), r.destroyed.begin(), r.destroyed.end());
				log.insert(log.end(), r.broken.begin(), r.broken.end());
				log.insert(log.end(), r.detached.begin(), r.detached.end());
			}
		}
	};
	std::vector<uint32_t> a, c;
	run(a);
	run(c);
	CHECK(!a.empty());
	CHECK(a == c);
}

// The scenarios of the engine layer: the names, the end, nothing left hanging, the same collapse
// every time.
void TestScenarios()
{
	for (const char* name : { "idle", "grenades", "hundred", "half" }) {
		ScenarioKind kind = ScenarioKind::Idle;
		CHECK(ParseScenario(name, kind));
		CHECK(std::strcmp(ScenarioName(kind), name) == 0);
	}
	ScenarioKind unknown = ScenarioKind::Idle;
	CHECK(!ParseScenario("grenade", unknown));
	CHECK(ScenarioMode(ScenarioKind::Hundred) == CollapseMode::Connectivity);
	CHECK(ScenarioMode(ScenarioKind::Grenades) == CollapseMode::Original);

	const TestBuildingDesc desc{};
	auto run = [&](ScenarioKind kind, int& outSteps, int& outLastDamage) {
		Building b(GenerateTestBuilding(desc), ScenarioMode(kind));
		Scenario plan(kind, 4 * desc.floors - 1);
		Trace trace;
		StepResult r;
		for (outSteps = 0; outSteps < kScenarioMaxSteps; ++outSteps) {
			plan.BeforeStep(b, outSteps);
			b.EndStep(r);
			trace.Step(r);
			if (plan.Finished(b, outSteps))
				break;
		}
		outLastDamage = plan.LastDamageStep();
		CHECK(b.CountUnsupported() == 0);
		return trace.h;
	};
	int steps = 0, lastDamage = 0, stepsAgain = 0, lastDamageAgain = 0;
	const uint64_t grenades = run(ScenarioKind::Grenades, steps, lastDamage);
	CHECK(steps < kScenarioMaxSteps - 1);
	CHECK(lastDamage == kScenarioFirstDamageStep + kGrenadeInterval * (kGrenadeCount - 1));
	CHECK(run(ScenarioKind::Grenades, stepsAgain, lastDamageAgain) == grenades);
	CHECK(stepsAgain == steps);
	run(ScenarioKind::Idle, steps, lastDamage);
	CHECK(steps == 199 && lastDamage == -1);
	run(ScenarioKind::Half, steps, lastDamage);
	CHECK(lastDamage == kScenarioFirstDamageStep && steps < kScenarioMaxSteps - 1);
	run(ScenarioKind::Hundred, steps, lastDamage);
	CHECK(lastDamage >= kScenarioFirstDamageStep && steps < kScenarioMaxSteps - 1);
}

} // namespace

int main()
{
	std::printf("ssproto core tests\n");
	TestRayBox();
	TestGeneration();
	TestRaycastMatchesBruteForce();
	TestOverlapMatchesBruteForce();
	TestDamageSpot();
	TestExplodeFormula();
	TestConnectivityColumn();
	TestConnectivitySlice();
	TestDirtyParts();
	TestStaticsGeneratedBuildingStands();
	TestStaticsSpanHolds();
	TestStaticsDiagonal();
	TestStaticsCantileverLoads();
	TestStaticsMomentBreak();
	TestStaticsPressureBreak();
	TestStaticsRhythm();
	TestStaticsSliceOriginal();
	TestDeterminism();
	TestScenarios();
	std::printf("%d checks, %d failed\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
