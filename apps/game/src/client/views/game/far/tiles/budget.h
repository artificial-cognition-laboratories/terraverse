// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
#include <chrono>
#include <cstddef>

namespace far {

/*
	A frame's time for the far view's main-thread work: taking built tiles
	onto the GPU, recolouring them for a tint, cutting the stand-ins. Shared
	by every Tiles drawn in one frame (the Planetarium's body and each other
	body with land), the first to update served first: given each its own,
	they added up, a frame's work growing with every body and every world
	shown before the last had come in.

	An item's started only if what one has cost lately fits what's left (the
	check was only that time was left: one large tile's upload ran 5 ms past
	it). The first of a kind each frame always goes, so nothing waits forever.
*/
class Budget
{
public:
	enum class Work { Install, Paint, Count };

	// A new frame: each kind's time back.
	void frame();
	// Whether another of this kind fits this frame.
	bool room(Work work) const;
	// Does one, timed: counted against this frame, and toward what one costs.
	template <typename F>
	void run(Work work, F &&f)
	{
		const auto from = std::chrono::steady_clock::now();
		f();
		spent(work, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count());
	}

private:
	void spent(Work work, double ms);

	struct Kind
	{
		double spent = 0; // ms this frame
		double usual = 0; // ms one costs, lately
	};
	std::array<Kind, (size_t)Work::Count> m_kinds{};
};

} // namespace far
