// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/rivers/river_line.h"
#include <algorithm>
#include <cstddef>

namespace terragen {

namespace {

constexpr int PER = 16;      // points a loop
constexpr int CHUNK = 8;     // points a box
constexpr int NEAR = 8;      // fractional bits of a point
constexpr int64_t SHORTEST = 32 << FRAC; // the shortest loop (blocks)
constexpr fix MOST = ratio(195, 100); // the sharpest turn (radians, 112°): past it loops touch

uint32_t hash(uint32_t a, uint32_t b)
{
	uint32_t h = a * 0x9e3779b1u ^ b * 0x85ebca77u;
	h ^= h >> 15;
	h *= 0x2c1b3c6du;
	h ^= h >> 12;
	return h;
}

// the turn at a loop's phase φ, for its angle and the skew
fix turn(fix angle, fix skew, fix phi)
{
	return mul(angle, sin(phi) + mul(skew, cos(3 * (int64_t)phi % TWO_PI) - cos(phi)));
}

// how far along a loop runs, of its length, at an angle: the mean of cos θ
fix advance(fix angle, fix skew)
{
	int64_t sum = 0;
	for (int k = 0; k < PER; k++)
		sum += cos(turn(angle, skew, (fix)((int64_t)TWO_PI * (2 * k + 1) / (2 * PER))));
	return (fix)std::max<int64_t>(sum / PER, ratio(5, 100));
}

} // namespace

RiverLine::RiverLine(Opts o)
{
	const fix angle = std::clamp<fix>(o.angle, 0, MOST);
	const int64_t arc = std::max(o.loop, SHORTEST);
	m_loops = (int32_t)std::max<int64_t>(1, (o.run + mulShift(arc, advance(angle, o.skew), FRAC) / 2) / std::max<int64_t>(1, mulShift(arc, advance(angle, o.skew), FRAC)));
	// the curve in loops' lengths (a loop's length ONE), its turns
	const int n = m_loops * PER;
	std::vector<int64_t> x(n + 1, 0), y(n + 1, 0);
	std::vector<fix> curve(n + 1, 0);
	fix sharpest = 1;
	for (int32_t l = 0; l < m_loops; l++) {
		// (each loop's own: its angle 0.6 to 1.4 of the river's, its length
		// 0.7 to 1.3 of a loop, so no two alike)
		const uint32_t h = hash(o.seed, (uint32_t)l);
		const fix mine = std::min<fix>(MOST, mul(angle, ratio(60, 100) + (fix)(h % (uint32_t)(ratio(80, 100) + 1))));
		const fix size = ratio(70, 100) + (fix)((h >> 16) % (uint32_t)(ratio(60, 100) + 1));
		for (int k = 0; k < PER; k++) {
			const int i = l * PER + k;
			const fix mid = (fix)((int64_t)TWO_PI * (2 * k + 1) / (2 * PER));
			const fix th = turn(mine, o.skew, mid);
			x[i + 1] = x[i] + mul(cos(th), size) / PER;
			y[i + 1] = y[i] + mul(sin(th), size) / PER;
			// (its turn: dθ/dφ, θ₀ cos φ and the skew's)
			const fix phi = (fix)((int64_t)TWO_PI * k / PER);
			curve[i] = mul(mine, cos(phi) + mul(o.skew, mul(toFix(-3), sin(3 * (int64_t)phi % TWO_PI)) + sin(phi)));
			curve[i] = div(curve[i], size);
			sharpest = std::max<fix>(sharpest, std::abs(curve[i]));
		}
	}
	curve[n] = curve[0];
	// scaled to the run, sheared so it ends on it, bowed to its ends' slopes
	const int64_t xe = std::max<int64_t>(1, x[n]), ye = y[n];
	m_x.resize(n + 1);
	m_y.resize(n + 1);
	m_curve.resize(n + 1);
	for (int i = 0; i <= n; i++) {
		const int64_t bx = (int64_t)((__int128)x[i] * o.run / xe);
		const fix u = (fix)std::clamp<int64_t>((x[i] << FRAC) / xe, 0, ONE);
		const fix u2 = mul(u, u), u3 = mul(u2, u);
		const int64_t by = (int64_t)((__int128)(y[i] - mulShift(ye, u, FRAC)) * o.run / xe) +
				mulShift(o.run, mul(o.m0, u - 2 * u2 + u3) + mul(o.m1, u3 - u2), FRAC);
		m_x[i] = (int32_t)(bx >> (FRAC - NEAR));
		m_y[i] = (int32_t)(by >> (FRAC - NEAR));
		m_curve[i] = (int16_t)std::clamp<int64_t>(((int64_t)curve[i] << 15) / sharpest, -32767, 32767);
	}
	// how far along each point is
	m_t.assign(n + 1, 0);
	std::vector<int64_t> s(n + 1, 0);
	for (int i = 1; i <= n; i++) {
		const int64_t dx = m_x[i] - m_x[i - 1], dy = m_y[i] - m_y[i - 1];
		s[i] = s[i - 1] + (int64_t)isqrt((uint64_t)(dx * dx + dy * dy));
	}
	for (int i = 0; i <= n; i++)
		m_t[i] = (fix)(s[n] > 0 ? (s[i] << FRAC) / s[n] : 0);
	for (int i = 0; i < n; i += CHUNK) {
		Box b{ m_x[i], m_y[i], m_x[i], m_y[i] };
		for (int k = i; k <= std::min(n, i + CHUNK); k++) {
			b.x0 = std::min(b.x0, m_x[k]);
			b.y0 = std::min(b.y0, m_y[k]);
			b.x1 = std::max(b.x1, m_x[k]);
			b.y1 = std::max(b.y1, m_y[k]);
		}
		m_boxes.push_back(b);
	}
}

RiverLine::Near RiverLine::nearest(int64_t x, int64_t y) const
{
	Near out;
	if (m_x.empty())
		return out;
	const int64_t px = x >> (FRAC - NEAR), py = y >> (FRAC - NEAR);
	int64_t best = INT64_MAX;
	int at = 0;
	fix frac = 0;
	const auto chunk = [&](size_t c) {
		const Box &b = m_boxes[c];
		const int64_t dx = std::max<int64_t>({ 0, b.x0 - px, px - b.x1 }), dy = std::max<int64_t>({ 0, b.y0 - py, py - b.y1 });
		if (dx * dx + dy * dy >= best)
			return;
		const int end = std::min((int)m_x.size() - 1, (int)c * CHUNK + CHUNK);
		for (int i = (int)c * CHUNK; i < end; i++) {
			const int64_t ax = m_x[i], ay = m_y[i], ex = m_x[i + 1] - ax, ey = m_y[i + 1] - ay;
			const int64_t l2 = ex * ex + ey * ey;
			const fix f = l2 > 0 ? (fix)std::clamp<int64_t>((((px - ax) * ex + (py - ay) * ey) << FRAC) / l2, 0, ONE) : 0;
			const int64_t qx = ax + (ex * f >> FRAC) - px, qy = ay + (ey * f >> FRAC) - py;
			const int64_t d2 = qx * qx + qy * qy;
			if (d2 < best) {
				best = d2;
				at = i;
				frac = f;
			}
		}
	};
	// (from the chunk at the column's way along, so most others are passed by their boxes)
	const size_t guess = std::min(m_boxes.size() - 1, (size_t)std::clamp<int64_t>(m_x.back() > 0 ? px * (int64_t)m_boxes.size() / m_x.back() : 0, 0, (int64_t)m_boxes.size() - 1));
	chunk(guess);
	for (size_t c = 0; c < m_boxes.size(); c++)
		if (c != guess)
			chunk(c);
	const int64_t ax = m_x[at], ay = m_y[at], ex = m_x[at + 1] - ax, ey = m_y[at + 1] - ay;
	const int64_t qx = ax + (ex * frac >> FRAC), qy = ay + (ey * frac >> FRAC);
	out.dist = (int64_t)isqrt((uint64_t)best) << (FRAC - NEAR);
	const int64_t side = ex * (py - qy) - ey * (px - qx);
	out.across = side >= 0 ? out.dist : -out.dist;
	out.along = qx << (FRAC - NEAR);
	out.t = m_t[at] + mulShift(m_t[at + 1] - m_t[at], frac, FRAC);
	out.curve = (fix)(((int64_t)m_curve[at] + (((int64_t)m_curve[at + 1] - m_curve[at]) * frac >> FRAC)) << (FRAC - 15));
	out.loop = at / PER;
	return out;
}

RiverLine::At RiverLine::at(fix t) const
{
	if (m_x.empty())
		return { 0, 0 };
	const size_t i = (size_t)std::max<std::ptrdiff_t>(0, std::upper_bound(m_t.begin(), m_t.end(), t) - m_t.begin() - 1);
	if (i + 1 >= m_t.size())
		return { (int64_t)m_x.back() << (FRAC - NEAR), (int64_t)m_y.back() << (FRAC - NEAR) };
	const int64_t span = std::max<int64_t>(1, m_t[i + 1] - m_t[i]);
	const int64_t f = ((int64_t)(t - m_t[i]) << FRAC) / span;
	return { ((int64_t)m_x[i] + ((m_x[i + 1] - m_x[i]) * f >> FRAC)) << (FRAC - NEAR), ((int64_t)m_y[i] + ((m_y[i + 1] - m_y[i]) * f >> FRAC)) << (FRAC - NEAR) };
}

} // namespace terragen
