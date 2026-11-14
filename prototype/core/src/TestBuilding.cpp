#include "ssproto/TestBuilding.h"

#include <algorithm>
#include <cmath>

namespace ssproto {

Lattice GenerateTestBuilding(const TestBuildingDesc& d, TestBuildingInfo* info)
{
	Lattice lat;
	lat.origin = d.origin;
	lat.wallThickness = d.wallThickness;
	lat.slabThickness = d.slabThickness;
	const int nx = 2 * d.blocksX, ny = 2 * d.blocksY, nz = 4 * d.floors + 1;
	lat.Resize(nx, ny, nz);

	// Wall lines in plan: node columns carry walls parallel to y, node rows walls parallel to x.
	std::vector<int> cols{ 0 }, rows{ 0 };
	for (int p = 0; p < d.innerWallsX; ++p)
		cols.push_back(2 * (d.blocksX * (p + 1) / (d.innerWallsX + 1)));
	cols.push_back(nx - 1);
	for (int p = 0; p < d.innerWallsY; ++p)
		rows.push_back(2 * (d.blocksY * (p + 1) / (d.innerWallsY + 1)));
	rows.push_back(ny - 1);
	std::sort(cols.begin(), cols.end());
	cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
	std::sort(rows.begin(), rows.end());
	rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

	auto isCol = [&](int x) { return std::binary_search(cols.begin(), cols.end(), x); };
	auto isRow = [&](int y) { return std::binary_search(rows.begin(), rows.end(), y); };
	auto isOuter = [&](int x, int y) { return x == 0 || y == 0 || x == nx - 1 || y == ny - 1; };
	auto put = [&](int x, int y, int z, PieceKind k, MaterialId m) {
		const NodeId n = lat.Node(x, y, z);
		lat.kind[n] = uint8_t(k);
		lat.material[n] = uint8_t(m);
	};
	auto clear = [&](int x, int y, int z) { put(x, y, z, PieceKind::None, MaterialId::None); };

	// Walls: the four planes of every floor.
	for (int f = 0; f < d.floors; ++f)
		for (int z = 4 * f; z < 4 * f + 4; ++z)
			for (int y = 0; y < ny; ++y)
				for (int x = 0; x < nx; ++x)
					if (isCol(x) || isRow(y))
						put(x, y, z, PieceKind::Wall, isOuter(x, y) ? d.outerWalls : d.innerWalls);

	// Slabs: the bottom plane of every floor between the walls, and the whole roof plane.
	for (int f = 0; f <= d.floors; ++f)
		for (int y = 0; y < ny; ++y)
			for (int x = 0; x < nx; ++x)
				if (f == d.floors || !(isCol(x) || isRow(y)))
					put(x, y, 4 * f, PieceKind::Slab, d.slabs);

	// Windows: 2 nodes wide, planes 1-2 of the floor (sill at 0.625 m, top at 1.875 m), never in a
	// corner block or where an inner wall meets the outer one.
	size_t windows = 0;
	auto windowBlock = [&](int b) { return d.windowEvery > 0 && b % d.windowEvery == (d.windowEvery > 1 ? 1 : 0); };
	for (int f = 0; f < d.floors; ++f) {
		for (int b = 1; b + 1 < d.blocksX; ++b) {
			const int xa = 2 * b, xb = xa + 1;
			if (!windowBlock(b) || isCol(xa) || isCol(xb))
				continue;
			for (const int y : { 0, ny - 1 }) {
				for (int z = 4 * f + 1; z <= 4 * f + 2; ++z) {
					clear(xa, y, z);
					clear(xb, y, z);
				}
				++windows;
			}
		}
		for (int b = 1; b + 1 < d.blocksY; ++b) {
			const int ya = 2 * b, yb = ya + 1;
			if (!windowBlock(b) || isRow(ya) || isRow(yb))
				continue;
			for (const int x : { 0, nx - 1 }) {
				for (int z = 4 * f + 1; z <= 4 * f + 2; ++z) {
					clear(x, ya, z);
					clear(x, yb, z);
				}
				++windows;
			}
		}
	}

	// Doors: one per stretch of inner wall between two crossing walls, 2 nodes wide, planes 0-2;
	// the slab continues under the door.
	size_t doors = 0;
	auto door = [&](int x0, int y0, int x1, int y1) {
		for (int f = 0; f < d.floors; ++f) {
			for (int z = 4 * f; z <= 4 * f + 2; ++z) {
				clear(x0, y0, z);
				clear(x1, y1, z);
			}
			put(x0, y0, 4 * f, PieceKind::Slab, d.slabs);
			put(x1, y1, 4 * f, PieceKind::Slab, d.slabs);
			++doors;
		}
	};
	for (size_t c = 1; c + 1 < cols.size(); ++c)
		for (size_t r = 0; r + 1 < rows.size(); ++r) {
			const int first = rows[r] + 1, len = rows[r + 1] - rows[r] - 1;
			if (len >= 4)
				door(cols[c], first + len / 2 - 1, cols[c], first + len / 2);
		}
	for (size_t r = 1; r + 1 < rows.size(); ++r)
		for (size_t c = 0; c + 1 < cols.size(); ++c) {
			const int first = cols[c] + 1, len = cols[c + 1] - cols[c] - 1;
			if (len >= 4)
				door(first + len / 2 - 1, rows[r], first + len / 2, rows[r]);
		}

	// Wall shapes: a wall piece reaches the cell face where the wall continues in the same plane.
	for (int z = 0; z < nz; ++z)
		for (int y = 0; y < ny; ++y)
			for (int x = 0; x < nx; ++x) {
				const NodeId n = lat.Node(x, y, z);
				if (PieceKind(lat.kind[n]) != PieceKind::Wall)
					continue;
				auto wallAt = [&](int ax, int ay) {
					return lat.InBounds(ax, ay, z) && PieceKind(lat.kind[lat.Node(ax, ay, z)]) == PieceKind::Wall;
				};
				uint8_t s = 0;
				if (wallAt(x - 1, y))
					s |= kReachXM;
				if (wallAt(x + 1, y))
					s |= kReachXP;
				if (wallAt(x, y - 1))
					s |= kReachYM;
				if (wallAt(x, y + 1))
					s |= kReachYP;
				lat.shape[n] = s;
			}

	// HP.
	size_t walls = 0, slabs = 0;
	const uint32_t count = lat.NodeCount();
	for (NodeId n = 0; n < count; ++n) {
		const PieceKind k = PieceKind(lat.kind[n]);
		if (k == PieceKind::None)
			continue;
		(k == PieceKind::Wall ? walls : slabs) += 1;
		if (n < lat.LayerSize()) {
			lat.hp[n] = kHpCellar;
			continue;
		}
		const float v = Volume(lat.PieceBox(n));
		const int h = int(std::ceil(2.0f * float(MaterialVp(MaterialId(lat.material[n]))) * v));
		lat.hp[n] = uint8_t(std::clamp(h, 1, int(kHpMax)));
	}

	if (info) {
		info->wallPieces = walls;
		info->slabPieces = slabs;
		info->windows = windows;
		info->doors = doors;
		info->lights.clear();
		const float s = lat.step;
		for (int f = 0; f < d.floors; ++f)
			for (size_t r = 0; r + 1 < rows.size(); ++r)
				for (size_t c = 0; c + 1 < cols.size(); ++c) {
					const float x0 = (float(cols[c]) + 1.0f) * s, x1 = float(cols[c + 1]) * s;
					const float y0 = (float(rows[r]) + 1.0f) * s, y1 = float(rows[r + 1]) * s;
					LightSpot light;
					light.position = { d.origin.x + 0.5f * (x0 + x1), d.origin.y + 0.5f * (y0 + y1),
						d.origin.z + float(4 * f) * s + 2.0f };
					light.radius = 0.5f * std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) + 1.0f;
					light.floor = f;
					info->lights.push_back(light);
				}
	}
	return lat;
}

} // namespace ssproto
