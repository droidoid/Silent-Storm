#include "ssproto/Lattice.h"

namespace ssproto {

int MaterialVp(MaterialId m)
{
	switch (m) {
	case MaterialId::Bricks: return 650;
	case MaterialId::Stone:
	case MaterialId::Wood:
	case MaterialId::Concrete: return 400;
	default: return 0;
	}
}

void Lattice::Resize(int sizeX, int sizeY, int sizeZ)
{
	nx = sizeX;
	ny = sizeY;
	nz = sizeZ;
	const size_t n = size_t(sizeX) * size_t(sizeY) * size_t(sizeZ);
	hp.assign(n, kHpEmpty);
	kind.assign(n, uint8_t(PieceKind::None));
	material.assign(n, uint8_t(MaterialId::None));
	shape.assign(n, 0);
}

Aabb Lattice::CellBox(int x, int y, int z) const
{
	Aabb b;
	b.min = { origin.x + float(x) * step, origin.y + float(y) * step, origin.z + float(z) * step };
	b.max = { b.min.x + step, b.min.y + step, b.min.z + step };
	return b;
}

Vec3 Lattice::NodeCenter(NodeId n) const
{
	int x, y, z;
	Coords(n, x, y, z);
	return Center(CellBox(x, y, z));
}

Aabb Lattice::PieceBox(NodeId n) const
{
	int x, y, z;
	Coords(n, x, y, z);
	const Aabb cell = CellBox(x, y, z);
	Aabb b = cell;
	switch (PieceKind(kind[n])) {
	case PieceKind::Slab:
		b.max.z = cell.min.z + slabThickness;
		break;
	case PieceKind::Wall: {
		const Vec3 c = Center(cell);
		const float h = wallThickness * 0.5f;
		const uint8_t s = shape[n];
		b.min.x = (s & kReachXM) ? cell.min.x : c.x - h;
		b.max.x = (s & kReachXP) ? cell.max.x : c.x + h;
		b.min.y = (s & kReachYM) ? cell.min.y : c.y - h;
		b.max.y = (s & kReachYP) ? cell.max.y : c.y + h;
		break;
	}
	default:
		break;
	}
	return b;
}

Aabb Lattice::Bounds() const
{
	Aabb b;
	b.min = origin;
	b.max = { origin.x + float(nx) * step, origin.y + float(ny) * step, origin.z + float(nz) * step };
	return b;
}

} // namespace ssproto
