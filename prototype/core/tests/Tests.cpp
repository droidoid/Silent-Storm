// Tests of the prototype core. No framework: CHECK counts failures, the exit code is their number.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "ssproto/Building.h"
#include "ssproto/TestBuilding.h"

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

void TestRayBox()
{
	const Aabb box{ { 0, 0, 0 }, { 1, 1, 1 } };
	float te = 0, tx = 0;
	CHECK(RayBox({ -1, 0.5f, 0.5f }, { 1, 0, 0 }, box, 0.0f, 100.0f, te, tx) && std::fabs(te - 1) < 1e-6f && std::fabs(tx - 2) < 1e-6f);
	CHECK(!RayBox({ -1, 1.5f, 0.5f }, { 1, 0, 0 }, box, 0.0f, 100.0f, te, tx));
	CHECK(!RayBox({ -1, 0.5f, 0.5f }, { 1, 0, 0 }, box, 0.0f, 0.5f, te, tx));      // too short
	CHECK(RayBox({ 0.5f, 0.5f, 0.5f }, { 0, 0, 1 }, box, 0.0f, 100.0f, te, tx) && te == 0.0f && std::fabs(tx - 0.5f) < 1e-6f);
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

	Building b(lat);
	StepResult r;
	b.EndStep(r);
	CHECK(r.destroyed.empty() && r.detached.empty() && r.dirtyParts.empty());
	CHECK(r.piecesLeft == pieces);
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
			same = fast[k].node == slow[k].node && std::fabs(fast[k].tEnter - slow[k].tEnter) < 1e-3f &&
				std::fabs(fast[k].tExit - slow[k].tExit) < 1e-3f;
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
	CHECK(b.DamageSpot(mid, 60));                     // exactly to zero
	CHECK(!b.DamageSpot(mid, 10));                    // already gone
	CHECK(!b.DamageSpot(1000000, 10));                // outside the lattice
	StepResult r;
	b.EndStep(r);
	CHECK(r.destroyed.size() == 1 && r.destroyed[0] == mid);
	CHECK(r.detached.empty());                        // the indestructible node holds itself
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

void TestDetachColumn()
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
	Building b(lat);
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
	CHECK(r.destroyed.empty() && r.detached.empty() && r.dirtyParts.empty());
}

void TestSliceCollapse()
{
	Building b(GenerateTestBuilding(TestBuildingDesc{}));
	const Lattice& lat = b.GetLattice();
	const int cut = lat.nx / 2;
	size_t leftAbove = 0;
	for (NodeId n = 0; n < lat.NodeCount(); ++n) {
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (lat.hp[n] != kHpEmpty && x < cut && z >= 2)
			++leftAbove;
	}
	for (int z = 1; z < lat.nz; ++z)
		for (int y = 0; y < lat.ny; ++y)
			b.DamageSpot(lat.Node(cut, y, z), 255);
	for (int y = 0; y < lat.ny; ++y)
		for (int x = 0; x < cut; ++x)
			b.DamageSpot(lat.Node(x, y, 1), 255);
	StepResult r;
	b.EndStep(r);
	bool allLeftAbove = true;
	for (const NodeId n : r.detached) {
		int x, y, z;
		lat.Coords(n, x, y, z);
		if (!(x < cut && z >= 2))
			allLeftAbove = false;
	}
	CHECK(allLeftAbove);
	CHECK(r.detached.size() == leftAbove);
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

void TestDeterminism()
{
	auto run = [](std::vector<uint32_t>& log) {
		Building b(GenerateTestBuilding(TestBuildingDesc{}));
		SRand rnd(12345);
		StepResult r;
		for (int i = 0; i < 30; ++i) {
			b.Explode(RandomPoint(rnd, b.GetLattice().Bounds(), 0.0f), 1000, 8.0f);
			b.EndStep(r);
			log.push_back(uint32_t(r.destroyed.size()));
			log.push_back(uint32_t(r.detached.size()));
			log.push_back(uint32_t(r.dirtyParts.size()));
			log.push_back(r.piecesLeft);
			log.insert(log.end(), r.destroyed.begin(), r.destroyed.end());
			log.insert(log.end(), r.detached.begin(), r.detached.end());
		}
	};
	std::vector<uint32_t> a, c;
	run(a);
	run(c);
	CHECK(!a.empty());
	CHECK(a == c);
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
	TestDetachColumn();
	TestSliceCollapse();
	TestDirtyParts();
	TestDeterminism();
	std::printf("%d checks, %d failed\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
