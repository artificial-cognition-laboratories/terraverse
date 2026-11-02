// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "orbit/field.h"
#include "orbit/flight.h"
#include "planet/atlas.h"
#include "terragen/climate.h"
#include <cmath>

using namespace orbit;

class TestOrbit : public TestBase {
public:
	TestOrbit() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestOrbit"; }

	void runTests(IGameDef *gamedef);

	void testPlanetCard();
	void testCircularOrbitCloses();
	void testSpinHelpsEastward();
	void testTerminalSpeed();
	void testCannonball();
	void testWhereRoundTrip();
	void testFrameIsTheGround();
	void testFieldPullsToTheCentre();
	void testFieldFlat();
	void testOrbitAcrossGrids();
	void testPathIsTheOrbit();
};

static TestOrbit g_test_instance;

void TestOrbit::runTests(IGameDef *gamedef)
{
	TEST(testPlanetCard);
	TEST(testCircularOrbitCloses);
	TEST(testSpinHelpsEastward);
	TEST(testTerminalSpeed);
	TEST(testCannonball);
	TEST(testWhereRoundTrip);
	TEST(testFrameIsTheGround);
	TEST(testFieldPullsToTheCentre);
	TEST(testFieldFlat);
	TEST(testOrbitAcrossGrids);
	TEST(testPathIsTheOrbit);
}

static const Craft PERSON{ 100, 0.7 };

// at the equator, `altitude` up, moving east at `ground` m/s over the ground
static State east(const Flight &f, double altitude, double ground)
{
	return f.at({ 0, 0, altitude, { ground, 0, 0 } });
}

void TestOrbit::testPlanetCard()
{
	terragen::Planet p = terragen::home();
	// the weather and climate run on it at Earth's size: exactly Earth's day
	terragen::Planet e = terragen::resized(p, terragen::EARTH_RADIUS);
	UASSERT(e.day == 86400 && e.radius == terragen::EARTH_RADIUS && e.gravity == p.gravity);
	// its equator moves as Earth's does (~463 m/s)
	UASSERT(std::fabs(p.spin() * p.radius - 463.3) < 1);
	// low orbit is a few km/s
	double v = std::sqrt(p.mu() / (p.radius + 150e3));
	UASSERT(v > 2000 && v < 2500);
}

void TestOrbit::testCircularOrbitCloses()
{
	Flight f(terragen::home());
	const terragen::Planet &p = f.planet();
	double r = p.radius + 200e3, v = std::sqrt(p.mu() / r);
	// circular against the stars: over the ground, less the equator's motion there
	State s = east(f, 200e3, v - p.spin() * r);
	Elements e = f.elements(s);
	UASSERT(e.bound && std::fabs(e.apoapsis - 200e3) < 100 && std::fabs(e.periapsis - 200e3) < 100);
	UASSERT(e.inclination < 1e-6);
	// one period later it's back where it started against the stars; the
	// ground has turned under it
	double T = e.period;
	State later = f.step(s, PERSON, {}, T);
	double turned = p.spin() * T, ca = std::cos(turned), sa = std::sin(turned);
	Vec back{ ca * later.pos.x + sa * later.pos.z, later.pos.y, -sa * later.pos.x + ca * later.pos.z };
	UASSERT((back - s.pos).length() < 2000); // under 2 km after ~1/2 hour, with a whisper of air
	UASSERT(std::fabs(later.pos.length() - r) < 1000);
}

void TestOrbit::testSpinHelpsEastward()
{
	Flight f(terragen::home());
	const terragen::Planet &p = f.planet();
	double r = p.radius + 200e3, v = std::sqrt(p.mu() / r);
	State eastward = east(f, 200e3, v - p.spin() * r), westward = east(f, 200e3, -(v + p.spin() * r));
	UASSERT(std::fabs(f.elements(eastward).periapsis - f.elements(westward).periapsis) < 100);
	UASSERT(-westward.vel.x - eastward.vel.x > 2 * p.spin() * r - 1); // west costs ~930 m/s more
}

void TestOrbit::testTerminalSpeed()
{
	Flight f(terragen::home());
	// dropped near the ground: it settles at √(2mg / ρ·CdA)
	State s = f.near({ { 0, 3000, 0 }, {} }, PERSON, {}, 60);
	double expect = std::sqrt(2 * PERSON.mass * f.planet().gravity / (f.density(s.pos.y) * PERSON.drag_area));
	UASSERT(std::fabs(-s.vel.y - expect) < 0.02 * expect);
	// pushed sideways it keeps nothing of a curve: x moves straight
	State side = f.near({ { 0, 100, 0 }, { 50, 0, 0 } }, PERSON, {}, 1);
	UASSERT(side.vel.x < 50 && side.vel.x > 40 && side.pos.z == 0);
}

void TestOrbit::testCannonball()
{
	Flight f(terragen::home());
	const terragen::Planet &p = f.planet();
	// a cannonball from a mountain top comes down, not far
	Craft ball{ 10, 0.01 };
	Arc low = f.coast(east(f, 2000, 300), ball, 3600, 1);
	UASSERT(low.end == Arc::End::Ground && low.seconds < 120);
	// fast enough above the air, it goes all the way round
	double r = p.radius + 150e3;
	Arc high = f.coast(east(f, 150e3, std::sqrt(p.mu() / r) - p.spin() * r), ball, 7200, 5);
	UASSERT(high.end == Arc::End::Around);
	// the same speed down in the thick air never makes it
	Arc thick = f.coast(east(f, 2000, std::sqrt(p.mu() / r) - p.spin() * r), ball, 7200, 1);
	UASSERT(thick.end == Arc::End::Ground);
}

void TestOrbit::testWhereRoundTrip()
{
	Flight f(terragen::home());
	Where w{ 0.6, -2.1, 12345, { 10, -3, 7 } };
	Where b = f.where(f.at(w));
	UASSERT(std::fabs(b.latitude - w.latitude) < 1e-9 && std::fabs(b.longitude - w.longitude) < 1e-9);
	UASSERT(std::fabs(b.altitude - w.altitude) < 1e-6);
	UASSERT((b.velocity - w.velocity).length() < 1e-9);
}

// The frame over a place reads a velocity as `where` does (east, up, north),
// and a step along its north is a step north on the planet.
void TestOrbit::testFrameIsTheGround()
{
	Flight f(terragen::home());
	Where w{ 0.6, -2.1, 30000, { 10, -3, 7 } };
	State s = f.at(w);
	Frame fr = f.frame(s);
	UASSERT((fr.in(s.vel) - w.velocity).length() < 1e-9);
	UASSERT((fr.in(fr.out({ 1, 2, 3 })) - Vec{ 1, 2, 3 }).length() < 1e-12);
	Where north = f.where({ s.pos + fr.north * 1000, s.vel });
	UASSERT(north.latitude > w.latitude && std::fabs(north.longitude - w.longitude) < 1e-9);
	bool threw = false;
	try { f.frame({ { 0, f.planet().radius, 0 }, {} }); } catch (const std::invalid_argument &) { threw = true; }
	UASSERT(threw);
}

// Sprint 59: gravity as a field in the atlas's grids.

static const terragen::Atlas::Params SMALL = terragen::Atlas::standard(8192 / (2 * M_PI));

// The node storing the place `altitude` above the surface over `dir` (a unit vector).
static v3d above(const planet::Atlas &atlas, Vec dir, double altitude)
{
	const double r = SMALL.radius + altitude;
	const terragen::Atlas::Cell c = atlas.body(0).locate({ dir.x * r, dir.y * r, dir.z * r });
	const planet::Atlas::Node n = atlas.node(planet::Atlas::Cell(0, c));
	return v3d(n.x, n.y, n.z);
}

static double radiusAt(const planet::Atlas &atlas, const v3d &p)
{
	const auto at = atlas.point(planet::Atlas::moverNode(p.X, p.Y, p.Z), p.X, p.Y, p.Z);
	UASSERT(at);
	return std::sqrt(at->x * at->x + at->y * at->y + at->z * at->z);
}

// Down is the grid's down, as strong as the planet pulls at that height.
void TestOrbit::testFieldPullsToTheCentre()
{
	const planet::Atlas atlas = planet::Atlas::resident(SMALL);
	const Field field(atlas, { terragen::sphere(SMALL.radius) });
	const double mu = terragen::sphere(SMALL.radius).mu();
	for (Vec dir : { Vec{ 1, 0, 0 }, Vec{ 0.6, 0.48, 0.64 }, Vec{ 0, 1, 0 }, Vec{ 0.3, -0.9, 0.316 } }) {
		const double len = dir.length();
		dir = dir * (1 / len);
		for (double h : { 1.0, 200.0 }) {
			const v3d p = above(atlas, dir, h);
			const std::optional<Field::Here> here = field.at(p, v3f(0));
			UASSERT(here);
			const double r = radiusAt(atlas, p), g = mu / (r * r);
			// within the turning frame's centrifugal share (a fraction of a percent)
			UASSERT(std::fabs(here->gravity.getLength() - g) < 0.01 * g);
			UASSERT(-here->gravity.Y > 0.99f * here->gravity.getLength());
			UASSERT(here->air > 0 && here->air < 1.23f);
		}
	}
	// weaker up high
	const v3d low = above(atlas, { 1, 0, 0 }, 1), high = above(atlas, { 1, 0, 0 }, 200);
	UASSERT(field.at(high, v3f(0))->gravity.getLength() < 0.9f * field.at(low, v3f(0))->gravity.getLength());
	// above the atlas's top (flown past the sky): charted from the column under it
	const v3d top = above(atlas, { 1, 0, 0 }, SMALL.height - 1), past = top + v3d(0, 300, 0);
	UASSERT(!atlas.cell(planet::Atlas::moverNode(past.X, past.Y, past.Z)));
	const std::optional<Field::Here> sky = field.at(past, v3f(0));
	UASSERT(sky);
	const double r = SMALL.radius + SMALL.height + 300;
	UASSERT(std::fabs(sky->gravity.getLength() - mu / (r * r)) < 0.02 * mu / (r * r));
}

// No atlas: the home card's pull straight down, its air at y = 0.
void TestOrbit::testFieldFlat()
{
	const Field field(terragen::home());
	const Field::Here h = *field.at(v3d(10, 0, -20), v3f(0));
	UASSERT(std::fabs(h.gravity.Y + 9.81f) < 0.05f && std::fabs(h.gravity.X) < 0.01f && std::fabs(h.gravity.Z) < 0.01f);
	UASSERT(std::fabs(h.air - 1.225f) < 1e-3f);
	UASSERT(field.carry(v3d(0, 0, 0), v3d(500, 3, 0), v3f(1, 2, 3)) == v3f(1, 2, 3));
}

// Thrown sideways at orbital speed in a grid's terms, it falls around the
// planet: its height holds for half a lap, over the pole, across seams.
// (Without carrying the velocity it would ride the grid's straight line and
// come down; turned across a seam only about up, it's kicked off its orbit.)
void TestOrbit::testOrbitAcrossGrids()
{
	const planet::Atlas atlas = planet::Atlas::resident(SMALL);
	const Field field(atlas, { terragen::sphere(SMALL.radius) });
	const terragen::Planet planet = terragen::sphere(SMALL.radius);
	Vec dir{ 0.05, 1, 0.02 };
	dir = dir * (1 / dir.length());
	v3d pos = above(atlas, dir, 200);
	const planet::Atlas::Node n = planet::Atlas::moverNode(pos.X, pos.Y, pos.Z);
	const planet::Atlas::Frame f = *atlas.frame(n, { pos.X, pos.Y, pos.Z });
	const Vec at{ f.origin.x, f.origin.y, f.origin.z };
	const double r0 = at.length();
	const Vec out = at * (1 / r0);
	Vec ax{ f.axes[0].x, f.axes[0].y, f.axes[0].z };
	Vec along = ax - out * ax.dot(out);
	along = along * (1 / along.length());
	// circular against the stars, less the ground's motion there
	const Vec spin{ 0, planet.spin(), 0 };
	const Vec v = along * std::sqrt(planet.mu() / r0) - spin.cross(at);
	// into the grid's axes
	Vec A[3];
	for (int i = 0; i < 3; i++)
		A[i] = { f.axes[i].x, f.axes[i].y, f.axes[i].z };
	const Vec c0 = A[1].cross(A[2]), c1 = A[2].cross(A[0]), c2 = A[0].cross(A[1]);
	const double det = A[0].dot(c0);
	v3f vel((f32)(c0.dot(v) / det), (f32)(c1.dot(v) / det), (f32)(c2.dot(v) / det));

	const Elements e = *field.elements(pos, vel);
	UASSERT(e.bound && std::fabs(e.apoapsis - e.periapsis) < 2);

	const double dt = 1.0 / 60;
	int crossed = 0;
	double worst = 0;
	for (double t = 0; t < e.period / 2; t += dt) {
		vel += field.at(pos, vel)->gravity * (f32)dt;
		// moved by its velocity as it is halfway (the grid's lines curve)
		const v3f mid = field.carry(pos, pos + v3d::from(vel) * (dt / 2), vel);
		const v3d next = pos + v3d::from(mid) * dt;
		if (const auto c = atlas.carry(pos.X, pos.Y, pos.Z, next.X, next.Y, next.Z)) {
			// into the next grid: the same motion in its axes
			const v3d there(c->x, c->y, c->z);
			vel = field.carry(pos, there, vel);
			pos = there;
			crossed++;
		} else {
			vel = field.carry(pos, next, vel);
			pos = next;
		}
		worst = std::max(worst, std::fabs(radiusAt(atlas, pos) - r0));
	}
	UASSERT(crossed >= 2);
	UASSERT(worst < 2); // metres, of 200 up, over ~45 s and half the planet
}

// The path drawn is the orbit the elements describe: it starts where the
// craft is, its lowest and highest points are the periapsis and apoapsis,
// and an escape runs out to the reach and stops.
void TestOrbit::testPathIsTheOrbit()
{
	const terragen::Planet p = terragen::sphere(2607.6);
	const Flight f(p);
	const State s = east(f, 300, 150); // a little fast: an ellipse up from here
	const Elements e = f.elements(s);
	UASSERT(e.bound);
	const std::vector<Vec> path = f.path(s, 512, 5 * p.radius);
	UASSERT(path.size() == 512);
	UASSERT((path.front() - s.pos).length() < 0.01);
	double low = 1e30, high = 0;
	for (const Vec &v : path) {
		low = std::min(low, v.length() - p.radius);
		high = std::max(high, v.length() - p.radius);
	}
	UASSERT(std::fabs(low - e.periapsis) < 1 && std::fabs(high - e.apoapsis) < 1);

	const State fast = east(f, 300, 400); // past escape
	UASSERT(!f.elements(fast).bound);
	const std::vector<Vec> out = f.path(fast, 64, 5 * p.radius);
	UASSERT((out.front() - fast.pos).length() < 0.01);
	UASSERT(std::fabs(out.back().length() - 5 * p.radius) < 1);
}
