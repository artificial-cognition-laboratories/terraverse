// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "atlas.h"
#include "grid.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace terragen {

using Vec = Atlas::Vec;

namespace {

constexpr double TAU = 6.283185307179586476925;
constexpr double DEGREE = 0.017453292519943295769;
constexpr double NONE = std::numeric_limits<double>::infinity();
// A cell further than this inside its territory (blocks, from its centre)
// is wholly in it: a cell's half diagonal is under 0.9.
constexpr double CLEAR = 0.9;

Vec operator+(Vec a, Vec b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec operator-(Vec a, Vec b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec operator*(double k, Vec a) { return { k * a.x, k * a.y, k * a.z }; }
double dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(Vec a) { return std::sqrt(dot(a, a)); }

// atan2 of |a×b| and a·b: exact near 0° and 90°, where acos of a cosine
// loses half the digits.
double angleBetween(Vec a, Vec b)
{
	const Vec c{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	return std::atan2(length(c), dot(a, b)) / DEGREE;
}

// The edge, of four parallel ones, furthest from a metre: signed.
double worstStretch(const std::array<Vec, 4> &edges)
{
	double worst = 0;
	for (const Vec &e : edges) {
		const double s = length(e) - 1;
		if (std::abs(s) > std::abs(worst))
			worst = s;
	}
	return worst;
}

/*
	A rounded row (a segment's east, a cap's out from the pole): x blocks
	along it from its start, at height h (radius R + h). Round, it's the
	great circle's arc, x/R radians at that radius; flat, the straight line
	(x blocks, the same at every height). Rounded `rho` of the way, its
	direction is
	  A = (R + h)(1 − rho + rho·cos(x/R))    along the start's up,
	  B = (1 − rho)·x + rho·(R + h)·sin(x/R) along the row,
	and the point is that way at exactly R + h: layers are spheres, so
	grids meet level at every seam (unscaled, a layer rises off the sphere
	by about (1 − rho)·x²/2R toward a grid's edges: 1.2 km at an
	Earth-sized cap's circle).
*/
struct Row
{
	double R, rho;

	// The rounded row's direction, and how fast it turns along x
	std::array<double, 2> rounded(double x, double h) const
	{
		const double s = R + h, a = x / R;
		return { s * (1 - rho + rho * std::cos(a)), (1 - rho) * x + rho * s * std::sin(a) };
	}

	// Layers are spheres: the point is the rounded row's direction at
	// exactly R + h from the centre (every grid's layer h meets every
	// other's at the same height, at any seam)
	std::array<double, 2> at(double x, double h) const
	{
		const auto d = rounded(x, h);
		const double k = (R + h) / std::hypot(d[0], d[1]);
		return { d[0] * k, d[1] * k };
	}

	// (x, h) from (A, B): h off the radius; x from the direction, by Newton's method
	std::array<double, 2> back(double A, double B) const
	{
		const double h = std::hypot(A, B) - R, s = R + h, target = std::atan2(B, A);
		double x = target * R;
		for (int n = 0; n < 20; n++) {
			const auto d = rounded(x, h);
			const double f = std::atan2(d[1], d[0]) - target;
			if (std::abs(f) < 1e-15)
				break;
			const double a = x / R;
			const double dA = -s * rho * std::sin(a) / R, dB = 1 - rho + rho * s * std::cos(a) / R;
			x -= f * (d[0] * d[0] + d[1] * d[1]) / (d[0] * dB - d[1] * dA);
		}
		return { x, h };
	}
};

} // namespace

Atlas::Atlas(const Params &params) : m_params(params)
{
	const Params &p = m_params;
	if (!(p.radius > 0) || p.segments < 3 || !(p.capLatitude > 0 && p.capLatitude < 90) || !(p.core >= 0) ||
			p.core >= p.radius || !(p.rounding >= 0 && p.rounding <= 1) || p.height < 1)
		throw std::invalid_argument("atlas: no such layout (" + code() + ")");
	// Each region's extent is thousands of the projection's probes (a 100k
	// system's sixteen bodies were 0.7 s): worked out once a layout, kept for
	// the process and beside the grids (sprint 110)
	static std::mutex lock;
	static std::map<std::string, std::vector<Grid>> known;
	const std::string key = code();
	{
		std::lock_guard<std::mutex> hold(lock);
		auto it = known.find(key);
		if (it != known.end()) {
			m_grids = it->second;
			return;
		}
	}
	std::vector<std::pair<Region, int>> regions;
	for (int s = 0; s < p.segments; s++)
		regions.emplace_back(Region::Segment, s);
	for (int c : { 1, -1 })
		regions.emplace_back(Region::Cap, c);
	if (p.core > 0)
		regions.emplace_back(Region::Core, 0);
	constexpr size_t FIELDS = 8; // a grid's: region, index, low, high
	const std::optional<std::vector<int64_t>> kept = loadAtlasExtents(key);
	if (kept && kept->size() == regions.size() * FIELDS) {
		for (size_t r = 0; r < regions.size(); r++) {
			const int64_t *f = &(*kept)[r * FIELDS];
			if (f[0] != (int64_t)regions[r].first || f[1] != regions[r].second)
				throw std::runtime_error("atlas: kept extents are another layout's (" + key + ")");
			m_grids.push_back({ regions[r].first, regions[r].second, { f[2], f[3], f[4] }, { f[5], f[6], f[7] } });
		}
	} else {
		std::vector<int64_t> extents;
		for (const auto &[region, index] : regions) {
			const Grid g = extent(region, index);
			m_grids.push_back(g);
			extents.insert(extents.end(), { (int64_t)region, index, g.low[0], g.low[1], g.low[2], g.high[0], g.high[1], g.high[2] });
		}
		saveAtlasExtents(key, extents);
	}
	std::lock_guard<std::mutex> hold(lock);
	known.emplace(key, m_grids);
}

Atlas::Frame Atlas::frame(Region region, int index) const
{
	switch (region) {
	case Region::Segment: {
		const double lon = (index + 0.5) * TAU / m_params.segments;
		return { { std::cos(lon), 0, std::sin(lon) }, { -std::sin(lon), 0, std::cos(lon) }, { 0, 1, 0 } };
	}
	case Region::Cap:
		return { { 0, (double)index, 0 }, { 0, 0, 1 }, { -(double)index, 0, 0 } };
	default:
		return { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } };
	}
}

/*
	Positions. A segment's row j is the centre meridian at latitude j/R; its
	east runs along the great circle square to the meridian there (its
	direction, `east`, is the same at every latitude). A cap's grid runs out
	from the pole the same way in every direction.
*/
Vec Atlas::at(const Cell &c, double di, double dj, double dk) const
{
	const double a = c.i + 0.5 + di, b = c.j + 0.5 + dj, h = c.k + 0.5 + dk;
	const Frame f = frame(c.region, c.index);
	const Row row{ m_params.radius, m_params.rounding };
	switch (c.region) {
	case Region::Segment: {
		const double lat = b / m_params.radius;
		const Vec m = std::cos(lat) * f.up + std::sin(lat) * f.north;
		const auto ab = row.at(a, h);
		return ab[0] * m + ab[1] * f.east;
	}
	case Region::Cap: {
		const double r = std::hypot(a, b);
		const auto ab = row.at(r, h);
		if (r < 1e-12)
			return ab[0] * f.up;
		return ab[0] * f.up + (ab[1] / r) * (a * f.east + b * f.north);
	}
	default:
		return a * f.east + h * f.up + b * f.north;
	}
}

std::array<double, 3> Atlas::grid(Region region, int index, const Vec &p) const
{
	const Frame f = frame(region, index);
	const Row row{ m_params.radius, m_params.rounding };
	switch (region) {
	case Region::Segment: {
		const double B = dot(p, f.east);
		const Vec q = p - B * f.east; // in the centre meridian's plane
		const double lat = std::atan2(dot(q, f.north), dot(q, f.up));
		const auto xh = row.back(length(q), B);
		return { xh[0], lat * m_params.radius, xh[1] };
	}
	case Region::Cap: {
		const double A = dot(p, f.up);
		const Vec t = p - A * f.up;
		const double B = length(t);
		const auto xh = row.back(A, B);
		if (B < 1e-12)
			return { 0, 0, xh[1] };
		return { xh[0] * dot(t, f.east) / B, xh[0] * dot(t, f.north) / B, xh[1] };
	}
	default:
		return { dot(p, f.east), dot(p, f.north), dot(p, f.up) };
	}
}

/*
	Territories: the core inside its radius; the caps poleward of their
	latitude; the segments between their meridians. How far inside one a
	point is, to its nearest edge, along the surface it's on.
*/
double Atlas::inside(Region region, int index, const Vec &p) const
{
	const Params &prm = m_params;
	const double r = length(p);
	const double core = prm.core > 0 ? r - prm.core : NONE; // above the core
	if (region == Region::Core)
		return -core;
	const double lat = std::asin(std::clamp(p.y / std::max(r, 1e-300), -1.0, 1.0));
	const double cap = (prm.capLatitude * DEGREE - std::abs(lat)) * r; // short of the caps' circles
	if (region == Region::Cap)
		return std::min(index * lat > 0 ? -cap : -std::abs(lat) * r, core);
	const double width = TAU / prm.segments;
	double lon = std::atan2(p.z, p.x) - index * width;
	lon -= std::floor(lon / TAU) * TAU; // 0 .. τ east of the segment's west edge
	const double across = r * std::cos(lat);
	const double between = lon <= width ? std::min(lon, width - lon) * across : -std::min(lon - width, TAU - lon) * across;
	return std::min({ between, cap, core });
}

std::array<double, 4> Atlas::edges(const Cell &cell, const Vec &p) const
{
	const Params &prm = m_params;
	const double r = length(p);
	const double core = prm.core > 0 ? r - prm.core : NONE;
	if (cell.region == Region::Core)
		return { -core, NONE, NONE, NONE };
	const double lat = std::asin(std::clamp(p.y / std::max(r, 1e-300), -1.0, 1.0));
	const double cap = (prm.capLatitude * DEGREE - std::abs(lat)) * r;
	if (cell.region == Region::Cap)
		return { cell.index * lat > 0 ? -cap : -std::abs(lat) * r, core, NONE, NONE };
	const double width = TAU / prm.segments;
	// east of the west meridian, −π .. π (a cell's points are near it or the east one)
	double lon = std::atan2(p.z, p.x) - cell.index * width;
	lon -= std::floor(lon / TAU + 0.5) * TAU;
	const double across = r * std::cos(lat);
	return { lon * across, (width - lon) * across, cap, core };
}

bool Atlas::above(const Vec &p) const
{
	return length(p) >= m_params.radius + m_params.height;
}

Atlas::Cell Atlas::owner(const Vec &p) const
{
	const Params &prm = m_params;
	const double r = length(p);
	if (above(p))
		throw std::out_of_range("atlas: point above the atlas");
	Region region = Region::Segment;
	int index;
	if (prm.core > 0 && r < prm.core) {
		region = Region::Core;
		index = 0;
	} else if (std::abs(p.y) >= r * std::sin(prm.capLatitude * DEGREE)) {
		region = Region::Cap;
		index = p.y > 0 ? 1 : -1;
	} else {
		double lon = std::atan2(p.z, p.x);
		if (lon < 0)
			lon += TAU;
		index = std::min((int)(lon / (TAU / prm.segments)), prm.segments - 1);
	}
	const auto g = grid(region, index, p);
	return { region, index, (int64_t)std::floor(g[0]), (int64_t)std::floor(g[1]), (int64_t)std::floor(g[2]) };
}

// A cell exists where some of it (a 64th, by its samples) is in its
// territory: a sliver less than that is left to the grid reaching over it
// (locate), so every cell has faces to meet its neighbours with.
bool Atlas::exists(const Cell &c) const
{
	return retained(c) > 0;
}

double Atlas::retained(const Cell &c) const
{
	if (c.region != Region::Core && c.k >= m_params.height)
		return 0;
	const double d = inside(c.region, c.index, centre(c));
	if (d >= CLEAR)
		return 1;
	if (d <= -CLEAR)
		return 0;
	// a cut cell: sampled once, kept (the planet never changes)
	const Kept::Key key{ (int)c.region, c.index, c.i, c.j, c.k };
	{
		std::lock_guard<std::mutex> hold(m_kept->lock);
		const auto it = m_kept->retained.find(key);
		if (it != m_kept->retained.end())
			return it->second;
	}
	int in = 0;
	constexpr int N = 4;
	for (int a = 0; a < N; a++)
		for (int b = 0; b < N; b++)
			for (int h = 0; h < N; h++)
				in += inside(c.region, c.index, at(c, (a + 0.5) / N - 0.5, (b + 0.5) / N - 0.5, (h + 0.5) / N - 0.5)) >= 0;
	const double kept = (double)in / (N * N * N);
	std::lock_guard<std::mutex> hold(m_kept->lock);
	m_kept->retained.emplace(key, kept);
	return kept;
}

Atlas::Cell Atlas::locate(const Vec &p) const
{
	const Cell own = owner(p);
	if (exists(own))
		return own;
	// a sliver of its territory its own grid leaves (too little of the cell
	// in it): the grid reaching over it, if that grid's cell there is one
	std::vector<Cell> there{ own };
	for (const Grid &g : m_grids) {
		if ((g.region == own.region && g.index == own.index) || inside(g.region, g.index, p) < -2)
			continue; // its own, or a region too far off to reach over
		const auto q = grid(g.region, g.index, p);
		const Cell c{ g.region, g.index, (int64_t)std::floor(q[0]), (int64_t)std::floor(q[1]), (int64_t)std::floor(q[2]) };
		bool within = true;
		for (int k = 0; k < 3; k++) {
			const int64_t v = k == 0 ? c.i : k == 1 ? c.j : c.k;
			within &= v >= g.low[k] - 1 && v <= g.high[k] + 1;
		}
		if (!within)
			continue;
		if (exists(c))
			return c;
		there.push_back(c);
	}
	// else merged into a neighbour: of the cells round it in either grid,
	// the one whose centre is nearest
	std::optional<Cell> best;
	double nearest = NONE;
	for (const Cell &c : there)
		for (int di = -1; di <= 1; di++)
			for (int dj = -1; dj <= 1; dj++)
				for (int dk = -1; dk <= 1; dk++) {
					const Cell n{ c.region, c.index, c.i + di, c.j + dj, c.k + dk };
					if (!exists(n))
						continue;
					const Vec m = centre(n);
					const double d = dot(m - p, m - p);
					if (d < nearest) {
						nearest = d;
						best = n;
					}
				}
	if (!best)
		throw std::logic_error("atlas: no cell near a point of the planet");
	return *best;
}

std::array<Vec, 8> Atlas::corners(const Cell &c) const
{
	std::array<Vec, 8> out;
	for (int b = 0; b < 8; b++)
		out[b] = at(c, (b & 1) - 0.5, (b >> 1 & 1) - 0.5, (b >> 2 & 1) - 0.5);
	return out;
}

Vec Atlas::centre(const Cell &cell) const
{
	return at(cell, 0, 0, 0);
}

Atlas::Quality Atlas::quality(const Cell &c) const
{
	const std::array<Vec, 8> k = corners(c);
	Quality q;
	q.east = worstStretch({ k[1] - k[0], k[3] - k[2], k[5] - k[4], k[7] - k[6] });
	q.north = worstStretch({ k[2] - k[0], k[3] - k[1], k[6] - k[4], k[7] - k[5] });
	q.up = worstStretch({ k[4] - k[0], k[5] - k[1], k[6] - k[2], k[7] - k[3] });
	const Vec e = at(c, 0.5, 0, 0) - at(c, -0.5, 0, 0), n = at(c, 0, 0.5, 0) - at(c, 0, -0.5, 0),
		  u = at(c, 0, 0, 0.5) - at(c, 0, 0, -0.5);
	q.skew = std::max({ std::abs(angleBetween(e, n) - 90), std::abs(angleBetween(n, u) - 90),
		std::abs(angleBetween(u, e) - 90) });
	q.gravity = angleBetween(u, centre(c));
	q.retained = retained(c);
	q.seams = seams(c);
	return q;
}

// The seams within reach of a cell (its centre within a cell and a half of
// each edge), and whether it's cut.
uint8_t Atlas::seams(const Cell &c) const
{
	const Params &prm = m_params;
	const Vec p = centre(c);
	const double r = length(p);
	const double lat = std::asin(std::clamp(p.y / std::max(r, 1e-300), -1.0, 1.0));
	const double near = CLEAR + 0.5;
	uint8_t out = 0;
	if (prm.core > 0 && std::abs(r - prm.core) < near)
		out |= CORE;
	if (c.region != Region::Core && std::abs(prm.capLatitude * DEGREE - std::abs(lat)) * r < near)
		out |= CAP;
	if (c.region == Region::Segment) {
		const double width = TAU / prm.segments;
		double lon = std::atan2(p.z, p.x) - c.index * width;
		lon -= std::floor(lon / TAU) * TAU;
		if (lon > width || std::min(lon, width - lon) * r * std::cos(lat) < near)
			out |= SEGMENT;
	}
	if (retained(c) < 1)
		out |= CUT;
	return out;
}

std::vector<Atlas::Cell> Atlas::neighbours(const Cell &c, int face) const
{
	const int axis = face / 2, sign = face % 2 ? 1 : -1;
	Cell plain = c;
	(axis == 0 ? plain.i : axis == 1 ? plain.j : plain.k) += sign;
	if (exists(plain) && retained(c) == 1 && retained(plain) == 1)
		return { plain };
	// At a seam or a cut: two cells are neighbours where either's face,
	// sampled just past it, falls in the other (grids at an angle don't
	// meet face to face, and a neighbour must be one both ways). This face's
	// samples; then the cells a little past its edges too, any of whose
	// faces' samples fall in this one.
	std::vector<Cell> out, near;
	constexpr int N = 5;
	for (int u = -1; u <= N; u++)
		for (int v = -1; v <= N; v++) {
			const double s = (u + 0.5) / N - 0.5, t = (v + 0.5) / N - 0.5, d = sign * 0.501;
			const bool onFace = u >= 0 && u < N && v >= 0 && v < N;
			std::optional<Cell> found;
			if (onFace)
				found = beyond(c, axis, d, s, t);
			else if (const Vec p = axis == 0 ? at(c, d, s, t) : axis == 1 ? at(c, s, d, t) : at(c, s, t, d); !above(p))
				found = locate(p);
			if (!found || *found == c || !exists(*found))
				continue; // past the top, or still this cell
			const Cell across = *found;
			if (across.region == c.region && across.index == c.index && !(across == plain))
				continue; // its own grid's, across another face (a sliver merged into it)
			std::vector<Cell> &into = onFace ? out : near;
			if (std::find(into.begin(), into.end(), across) == into.end())
				into.push_back(across);
		}
	// and deeper (a face's samples walk on through slivers merged into it)
	for (const double deeper : { 0.8, 1.2 })
		for (int u = 0; u < N; u++)
			for (int v = 0; v < N; v++) {
				const double s = (u + 0.5) / N - 0.5, t = (v + 0.5) / N - 0.5, d = sign * deeper;
				const Vec p = axis == 0 ? at(c, d, s, t) : axis == 1 ? at(c, s, d, t) : at(c, s, t, d);
				if (above(p))
					continue;
				const Cell across = locate(p);
				if (across == c || !exists(across) || (across.region == c.region && across.index == c.index))
					continue;
				if (std::find(near.begin(), near.end(), across) == near.end())
					near.push_back(across);
			}
	for (const Cell &n : near) {
		if (std::find(out.begin(), out.end(), n) != out.end())
			continue;
		bool touches = false;
		for (int f = 0; f < 6 && !touches; f++) {
			const int ax = f / 2;
			const double d = (f % 2 ? 1 : -1) * 0.501;
			for (int u = 0; u < N && !touches; u++)
				for (int v = 0; v < N && !touches; v++) {
					const double s = (u + 0.5) / N - 0.5, t = (v + 0.5) / N - 0.5;
					const std::optional<Cell> b = beyond(n, ax, d, s, t);
					touches = b && *b == c;
				}
		}
		if (touches)
			out.push_back(n);
	}
	return out;
}

std::optional<Atlas::Cell> Atlas::through(const Cell &c, int face) const
{
	const int axis = face / 2;
	const double d = face % 2 ? 0.501 : -0.501;
	Cell plain = c;
	(axis == 0 ? plain.i : axis == 1 ? plain.j : plain.k) += face % 2 ? 1 : -1;
	// through the face's middle; where the cell doesn't reach that line (a
	// cut cell whose centre is past its edge), through the nearest of the
	// face's samples (neighbours' own) it does
	static const std::array<std::array<double, 2>, 9> LINES{ { { 0, 0 }, { 0.2, 0 }, { -0.2, 0 }, { 0, 0.2 }, { 0, -0.2 },
			{ 0.4, 0 }, { -0.4, 0 }, { 0, 0.4 }, { 0, -0.4 } } };
	for (const auto &[s, t] : LINES) {
		const std::optional<Cell> there = beyond(c, axis, d, s, t);
		if (there && !(*there == c) && exists(*there) &&
				!(there->region == c.region && there->index == c.index && !(*there == plain)))
			return there;
	}
	const std::vector<Cell> across = neighbours(c, face);
	if (across.empty())
		return std::nullopt;
	const Vec middle = axis == 0 ? at(c, d, 0, 0) : axis == 1 ? at(c, 0, d, 0) : at(c, 0, 0, d);
	return *std::min_element(across.begin(), across.end(), [&](const Cell &a, const Cell &b) {
		const Vec ma = centre(a) - middle, mb = centre(b) - middle;
		return dot(ma, ma) < dot(mb, mb);
	});
}

/*
	The cell past a cell's face at (s, t) on it, along axis ax (d: ±0.501):
	the one just past it (pastFace), or where that's a sliver merged into
	this cell (past a face it isn't cut short of), the first one on along
	the line (a sliver kept by none of its grid's cells is under ⅛ of a block: up to 0.3 on). None past
	the top of the atlas, or where the cell doesn't reach.
*/
std::optional<Atlas::Cell> Atlas::beyond(const Cell &c, int ax, double d, double s, double t) const
{
	const std::optional<Vec> p = pastFace(c, ax, d, s, t);
	if (!p || above(*p))
		return std::nullopt;
	// on from where it left, the grid's way along the axis
	const auto line = [&](double along) { return ax == 0 ? at(c, along, s, t) : ax == 1 ? at(c, s, along, t) : at(c, s, t, along); };
	const Vec way = line(d + (d > 0 ? 0.05 : -0.05)) - line(d);
	const Vec off = *p - line(d);
	const bool straight = dot(off, off) < 1e-12; // a cut cell left by its edge has no sliver of its own past it
	Cell there = locate(*p);
	for (int n = 1; straight && there == c && n <= 6; n++) {
		const Vec q{ p->x + way.x * n, p->y + way.y * n, p->z + way.z * n };
		if (above(q))
			return std::nullopt;
		there = locate(q);
	}
	return there;
}

/*
	Just past a cell's face at (s, t) on it (d: ±0.501, along axis ax): where
	a step that way leaves the cell. A cut cell is bounded by its
	territory's edge where that comes first: then just past the edge, along
	the way from its middle to the face. None where the cell doesn't reach
	(that line of it wholly outside its territory).
*/
std::optional<Vec> Atlas::pastFace(const Cell &c, int ax, double d, double s, double t) const
{
	const auto point = [&](double along) {
		return ax == 0 ? at(c, along, s, t) : ax == 1 ? at(c, s, along, t) : at(c, s, t, along);
	};
	const double face = d > 0 ? 0.5 : -0.5;
	if (inside(c.region, c.index, point(face - (d > 0 ? 1e-3 : -1e-3))) >= 0)
		return point(d);
	// from the farthest point of the line inside the territory, out to its edge
	constexpr int SAMPLES = 8;
	double in = NONE;
	for (int n = SAMPLES; n >= 0 && in == NONE; n--) {
		const double along = -face + 2 * face * n / SAMPLES;
		if (inside(c.region, c.index, point(along)) >= 0)
			in = along;
	}
	if (in == NONE)
		return std::nullopt;
	double out = face;
	for (int n = 0; n < 24; n++) {
		const double mid = (in + out) / 2;
		(inside(c.region, c.index, point(mid)) >= 0 ? in : out) = mid;
	}
	return point(out + (d > 0 ? 0.01 : -0.01));
}

/*
	A region's grid's extent: its territory's points (edges and corners, the
	surface's top to the core) in its grid, with cells to spare.
*/
Atlas::Grid Atlas::extent(Region region, int index) const
{
	const Params &p = m_params;
	Grid g{ region, index, {}, {} };
	if (region == Region::Core) {
		const int64_t reach = (int64_t)std::ceil(p.core) + 1;
		g.low = { -reach, -reach, -reach };
		g.high = { reach - 1, reach - 1, reach - 1 };
		return g;
	}
	std::array<double, 3> lo{ NONE, NONE, NONE }, hi{ -NONE, -NONE, -NONE };
	const double width = TAU / p.segments, cap = p.capLatitude * DEGREE;
	const double bottom = p.core > 0 ? p.core : 1, top = p.radius + p.height;
	constexpr int STEPS = 48;
	for (int a = 0; a <= STEPS; a++)
		for (int b = 0; b <= STEPS; b++)
			for (double r : { bottom, (bottom + p.radius) / 2, p.radius, top }) {
				double lat, lon;
				if (region == Region::Segment) {
					lon = (index + (double)a / STEPS) * width;
					lat = -cap + 2 * cap * b / STEPS;
				} else {
					lon = TAU * a / STEPS;
					lat = index * (cap + (TAU / 4 - cap) * b / STEPS);
				}
				const Vec v{ r * std::cos(lat) * std::cos(lon), r * std::sin(lat), r * std::cos(lat) * std::sin(lon) };
				const auto q = grid(region, index, v);
				for (int k = 0; k < 3; k++) {
					lo[k] = std::min(lo[k], q[k]);
					hi[k] = std::max(hi[k], q[k]);
				}
			}
	for (int k = 0; k < 3; k++) {
		g.low[k] = (int64_t)std::floor(lo[k]) - 2;
		g.high[k] = (int64_t)std::floor(hi[k]) + 2;
	}
	g.high[2] = p.height - 1;
	return g;
}

std::string Atlas::code() const
{
	const Params &p = m_params;
	std::ostringstream out;
	out << std::setprecision(17) << VERSION << " radius=" << p.radius << " segments=" << p.segments
	    << " capLatitude=" << p.capLatitude << " core=" << p.core << " rounding=" << p.rounding
	    << " height=" << p.height;
	return out.str();
}

Atlas::Params Atlas::parse(const std::string &code)
{
	std::istringstream in(code);
	std::string version;
	in >> version;
	if (version != VERSION)
		throw std::invalid_argument("atlas: unknown mapping \"" + version + "\" (this engine reads " + VERSION + ")");

	std::map<std::string, std::string> fields;
	for (std::string field; in >> field;) {
		const size_t eq = field.find('=');
		if (eq == std::string::npos || !fields.emplace(field.substr(0, eq), field.substr(eq + 1)).second)
			throw std::invalid_argument("atlas: bad or repeated field \"" + field + "\"");
	}
	const auto number = [&](const char *name) {
		const auto it = fields.find(name);
		if (it == fields.end())
			throw std::invalid_argument(std::string("atlas: mapping lacks ") + name);
		const std::string v = it->second;
		fields.erase(it);
		size_t used = 0;
		const double d = std::stod(v, &used);
		if (used != v.size())
			throw std::invalid_argument(std::string("atlas: ") + name + " is not a number");
		return d;
	};

	Params p;
	p.radius = number("radius");
	p.segments = (int)number("segments");
	p.capLatitude = number("capLatitude");
	p.core = number("core");
	p.rounding = number("rounding");
	p.height = (int)number("height");
	if (!fields.empty())
		throw std::invalid_argument("atlas: unknown field \"" + fields.begin()->first + "\"");
	return p;
}

} // namespace terragen
