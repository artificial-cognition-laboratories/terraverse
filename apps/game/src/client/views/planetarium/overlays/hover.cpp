// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/overlays/hover.h"
#include <algorithm>
#include <cmath>

namespace planetarium::hover {

std::optional<Hover> under(const Ask &ask)
{
	const size_t count = ask.seen.size() - 1; // the star's index
	const auto off = [&](const Seen &s) {
		const float dx = s.at.X - ask.mouse.X, dy = s.at.Y - ask.mouse.Y;
		return std::sqrt(dx * dx + dy * dy);
	};
	// what's nearest under the mouse hides whatever's behind it (the body it's
	// round too, though that isn't itself offered when it fills the screen)
	double blocked = INFINITY;
	for (const std::optional<Seen> &s : ask.seen)
		if (s && off(*s) <= s->shown)
			blocked = std::min(blocked, s->distance);
	std::optional<Hover> best;
	double nearest = 0;
	bool bestInside = false;
	for (size_t i = 0; i <= count; i++) {
		const std::optional<Seen> &s = ask.seen[i];
		if (!s || s->distance > blocked)
			continue;
		const std::optional<size_t> body = i == count ? std::nullopt : std::optional<size_t>(i);
		const float away = off(*s);
		// what's hovered now keeps it a little past where it would start:
		// on an edge it doesn't flick on and off frame to frame
		const bool held = ask.was && ask.was->body == body;
		if (i == ask.target && s->shown > ask.height * (held ? 0.5f : 0.4f))
			continue; // the body it's round, filling the screen
		if (away > std::max(s->shown, 10.f) + (held ? 14 : 4))
			continue;
		// a disc the mouse is inside beats a dot's reach round it (a planet's
		// dot drawn over the star isn't the star's click), the nearest in
		// front among discs; among reaches, the one whose middle is closest
		const bool inside = away <= s->shown;
		const double rank = inside ? s->distance : away;
		if (!best || inside > bestInside || (inside == bestInside && rank < nearest)) {
			best = Hover{ body, s->at, s->shown };
			nearest = rank;
			bestInside = inside;
		}
	}
	return best;
}

} // namespace planetarium::hover
