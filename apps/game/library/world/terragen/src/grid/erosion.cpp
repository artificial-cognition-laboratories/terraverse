// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/erosion.h"
#include "library/world/terragen/src/grid/grid.h"
#include <algorithm>

namespace terragen {

void streamPower(std::vector<fix> &h, const Routing &route, const std::vector<int64_t> &area,
		const std::vector<fix> &k, std::vector<fix> *power, Lakes lakes)
{
	const Sphere &sphere = gridSphere();
	const fix spacing = sphere.spacing();
	if (power)
		power->assign(h.size(), 0);
	// (under a lake before the round: what's under one doesn't wear, so it
	// still is after)
	std::vector<uint8_t> wet(h.size());
	for (size_t i = 0; i < h.size(); i++)
		wet[i] = route.surface[i] > h[i];
	for (int32_t c : route.order) {
		const int32_t r = route.receiver[c];
		if (r < 0 || (lakes == Lakes::Base ? wet[c] : h[r] > h[c]))
			continue;
		fix gap = spacing;
		const fix *d = sphere.distancesBegin(c);
		for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++, d++)
			if (*p == r)
				gap = *d;
		const fix F = div(mul(k[c], sqrtWide64(area[c])), div(gap, spacing));
		// what it drains to as it is now (worn this round already: the scheme
		// is implicit), or the lake over it; never below the sea
		const fix floor = std::max<fix>(lakes == Lakes::Base && wet[r] ? route.surface[r] : h[r], 0);
		// (exactly, never under the floor; fixed point's rounding down could
		// leave it a unit under, a pit by a hair)
		h[c] = std::max(div(h[c] + mul(F, floor), ONE + F), floor);
		if (power)
			(*power)[c] = mul(F, h[c] - floor);
	}
}

void creep(std::vector<fix> &h, fix kappa, std::vector<int64_t> &crumbs)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = (int32_t)h.size();
	std::vector<int64_t> moved(crumbs);
	for (int32_t i = 0; i < n; i++) {
		if (h[i] < 0)
			continue;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
			const int32_t k = *p;
			if (k < i || h[k] < 0)
				continue;
			const int64_t q = (int64_t)mul(kappa, h[k] - h[i]) * std::min(sphere.area(i), sphere.area(k));
			moved[i] += q;
			moved[k] -= q;
		}
	}
	for (int32_t i = 0; i < n; i++) {
		const int64_t a = sphere.area(i);
		const int64_t whole = moved[i] >= 0 ? moved[i] / a : -((-moved[i] + a - 1) / a);
		h[i] += (fix)whole;
		crumbs[i] = moved[i] - whole * a;
	}
}

int64_t rebound(std::vector<fix> &h, const std::vector<fix> &before, fix share, int passes)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = (int32_t)h.size();
	std::vector<fix> load(n);
	for (int32_t i = 0; i < n; i++)
		load[i] = before[i] >= 0 ? before[i] - h[i] : 0;
	for (int pass = 0; pass < passes; pass++) {
		const std::vector<fix> was = load;
		for (int32_t i = 0; i < n; i++) {
			int64_t sum = was[i];
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
				sum += was[*p];
			load[i] = (fix)(sum / (sphere.neighbourCount(i) + 1));
		}
	}
	int64_t raised = 0;
	for (int32_t i = 0; i < n; i++) {
		if (before[i] < 0 || h[i] < 0)
			continue;
		const fix was = h[i];
		h[i] = std::max<fix>(h[i] + mul(share, load[i]), 0);
		raised += (int64_t)(h[i] - was) * sphere.area(i);
	}
	return raised;
}

} // namespace terragen
