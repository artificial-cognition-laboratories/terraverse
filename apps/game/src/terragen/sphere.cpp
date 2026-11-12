// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sphere.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>

namespace terragen {

// The icosahedron's corners, (±1, ±φ, 0) and its turns, at 2²⁰: exact
// integers, so a lattice point is the same sum from either face it's on.
static constexpr int64_t S = 1 << 20;
static constexpr int64_t P = 1696631; // φ · 2²⁰
static const int64_t CORNER[12][3] = {
	{ -S, P, 0 }, { S, P, 0 }, { -S, -P, 0 }, { S, -P, 0 },
	{ 0, -S, P }, { 0, S, P }, { 0, -S, -P }, { 0, S, -P },
	{ P, 0, -S }, { P, 0, S }, { -P, 0, -S }, { -P, 0, S },
};
static const int32_t FACE[20][3] = {
	{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
	{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
	{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
	{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 },
};

using V = std::array<int64_t, 3>;

static V cross(const V &a, const V &b)
{
	return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}

static int64_t dot(const Dir &d, const V &v)
{
	return d.x * v[0] + d.y * v[1] + d.z * v[2];
}

static int64_t dot(const Dir &a, const Dir &b)
{
	return (int64_t)a.x * b.x + (int64_t)a.y * b.y + (int64_t)a.z * b.z;
}

static Dir unit(const V &v)
{
	const int64_t length = (int64_t)isqrt((uint64_t)(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
	return { (fix)(v[0] * ONE / length), (fix)(v[1] * ONE / length), (fix)(v[2] * ONE / length) };
}

struct VHash
{
	size_t operator()(const V &v) const
	{
		return (size_t)(v[0] * 73856093) ^ (size_t)(v[1] * 19349663) ^ (size_t)(v[2] * 83492791);
	}
};

// A face's lattice is rows j = 0..n, row j holding i = 0..n−j.
static int32_t rowStart(int32_t n, int32_t j)
{
	return j * (n + 1) - j * (j - 1) / 2;
}

Sphere::Sphere(int32_t frequency) : m_n(frequency)
{
	// corners at 2²⁰ times up to n: their squared lengths stay inside 64 bits
	if (frequency < 1 || frequency > 512)
		throw std::invalid_argument("sphere: frequency must be 1..512");
	const int32_t n = m_n;
	const int32_t per = rowStart(n, n + 1);
	m_lattice.resize((size_t)20 * per);

	// Every face's lattice points, each cell once: a point on an edge or a
	// corner is found again, exactly, from its other faces.
	std::unordered_map<V, int32_t, VHash> seen;
	seen.reserve((size_t)10 * n * n + 2);
	std::vector<V> raw;
	for (int32_t f = 0; f < 20; f++) {
		const int64_t *a = CORNER[FACE[f][0]], *b = CORNER[FACE[f][1]], *c = CORNER[FACE[f][2]];
		// outward winding, so a direction on the face has all three volumes ≥ 0
		const V A{ a[0], a[1], a[2] }, B{ b[0], b[1], b[2] }, C{ c[0], c[1], c[2] };
		const V bc = cross(B, C);
		const bool outward = A[0] * bc[0] + A[1] * bc[1] + A[2] * bc[2] > 0;
		if (!outward)
			throw std::logic_error("sphere: an icosahedron face winds inward");
		m_cross[f] = { bc, cross(C, A), cross(A, B) };
		for (int32_t j = 0; j <= n; j++)
			for (int32_t i = 0; i + j <= n; i++) {
				const int64_t k = n - i - j;
				const V p{ A[0] * k + B[0] * i + C[0] * j, A[1] * k + B[1] * i + C[1] * j, A[2] * k + B[2] * i + C[2] * j };
				auto [it, fresh] = seen.try_emplace(p, (int32_t)raw.size());
				if (fresh)
					raw.push_back(p);
				m_lattice[(size_t)f * per + rowStart(n, j) + i] = it->second;
			}
	}
	if ((int64_t)raw.size() != (int64_t)10 * n * n + 2)
		throw std::logic_error("sphere: the lattice doesn't close");
	m_centre.reserve(raw.size());
	for (const V &p : raw)
		m_centre.push_back(unit(p));

	// The lattice's triangles: each cell's ring of them gives its neighbours
	// in turn (each triangle the step from one neighbour to the next), and a
	// third of each one's area.
	const size_t cells = m_centre.size();
	std::vector<std::vector<std::array<int32_t, 2>>> fan(cells);
	std::vector<int64_t> twice(cells, 0); // twice the area of each cell's triangles: three times its own
	auto triangle = [&](int32_t p, int32_t q, int32_t r) {
		const Dir &a = m_centre[p], &b = m_centre[q], &c = m_centre[r];
		const V u{ b.x - a.x, b.y - a.y, b.z - a.z }, w{ c.x - a.x, c.y - a.y, c.z - a.z };
		const V x = cross(u, w);
		const int64_t area = (int64_t)isqrt((uint64_t)(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]));
		for (int32_t v : { p, q, r })
			twice[v] += area;
		fan[p].push_back({ q, r });
		fan[q].push_back({ r, p });
		fan[r].push_back({ p, q });
	};
	for (int32_t f = 0; f < 20; f++)
		for (int32_t j = 0; j < n; j++)
			for (int32_t i = 0; i + j < n; i++) {
				triangle(lattice(f, i, j), lattice(f, i + 1, j), lattice(f, i, j + 1));
				if (i + j + 1 < n)
					triangle(lattice(f, i + 1, j), lattice(f, i + 1, j + 1), lattice(f, i, j + 1));
			}

	int64_t total = 0;
	for (int64_t t : twice)
		total += t;
	m_start.reserve(cells + 1);
	m_area.reserve(cells);
	for (size_t c = 0; c < cells; c++) {
		m_start.push_back((int32_t)m_neighbours.size());
		// chain the fan: each triangle's far edge runs from one neighbour to the next
		std::vector<std::array<int32_t, 2>> &edges = fan[c];
		if (edges.size() != 5 && edges.size() != 6)
			throw std::logic_error("sphere: a cell without five or six triangles");
		int32_t at = edges[0][0];
		for (size_t k = 0; k < edges.size(); k++) {
			m_neighbours.push_back(at);
			auto next = std::find_if(edges.begin(), edges.end(), [&](const auto &e) { return e[0] == at; });
			if (next == edges.end())
				throw std::logic_error("sphere: a cell's triangles don't ring it");
			at = (*next)[1];
		}
		if (at != edges[0][0])
			throw std::logic_error("sphere: a cell's ring doesn't close");
		m_area.push_back((fix)(twice[c] * (int64_t)cells * ONE / total));
	}
	m_start.push_back((int32_t)m_neighbours.size());
	int64_t spans = 0;
	m_distance.reserve(m_neighbours.size());
	for (size_t c = 0; c < cells; c++)
		for (int32_t k = m_start[c]; k < m_start[c + 1]; k++) {
			m_distance.push_back(chord(m_centre[c], m_centre[m_neighbours[k]]));
			spans += m_distance.back();
		}
	m_spacing = (fix)(spans / (int64_t)m_distance.size());
}

int32_t Sphere::lattice(int32_t face, int32_t i, int32_t j) const
{
	return m_lattice[(size_t)face * rowStart(m_n, m_n + 1) + rowStart(m_n, j) + i];
}

Sphere::Onface Sphere::onFace(const Dir &d) const
{
	for (int32_t f = 0; f < 20; f++) {
		// the volumes d makes with each pair of corners: all ≥ 0 inside the
		// face's cone, and in proportion to where it meets the face's plane
		int64_t w[3];
		for (int k = 0; k < 3; k++)
			w[k] = dot(d, m_cross[f][k]);
		if (w[0] < 0 || w[1] < 0 || w[2] < 0)
			continue;
		int64_t sum = w[0] + w[1] + w[2];
		// down to where n · ONE · w stays in 64 bits
		while (sum >= ((int64_t)1 << 38)) {
			for (int64_t &v : w)
				v >>= 1;
			sum = w[0] + w[1] + w[2];
		}
		if (sum == 0)
			continue;
		const int64_t scale = (int64_t)m_n * ONE;
		return { f, (fix)(scale * w[1] / sum), (fix)(scale * w[2] / sum) };
	}
	throw std::invalid_argument("sphere: a direction on no face (zero?)");
}

int32_t Sphere::cell(const Dir &d) const
{
	const Onface o = onFace(d);
	int32_t i = roundInt(o.i), j = roundInt(o.j);
	i = std::clamp(i, 0, m_n);
	j = std::clamp(j, 0, m_n - i);
	// the lattice's nearest point is nearly always the nearest centre (the
	// projection bends a little): walk to whichever neighbour is nearer
	int32_t at = lattice(o.face, i, j);
	int64_t best = dot(d, m_centre[at]);
	for (bool moved = true; moved;) {
		moved = false;
		const int32_t from = at;
		for (const int32_t *p = neighboursBegin(from); p != neighboursEnd(from); p++) {
			const int64_t near = dot(d, m_centre[*p]);
			if (near > best) {
				best = near;
				at = *p;
				moved = true;
			}
		}
	}
	return at;
}

Sphere::Spot Sphere::spot(const Dir &d) const
{
	// the last asked for, the thread's: a column reads a dozen fields between
	// the cells at one place (World::between, surface, column), and finding
	// them each time was a tenth of a far tile's time (sprint 114)
	thread_local const Sphere *last_sphere = nullptr;
	thread_local Dir last_dir{};
	thread_local Spot last{};
	if (last_sphere == this && last_dir.x == d.x && last_dir.y == d.y && last_dir.z == d.z)
		return last;
	last = spotAt(d);
	last_sphere = this;
	last_dir = d;
	return last;
}

Sphere::Spot Sphere::spotAt(const Dir &d) const
{
	const Onface o = onFace(d);
	int32_t i = std::clamp(floorInt(o.i), 0, m_n - 1), j = std::clamp(floorInt(o.j), 0, m_n - 1 - i);
	fix a = o.i - toFix(i), b = o.j - toFix(j);
	if (i + j + 1 < m_n && a + b > ONE) {
		// the lattice triangle pointing the other way: from its corner (i+1, j+1)
		const fix a2 = ONE - a, b2 = ONE - b; // toward (i, j+1) and (i+1, j)
		return { { lattice(o.face, i + 1, j + 1), lattice(o.face, i, j + 1), lattice(o.face, i + 1, j) },
			{ ONE - a2 - b2, a2, b2 } };
	}
	a = clamp(a, 0, ONE);
	b = clamp(b, 0, ONE - a);
	return { { lattice(o.face, i, j), lattice(o.face, i + 1, j), lattice(o.face, i, j + 1) }, { ONE - a - b, a, b } };
}

Dir dirAt(fix latitude, fix longitude)
{
	fix sl, cl, so, co;
	sincos(latitude, sl, cl);
	sincos(longitude, so, co);
	return normalise(mul(cl, co), sl, mul(cl, so));
}

fix latitudeOf(const Dir &d)
{
	const fix across = (fix)isqrt((uint64_t)((int64_t)d.x * d.x + (int64_t)d.z * d.z));
	return atan2(d.y, across);
}

fix longitudeOf(const Dir &d)
{
	return atan2(d.z, d.x);
}

fix chord(const Dir &a, const Dir &b)
{
	const int64_t x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
	return (fix)isqrt((uint64_t)(x * x + y * y + z * z));
}

Dir normalise(int64_t x, int64_t y, int64_t z)
{
	// up to where the squares fit, then down to fixed
	while (std::max({ std::abs(x), std::abs(y), std::abs(z) }) >= ((int64_t)1 << 30)) {
		x /= 2;
		y /= 2;
		z /= 2;
	}
	const int64_t length = (int64_t)isqrt((uint64_t)(x * x + y * y + z * z));
	if (length == 0)
		throw std::invalid_argument("sphere: no direction to a zero vector");
	return { (fix)(x * ONE / length), (fix)(y * ONE / length), (fix)(z * ONE / length) };
}

Dir Place::dir() const
{
	constexpr int SHIFT = PLACE_FRAC - FRAC;
	const auto round = [](int32_t c) { return (fix)((c + (1 << (SHIFT - 1))) >> SHIFT); };
	return { round(x), round(y), round(z) };
}

Place placeOf(int64_t x, int64_t y, int64_t z)
{
	// down to where the squares' sum fits 64 bits unsigned
	while (std::max({ std::abs(x), std::abs(y), std::abs(z) }) >= ((int64_t)1 << 31)) {
		x /= 2;
		y /= 2;
		z /= 2;
	}
	const int64_t length = (int64_t)isqrt((uint64_t)(x * x) + (uint64_t)(y * y) + (uint64_t)(z * z));
	if (length == 0)
		throw std::invalid_argument("sphere: no place in a zero vector");
	return { (int32_t)((x << PLACE_FRAC) / length), (int32_t)((y << PLACE_FRAC) / length),
		(int32_t)((z << PLACE_FRAC) / length) };
}

int64_t chord(const Place &a, const Place &b)
{
	const int64_t x = (int64_t)a.x - b.x, y = (int64_t)a.y - b.y, z = (int64_t)a.z - b.z;
	return (int64_t)isqrt((uint64_t)(x * x) + (uint64_t)(y * y) + (uint64_t)(z * z));
}

} // namespace terragen
