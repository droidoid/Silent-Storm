// Strength lattice of one building: the data model of the prototype core.
//
// Follows the original's "решётка прочности" (spec "Мир 2", sections 4 and 6): a 3D grid with a
// 0.625 m pitch on all axes (4 planes per 2.5 m floor), one byte of HP per node, one piece per node.
// Simplification of the prototype: the node sits in the middle of its cell and the piece is a box
// inside the cell (a wall piece is the cell cut down to the wall thickness, a slab piece is the
// bottom of the cell); the original pieces are irregular meshes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ssproto/Math.h"

namespace ssproto {

using NodeId = uint32_t;

// HP byte semantics of the original.
constexpr uint8_t kHpEmpty = 0;             // no piece, or destroyed
constexpr uint8_t kHpMax = 253;             // the most a destructible node can hold
constexpr uint8_t kHpCellar = 254;          // bottom/cellar node: never damaged, rests on the ground
constexpr uint8_t kHpIndestructible = 255;  // never damaged

enum class PieceKind : uint8_t { None = 0, Wall = 1, Slab = 2 };

// RPGMaterials ids of the original database (spec "Мир 2", appendix A).
enum class MaterialId : uint8_t { None = 0, Stone = 3, Wood = 4, Bricks = 5, Concrete = 24 };

// RPGMaterials.VP: HP of a piece = ceil(2 * VP * volume in m^3).
int MaterialVp(MaterialId m);

// Lattice::shape bits of a wall piece: the piece reaches the cell face on that side (a wall
// piece continues there); otherwise it ends at half the wall thickness from the node.
enum ShapeBits : uint8_t { kReachXM = 1, kReachXP = 2, kReachYM = 4, kReachYP = 8 };

struct Lattice {
	int nx = 0, ny = 0, nz = 0;   // nodes along x, y, z (z = planes, 4 per floor)
	float step = 0.625f;          // pitch in metres on every axis (FP_GRID_STEP of the original)
	Vec3 origin;                  // min corner of the cell of node (0, 0, 0)
	float wallThickness = 0.25f;
	float slabThickness = 0.2f;
	std::vector<uint8_t> hp;      // kHp* semantics
	std::vector<uint8_t> kind;    // PieceKind
	std::vector<uint8_t> material;// MaterialId
	std::vector<uint8_t> shape;   // ShapeBits (walls only)

	void Resize(int sizeX, int sizeY, int sizeZ);
	uint32_t NodeCount() const { return uint32_t(hp.size()); }
	uint32_t LayerSize() const { return uint32_t(nx) * uint32_t(ny); }
	bool InBounds(int x, int y, int z) const { return x >= 0 && y >= 0 && z >= 0 && x < nx && y < ny && z < nz; }
	NodeId Node(int x, int y, int z) const { return (uint32_t(z) * uint32_t(ny) + uint32_t(y)) * uint32_t(nx) + uint32_t(x); }
	void Coords(NodeId n, int& x, int& y, int& z) const
	{
		const uint32_t ux = uint32_t(nx), uy = uint32_t(ny);
		x = int(n % ux);
		y = int((n / ux) % uy);
		z = int(n / (ux * uy));
	}
	Aabb CellBox(int x, int y, int z) const;
	Vec3 NodeCenter(NodeId n) const;
	Aabb PieceBox(NodeId n) const;   // meaningful when kind[n] != None
	Aabb Bounds() const;
};

} // namespace ssproto
