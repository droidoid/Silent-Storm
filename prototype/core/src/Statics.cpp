#include "ssproto/Statics.h"

#include <algorithm>
#include <cmath>

namespace ssproto {

namespace {

constexpr uint32_t kGround = 1u << 16;     // ЗЕМЛЯ: rests on the ground, always stable
constexpr uint32_t kChecked = 1u << 19;    // ПРОВЕРЕН_НА_СВОБОДУ (pass 2)
constexpr uint32_t kDestroyed = 1u << 21;  // РАЗРУШЕН
constexpr uint32_t kFront = 1u << 22;      // ФРОНТ_ВОЛНЫ (pass 3)
constexpr uint8_t kRodLocks = 2;           // NLOCKS: how many times waves may pass one rod
constexpr float kWeightFactor = 0.1f;      // AddNode: weight = 0.1 * volume * density

int Opposite(int d) { return d ^ 1; }      // XP<->XM, YP<->YM, UP<->DN

void DiagonalAxes(int d, int& ax, int& ay)
{
	ax = (d == kXPYP || d == kXPYM) ? kXP : kXM;
	ay = (d == kXPYP || d == kXMYP) ? kYP : kYM;
}

bool HasOppositeArrows(uint32_t f)
{
	auto has = [f](int d) { return (f & (1u << d)) != 0; };
	return (has(kXP) && has(kXM)) || (has(kYP) && has(kYM)) || (has(kXPYP) && has(kXMYM)) || (has(kXMYP) && has(kXPYM));
}

} // namespace

MaterialStatics MaterialStaticsOf(MaterialId m)
{
	switch (m) {
	case MaterialId::Stone: return { 2500.0f, 40.0f, 140.0f };
	case MaterialId::Wood: return { 400.0f, 40.0f, 140.0f };
	case MaterialId::Bricks: return { 1600.0f, 50.0f, 140.0f };
	case MaterialId::Concrete: return { 2200.0f, 75.0f, 140.0f };
	default: return { 1.0f, 40.0f, 140.0f };   // no armour: pressure 40, moment 140
	}
}

void Schema::Clear()
{
	joints.clear();
	rods.clear();
	nodeToJoint.clear();
	layerStart.clear();
	waves.clear();
	records.clear();
	front.clear();
	nextFront.clear();
}

void Schema::Build(const Lattice& lat)
{
	Clear();
	const uint32_t count = lat.NodeCount();
	nodeToJoint.assign(count, -1);
	layerStart.assign(size_t(lat.nz) + 1, 0);
	std::vector<MaterialStatics> own;
	for (NodeId n = 0; n < count; ++n) {
		if (lat.hp[n] == kHpEmpty)
			continue;
		int x, y, z;
		lat.Coords(n, x, y, z);
		const MaterialStatics ms = MaterialStaticsOf(MaterialId(lat.material[n]));
		Joint j{};
		j.node = n;
		j.x = int16_t(x);
		j.y = int16_t(y);
		j.z = int16_t(z);
		for (int32_t& r : j.rod)
			r = -1;
		const bool ground = z == 0 || lat.hp[n] == kHpIndestructible;
		j.flags = ground ? kGround : 0u;
		j.stability = ground ? Stability::Stable : Stability::Unknown;
		j.weight = kWeightFactor * Volume(lat.PieceBox(n)) * ms.weight;
		j.limitPressure = ms.ultimatePressure;
		j.limitMoment = ms.ultimateMoment;
		j.wave = -1;
		j.waveStep = -1;
		nodeToJoint[n] = int32_t(joints.size());
		joints.push_back(j);
		own.push_back(ms);
		++layerStart[size_t(z) + 1];
	}
	for (size_t z = 1; z < layerStart.size(); ++z)
		layerStart[z] += layerStart[z - 1];

	// A rod between every two face-adjacent pieces. Both ends take the larger limits of the two
	// materials (the original: maximum over the parts whose rods touch the joint).
	const uint32_t row = uint32_t(lat.nx), layer = lat.LayerSize();
	for (int32_t a = 0; a < int32_t(joints.size()); ++a) {
		const Joint ja = joints[size_t(a)];
		const struct {
			int dir;
			bool inside;
			NodeId neighbour;
		} steps[3] = { { kXP, ja.x + 1 < lat.nx, ja.node + 1 }, { kYP, ja.y + 1 < lat.ny, ja.node + row },
			{ kUP, ja.z + 1 < lat.nz, ja.node + layer } };
		for (const auto& s : steps) {
			if (!s.inside)
				continue;
			const int32_t b = nodeToJoint[s.neighbour];
			if (b < 0)
				continue;
			const int32_t r = int32_t(rods.size());
			rods.push_back({ a, b, 0, false });
			Joint& A = joints[size_t(a)];
			Joint& B = joints[size_t(b)];
			A.rod[s.dir] = r;
			B.rod[Opposite(s.dir)] = r;
			A.limitPressure = std::max(A.limitPressure, own[size_t(b)].ultimatePressure);
			A.limitMoment = std::max(A.limitMoment, own[size_t(b)].ultimateMoment);
			B.limitPressure = std::max(B.limitPressure, own[size_t(a)].ultimatePressure);
			B.limitMoment = std::max(B.limitMoment, own[size_t(a)].ultimateMoment);
		}
	}
}

const Schema::Joint* Schema::JointOf(NodeId node) const
{
	if (node >= nodeToJoint.size() || nodeToJoint[node] < 0)
		return nullptr;
	return &joints[size_t(nodeToJoint[node])];
}

Stability Schema::StabilityOfNode(NodeId node) const
{
	const Joint* j = JointOf(node);
	return j ? j->stability : Stability::Unknown;
}

float Schema::PressureOfNode(NodeId node) const
{
	const Joint* j = JointOf(node);
	return j ? j->pressure : 0.0f;
}

float Schema::MomentOfNode(NodeId node, int direction) const
{
	const Joint* j = JointOf(node);
	return (j && direction >= kXP && direction <= kYM) ? j->moment[direction] : 0.0f;
}

float Schema::WeightOfNode(NodeId node) const
{
	const Joint* j = JointOf(node);
	return j ? j->weight : 0.0f;
}

// ---- pass 1 ------------------------------------------------------------------------------------

void Schema::ComputeStability()
{
	static const int kShots[8] = { kXP, kXM, kYP, kYM, kXPYP, kXMYP, kXMYM, kXPYM };
	for (size_t z = 0; z + 1 < layerStart.size(); ++z) {
		const int32_t begin = layerStart[z], end = layerStart[z + 1];
		// Vertical: stable if standing on a stable joint; unstable without a rod down (not ground).
		for (int32_t j = begin; j < end; ++j) {
			Joint& J = joints[size_t(j)];
			if ((J.flags & kDestroyed) || J.stability == Stability::Stable)
				continue;
			const int32_t down = J.rod[kDN];
			if (!Alive(down)) {
				if (!(J.flags & kGround))
					J.stability = Stability::Unstable;
			} else if (joints[size_t(Other(down, j))].stability == Stability::Stable) {
				J.stability = Stability::Stable;
			}
		}
		// Horizontal: stable joints shoot arrows until nothing changes.
		bool changed;
		do {
			changed = false;
			for (int32_t j = begin; j < end; ++j) {
				const Joint& J = joints[size_t(j)];
				if ((J.flags & kDestroyed) || J.stability != Stability::Stable)
					continue;
				for (const int d : kShots)
					if (Shoot(j, d))
						changed = true;
			}
		} while (changed);
	}
}

// The arrow d in a joint means: somewhere on the side opposite to d, along a continuous chain of
// rods in this layer, there is a stable joint. Returns true if some joint became stable.
bool Schema::Shoot(int32_t joint, int d)
{
	const uint32_t bit = 1u << d;
	bool changed = false;
	for (int32_t cur = joint; cur >= 0;) {
		Joint& J = joints[size_t(cur)];
		if (J.flags & bit)
			break;
		J.flags |= bit;
		int32_t next = -1;
		if (d < 4) {
			if (Alive(J.rod[d]))
				next = Other(J.rod[d], cur);
		} else {
			// Diagonal: first along x, then along y; along y first only if there is no x rod at all.
			int ax, ay;
			DiagonalAxes(d, ax, ay);
			if (J.rod[ax] >= 0) {
				const int32_t n1 = Other(J.rod[ax], cur);
				const int32_t r2 = joints[size_t(n1)].rod[ay];
				if (Alive(r2))
					next = Other(r2, n1);
			} else if (J.rod[ay] >= 0) {
				const int32_t n1 = Other(J.rod[ay], cur);
				const int32_t r2 = joints[size_t(n1)].rod[ax];
				if (Alive(r2))
					next = Other(r2, n1);
			}
		}
		if (J.stability != Stability::Stable && HasOppositeArrows(J.flags)) {
			J.stability = Stability::Stable;
			changed = true;
		}
		cur = next;
	}
	return changed;
}

// ---- pass 2 ------------------------------------------------------------------------------------

void Schema::FindFree()
{
	for (int32_t j = 0; j < int32_t(joints.size()); ++j) {
		const Joint& start = joints[size_t(j)];
		if (start.stability != Stability::Unstable && start.stability != Stability::Unknown)
			continue;
		if (start.flags & kChecked)
			continue;
		region.clear();
		stack.clear();
		stack.push_back(j);
		bool touchesSupport = false;
		while (!stack.empty()) {
			const int32_t k = stack.back();
			stack.pop_back();
			Joint& K = joints[size_t(k)];
			if (K.stability == Stability::Stable || K.stability == Stability::UnstableNotFree) {
				touchesSupport = true;
				continue;
			}
			if (K.flags & kChecked)
				continue;
			K.flags |= kChecked;
			region.push_back(k);
			for (int d = 0; d < 6; ++d)
				if (Alive(K.rod[d]))
					stack.push_back(Other(K.rod[d], k));
		}
		for (const int32_t k : region) {
			Joint& K = joints[size_t(k)];
			if (K.stability == Stability::Unstable || K.stability == Stability::Unknown)
				K.stability = touchesSupport ? Stability::UnstableNotFree : Stability::Free;
		}
	}
}

// ---- pass 3 ------------------------------------------------------------------------------------

bool Schema::Advance(int32_t wave, int32_t rod, int32_t from, int direction, int32_t step)
{
	Rod& r = rods[size_t(rod)];
	if (r.locks >= kRodLocks)
		return false;
	++r.locks;
	const int32_t to = Other(rod, from);
	Joint& T = joints[size_t(to)];
	T.flags |= kFront;
	T.wave = wave;
	T.waveDir = uint8_t(direction);
	T.waveStep = step;
	nextFront.push_back(to);
	advanced = true;
	return true;
}

void Schema::ProcessFront(int32_t j, int32_t step)
{
	Joint& J = joints[size_t(j)];
	if (!(J.flags & kFront) || J.waveStep == step || J.stability == Stability::Stable)
		return;
	records.push_back({ j, J.wave });
	J.supportSum += waves[size_t(J.wave)].weight;
	++J.supportCount;
	const int back = Opposite(J.waveDir);
	int candidates[6];
	int count = 0;
	for (int d = 0; d < 6; ++d) {
		if (d == back)
			continue;
		const int32_t r = J.rod[d];
		if (!Alive(r) || (joints[size_t(Other(r, j))].flags & kFront) || rods[size_t(r)].locks >= kRodLocks)
			continue;
		candidates[count++] = d;
	}
	// With two or more ways on, the joint stays a front: next step it records the same wave again.
	if (count < 2)
		J.flags &= ~kFront;
	else
		nextFront.push_back(j);
	if (count == 0)
		return;
	const int d = candidates[rnd.Get(count)];
	Advance(J.wave, J.rod[d], j, d, step);
}

void Schema::AddMoment(int32_t source, float share, const Joint& mass, int firstStep)
{
	Joint& S = joints[size_t(source)];
	const float dx = float(mass.x - S.x), dy = float(mass.y - S.y);   // lever in lattice steps
	switch (firstStep) {
	case kXP: S.moment[kXP] += dx * share; break;
	case kXM: S.moment[kXM] -= dx * share; break;
	case kYP: S.moment[kYP] += dy * share; break;
	case kYM: S.moment[kYM] -= dy * share; break;
	default: break;   // up and down: pressure only
	}
	S.pressure += share;
}

void Schema::ComputeMoments()
{
	waves.clear();
	records.clear();
	front.clear();
	nextFront.clear();
	advanced = false;
	// Every stable joint sends a wave into each unstable neighbour. The wave weight is 1 / the number
	// of directions without a stable neighbour (directions without a rod count too).
	for (int32_t s = 0; s < int32_t(joints.size()); ++s) {
		if (joints[size_t(s)].stability != Stability::Stable)
			continue;
		int edges[6];
		int count = 0;
		for (int d = 0; d < 6; ++d) {
			const int32_t r = joints[size_t(s)].rod[d];
			if (Alive(r) && joints[size_t(Other(r, s))].stability == Stability::Stable)
				continue;
			edges[count++] = d;
		}
		if (count == 0)
			continue;
		const float w = 1.0f / float(count);
		for (int k = 0; k < count; ++k) {
			const int32_t r = joints[size_t(s)].rod[edges[k]];
			if (!Alive(r))
				continue;
			const int32_t id = int32_t(waves.size());
			waves.push_back({ s, w, uint8_t(Opposite(edges[k])) });
			Advance(id, r, s, edges[k], -1);
		}
	}
	rnd = SRand(1);   // the same seed at every iteration: the result is deterministic
	for (int32_t step = 0; advanced; ++step) {
		advanced = false;
		front.swap(nextFront);
		nextFront.clear();
		for (size_t i = 0; i < front.size(); ++i)
			ProcessFront(front[i], step);
		front.clear();
	}
	// Every hanging joint hands its weight to the sources of the waves that reached it.
	for (Joint& J : joints)
		if (!(J.flags & kDestroyed) && J.stability != Stability::Stable && J.supportCount == 0 && J.stability != Stability::Free)
			J.stability = Stability::UnknownMoment;   // "Cant compute moment": its weight is lost
	for (const auto& rec : records) {
		const Joint& J = joints[size_t(rec.first)];
		if ((J.flags & kDestroyed) || J.stability == Stability::Stable)
			continue;
		const Wave& w = waves[size_t(rec.second)];
		AddMoment(w.source, J.weight * w.weight / J.supportSum, J, Opposite(w.back));
	}
}

// ---- rules -------------------------------------------------------------------------------------

void Schema::DestroyRod(int32_t rod, bool checkVerticalChain)
{
	rods[size_t(rod)].destroyed = true;
	if (!checkVerticalChain)
		return;
	// A joint left with a rod up and a rod down but nothing sideways drops out of the schema (its
	// piece stays in the lattice); its two vertical rods go, without a cascade.
	for (const int32_t end : { rods[size_t(rod)].a, rods[size_t(rod)].b }) {
		Joint& E = joints[size_t(end)];
		if (E.flags & kDestroyed)
			continue;
		if (Alive(E.rod[kUP]) && Alive(E.rod[kDN]) && !Alive(E.rod[kXP]) && !Alive(E.rod[kXM]) && !Alive(E.rod[kYP]) &&
			!Alive(E.rod[kYM])) {
			E.flags |= kDestroyed;
			DestroyRod(E.rod[kUP], false);
			DestroyRod(E.rod[kDN], false);
		}
	}
}

void Schema::MarkDestroyed(int32_t j)
{
	Joint& J = joints[size_t(j)];
	J.flags |= kDestroyed;
	for (int d = 0; d < 6; ++d)
		if (Alive(J.rod[d]))
			DestroyRod(J.rod[d], true);
}

void Schema::OnNodeDestroyed(NodeId node)
{
	if (node < nodeToJoint.size() && nodeToJoint[node] >= 0)
		MarkDestroyed(nodeToJoint[node]);
}

bool Schema::DestroyJoint(int32_t j, bool effects, IBreaker& breaker)
{
	if (joints[size_t(j)].flags & kGround)
		return false;
	return breaker.BreakNode(joints[size_t(j)].node, effects);
}

bool Schema::ApplyRules(IBreaker& breaker)
{
	bool changed = false;
	for (int32_t j = 0; j < int32_t(joints.size()); ++j) {
		const Stability st = joints[size_t(j)].stability;
		if (st == Stability::Free) {
			if (DestroyJoint(j, false, breaker))   // quiet: no dust, no sound
				changed = true;
			continue;
		}
		if (st != Stability::Stable)
			continue;
		if (joints[size_t(j)].pressure > joints[size_t(j)].limitPressure && DestroyJoint(j, true, breaker)) {
			changed = true;
			continue;
		}
		for (int d = kXP; d <= kYM; ++d) {
			const Joint& J = joints[size_t(j)];
			if (!(std::fabs(J.moment[d]) > J.limitMoment))
				continue;
			const int32_t r = J.rod[d];
			if (!Alive(r))
				continue;
			if (DestroyJoint(Other(r, j), true, breaker)) {
				changed = true;
				DestroyRod(r, true);
			}
		}
	}
	return changed;
}

void Schema::RunPasses()
{
	ComputeStability();
	FindFree();
	ComputeMoments();
}

bool Schema::Recalc(IBreaker& breaker)
{
	RunPasses();
	return ApplyRules(breaker);
}

void Schema::Reset()
{
	for (Joint& J : joints) {
		if (J.flags & kDestroyed)
			continue;
		J.flags &= kGround;
		J.stability = (J.flags & kGround) ? Stability::Stable : Stability::Unknown;
		for (float& m : J.moment)
			m = 0.0f;
		J.pressure = 0.0f;
		J.supportSum = 0.0f;
		J.supportCount = 0;
		J.wave = -1;
		J.waveStep = -1;
		bool anyRod = false;
		for (const int32_t r : J.rod)
			if (Alive(r))
				anyRod = true;
		if (!anyRod)
			J.flags |= kDestroyed;
	}
	for (Rod& r : rods)
		r.locks = 0;
	waves.clear();
	records.clear();
}

} // namespace ssproto
