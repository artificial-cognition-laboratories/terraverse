// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/game/cut/snap.h"

#include <cstdlib>

namespace snap {

namespace {
s32 of(v3s32 v, int axis) { return axis == 0 ? v.X : axis == 1 ? v.Y : v.Z; }
s32 &on(v3s32 &v, int axis) { return axis == 0 ? v.X : axis == 1 ? v.Y : v.Z; }
} // namespace

peel::Box align(const peel::Box &box, const std::vector<record::Member> &members, int normal)
{
	peel::Box out = box;
	for (int i = 0; i < 3; i++) {
		if (i == normal)
			continue;
		const s32 lo = of(box.min, i), hi = of(box.max, i);
		s32 best = NEAR + 1;
		for (const record::Member &m : members) {
			// only what's beside it: within NEAR of it on the other axes
			bool near = true;
			for (int j = 0; j < 3; j++)
				if (j != i && (of(m.box.min, j) > of(box.max, j) + NEAR || of(box.min, j) > of(m.box.max, j) + NEAR))
					near = false;
			if (!near)
				continue;
			const s32 mlo = of(m.box.min, i), mhi = of(m.box.max, i);
			// flush at either end, butted on either side, centred
			const s32 offers[] = { mlo - lo, mhi - hi, mhi + 1 - lo, mlo - 1 - hi, ((mlo + mhi) - (lo + hi)) / 2 };
			for (s32 o : offers)
				if (std::abs(o) < std::abs(best))
					best = o;
		}
		if (std::abs(best) <= NEAR) {
			on(out.min, i) += best;
			on(out.max, i) += best;
		}
	}
	return out;
}

peel::Box clear(const peel::Box &box, const std::vector<record::Member> &members, v3s16 face)
{
	const int i = face.X ? 0 : face.Y ? 1 : 2;
	const bool out = face.X + face.Y + face.Z > 0;
	peel::Box b = box;
	// each pass past what it meets; another may be met further out
	for (bool moved = true; moved;) {
		moved = false;
		for (const record::Member &m : members) {
			if (!record::meets(m.box, b))
				continue;
			const s32 shift = out ? of(m.box.max, i) + 1 - of(b.min, i) : of(m.box.min, i) - 1 - of(b.max, i);
			on(b.min, i) += shift;
			on(b.max, i) += shift;
			moved = true;
		}
	}
	return b;
}

} // namespace snap
