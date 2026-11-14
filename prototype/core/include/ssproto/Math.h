// Small vector/box helpers for the prototype core. No engine types: the engine layer converts.
#pragma once

#include <cmath>
#include <utility>

namespace ssproto {

struct Vec3 {
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float LengthSq(Vec3 a) { return Dot(a, a); }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

struct Aabb {
	Vec3 min, max;
};

inline Vec3 Center(const Aabb& b) { return (b.min + b.max) * 0.5f; }
inline float Volume(const Aabb& b) { return (b.max.x - b.min.x) * (b.max.y - b.min.y) * (b.max.z - b.min.z); }

// Ray against box (slab method). The ray is origin + t * dir; the hit interval is clipped to
// [tMin, tMax]. A zero component of dir is handled explicitly (no 0 * inf).
inline bool RayBox(Vec3 origin, Vec3 dir, const Aabb& box, float tMin, float tMax, float& tEnter, float& tExit)
{
	const float o[3] = { origin.x, origin.y, origin.z };
	const float d[3] = { dir.x, dir.y, dir.z };
	const float lo[3] = { box.min.x, box.min.y, box.min.z };
	const float hi[3] = { box.max.x, box.max.y, box.max.z };
	for (int a = 0; a < 3; ++a) {
		if (d[a] == 0.0f) {
			if (o[a] < lo[a] || o[a] > hi[a])
				return false;
			continue;
		}
		const float inv = 1.0f / d[a];
		float t0 = (lo[a] - o[a]) * inv;
		float t1 = (hi[a] - o[a]) * inv;
		if (t0 > t1)
			std::swap(t0, t1);
		if (t0 > tMin)
			tMin = t0;
		if (t1 < tMax)
			tMax = t1;
		if (tMin > tMax)
			return false;
	}
	tEnter = tMin;
	tExit = tMax;
	return true;
}

// Squared distance from a point to a box; 0 when the point is inside.
inline float DistanceSq(Vec3 p, const Aabb& b)
{
	auto axis = [](float v, float lo, float hi) {
		const float d = v < lo ? lo - v : (v > hi ? v - hi : 0.0f);
		return d * d;
	};
	return axis(p.x, b.min.x, b.max.x) + axis(p.y, b.min.y, b.max.y) + axis(p.z, b.min.z, b.max.z);
}

// Linear congruential generator of the original (MS rand), spec "Мир 2", terms: SRand.
struct SRand {
	unsigned seed = 1;
	explicit SRand(unsigned s = 1) : seed(s) {}
	unsigned Next() { seed = seed * 214013u + 2531011u; return (seed >> 16) & 0x7FFFu; }
	// 0 .. n-1
	int Get(int n) { return int((unsigned long long)Next() * unsigned(n) / 0x8000u); }
	float GetFloat(float a, float b) { return a + float(Get(0xFFFF)) * (b - a) / float(0xFFFF); }
};

} // namespace ssproto
