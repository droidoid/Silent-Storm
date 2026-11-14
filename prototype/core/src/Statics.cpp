#include "ssproto/Statics.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <initializer_list>

namespace ssproto {

namespace {

// Adds the time of one phase to the profile, if there is one.
class PhaseTimer {
public:
	PhaseTimer(StaticsProfile* p, int ph)
		: profile(p)
		, phase(ph)
	{
		if (profile)
			start = std::chrono::steady_clock::now();
	}
	~PhaseTimer()
	{
		if (!profile)
			return;
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		profile->ms[phase] += ms;
		profile->maxMs[phase] = std::max(profile->maxMs[phase], ms);
		++profile->runs[phase];
	}
	PhaseTimer(const PhaseTimer&) = delete;
	PhaseTimer& operator=(const PhaseTimer&) = delete;

private:
	StaticsProfile* profile;
	int phase;
	std::chrono::steady_clock::time_point start;
};

// Joint flags, one byte per joint. Ground is fixed at Build. A destroyed joint keeps all its flags,
// its stability and its loads as they were: the original's reset skips it, and the stale values
// still count in the later passes and rules (spec "Мир 2", sections 9 and 10).
constexpr uint8_t kGround = 1u << 0;     // ЗЕМЛЯ: rests on the ground, always stable
constexpr uint8_t kDestroyed = 1u << 1;  // РАЗРУШЕН
constexpr uint8_t kChecked = 1u << 2;    // ПРОВЕРЕН_НА_СВОБОДУ (pass 2)
constexpr uint8_t kFront = 1u << 3;      // ФРОНТ_ВОЛНЫ (pass 3)
constexpr uint8_t kLoaded = 1u << 4;     // pass 3 gave the joint pressure or moments (otherwise they are 0)
constexpr uint8_t kGone = 1u << 5;       // the lattice reported the node destroyed (OnNodeDestroyed)
constexpr uint8_t kRodLocks = 2;         // NLOCKS: how many times waves may pass one rod
constexpr float kWeightFactor = 0.1f;    // AddNode: weight = 0.1 * volume * density
// All eight arrows: what every joint of a layer ends pass 1 with once all of them are stable (each
// stable joint shoots every arrow, and the arrow is set on the shooter too).
constexpr uint16_t kAllArrows = uint16_t((1u << kXP) | (1u << kXM) | (1u << kYP) | (1u << kYM) | (1u << kXPYP) |
	(1u << kXMYP) | (1u << kXMYM) | (1u << kXPYM));

int Opposite(int d) { return d ^ 1; }             // XP<->XM, YP<->YM, UP<->DN
uint8_t Bit(int d) { return uint8_t(1u << d); }   // the bit of an axial direction in a mask of rods
constexpr uint8_t kVerticalOnly = uint8_t((1u << kUP) | (1u << kDN));

void DiagonalAxes(int d, int& ax, int& ay)
{
	ax = (d == kXPYP || d == kXPYM) ? kXP : kXM;
	ay = (d == kXPYP || d == kXMYP) ? kYP : kYM;
}

// The pairs of opposite arrows are the bits 0-1 (XP, XM), 2-3 (YP, YM), 7-9 (XPYP, XMYM) and 8-10
// (XMYP, XPYM).
bool HasOppositeArrows(uint32_t f) { return ((f & (f >> 1) & 0x5u) | (f & (f >> 2) & 0x180u)) != 0; }

// kNthBit[mask][k]: the position of the k-th set bit (from 0, lowest first) of a 6-bit mask.
struct NthBitTable {
	uint8_t at[64][6];
	constexpr NthBitTable()
		: at()
	{
		for (int m = 0; m < 64; ++m) {
			int k = 0;
			for (int b = 0; b < 6; ++b)
				if (m & (1 << b))
					at[m][k++] = uint8_t(b);
		}
	}
	const uint8_t* operator[](uint32_t m) const { return at[m]; }
};
constexpr NthBitTable kNthBit;

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
	info.clear();
	links.clear();
	flags.clear();
	alive.clear();
	stability.clear();
	arrows.clear();
	loads.clear();
	walkers.clear();
	sources.clear();
	nodeToJoint.clear();
	layerStart.clear();
	waves.clear();
	records.clear();
	front.clear();
	nextFront.clear();
	nextCount = 0;
	region.clear();
	work.clear();
	hanging.clear();
	hangingCount = 0;
	walked.clear();
	walkedCount = 0;
	rodCount = 0;
	fresh = false;
}

void Schema::Build(const Lattice& lat)
{
	const PhaseTimer timer(profile, kPhaseBuild);
	Clear();
	const uint32_t count = lat.NodeCount();
	size_t pieces = 0;
	for (NodeId n = 0; n < count; ++n)
		pieces += lat.hp[n] != kHpEmpty ? 1 : 0;
	info.resize(pieces);
	links.assign(6 * pieces, -1);
	flags.assign(pieces, 0);
	alive.assign(pieces, 0);
	stability.assign(pieces, Stability::Unknown);
	arrows.assign(pieces, 0);
	loads.assign(pieces, Load{});
	walkers.assign(pieces, Walker{});
	walked.assign(pieces, 0);   // every joint at most once
	walkedCount = 0;
	region.assign(pieces, 0);
	hanging.assign(pieces, 0);
	hangingCount = 0;
	sources.assign((pieces + 63) / 64, 0);
	nodeToJoint.assign(count, -1);
	layerStart.assign(size_t(lat.nz) + 1, 0);

	// One pass in node order, which is the joint order. A rod joins every two face-adjacent pieces;
	// it is made when its upper end (the one with the larger z, else y, else x) is reached. Both
	// ends take the larger limits of the two materials (the original: maximum over the parts whose
	// rods touch the joint).
	const uint32_t row = uint32_t(lat.nx), layer = lat.LayerSize();
	int32_t j = 0;
	NodeId n = 0;
	for (int z = 0; z < lat.nz; ++z) {
		layerStart[size_t(z)] = j;
		for (int y = 0; y < lat.ny; ++y)
			for (int x = 0; x < lat.nx; ++x, ++n) {
				if (lat.hp[n] == kHpEmpty)
					continue;
				const MaterialStatics ms = MaterialStaticsOf(MaterialId(lat.material[n]));
				JointInfo& J = info[size_t(j)];
				J.node = n;
				Walker& W = walkers[size_t(j)];
				W.x = int16_t(x);
				W.y = int16_t(y);
				W.weight = kWeightFactor * Volume(lat.PieceBox(n, x, y, z)) * ms.weight;
				J.limitPressure = ms.ultimatePressure;
				J.limitMoment = ms.ultimateMoment;
				const bool ground = z == 0 || lat.hp[n] == kHpIndestructible;
				flags[size_t(j)] = ground ? kGround : 0;
				stability[size_t(j)] = ground ? Stability::Stable : Stability::Unknown;
				nodeToJoint[n] = j;
				auto connect = [&](int dir, NodeId neighbour) {
					const int32_t k = nodeToJoint[neighbour];
					if (k < 0)
						return;
					links[6 * size_t(j) + size_t(dir)] = k;
					links[6 * size_t(k) + size_t(Opposite(dir))] = j;
					alive[size_t(j)] |= Bit(dir);
					alive[size_t(k)] |= Bit(Opposite(dir));
					++rodCount;
					const MaterialStatics other = MaterialStaticsOf(MaterialId(lat.material[neighbour]));
					J.limitPressure = std::max(J.limitPressure, other.ultimatePressure);
					J.limitMoment = std::max(J.limitMoment, other.ultimateMoment);
					JointInfo& K = info[size_t(k)];
					K.limitPressure = std::max(K.limitPressure, ms.ultimatePressure);
					K.limitMoment = std::max(K.limitMoment, ms.ultimateMoment);
				};
				if (x > 0)
					connect(kXM, n - 1);
				if (y > 0)
					connect(kYM, n - row);
				if (z > 0)
					connect(kDN, n - layer);
				++j;
			}
	}
	layerStart[size_t(lat.nz)] = j;
	for (size_t k = 0; k < pieces; ++k)
		for (int d = 0; d < 6; ++d) {
			const int32_t other = links[6 * k + size_t(d)];
			walkers[k].link[d] = other >= 0 ? other : int32_t(k);   // no rod: the joint itself (pass 3 reads it, masked)
		}
	fresh = true;
}

int32_t Schema::JointOf(NodeId node) const { return node < nodeToJoint.size() ? nodeToJoint[node] : -1; }

Stability Schema::StabilityOfNode(NodeId node) const
{
	const int32_t j = JointOf(node);
	return j >= 0 ? stability[size_t(j)] : Stability::Unknown;
}

float Schema::PressureOfNode(NodeId node) const
{
	const int32_t j = JointOf(node);
	return j >= 0 ? loads[size_t(j)].pressure : 0.0f;
}

float Schema::MomentOfNode(NodeId node, int direction) const
{
	const int32_t j = JointOf(node);
	return (j >= 0 && direction >= kXP && direction <= kYM) ? loads[size_t(j)].moment[direction] : 0.0f;
}

float Schema::WeightOfNode(NodeId node) const
{
	const int32_t j = JointOf(node);
	return j >= 0 ? walkers[size_t(j)].weight : 0.0f;
}

// ---- pass 1 ------------------------------------------------------------------------------------

// Stability spreads in a layer until nothing changes. It only grows, and the order in which the
// joints shoot does not change where it ends (spec "Мир 2", section 8: the least fixed point), so
// instead of sweeping the whole layer again and again every stable joint shoots once, when it
// becomes stable, and only the arrows it does not have yet (a shot stops at the first joint that
// has the arrow, and that joint passes the arrow on along its chain by then). On a fresh schema the
// arrows the first stable joints would spread are pulled instead (PullArrows). Once every joint of
// the layer is stable nothing can change any more; the arrows are then set as the sweeps would leave
// them: all eight on every joint.
void Schema::ComputeStability(bool pull)
{
	static const int kShots[8] = { kXP, kXM, kYP, kYM, kXPYP, kXMYP, kXMYM, kXPYM };
	const uint8_t* const flg = flags.data();
	Stability* const stab = stability.data();
	const uint8_t* const alv = alive.data();
	const int32_t* const lnk = links.data();
	uint16_t* const arr = arrows.data();
	for (size_t z = 0; z + 1 < layerStart.size(); ++z) {
		const int32_t begin = layerStart[z], end = layerStart[z + 1];
		// Vertical: stable if standing on a stable joint; unstable without a rod down (not ground).
		work.clear();
		int32_t unstable = 0;   // joints of the layer that are not stable (yet)
		for (int32_t j = begin; j < end; ++j) {
			if (flg[j] & kDestroyed)
				continue;
			Stability st = stab[j];
			if (st != Stability::Stable) {
				if (!(alv[j] & Bit(kDN))) {
					if (!(flg[j] & kGround))
						st = Stability::Unstable;
				} else if (stab[lnk[6 * size_t(j) + kDN]] == Stability::Stable) {
					st = Stability::Stable;
				}
				stab[j] = st;
			}
			if (st != Stability::Stable)
				++unstable;
			else if (pull)
				arr[j] = kAllArrows;   // as if it had shot already; PullArrows spreads them
			else
				work.push_back(j);
		}
		if (pull && unstable > 0)
			PullArrows(begin, end, unstable);
		// Horizontal: the stable joints shoot arrows; the joints the arrows make stable shoot too.
		for (size_t i = 0; i < work.size() && unstable > 0; ++i) {
			const int32_t j = work[i];
			for (int k = 0; k < 8 && unstable > 0; ++k)
				if (!(arr[j] & (1u << kShots[k])))
					Shoot(j, kShots[k], unstable);
		}
		if (unstable == 0)
			for (int32_t j = begin; j < end; ++j)
				if (!(flg[j] & kDestroyed))
					arr[j] = kAllArrows;
	}
}

namespace {

// The arrow d (diagonal) of the joint before j on the chain of d: the joint whose shot d goes on
// into j (see Schema::Shoot), or 0 when there is none.
inline uint16_t DiagonalArrowBefore(const int32_t* lnk, const uint8_t* alv, const uint8_t* flg, const uint16_t* arr, int32_t j,
	int d)
{
	int ax, ay;
	DiagonalAxes(d, ax, ay);
	// A shot goes first along ax and turns to ay at the corner, unless the shooter has no rod along
	// ax at all; then it goes along ay first. The corner of the first way is the joint before j
	// along ay: if it exists, the shooter has a rod to it (destroyed or not).
	const int32_t* const l = lnk + 6 * size_t(j);
	int32_t from;
	const int32_t corner = l[Opposite(ay)];
	if (corner >= 0) {
		if (!(alv[j] & Bit(Opposite(ay))))
			return 0;
		from = lnk[6 * size_t(corner) + size_t(Opposite(ax))];
	} else {
		if (!(alv[j] & Bit(Opposite(ax))))
			return 0;
		from = lnk[6 * size_t(l[Opposite(ax)]) + size_t(Opposite(ay))];
	}
	if (from < 0 || (flg[from] & kDestroyed))
		return 0;
	return uint16_t(arr[from] & (1u << d));
}

} // namespace

// Arrows of the joints of a layer that are not stable, on a fresh schema (no arrows yet) where every
// stable joint counts as having shot all its arrows: the arrow d of a joint is the arrow d of the
// joint before it on the chain of d. The chains of XP, YP, XPYP and XMYP lead to higher indices,
// those of XM, YM, XMYM and XPYM to lower ones, so one pass up and one pass down give all arrows.
// A joint with two opposite arrows becomes stable, as in Shoot, and joins the work list. By then it
// has as good as shot the arrows of the pass down (the joints after it on those chains are still to
// come); the arrows of the pass up it lacks, it shoots later.
void Schema::PullArrows(int32_t begin, int32_t end, int32_t& unstable)
{
	constexpr uint16_t kDown = uint16_t((1u << kXM) | (1u << kYM) | (1u << kXMYM) | (1u << kXPYM));
	const uint8_t* const flg = flags.data();
	Stability* const stab = stability.data();
	const uint8_t* const alv = alive.data();
	const int32_t* const lnk = links.data();
	uint16_t* const arr = arrows.data();
	for (int32_t j = begin; j < end; ++j) {
		if ((flg[j] & kDestroyed) || stab[j] == Stability::Stable)
			continue;
		const uint8_t al = alv[j];
		const int32_t* const l = lnk + 6 * size_t(j);
		uint16_t a = 0;
		if (al & Bit(kXM))
			a |= uint16_t(arr[l[kXM]] & (1u << kXP));
		if (al & Bit(kYM))
			a |= uint16_t(arr[l[kYM]] & (1u << kYP));
		a |= DiagonalArrowBefore(lnk, alv, flg, arr, j, kXPYP);
		a |= DiagonalArrowBefore(lnk, alv, flg, arr, j, kXMYP);
		arr[j] = a;
	}
	size_t added = 0;
	for (int32_t j = end - 1; j >= begin; --j) {
		if ((flg[j] & kDestroyed) || stab[j] == Stability::Stable)
			continue;
		const uint8_t al = alv[j];
		const int32_t* const l = lnk + 6 * size_t(j);
		uint16_t a = arr[j];
		if (al & Bit(kXP))
			a |= uint16_t(arr[l[kXP]] & (1u << kXM));
		if (al & Bit(kYP))
			a |= uint16_t(arr[l[kYP]] & (1u << kYM));
		a |= DiagonalArrowBefore(lnk, alv, flg, arr, j, kXMYM);
		a |= DiagonalArrowBefore(lnk, alv, flg, arr, j, kXPYM);
		if (HasOppositeArrows(a)) {
			stab[j] = Stability::Stable;
			a |= kDown;
			work.push_back(j);
			++added;
		}
		arr[j] = a;
	}
	unstable -= int32_t(added);
}

// The arrow d in a joint means: somewhere on the side opposite to d, along a continuous chain of
// rods in this layer, there is a stable joint. A joint the arrow makes stable joins the work list.
void Schema::Shoot(int32_t joint, int d, int32_t& unstable)
{
	const uint16_t bit = uint16_t(1u << d);
	int ax = 0, ay = 0;
	if (d >= 4)
		DiagonalAxes(d, ax, ay);
	for (int32_t cur = joint; cur >= 0;) {
		const size_t c = size_t(cur);
		const uint16_t a = arrows[c];
		if (a & bit)
			break;
		const uint16_t now = uint16_t(a | bit);
		arrows[c] = now;
		int32_t next = -1;
		if (d < 4) {
			if (alive[c] & Bit(d))
				next = Link(cur, d);
		} else {
			// Diagonal: first along x, then along y; along y first only if there is no x rod at all
			// (a destroyed rod still counts).
			const int32_t nx = Link(cur, ax);
			if (nx >= 0) {
				if (alive[size_t(nx)] & Bit(ay))
					next = Link(nx, ay);
			} else {
				const int32_t ny = Link(cur, ay);
				if (ny >= 0 && (alive[size_t(ny)] & Bit(ax)))
					next = Link(ny, ax);
			}
		}
		if (stability[c] != Stability::Stable && HasOppositeArrows(now)) {
			stability[c] = Stability::Stable;
			work.push_back(cur);
			--unstable;
		}
		cur = next;
	}
}

// ---- pass 2 ------------------------------------------------------------------------------------

// The region of a joint: every joint reached along alive rods without passing a stable or hanging
// one. Which joints it holds and whether it touches a support do not depend on the order of the
// search, so a joint is judged when it is first seen and enters the region list at most once (the
// original pushes every neighbour and judges it when it is popped).
// Also marks for pass 3 the stable joints met at the border of the hanging regions (the sources of
// the waves) and lists the joints of those regions.
void Schema::FindFree()
{
	uint8_t* const flg = flags.data();
	Stability* const stab = stability.data();
	const uint8_t* const alv = alive.data();
	const int32_t* const lnk = links.data();
	uint64_t* const src = sources.data();
	int32_t* const reg = region.data();     // room for every joint (Build): a joint is in one region at most
	int32_t* const hang = hanging.data();   // the same
	size_t hangCount = 0;
	const int32_t count = int32_t(info.size());
	for (int32_t j = 0; j < count; ++j) {
		const Stability start = stab[j];
		if ((start != Stability::Unstable && start != Stability::Unknown) || (flg[j] & kChecked))
			continue;
		flg[j] |= kChecked;
		reg[0] = j;
		size_t regionCount = 1;
		bool touchesSupport = false;
		for (size_t r = 0; r < regionCount; ++r) {
			const int32_t k = reg[r];
			const int32_t* const l = lnk + 6 * size_t(k);
			for (uint32_t rods = alv[k]; rods; rods &= rods - 1) {
				const int32_t m = l[std::countr_zero(rods)];
				const Stability sm = stab[m];
				if (sm == Stability::Stable || sm == Stability::UnstableNotFree) {
					touchesSupport = true;
					if (sm == Stability::Stable)
						src[size_t(m) >> 6] |= uint64_t(1) << (m & 63);
					continue;
				}
				if (flg[m] & kChecked)
					continue;
				flg[m] |= kChecked;
				reg[regionCount++] = m;
			}
		}
		const Stability to = touchesSupport ? Stability::UnstableNotFree : Stability::Free;
		for (size_t r = 0; r < regionCount; ++r) {
			const int32_t k = reg[r];
			if (stab[k] == Stability::Unstable || stab[k] == Stability::Unknown) {
				stab[k] = to;
				if (touchesSupport)
					hang[hangCount++] = k;
			}
		}
	}
	hangingCount = hangCount;
}

// ---- pass 3 ------------------------------------------------------------------------------------

// The wave goes from a source into a neighbour (the original's Advance, step -1).
void Schema::Advance(int32_t wave, int32_t from, int direction, int32_t step)
{
	Walker& F = walkers[size_t(from)];
	if (F.locks[direction] >= kRodLocks)
		return;
	const int32_t to = F.link[direction];
	Walker& T = walkers[size_t(to)];
	++F.locks[direction];
	++T.locks[Opposite(direction)];
	if (!F.listed) {
		F.listed = 1;
		walked[walkedCount++] = from;
	}
	if (!T.listed) {
		T.listed = 1;
		walked[walkedCount++] = to;
	}
	flags[size_t(to)] |= kFront;
	T.wave = wave;
	T.step = step;
	T.dir = uint8_t(direction);
	if (nextCount == nextFront.size())
		nextFront.resize(std::max<size_t>(64, 2 * nextCount));
	nextFront[nextCount++] = to;
	advanced = true;
}

// The steps of the waves (the original's ProcessFront and Advance): every front joint records its
// wave, then the wave goes on along one of the ways left, picked at random. The hot loop of the
// statics, so everything it touches is held in locals and the lists are written by hand (front and
// nextFront only grow; their sizes are the room, the counts are the lengths). Returns the number of
// records.
size_t Schema::Walk()
{
	uint8_t* const flg = flags.data();
	const Stability* const stab = stability.data();
	const uint8_t* const alv = alive.data();
	Walker* const wk = walkers.data();
	const Wave* const wv = waves.data();
	SRand random(1);   // the same seed at every iteration: the result is deterministic
	int32_t* const list = walked.data();
	size_t listCount = walkedCount;
	size_t recordCount = 0;
	bool moved = advanced;
	for (int32_t step = 0; moved; ++step) {
		moved = false;
		front.swap(nextFront);
		const size_t count = nextCount;
		// Every entry records at most once and adds at most two entries: itself and where its wave goes.
		if (nextFront.size() < 2 * count)
			nextFront.resize(2 * count);
		if (records.size() < recordCount + count)
			records.resize(std::max(recordCount + count, 2 * records.size()));
		const int32_t* const in = front.data();
		int32_t* const out = nextFront.data();
		std::pair<int32_t, int32_t>* const rec = records.data();
		size_t outCount = 0;
		for (size_t i = 0; i < count; ++i) {
			const int32_t j = in[i];
			const uint8_t fj = flg[j];
			if (!(fj & kFront))
				continue;
			Walker& W = wk[j];
			if (W.step == step || stab[j] == Stability::Stable)
				continue;
			const int32_t wave = W.wave;
			rec[recordCount++] = { j, wave };
			W.support += wv[wave].weight;
			// The ways on, as a mask in the order of the directions (the order in which the original
			// lists them): alive rods except the way back, with capacity left, to a joint that is not
			// a front. Without a rod the copied link is the joint itself, so all six can be read.
			uint32_t open = 0;
			for (int d = 0; d < 6; ++d)
				open |= (uint32_t((flg[W.link[d]] & kFront) == 0) & uint32_t(W.locks[d] < kRodLocks)) << d;
			const uint32_t ways = open & alv[j] & ~(1u << Opposite(W.dir));
			const int n = std::popcount(ways);
			// With two or more ways on, the joint stays a front: next step it records the same wave again.
			if (n < 2)
				flg[j] = uint8_t(fj & ~kFront);
			else
				out[outCount++] = j;
			if (n == 0)
				continue;
			// The wave goes on along a way picked at random (it has capacity left: it is open).
			const int d = kNthBit[ways][random.Get(n)];
			const int32_t to = W.link[d];
			Walker& T = wk[to];
			++W.locks[d];
			++T.locks[Opposite(d)];
			if (!T.listed) {
				T.listed = 1;
				list[listCount++] = to;
			}
			flg[to] |= kFront;
			T.wave = wave;
			T.step = step;
			T.dir = uint8_t(d);
			out[outCount++] = to;
			moved = true;
		}
		nextCount = outCount;
	}
	walkedCount = listCount;
	return recordCount;
}

// The share of the mass M that the wave sent in direction firstStep from the source S brings back
// to it.
void Schema::AddMoment(Load& L, const Walker& S, float share, const Walker& M, int firstStep)
{
	const float dx = float(M.x - S.x), dy = float(M.y - S.y);   // lever in lattice steps
	// XP: moment[XP] += dx * share; XM: moment[XM] -= dx * share; YP and YM the same with dy; up and
	// down: pressure only (their share goes to the fifth slot, never read). Without branches: the
	// direction changes from record to record. Subtracting a product is adding the product of the
	// negated lever, bit for bit.
	const float lever = firstStep < kYP ? dx : dy;
	const float signedLever = (firstStep & 1) ? -lever : lever;
	L.moment[firstStep < kUP ? firstStep : 4] += signedLever * share;
	L.pressure += share;
}

// A stable joint sends a wave into each unstable neighbour. The wave weight is 1 / the number of
// directions without a stable neighbour (directions without a rod count too).
void Schema::Emit(int32_t s)
{
	const uint8_t a = alive[size_t(s)];
	int edges[6];
	int count = 0;
	for (int d = 0; d < 6; ++d) {
		if ((a & Bit(d)) && stability[size_t(Link(s, d))] == Stability::Stable)
			continue;
		edges[count++] = d;
	}
	if (count == 0)
		return;
	const float w = 1.0f / float(count);
	for (int k = 0; k < count; ++k) {
		const int d = edges[k];
		if (!(a & Bit(d)))
			continue;
		const int32_t id = int32_t(waves.size());
		waves.push_back({ s, w, uint8_t(Opposite(d)) });
		Advance(id, s, d, -1);
	}
}

// tracked: pass 2 ran on a fresh schema, so its marks name exactly the stable joints that have an
// unstable neighbour (the only ones that send waves) and its list holds every hanging joint.
// Otherwise every joint is looked at, as in the original.
void Schema::ComputeMoments(bool tracked)
{
	waves.clear();
	nextCount = 0;
	advanced = false;
	if (tracked) {
		for (size_t w = 0; w < sources.size(); ++w) {
			uint64_t bits = sources[w];
			if (!bits)
				continue;
			sources[w] = 0;
			do {
				Emit(int32_t(w * 64 + size_t(std::countr_zero(bits))));   // in index order
				bits &= bits - 1;
			} while (bits);
		}
	} else {
		std::fill(sources.begin(), sources.end(), uint64_t(0));
		for (int32_t s = 0; s < int32_t(info.size()); ++s)
			if (stability[size_t(s)] == Stability::Stable)
				Emit(s);
	}
	const size_t recordCount = Walk();
	// Every hanging joint hands its weight to the sources of the waves that reached it. A joint no
	// wave reached has support 0 (every wave weighs more than 0).
	if (tracked) {
		for (size_t i = 0; i < hangingCount; ++i)
			if (const int32_t k = hanging[i]; walkers[size_t(k)].support == 0.0f)
				stability[size_t(k)] = Stability::UnknownMoment;   // "Cant compute moment": its weight is lost
	} else {
		for (size_t k = 0; k < info.size(); ++k) {
			const Stability st = stability[k];
			if (!(flags[k] & kDestroyed) && st != Stability::Stable && walkers[k].support == 0.0f && st != Stability::Free)
				stability[k] = Stability::UnknownMoment;
		}
	}
	uint8_t* const flg = flags.data();
	const Stability* const stab = stability.data();
	const Walker* const wk = walkers.data();
	const Wave* const wv = waves.data();
	Load* const ld = loads.data();
	const std::pair<int32_t, int32_t>* const rec = records.data();
	for (size_t i = 0; i < recordCount; ++i) {
		const int32_t k = rec[i].first;
		if ((flg[k] & kDestroyed) || stab[k] == Stability::Stable)
			continue;
		const Wave& w = wv[rec[i].second];
		const Walker& M = wk[k];
		AddMoment(ld[w.source], wk[w.source], M.weight * w.weight / M.support, M, Opposite(w.back));
		flg[w.source] |= kLoaded;
	}
}

// ---- rules -------------------------------------------------------------------------------------

void Schema::DestroyRod(int32_t j, int d, bool checkVerticalChain)
{
	const int32_t k = Link(j, d);
	alive[size_t(j)] &= uint8_t(~Bit(d));
	alive[size_t(k)] &= uint8_t(~Bit(Opposite(d)));
	if (!checkVerticalChain)
		return;
	// A joint left with a rod up and a rod down but nothing sideways drops out of the schema (its
	// piece stays in the lattice); its two vertical rods go, without a cascade. The lower end of
	// the rod is looked at first.
	const int32_t lower = (d & 1) ? k : j, upper = (d & 1) ? j : k;
	for (const int32_t end : { lower, upper }) {
		uint8_t& f = flags[size_t(end)];
		if (f & kDestroyed)
			continue;
		if (alive[size_t(end)] == kVerticalOnly) {
			f |= kDestroyed;
			DestroyRod(end, kUP, false);
			DestroyRod(end, kDN, false);
		}
	}
}

void Schema::MarkDestroyed(int32_t j)
{
	flags[size_t(j)] |= kDestroyed;
	for (int d = 0; d < 6; ++d)
		if (alive[size_t(j)] & Bit(d))
			DestroyRod(j, d, true);
}

void Schema::OnNodeDestroyed(NodeId node)
{
	const int32_t j = JointOf(node);
	if (j < 0)
		return;
	flags[size_t(j)] |= kGone;
	MarkDestroyed(j);
}

bool Schema::DestroyJoint(int32_t j, bool effects, IBreaker& breaker)
{
	// A node that is gone cannot be destroyed again (see IBreaker). A free joint whose piece fell
	// stays free for the rest of the schema's life, and every iteration would ask again.
	if (flags[size_t(j)] & (kGround | kGone))
		return false;
	return breaker.BreakNode(info[size_t(j)].node, effects);
}

bool Schema::ApplyRules(IBreaker& breaker)
{
	bool changed = false;
	const int32_t count = int32_t(info.size());
	for (int32_t j = 0; j < count; ++j) {
		const Stability st = stability[size_t(j)];
		if (st == Stability::Free) {
			if (DestroyJoint(j, false, breaker))   // quiet: no dust, no sound
				changed = true;
			continue;
		}
		// A stable joint that pass 3 did not load has no pressure and no moments: nothing breaks.
		if (st != Stability::Stable || !(flags[size_t(j)] & kLoaded))
			continue;
		if (loads[size_t(j)].pressure > info[size_t(j)].limitPressure && DestroyJoint(j, true, breaker)) {
			changed = true;
			continue;
		}
		for (int d = kXP; d <= kYM; ++d) {
			if (!(std::fabs(loads[size_t(j)].moment[d]) > info[size_t(j)].limitMoment))
				continue;
			if (!(alive[size_t(j)] & Bit(d)))
				continue;
			if (DestroyJoint(Link(j, d), true, breaker)) {
				changed = true;
				DestroyRod(j, d, true);
			}
		}
	}
	return changed;
}

void Schema::RunPasses()
{
	// Only on a fresh schema pass 1 can pull the arrows and pass 2 collect the sources and the hanging
	// joints for pass 3; otherwise (passes run again without Reset) they work as the original.
	const bool tracked = fresh;
	fresh = false;
	{
		const PhaseTimer timer(profile, kPhasePass1);
		ComputeStability(tracked);
	}
	{
		const PhaseTimer timer(profile, kPhasePass2);
		FindFree();
	}
	const PhaseTimer timer(profile, kPhasePass3);
	ComputeMoments(tracked);
}

bool Schema::Recalc(IBreaker& breaker)
{
	RunPasses();
	const PhaseTimer timer(profile, kPhaseRules);
	return ApplyRules(breaker);
}

// Only joints that are not destroyed are reset. A joint left without rods is destroyed. The lock
// counters of every rod go back to 0 (only the walked joints can have others), and so does the
// support (a destroyed joint's is never read again).
void Schema::Reset()
{
	const PhaseTimer timer(profile, kPhaseReset);
	uint8_t* const flg = flags.data();
	Stability* const stab = stability.data();
	uint16_t* const arr = arrows.data();
	const uint8_t* const alv = alive.data();
	const size_t count = info.size();
	for (size_t j = 0; j < count; ++j) {
		uint8_t f = flg[j];
		if (f & kDestroyed)
			continue;
		if (f & kLoaded)
			loads[j] = Load{};
		f &= kGround;
		stab[j] = (f & kGround) ? Stability::Stable : Stability::Unknown;
		arr[j] = 0;
		if (alv[j] == 0)
			f |= kDestroyed;
		flg[j] = f;
	}
	for (size_t i = 0; i < walkedCount; ++i) {
		Walker& W = walkers[size_t(walked[i])];
		W.support = 0.0f;
		std::fill(std::begin(W.locks), std::end(W.locks), uint8_t(0));
		W.listed = 0;
	}
	walkedCount = 0;
	waves.clear();
	fresh = true;
}

} // namespace ssproto
