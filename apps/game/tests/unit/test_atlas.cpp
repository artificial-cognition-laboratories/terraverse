// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "library/world/terragen/src/out/atlas.h"
#include <algorithm>
#include <cmath>
#include <random>

using terragen::Atlas;
using Vec = Atlas::Vec;
using Region = Atlas::Region;

/*
	The atlas's invariants, layout v2 (planet.paper.md, section 2): a point
	goes to its cell and back; the cells tile the planet, every point in
	one; inside a region blocks are near cubes, near enough to gravity, and
	neighbours agree across seams; and a world reads back the mapping it was
	made on.
*/
class TestAtlas : public TestBase {
public:
	TestAtlas() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestAtlas"; }

	void runTests(IGameDef *gamedef);

	void testCodeRoundTrip();
	void testBadParams();
	void testGridRoundTrip();
	void testLocateCentre();
	void testCellsTileThePlanet();
	void testBlocksInsideRegions();
	void testRoundingTrades();
	void testSeamsAreCut();
	void testNeighboursAgree();
	void testOneGridDown();
	void testLayersAreSpheres();
};

static TestAtlas g_test_instance;

namespace {

constexpr double TAU = 6.283185307179586476925;
constexpr double DEGREE = 0.017453292519943295769;

// The dev world's planet (8192 round), a 10 km moon, and Kerbin
Atlas::Params dev() { return Atlas::standard(8192 / TAU); }
Atlas::Params moon() { return Atlas::standard(10000); }
Atlas::Params kerbin() { return Atlas::standard(600000); }

double length(Vec v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vec onPlanet(double r, double lat, double lon)
{
	return { r * std::cos(lat) * std::cos(lon), r * std::sin(lat), r * std::cos(lat) * std::sin(lon) };
}

// Random points of the planet, every latitude, between radii lo and hi
// (the whole planet by default), and `seams`: only on the meridians between
// segments.
template <typename F>
void points(const Atlas::Params &p, int count, unsigned seed, F f, double lo = 0.5, double hi = -1, bool seams = false)
{
	if (hi < 0)
		hi = p.radius + p.height * 0.9;
	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> u(0, 1);
	for (int n = 0; n < count; n++) {
		const double lat = std::asin(2 * u(rng) - 1);
		const double lon = seams ? (int)(u(rng) * p.segments) * TAU / p.segments + 1e-9 : TAU * u(rng);
		f(onPlanet(lo + u(rng) * (hi - lo), lat, lon));
	}
}

} // namespace

void TestAtlas::runTests(IGameDef *gamedef)
{
	TEST(testCodeRoundTrip);
	TEST(testBadParams);
	TEST(testGridRoundTrip);
	TEST(testLocateCentre);
	TEST(testCellsTileThePlanet);
	TEST(testBlocksInsideRegions);
	TEST(testRoundingTrades);
	TEST(testSeamsAreCut);
	TEST(testNeighboursAgree);
	TEST(testOneGridDown);
	TEST(testLayersAreSpheres);
}

void TestAtlas::testCodeRoundTrip()
{
	const Atlas a(dev());
	const Atlas::Params back = Atlas::parse(a.code());
	UASSERT(back.radius == a.params().radius && back.segments == 24 && back.capLatitude == 85);
	UASSERT(back.core == a.params().core && back.rounding == 0.95 && back.height == 256);
	UASSERT(Atlas(back).code() == a.code());
	for (const char *bad : { "atlas-v1 radius=1", "atlas-v2 radius=1 segments=8 capLatitude=85 core=0 rounding=1",
				"atlas-v2 radius=1 segments=8 capLatitude=85 core=0 rounding=1 height=4 extra=1" })
		EXCEPTION_CHECK(std::invalid_argument, Atlas::parse(bad));
}

void TestAtlas::testBadParams()
{
	for (const Atlas::Params &p : { Atlas::Params{ 100, 2, 85, 0, 0.95, 8 }, Atlas::Params{ 100, 8, 90, 0, 0.95, 8 },
				Atlas::Params{ 100, 8, 85, 100, 0.95, 8 }, Atlas::Params{ 100, 8, 85, 0, 1.5, 8 } })
		EXCEPTION_CHECK(std::invalid_argument, Atlas{ p });
}

// Every region's grid maps its cells to the planet and back exactly: the
// same cell coordinates from the point a cell's centre is at.
void TestAtlas::testGridRoundTrip()
{
	for (const Atlas::Params &p : { dev(), moon(), kerbin() }) {
		const Atlas a(p);
		std::mt19937 rng(7);
		for (const Atlas::Grid &g : a.grids())
			for (int n = 0; n < 200; n++) {
				const auto pick = [&](int k) {
					return std::uniform_int_distribution<int64_t>(g.low[k], g.high[k])(rng);
				};
				const Atlas::Cell c{ g.region, g.index, pick(0), pick(1), pick(2) };
				const auto q = a.grid(g.region, g.index, a.centre(c));
				const double off = std::max({ std::abs(q[0] - c.i - 0.5), std::abs(q[1] - c.j - 0.5), std::abs(q[2] - c.k - 0.5) });
				// doubles: a part in 10¹¹ of the planet
				UTEST(off < 1e-11 * p.radius + 1e-9, "%s %d cell (%lld, %lld, %lld) comes back %.2g off", g.region == Region::Segment ? "segment"
						: g.region == Region::Cap ? "cap" : "core", g.index, (long long)c.i, (long long)c.j, (long long)c.k, off);
			}
	}
}

// A cell's centre locates to the cell, wherever it is wholly in its territory.
void TestAtlas::testLocateCentre()
{
	const Atlas a(dev());
	int checked = 0;
	points(a.params(), 4000, 3, [&](const Vec &v) {
		const Atlas::Cell c = a.locate(v);
		if (a.retained(c) < 1)
			return;
		UASSERT(a.locate(a.centre(c)) == c);
		checked++;
	});
	UASSERT(checked > 3000);
}

// Every point of the planet belongs to a cell: one that exists, holding it
// (the point in its grid's box), or at a sliver, merged into it (the point
// within a cell of it). Anywhere, and right on the seams, where slivers are.
void TestAtlas::testCellsTileThePlanet()
{
	for (const Atlas::Params &p : { dev(), moon() }) {
		const Atlas a(p);
		int bad = 0, merged = 0;
		const auto check = [&](const Vec &v) {
			const Atlas::Cell c = a.locate(v);
			const auto q = a.grid(c.region, c.index, v);
			const auto off = [&](double x, int64_t i) { return x < i ? i - x : x >= i + 1 ? x - i - 1 : 0.0; };
			const double out = std::max({ off(q[0], c.i), off(q[1], c.j), off(q[2], c.k) });
			merged += out > 0;
			if ((!a.exists(c) || out > 1) && bad++ < 5)
				rawstream << "point (" << v.x << ", " << v.y << ", " << v.z << ") lands in cell (" << c.i << ", " << c.j
					<< ", " << c.k << ") of region " << (int)c.region << " " << c.index << (a.exists(c) ? "" : ", which isn't one")
					<< ", " << out << " cells from it" << std::endl;
		};
		points(p, 20000, 11, check);
		points(p, 20000, 12, check, 0.5, -1, true);
		UTEST(bad == 0, "%d points without their cell", bad);
		UASSERT(merged < 2000);
	}
}

// Inside a region (uncut, off the core) a block is near a cube, its sides
// square, and its up near gravity: by (1 − rounding) of half a segment at
// its edge, and its width less than (1 − rounding·depth/R) at depth.
void TestAtlas::testBlocksInsideRegions()
{
	for (const Atlas::Params &p : { dev(), kerbin() }) {
		const Atlas a(p);
		const double tilt = (1 - p.rounding) * 180.0 / p.segments + 0.05;
		int checked = 0;
		// the crust: what people build in
		points(p, 3000, 5, [&](const Vec &v) {
			const Atlas::Cell c = a.locate(v);
			const Atlas::Quality q = a.quality(c);
			if (q.seams)
				return;
			checked++;
			UTEST(q.gravity < tilt, "up %.3f° off gravity, over %.3f°", q.gravity, tilt);
			UTEST(q.skew < 0.5, "sides %.3f° off square", q.skew);
			const double squash = p.rounding * 80 / p.radius + 0.01;
			UTEST(std::abs(q.east) < squash && std::abs(q.north) < squash && std::abs(q.up) < squash,
					"stretch %.4f %.4f %.4f, over %.4f", q.east, q.north, q.up, squash);
		}, p.radius - 64, p.radius + 16);
		UASSERT(checked > 1000);
	}
}

// Rounding trades: flat (0) keeps a block's width all the way down and tilts
// it at a segment's edge; round (1) tilts nothing and squashes it with depth.
void TestAtlas::testRoundingTrades()
{
	const double R = dev().radius;
	const auto measure = [&](double rounding, double depth, double lonOff) {
		Atlas::Params p = dev();
		p.rounding = rounding;
		const Atlas a(p);
		const double lon = (0.5 + lonOff) * TAU / p.segments; // segment 0's middle, and off it
		const Atlas::Cell c = a.owner(onPlanet(R - depth + 0.5, 10 * DEGREE, lon));
		return a.quality(c);
	};
	UASSERT(std::abs(measure(0, 200, 0).east) < 1e-6);
	UASSERT(measure(1, 200, 0).east < -0.14);              // 200 deep on a 1.3 km planet: 15% narrower
	UASSERT(measure(1, 0, 0.45).gravity < 0.01);
	UASSERT(measure(0, 0, 0.45).gravity > 5);               // near the edge of a 15° segment
	UASSERT(measure(0.95, 0, 0.45).gravity < 0.4);
}

// Where two regions meet, cells are cut (and only there).
void TestAtlas::testSeamsAreCut()
{
	const Atlas a(dev());
	const double R = a.params().radius, w = TAU / a.params().segments;
	int cut = 0, whole = 0;
	for (int n = 0; n < 400; n++) {
		const double lat = (n % 40) * 2 * DEGREE - 39 * DEGREE;
		const Atlas::Cell onSeam = a.locate(onPlanet(R - 5.5, lat, w + 1e-9 + (n / 40) * 1e-5)); // within a block
		cut += (a.quality(onSeam).seams & Atlas::SEGMENT) != 0;
		const Atlas::Cell middle = a.locate(onPlanet(R - 5.5, lat, w / 2));
		whole += a.retained(middle) == 1;
	}
	UTEST(cut == 400, "%d of 400 cells on a seam know it", cut);
	UASSERTEQ(int, whole, 400);
}

// Across any face, a neighbour lists the cell back across its own faces.
void TestAtlas::testNeighboursAgree()
{
	const Atlas a(dev());
	int seamFaces = 0, bad = 0;
	// on the seams, and anywhere
	const auto check = [&](const Vec &v) {
		const Atlas::Cell c = a.locate(v);
		for (int f = 0; f < 6; f++)
			for (const Atlas::Cell &n : a.neighbours(c, f)) {
				bool back = false;
				for (int g = 0; g < 6 && !back; g++) {
					const auto ns = a.neighbours(n, g);
					back = std::find(ns.begin(), ns.end(), c) != ns.end();
				}
				if (!back && bad++ < 3)
					rawstream << "cell (" << (int)c.region << " " << c.index << ": " << c.i << "," << c.j << "," << c.k << ", kept "
						<< a.retained(c) << ") lists (" << (int)n.region << " " << n.index << ": " << n.i << "," << n.j << "," << n.k
						<< ", kept " << a.retained(n) << ") across face " << f << ", which doesn't list it back" << std::endl;
				seamFaces += !(n.region == c.region && n.index == c.index);
			}
	};
	points(a.params(), 300, 9, check, 0.5, a.params().radius + 32, true);
	points(a.params(), 300, 10, check, 0.5, a.params().radius + 32);
	UTEST(bad == 0, "%d neighbours one way only", bad);
	UTEST(seamFaces > 20, "only %d faces over a seam", seamFaces);
}

// Digging straight down from the surface stays in one grid until the core:
// the cell below is the grid's own, one layer down.
void TestAtlas::testOneGridDown()
{
	const Atlas a(dev());
	const double R = a.params().radius;
	for (const double lat : { 0.0, 33.0, 60.0, 84.0, 87.0, -70.0 })
		for (const double lon : { 10.0, 44.0, 200.0 }) {
			Atlas::Cell c = a.locate(onPlanet(R + 0.5, lat * DEGREE, lon * DEGREE));
			if (a.retained(c) < 1)
				continue;
			for (int d = 0; d < 300; d++) {
				const std::vector<Atlas::Cell> below = a.neighbours(c, 4);
				UASSERT(below.size() == 1);
				Atlas::Cell plain = c;
				plain.k--;
				UASSERT(below[0] == plain);
				c = plain;
			}
		}
}

// Every grid's layer k is the sphere of radius R + k, out to its grid's
// edges, Earth's size included: grids meet level at every seam (a cap's
// circle is no ledge).
void TestAtlas::testLayersAreSpheres()
{
	for (const Atlas::Params &p : { dev(), kerbin(), Atlas::standard(6371000) }) {
		const Atlas a(p);
		for (const Atlas::Grid &g : a.grids()) {
			if (g.region == Region::Core)
				continue;
			for (int n = 0; n <= 64; n++)
				for (const int64_t k : { (int64_t)0, (int64_t)-40, (int64_t)100 }) {
					const auto along = [&](int axis) { return g.low[axis] + (g.high[axis] - g.low[axis]) * n / 64; };
					const int64_t i = g.region == Region::Segment ? along(0) : along(0) / 2;
					const int64_t j = g.region == Region::Segment ? 0 : along(1) / 2;
					const Atlas::Cell c{ g.region, g.index, i, j, k };
					const double off = length(a.at(c, 0, 0, 0.5)) - (p.radius + k + 1);
					UTEST(std::abs(off) < 1e-9 * p.radius, "radius %.0f: %s %d (%lld, %lld) layer %lld %.3g off its sphere",
							p.radius, g.region == Region::Segment ? "segment" : "cap", g.index, (long long)i, (long long)j,
							(long long)k, off);
				}
		}
	}
}
