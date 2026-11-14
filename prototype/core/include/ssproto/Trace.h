// FNV-1a (64 bit) over results: equal traces mean equal results. The bench and the engine layer
// hash every world step with Step, so a change or a build that alters a collapse shows up.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "ssproto/Building.h"

namespace ssproto {

struct Trace {
	uint64_t h = 14695981039346656037ull;

	void Byte(uint8_t b) { h = (h ^ b) * 1099511628211ull; }
	void U32(uint32_t v)
	{
		for (int i = 0; i < 4; ++i)
			Byte(uint8_t(v >> (8 * i)));
	}
	void F32(float f)
	{
		uint32_t v;
		std::memcpy(&v, &f, sizeof v);
		U32(v);
	}
	void U64(uint64_t v)
	{
		U32(uint32_t(v));
		U32(uint32_t(v >> 32));
	}
	void Ids(const std::vector<uint32_t>& ids)
	{
		U32(uint32_t(ids.size()));
		for (const uint32_t id : ids)
			U32(id);
	}
	// Everything a world step reports, in order.
	void Step(const StepResult& r)
	{
		Ids(r.destroyed);
		Ids(r.broken);
		Ids(r.detached);
		Byte(r.rebuild ? 1 : 0);
		Ids(r.dirtyParts);
		Byte(r.busy ? 1 : 0);
		U32(r.piecesLeft);
	}
};

} // namespace ssproto
