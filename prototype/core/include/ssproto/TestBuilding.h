// Synthetic test building: a box of rooms with outer brick walls, inner walls with doors,
// windows, a slab on every floor and a roof. Stands in for authored buildings until real ones
// are imported; needs no data of the original.
#pragma once

#include <cstddef>
#include <vector>

#include "ssproto/Lattice.h"

namespace ssproto {

struct TestBuildingDesc {
	int blocksX = 20;                        // 1.25 m blocks along x
	int blocksY = 16;
	int floors = 3;                          // 2.5 m each, plus a roof slab
	int innerWallsX = 4;                     // inner walls parallel to y (they split x)
	int innerWallsY = 1;                     // inner walls parallel to x
	int windowEvery = 2;                     // a window in every n-th block of an outer wall; 0 = none
	MaterialId outerWalls = MaterialId::Bricks;
	MaterialId innerWalls = MaterialId::Wood;
	MaterialId slabs = MaterialId::Bricks;   // floor plates of the original use brick parameters
	float wallThickness = 0.25f;
	float slabThickness = 0.2f;
	Vec3 origin;
};

struct LightSpot {
	Vec3 position;   // middle of the room, 2 m above its floor
	float radius;    // reaches the corners of the room
	int floor;
};

struct TestBuildingInfo {
	size_t wallPieces = 0;
	size_t slabPieces = 0;
	size_t windows = 0;
	size_t doors = 0;
	std::vector<LightSpot> lights;   // one per room
};

// Pieces of the ground plane get kHpCellar (they rest on the ground, as the bottom nodes of the
// original). Other pieces: HP = min(253, ceil(2 * VP * volume)); the original passes this sum as
// a byte, so above 255 it wraps (263 -> 7) - not reproduced here.
Lattice GenerateTestBuilding(const TestBuildingDesc& desc, TestBuildingInfo* info = nullptr);

} // namespace ssproto
