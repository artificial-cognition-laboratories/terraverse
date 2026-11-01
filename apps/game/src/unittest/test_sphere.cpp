// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "terragen/sphere.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

using namespace terragen;

class TestSphere : public TestBase {
public:
	TestSphere() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSphere"; }

	void runTests(IGameDef *gamedef);

	void testCellsAndRings();
	void testAreasEven();
	void testNearestCell();
	void testSpotContinuous();
	void testSameEveryTime();
	void testPlacesToTheBlock();
};

static TestSphere g_test_instance;

void TestSphere::runTests(IGameDef *gamedef)
{
	TEST(testCellsAndRings);
	TEST(testAreasEven);
	TEST(testNearestCell);
	TEST(testSpotContinuous);
	TEST(testSameEveryTime);
	TEST(testPlacesToTheBlock);
}

static const Sphere &sphere()
{
	static const Sphere s(128);
	return s;
}

static int64_t dotOf(const Dir &a, const Dir &b)
{
	return (int64_t)a.x * b.x + (int64_t)a.y * b.y + (int64_t)a.z * b.z;
}

// A spread of directions, from a fixed hash: any machine, the same ones.
static Dir direction(uint32_t k)
{
	auto next = [&] { k = k * 1664525u + 1013904223u; return (int64_t)(k >> 8) - (1 << 23); };
	int64_t x = next(), y = next(), z = next();
	const int64_t length = (int64_t)isqrt((uint64_t)(x * x + y * y + z * z));
	return { (fix)(x * ONE / length), (fix)(y * ONE / length), (fix)(z * ONE / length) };
}

void TestSphere::testCellsAndRings()
{
	const Sphere &s = sphere();
	UASSERTEQ(int32_t, s.size(), 10 * 128 * 128 + 2);
	int32_t pentagons = 0;
	for (int32_t c = 0; c < s.size(); c++) {
		const int32_t count = s.neighbourCount(c);
		UASSERT(count == 5 || count == 6);
		pentagons += count == 5;
		// unit, give or take the fixed point
		const int64_t length = dotOf(s.centre(c), s.centre(c));
		UASSERT(std::abs(length - (int64_t)ONE * ONE) < (int64_t)ONE * 8);
		// neighbours both ways, each once
		std::set<int32_t> mine(s.neighboursBegin(c), s.neighboursEnd(c));
		UASSERTEQ(size_t, mine.size(), (size_t)count);
		for (int32_t n : mine) {
			UASSERT(n != c);
			UASSERT(std::find(s.neighboursBegin(n), s.neighboursEnd(n), c) != s.neighboursEnd(n));
		}
		// in turn round it: each next neighbour is also a neighbour of the last
		for (int32_t k = 0; k < count; k++) {
			const int32_t a = s.neighboursBegin(c)[k], b = s.neighboursBegin(c)[(k + 1) % count];
			UASSERT(std::find(s.neighboursBegin(a), s.neighboursEnd(a), b) != s.neighboursEnd(a));
		}
	}
	UASSERTEQ(int32_t, pentagons, 12);
}

void TestSphere::testAreasEven()
{
	// A plain (linearly cut) icosphere's cells run from about half the mean
	// at the pentagons to a fifth over it mid-face: what sums over area
	// weighs by area(), so this is resolution, not a bias. The mean is the mean.
	const Sphere &s = sphere();
	int64_t sum = 0;
	fix lo = ONE * 4, hi = 0;
	for (int32_t c = 0; c < s.size(); c++) {
		sum += s.area(c);
		lo = std::min(lo, s.area(c));
		hi = std::max(hi, s.area(c));
	}
	UASSERT(std::abs(sum / s.size() - ONE) < ONE / 256);
	UASSERT(lo > ONE * 45 / 100);
	UASSERT(hi < ONE * 13 / 10);
}

void TestSphere::testNearestCell()
{
	const Sphere &s = sphere();
	// each centre is its own cell
	for (int32_t c = 0; c < s.size(); c += 7)
		UASSERTEQ(int32_t, s.cell(s.centre(c)), c);
	// anywhere, the nearest centre, as a search of them all finds it
	for (uint32_t k = 0; k < 64; k++) {
		const Dir d = direction(k);
		int32_t best = 0;
		for (int32_t c = 1; c < s.size(); c++)
			if (dotOf(d, s.centre(c)) > dotOf(d, s.centre(best)))
				best = c;
		const int32_t found = s.cell(d);
		UASSERT(found == best || dotOf(d, s.centre(found)) == dotOf(d, s.centre(best)));
	}
	// the poles are places like any other
	for (fix y : { ONE, -ONE }) {
		const Dir pole{ 0, y, 0 };
		const int32_t c = s.cell(pole);
		UASSERT(dotOf(pole, s.centre(c)) > (int64_t)ONE * ONE * 99 / 100);
	}
}

void TestSphere::testSpotContinuous()
{
	const Sphere &s = sphere();
	for (uint32_t k = 0; k < 2000; k++) {
		const Dir d = direction(k + 100);
		const Sphere::Spot spot = s.spot(d);
		fix sum = 0;
		for (int i = 0; i < 3; i++) {
			UASSERT(spot.weights[i] >= 0);
			sum += spot.weights[i];
			// the three are close by: within two cells' spacing
			UASSERT(dotOf(d, s.centre(spot.cells[i])) > (int64_t)ONE * ONE * 9990 / 10000);
		}
		UASSERTEQ(fix, sum, ONE);
	}
	// a field read by the spot changes little between near directions,
	// on faces and across their edges alike
	std::vector<fix> field(s.size());
	for (int32_t c = 0; c < s.size(); c++)
		field[c] = s.centre(c).x + 2 * s.centre(c).y - s.centre(c).z;
	auto read = [&](const Dir &d) {
		const Sphere::Spot spot = s.spot(d);
		int64_t v = 0;
		for (int i = 0; i < 3; i++)
			v += (int64_t)field[spot.cells[i]] * spot.weights[i];
		return (fix)(v >> FRAC);
	};
	for (uint32_t k = 0; k < 2000; k++) {
		const Dir a = direction(k + 5000);
		const Dir b{ a.x + 7, a.y - 5, a.z + 3 }; // a hundred-thousandth of the way round
		UASSERT(std::abs(read(a) - read(b)) < ONE / 512);
	}
}

void TestSphere::testSameEveryTime()
{
	// The mesh is part of what a world code means: pinned, as the golden is.
	const Sphere &s = sphere();
	uint64_t h = 1469598103934665603ull;
	auto mix = [&](int64_t v) { h = (h ^ (uint64_t)v) * 1099511628211ull; };
	for (int32_t c = 0; c < s.size(); c++) {
		mix(s.centre(c).x);
		mix(s.centre(c).y);
		mix(s.centre(c).z);
		mix(s.area(c));
		for (const int32_t *n = s.neighboursBegin(c); n != s.neighboursEnd(c); n++)
			mix(*n);
	}
	UASSERTEQ(uint64_t, h, 0xd305a57276ea7c0cull);
}

// A place tells a column from its neighbour at every size, up to Earth's:
// 2 000 blocks along the equator are 2 000 places (a Dir alone gives 26 at
// Earth's size). Made as the atlas makes them, from the point.
void TestSphere::testPlacesToTheBlock()
{
	for (double R : { 2607.0, 50000.0, 250000.0, 6371000.0 }) {
		std::set<std::tuple<int32_t, int32_t, int32_t>> seen;
		for (int k = 0; k < 2000; k++) {
			const double t = 0.3 + k / R;
			const Place p = placeOf(std::llround(std::cos(t) * R * 1024), 0, std::llround(std::sin(t) * R * 1024));
			seen.insert({ p.x, p.y, p.z });
		}
		UASSERTEQ(size_t, seen.size(), (size_t)2000);
	}
	// and a Dir is a place exactly, and back
	const Dir d = dirAt(ratio(3, 10), ratio(7, 10));
	const Dir back = Place(d).dir();
	UASSERT(back.x == d.x && back.y == d.y && back.z == d.z);
}
