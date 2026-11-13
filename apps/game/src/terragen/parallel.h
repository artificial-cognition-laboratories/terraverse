// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <functional>

/*
	Loops over a world's cells on the machine's cores. Parts are fixed (never
	by how many threads there are) and whatever a part gathers is joined in
	part order, so a world comes out the same on every machine: only work
	whose cells don't read each other's results of this same loop goes here.
	Inside a part, or while another build has the crew, a loop runs on its
	caller's thread. What a part throws is rethrown by the caller.
*/
namespace terragen {

constexpr int32_t PARTS = 64;

// The calling thread yields to the rest: a worker (building a world, the
// far view, meshes, physics' ground) runs below the frame's thread, so a
// machine that's busy everywhere still draws on time. Once per thread.
void background();

// fn(part, begin, end) for PARTS slices of 0..n, in any order, on any thread.
void parallelParts(int32_t n, const std::function<void(int32_t, int32_t, int32_t)> &fn);

// fn(i) for each of 0..n.
template <typename F>
void parallelFor(int32_t n, F &&fn)
{
	parallelParts(n, [&](int32_t, int32_t begin, int32_t end) {
		for (int32_t i = begin; i < end; i++)
			fn(i);
	});
}

} // namespace terragen
