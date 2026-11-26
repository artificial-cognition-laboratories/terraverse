// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "src/shared/base/constants.h"
#include "src/client/views/planetarium/camera.h"
#include "src/client/views/planetarium/framing.h"
#include "src/client/views/planetarium/overlays/paint.h"
#include "library/world/terragen/src/code/worlds.h"
#include "library/world/terragen/src/code/code.h"
#include <chrono>
#include <set>
#include <algorithm>
#include <cmath>

using planetarium::OrbitCamera;
using planetarium::Framing;
using planetarium::Shot;
namespace framing = planetarium::framing;

/*
	The Planetarium's camera (planetarium/camera.h): in orbit round a body,
	looking at its centre, its tiles' eye under it.
*/
class TestPlanetarium : public TestBase {
public:
	TestPlanetarium() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetarium"; }

	void runTests(IGameDef *gamedef);

	void testZoomStaysOverTheBody();
	void testDragSlowsNearTheGround();
	void testLooksAtTheCentre();
	void testRayThroughTheScreen();
	void testSpinTurnsTheBody();
	void testDragOverThePole();
	void testCentreIsAhead();
	void testAimDrawsTheCentreThere();
	void testShotLandsWhereItSays();
	void testOverlaySharpUpClose();
	void testOverlaysOnEveryBody();
	void testOverlayKeepsWater();
};

static TestPlanetarium g_test_instance;

void TestPlanetarium::runTests(IGameDef *gamedef)
{
	TEST(testZoomStaysOverTheBody);
	TEST(testDragSlowsNearTheGround);
	TEST(testLooksAtTheCentre);
	TEST(testRayThroughTheScreen);
	TEST(testSpinTurnsTheBody);
	TEST(testDragOverThePole);
	TEST(testCentreIsAhead);
	TEST(testAimDrawsTheCentreThere);
	TEST(testShotLandsWhereItSays);
	TEST(testOverlaySharpUpClose);
	TEST(testOverlaysOnEveryBody);
	TEST(testOverlayKeepsWater);
}

static constexpr double SEA = 1305;

static double length(const OrbitCamera::Vec &v)
{
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// However far it's zoomed, it stays between 100 blocks over the sea and a
// few radii out; the tiles' eye is at the sea under it.
void TestPlanetarium::testZoomStaysOverTheBody()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.zoom(1000);
	UASSERT(std::fabs(c.altitude() - 100) < 1e-9);
	c.zoom(-1000);
	UASSERT(c.altitude() <= 6 * SEA + 1e-9 && c.altitude() > 5 * SEA);
	const far::Eye eye = OrbitCamera::eyeAt(c.position(), SEA, OrbitCamera::focal(1080, OrbitCamera::fovFor(1920, 1080)));
	UASSERT(std::fabs(length(eye.at) - SEA) < 1e-6);
	const OrbitCamera::Vec p = c.position();
	const double d = length(p);
	UASSERT(std::fabs(d - SEA - c.altitude()) < 1e-6);
	UASSERT(std::fabs(eye.at.x / SEA - p.x / d) < 1e-9 && std::fabs(eye.at.y / SEA - p.y / d) < 1e-9);
	UASSERT(eye.range > 0 && eye.range <= 2 * SEA + 1e-3 && eye.cut == 0);
	// it reaches its horizon: the chord to where its sight line grazes the sea
	const double horizon = std::acos(SEA / (SEA + c.altitude()));
	UASSERT(eye.range >= 2 * SEA * std::sin(horizon / 2));
	// close up it sees less of the body, and tiles split further off (in their spans)
	c.zoom(1000);
	const far::Eye low = OrbitCamera::eyeAt(c.position(), SEA, OrbitCamera::focal(1080, OrbitCamera::fovFor(1920, 1080)));
	UASSERT(low.range < eye.range / 3);
	UASSERT(std::fabs(low.split - eye.split) < 1e-6 && OrbitCamera::eyeAt(c.position(), SEA, OrbitCamera::focal(2160, OrbitCamera::fovFor(1920, 1080))).split > 1.9 * low.split);
}

// A pixel's drag moves the camera round less the lower it is.
void TestPlanetarium::testDragSlowsNearTheGround()
{
	const auto moved = [](double steps) {
		OrbitCamera c(SEA, 6 * SEA);
		c.zoom(steps);
		const OrbitCamera::Vec a = c.position();
		c.drag(10, 0);
		const OrbitCamera::Vec b = c.position();
		const double angle = std::acos(std::clamp((a.x * b.x + a.y * b.y + a.z * b.z) / (length(a) * length(b)), -1.0, 1.0));
		return angle;
	};
	UASSERT(moved(30) < moved(0) / 5);
}

// The body's centre is drawn in the middle of the screen, in front of the camera.
void TestPlanetarium::testLooksAtTheCentre()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.drag(123, -45);
	const OrbitCamera::Vec p = c.position();
	core::matrix4 m = c.projection(16.0f / 9, OrbitCamera::fovFor(1920, 1080));
	m *= c.view();
	const v3f centre = v3f((float)-p.x, (float)-p.y, (float)-p.z) * BS; // the centre, from the camera
	f32 clip[4] = { centre.X, centre.Y, centre.Z, 1 };
	m.multiplyWith1x4Matrix(clip);
	UASSERT(clip[3] > 0);
	UASSERT(std::fabs(clip[0] / clip[3]) < 1e-3 && std::fabs(clip[1] / clip[3]) < 1e-3);
	UASSERT(clip[2] / clip[3] > -1 && clip[2] / clip[3] < 1);
}

// Through the screen's middle the ray is straight at the centre; right of it,
// it leans east (the screen's right, with north up); focus puts the camera
// over the place asked.
void TestPlanetarium::testRayThroughTheScreen()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(0.3, 1.1, 2000.0);
	UASSERT(std::fabs(c.altitude() - 2000) < 1e-9);
	const OrbitCamera::Vec p = c.position();
	const double d = length(p);
	UASSERT(std::fabs(std::asin(p.y / d) - 0.3) < 1e-9 && std::fabs(std::atan2(p.z, p.x) - 1.1) < 1e-9);
	const OrbitCamera::Vec mid = OrbitCamera::ray(c.view(), 960, 540, 1920, 1080, OrbitCamera::fovFor(1920, 1080));
	UASSERT(std::fabs((mid.x * p.x + mid.y * p.y + mid.z * p.z) / d + 1) < 1e-5); // −up
	const OrbitCamera::Vec right = OrbitCamera::ray(c.view(), 1500, 540, 1920, 1080, OrbitCamera::fovFor(1920, 1080));
	const OrbitCamera::Vec east{ -std::sin(1.1), 0, std::cos(1.1) };
	UASSERT(right.x * east.x + right.y * east.y + right.z * east.z > 0.1);
	const OrbitCamera::Vec top = OrbitCamera::ray(c.view(), 960, 100, 1920, 1080, OrbitCamera::fovFor(1920, 1080));
	const OrbitCamera::Vec north{ -std::sin(0.3) * std::cos(1.1), std::cos(0.3), -std::sin(0.3) * std::sin(1.1) };
	UASSERT(top.x * north.x + top.y * north.y + top.z * north.z > 0.1);
}

// A spun body: its ground's longitude grows by the spin, and the view that
// draws it turned puts its points where turn() says.
void TestPlanetarium::testSpinTurnsTheBody()
{
	const OrbitCamera::Vec east = OrbitCamera::turn({ 1, 0, 0 }, M_PI / 2);
	UASSERT(std::fabs(east.z - 1) < 1e-12 && std::fabs(east.x) < 1e-12);
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(0.2, -0.7, 3000.0);
	const core::matrix4 view = c.view(), spun = OrbitCamera::turned(view, 0.9);
	const OrbitCamera::Vec p{ 300, -120, 800 }, q = OrbitCamera::turn(p, 0.9);
	v3f a((f32)p.x, (f32)p.y, (f32)p.z), b((f32)q.x, (f32)q.y, (f32)q.z);
	spun.transformVect(a);
	view.transformVect(b);
	UASSERT((a - b).getLength() < 1e-2f);
}

// Dragged north, it goes over the pole and down the other side, still
// looking at the centre; no latitude holds it back.
void TestPlanetarium::testDragOverThePole()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(1.2, 0.5);
	const double k = 0.004; // TURN at this height (a radius or more up)
	c.drag(0, (M_PI / 2 - 1.2 + 0.4) / k); // past the pole by 0.4 rad
	UASSERT(std::fabs(c.latitude() - (M_PI / 2 - 0.4)) < 1e-6);
	UASSERT(std::fabs(std::remainder(c.longitude() - (0.5 + M_PI), 2 * M_PI)) < 1e-6);
	const OrbitCamera::Vec p = c.position();
	UASSERT(std::fabs(length(p) - (SEA + c.altitude())) < 1e-6);
}

// The view takes a point from the camera into its own axes (as the hover
// test projects bodies): the body's centre straight ahead, its north up.
void TestPlanetarium::testCentreIsAhead()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(0.3, 1.1);
	const OrbitCamera::Vec p = c.position();
	core::vector3df centre((f32)-p.x, (f32)-p.y, (f32)-p.z);
	c.view().transformVect(centre);
	UASSERT(std::fabs(centre.X) < 1e-2 && std::fabs(centre.Y) < 1e-2);
	UASSERT(std::fabs(centre.Z - length(p)) < 1e-2 * length(p));
	// a point north of the centre is above it on the screen
	core::vector3df north((f32)-p.x, (f32)(-p.y + SEA), (f32)-p.z);
	c.view().transformVect(north);
	UASSERT(north.Y > 0);
}

// Across one pixel of the page's 256² painting (what the globe was tinted
// by), the native overlay keeps changing: as sharp as the tiles it's on.
// And its cost per vertex, for the tile budget.
// Where a view sees a point on screen (−1..1, +y up), through the one
// symmetric lens; the point given from the camera
static v2f onScreen(const core::matrix4 &view, core::vector3df p, float aspect)
{
	p = view.rotateAndScaleVect(p);
	const float t = std::tan(OrbitCamera::fovFor(1080 * aspect, 1080) / 2);
	return v2f(p.X / (p.Z * t * aspect), p.Y / (p.Z * t));
}

// Aimed, what's straight ahead is drawn where it's aimed, not the middle;
// and the pixel there rays straight back at it.
void TestPlanetarium::testAimDrawsTheCentreThere()
{
	const float aspect = 16.f / 9;
	for (const v2f at : { v2f(0, 0), v2f(-0.85f, -0.75f), v2f(0.4f, 0.2f), v2f(0.9f, -0.9f) }) {
		const core::matrix4 aim = OrbitCamera::aimed(at, aspect, OrbitCamera::fovFor(1080 * aspect, 1080));
		const v2f seen = onScreen(aim, core::vector3df(0, 0, 1), aspect);
		UASSERT(std::fabs(seen.X - at.X) < 1e-4f && std::fabs(seen.Y - at.Y) < 1e-4f);
		// a turn, nothing else: lengths kept
		core::vector3df x(1, 0, 0);
		x = aim.rotateAndScaleVect(x);
		UASSERT(std::fabs(x.getLength() - 1) < 1e-5f);
		const double px = (at.X + 1) / 2 * 1920, py = (1 - at.Y) / 2 * 1080;
		const OrbitCamera::Vec back = OrbitCamera::ray(aim, px, py, 1920, 1080, OrbitCamera::fovFor(1080 * aspect, 1080));
		UASSERT(std::fabs(back.z - 1) < 1e-4);
	}
}

// A shot composed: the body's centre and the star are drawn where the shot
// says, on any screen shape (the camera's only placed and turned).
void TestPlanetarium::testShotLandsWhereItSays()
{
	const Shot shot{ "", 0, 3.35, { -0.85f, -0.75f }, { 0.88f, 0.82f }, 1.9, 0.5f };
	const OrbitCamera::Vec sun = [] { const double l = std::sqrt(0.36 + 0.01 + 0.64); return OrbitCamera::Vec{ 0.6 / l, 0.1 / l, -0.8 / l }; }();
	for (const float aspect : { 16.f / 9, 4.f / 3, 21.f / 9 }) {
		const Framing f = framing::of(shot, 1080 * aspect, 1080);
		const framing::Place at = framing::place(f, sun);
		OrbitCamera c(SEA, 6 * SEA);
		c.place(at.up, at.north, f.altitude * SEA);
		const core::matrix4 view = OrbitCamera::aimed(f.centre, aspect, OrbitCamera::fovFor(1080 * aspect, 1080)) * c.view();
		const OrbitCamera::Vec p = c.position();
		const v2f centre = onScreen(view, core::vector3df((f32)-p.x, (f32)-p.y, (f32)-p.z), aspect);
		const v2f star = onScreen(view, core::vector3df((f32)sun.x, (f32)sun.y, (f32)sun.z), aspect);
		UASSERT(std::fabs(centre.X - shot.centre.X) < 1e-3f && std::fabs(centre.Y - shot.centre.Y) < 1e-3f);
		UASSERT(std::fabs(star.X - shot.star.X) < 1e-3f && std::fabs(star.Y - shot.star.Y) < 1e-3f);
		// and read back, it's the framing it was placed by
		const Framing back = framing::read(c, sun, SEA, f.centre, f.sky);
		UASSERT(std::fabs(back.off - f.off) < 1e-6 && std::fabs(std::remainder(back.roll - f.roll, 2 * M_PI)) < 1e-6);
		UASSERT(std::fabs(std::remainder(back.around - f.around, 2 * M_PI)) < 1e-6);
	}
}

void TestPlanetarium::testOverlaySharpUpClose()
{
	const auto world = std::make_shared<const terragen::World>(terragen::WorldCode::decode("TF24.dGVycmE"));
	planetarium::Painter painter({ world, planetarium::Overlay::Elevation, nullptr, std::nullopt, std::nullopt, 0 });
	const video::SColor land(255, 200, 200, 200);
	// a few places, the best of them: one may lie on an even slope or a
	// flat sea floor, where there's little to show at any sharpness
	const double pixel = 2 * M_PI / 256;
	size_t most = 0;
	for (const double lat : { 0.3, -0.5, 0.9 })
		for (const double from : { 0.1, 2.0, -2.5 }) {
			std::set<u32> seen;
			for (int i = 0; i < 64; i++) {
				const double lon = from + pixel * i / 64;
				const far::Tiling::Vec at{ std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
				seen.insert(painter.colour(land, at).color);
			}
			most = std::max(most, seen.size());
		}
	infostream << "TestPlanetarium: " << most << " colours across a pixel" << std::endl;
	UASSERT(most > 4);

	const auto start = std::chrono::steady_clock::now();
	const int n = 20000;
	for (int i = 0; i < n; i++) {
		const double lon = 2 * M_PI * i / n, la = 1.2 * std::sin(i * 0.37);
		painter.colour(land, { std::cos(la) * std::cos(lon), std::sin(la), std::cos(la) * std::sin(lon) });
	}
	const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / n;
	infostream << "TestPlanetarium: an overlay's fresh sample takes " << us << " µs" << std::endl;
	UASSERT(us < 50); // a 2k-vertex tile in well under the frame's paint budget twice over
}

// A liquid's top on land (a lake, a river) stays the land's water under the
// water overlays; the climate ones paint over it.
// Every overlay paints every kind of body: a sea-less lid, ice, a dead world.
void TestPlanetarium::testOverlaysOnEveryBody()
{
	const terragen::WorldCode home = terragen::WorldCode::decode("TF24.dGVycmE");
	const auto worlds = terragen::bodyWorlds(home, terragen::Atlas::standard(250000));
	UASSERT(worlds.size() > 4);
	using O = planetarium::Overlay;
	for (const terragen::BodyWorld &b : worlds) {
		const auto world = std::make_shared<const terragen::World>(b.code);
		for (const O o : { O::Biomes, O::Elevation, O::Temperature, O::Moisture, O::Seasons, O::Fertility, O::Currents, O::Plates, O::Geology }) {
			planetarium::Painter painter({ world, o, nullptr, std::nullopt, std::nullopt, 0 });
			for (int i = 0; i < 32; i++) {
				const double lon = 2 * M_PI * i / 32, la = 1.4 * std::sin(i * 0.7);
				painter.colour(video::SColor(255, 200, 200, 200), { std::cos(la) * std::cos(lon), std::sin(la), std::cos(la) * std::sin(lon) });
			}
		}
	}
}

void TestPlanetarium::testOverlayKeepsWater()
{
	const auto world = std::make_shared<const terragen::World>(terragen::WorldCode::decode("TF24.dGVycmE"));
	const video::SColor river(0, 40, 120, 215);
	// a place on land
	for (int i = 0; i < 4096; i++) {
		const double lon = 2 * M_PI * i / 4096, lat = 0.4 * std::sin(i * 0.11);
		const far::Tiling::Vec at{ std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
		const terragen::fix u = (terragen::fix)((lon / (2 * M_PI) + 0.5) * terragen::ONE), v = (terragen::fix)((0.5 - lat / M_PI) * terragen::ONE);
		const terragen::Place place = terragen::World::place(u, v);
		if (world->column(place, world->sunlight(place, std::nullopt)).elevation <= terragen::ONE / 20)
			continue;
		planetarium::Painter biomes({ world, planetarium::Overlay::Biomes, nullptr, std::nullopt, std::nullopt, 0 });
		planetarium::Painter heat({ world, planetarium::Overlay::Temperature, nullptr, std::nullopt, std::nullopt, 0 });
		UASSERT(biomes.colour(river, at) == river);
		UASSERT(heat.colour(river, at) != river);
		return;
	}
	UASSERT(!"no land found");
}
