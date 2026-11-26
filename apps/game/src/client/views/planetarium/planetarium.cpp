// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/code/worlds.h"
#include "src/client/views/planetarium/planetarium.h"
#include "src/client/views/planetarium/framing.h"
#include "library/trace/trace.h"
#include "src/client/render/shaders/shader.h"
#include "src/client/views/planetarium/overlays/ribbon.h"
#include "src/shared/base/constants.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/base/exceptions.h"
#include <IEventReceiver.h>
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <cmath>
#include <cstdlib>
#include <thread>

namespace planetarium {

// Every core: the Planetarium is all that runs, and its workers run below the
// frame's thread (terragen::background()), so they take only what it leaves.
// (Two held back left one worker on a 2-core machine: a cold pan took 16 s.)
static unsigned buildThreads()
{
	return far::Tiles::threadsFor(1);
}

size_t systemIndex(const planet::Atlas &atlas, const far::Horizon &horizon, const terragen::System &system, int body)
{
	if (body == planet::Atlas::HOME)
		return system.playable();
	const std::vector<terragen::BodyWorld> worlds = terragen::bodyWorlds(
			terragen::WorldCode::decode(horizon.code), atlas.body(planet::Atlas::HOME).params());
	if ((int)worlds.size() != atlas.bodies())
		throw BaseException("planetarium: the atlas holds " + std::to_string(atlas.bodies()) +
				" bodies, the world code's system " + std::to_string(worlds.size()));
	return worlds.at(body).system;
}

static size_t tiledOf(const Planetarium::Opts &o)
{
	if (!o.system)
		throw BaseException("planetarium: a body needs its system");
	return systemIndex(o.atlas, o.horizon, *o.system, o.body);
}

static double seaOf(const Planetarium::Opts &o)
{
	return o.atlas.body(o.body).params().radius + o.horizon.water_level + 1;
}

// How far out the camera may go: to see the whole system round the body.
static double farthest(const Planetarium::Opts &o)
{
	if (!o.system)
		throw BaseException("planetarium: a body needs its system");
	const size_t tiled = tiledOf(o);
	const double k = o.atlas.body(o.body).params().radius / o.system->radius(tiled);
	double out = 0;
	const terragen::System &s = *o.system;
	const auto home = s.position(tiled, 0);
	for (size_t i = 0; i < s.spec().bodies.size(); i++) {
		const auto p = s.position(i, 0);
		out = std::max(out, std::hypot(p.x - home.x, p.y - home.y, p.z - home.z) * k);
	}
	return std::max(out * 1.5, 6 * seaOf(o));
}

// A press let go within this many pixels of where it was is a click, not a drag.
static constexpr s32 CLICK = 4;

Planetarium::Planetarium(Opts opts) :
	m_palette(std::move(opts.palette)),
	m_atlas(opts.atlas.body(opts.body)),
	m_sea(seaOf(opts)),
	m_tiles(opts.atlas, opts.body, opts.horizon, m_palette, buildThreads(), opts.driver, &m_budget),
	m_camera(m_sea, farthest(opts)),
	m_seams(opts.atlas.body(opts.body).params(), m_sea),
	m_markers(m_sea),
	m_system({ opts.system, tiledOf(opts), opts.atlas.body(opts.body).params().radius / opts.system->radius(tiledOf(opts)), true, true }),
	m_body(opts.body),
	m_tiled(tiledOf(opts)),
	m_target(m_tiled),
	m_world(opts.atlas),
	m_horizon(opts.horizon),
	m_tinting({ m_world, m_horizon, opts.body })
{
	for (int b = 0; b < m_world.bodies(); b++)
		if (b != m_body)
			m_baked.push_back({ b, systemIndex(m_world, m_horizon, *opts.system, b),
				m_world.body(b).params().radius + m_horizon.water_level + 1, nullptr });
}

void Planetarium::focus(double latitude, double longitude, std::optional<double> altitude)
{
	m_camera.focus(latitude, longitude, altitude);
}

void Planetarium::day(double days, std::optional<double> timeOfDay)
{
	m_day = days;
	const terragen::System &s = m_system.system();
	if (timeOfDay) {
		m_spin = m_system.spin(days, *timeOfDay);
	} else {
		m_spin = s.turning(m_tiled, days);
	}
}

void Planetarium::layer(std::optional<Layer> layer)
{
	m_tinting.layer(std::move(layer));
	tint(m_body);
	for (Baked &b : m_baked)
		if (b.tiles)
			tint(b.body);
}

void Planetarium::tint(int body)
{
	far::Tiles *tiles = &m_tiles;
	if (body != m_body) {
		TF_ZONE("Planetarium: other bodies");
	auto b = std::find_if(m_baked.begin(), m_baked.end(), [&](const Baked &x) { return x.body == body; });
		if (b == m_baked.end() || !b->tiles)
			return;
		tiles = b->tiles.get();
	}
	tiles->tint(m_tinting.tintFor(body));
}

double Planetarium::spinOf(size_t index) const
{
	return m_system.system().turning(index, m_day);
}

// The body's drawn as its globe (planetarium/globe.h) once its coarsest
// tiles' cells are smaller than a tile's would be let be (OrbitCamera::cell:
// past that they'd merge, but they're the coarsest there are), till they're
// this many times that again
static constexpr double GLOBE_LEAVE = 1.4;
// What makes the globe's picture: a few threads, beside the tiles' (a second or two)
static constexpr unsigned GLOBE_THREADS = 2;

// How much wider than the screen tiles are split for: what's built before it
// turns into view.
static constexpr double VIEW_MARGIN = 1.1;

// A baked body is drawn from its tiles once it's this many pixels across (a sphere till then).
static constexpr double BAKED_PIXELS = 6;

bool Planetarium::drawBaked(video::IVideoDriver *driver, const core::matrix4 &look, float aspect, size_t index,
		const terragen::System::Vec &at, double pixels, float near, float far)
{
	TF_ZONE("Planetarium: other bodies");
	auto b = std::find_if(m_baked.begin(), m_baked.end(), [&](const Baked &x) { return x.index == index; });
	if (b == m_baked.end() || pixels < BAKED_PIXELS)
		return false;
	if (!b->tiles) {
		b->tiles = std::make_unique<far::Tiles>(m_world, b->body, m_horizon, m_palette, 1, driver, &m_budget);
		tint(b->body);
	}
	// the camera in the body's own frame: from its centre, turned back, at its scale
	const double k = m_system.radius(index) / b->sea, spin = spinOf(index);
	const OrbitCamera::Vec eye = OrbitCamera::turn({ -at.x / k, -at.y / k, -at.z / k }, -spin);
	b->tiles->update(OrbitCamera::eyeAt(eye, b->sea, OrbitCamera::focal((float)m_screen.Y, m_fov)));
	if (!b->tiles->based())
		return false; // its sphere till its coarse land is there
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, near, far, m_fov));
	driver->setTransform(video::ETS_VIEW, look);
	driver->setMaterial(m_system.lit(index, m_day, v3f((f32)at.x, (f32)at.y, (f32)at.z) * BS, 0, !m_tinting.layer(), m_tinting.contour()));
	// where it is, turned on its axis, at the scene's scale; a tile from its middle
	core::matrix4 place, turn, scale;
	place.setTranslation(v3f((f32)at.x, (f32)at.y, (f32)at.z) * BS);
	const f32 c = (f32)std::cos(spin), s = (f32)std::sin(spin);
	turn[0] = c;
	turn[8] = -s;
	turn[2] = s;
	turn[10] = c;
	scale.setScale((f32)k);
	const core::matrix4 body = place * turn * scale;
	for (const far::Tiles::Drawn &tile : b->tiles->drawn()) {
		core::matrix4 from;
		from.setTranslation(v3f((f32)tile.middle.x, (f32)tile.middle.y, (f32)tile.middle.z) * BS);
		driver->setTransform(video::ETS_WORLD, body * from);
		for (const auto &part : tile.parts)
			driver->drawMeshBuffer(part.get());
	}
	return true;
}

void Planetarium::align(const Planetarium &was)
{
	// its ground's frame to space (turn by its spin), then into this one's
	const OrbitCamera &c = was.m_flight ? was.m_flight->end : was.m_camera;
	const auto across = [&](const OrbitCamera::Vec &v) { return OrbitCamera::turn(OrbitCamera::turn(v, was.m_spin), -m_spin); };
	m_flight.reset();
	m_target = m_tiled;
	m_camera = cameraFor(m_tiled);
	m_camera.place(across(c.up()), across(c.north()), c.altitude() / was.m_sea * m_sea);
	m_aim = was.m_aim;
}

bool Planetarium::warm(video::IVideoDriver *driver, double ms)
{
	const auto from = std::chrono::steady_clock::now();
	for (const far::Tiles::Drawn &tile : m_tiles.drawn())
		for (const auto &part : tile.parts) {
			if (m_warmed.count(part.get()))
				continue;
			if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count() > ms)
				return false;
			driver->updateHardwareBuffer(part->getVertexBuffer());
			driver->updateHardwareBuffer(part->getIndexBuffer());
			m_warmed.insert(part.get());
		}
	return true;
}

Planetarium::Shaders::Shaders(IWritableShaderSource *made) : source(made), programs(made) {}
Planetarium::Shaders::~Shaders() = default;

void Planetarium::keep(const Planetarium &was)
{
	m_day = was.m_day;
	m_spin = was.m_spin;
	m_aim = was.m_aim; // (a camera taken over turned as it was: framed in the same place)
	if (!m_shaders)
		m_shaders = was.m_shaders;
	if (was.heading() > star())
		return;
	// where it was round its body, in that body's radii: a body made bigger or
	// smaller keeps the camera as far out, never inside it
	m_target = was.heading();
	const OrbitCamera fresh = cameraFor(m_target);
	m_camera = was.m_flight ? was.m_flight->end : was.m_camera;
	m_camera.resize(fresh.sea(), fresh.farthest());
}

OrbitCamera Planetarium::cameraFor(size_t body) const
{
	if (body > star())
		throw BaseException("planetarium: no body " + std::to_string(body) + " in the system");
	const double sea = radiusOf(body);
	// round the star the whole system's within reach: it's that far from the tiled body, and as far again
	const double extent = body == star() ? m_system.extent() * 2.5 : m_system.extent() * 1.5;
	return OrbitCamera(sea, std::max(extent, 6 * sea));
}

void Planetarium::focusBody(size_t body)
{
	if (body == m_target && !m_flight)
		return;
	m_flight.reset();
	m_camera = cameraFor(body);
	m_target = body;
}

void Planetarium::fly(const Flight &f)
{
	// where it ends: round the body, as asked, the rest as the camera is (or would be)
	OrbitCamera end = f.camera ? *f.camera : f.body == m_target ? m_camera : cameraFor(f.body);
	const double sea = radiusOf(f.body);
	const std::optional<double> altitude = f.radii ? std::optional<double>(*f.radii * sea) : f.altitude;
	if (f.latitude)
		end.focus(*f.latitude, *f.longitude, altitude);
	else if (altitude)
		end.altitude(*altitude);
	if (f.seconds <= 0) {
		m_flight.reset();
		m_target = f.body;
		m_camera = end;
		return;
	}
	// from where it is now (mid-flight or not), from the body it's round
	const Pose now = pose();
	const size_t from = m_flight ? m_flight->from : m_target;
	const OrbitCamera::Vec c = centreOf(from);
	const core::matrix4 look = view();
	m_flight = Flying{ from, { now.at.x - c.x, now.at.y - c.y, now.at.z - c.z }, f.body, end, f.seconds, 0, { look[1], look[5], look[9] } };
}

double Planetarium::litLongitude() const
{
	const terragen::System::Vec way = m_system.toStar(m_tiled, m_day);
	const OrbitCamera::Vec sun = OrbitCamera::turn({ way.x, way.y, way.z }, -m_spin);
	return std::atan2(sun.z, sun.x) + 0.7; // (off to one side: a day side with its edge of night in view)
}

OrbitCamera::Vec Planetarium::sun() const
{
	// the star's way from the tiled body, in its ground's frame (the camera's)
	const terragen::System::Vec way = m_system.toStar(m_tiled, m_day);
	const OrbitCamera::Vec v = OrbitCamera::turn({ way.x, way.y, way.z }, -m_spin);
	const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return { v.x / l, v.y / l, v.z / l };
}

Framing Planetarium::framed() const
{
	return framing::read(m_flight ? m_flight->end : m_camera, sun(), m_sea, m_aim, m_system.skyLevel());
}

void Planetarium::compose(const Framing &framing, double seconds)
{
	const framing::Place at = framing::place(framing, sun());
	OrbitCamera camera = cameraFor(m_tiled);
	camera.place(at.up, at.north, framing.altitude * m_sea);
	fly({ m_tiled, std::nullopt, std::nullopt, std::nullopt, std::nullopt, seconds, camera });
}

OrbitCamera::Vec Planetarium::offsetOf(size_t body, const OrbitCamera &camera) const
{
	// round the tiled body the camera turns with its ground
	return body == m_tiled ? OrbitCamera::turn(camera.position(), m_spin) : camera.position();
}

double Planetarium::radiusOf(size_t body) const
{
	return body == star() ? m_system.starRadius() : body == m_tiled ? m_sea : m_system.radius(body);
}

OrbitCamera::Vec Planetarium::centreOf(size_t body) const
{
	if (body == m_tiled)
		return { 0, 0, 0 };
	const terragen::System::Vec v = body == star() ? m_system.starCentre(m_day) : m_system.centre(body, m_day);
	return { v.x, v.y, v.z };
}

Planetarium::Pose Planetarium::pose() const
{
	if (!m_flight) {
		const OrbitCamera::Vec c = centreOf(m_target), p = offsetOf(m_target, m_camera);
		return { { c.x + p.x, c.y + p.y, c.z + p.z }, c };
	}
	const Flying &f = *m_flight;
	const double x = std::clamp(f.t / f.seconds, 0.0, 1.0), e = x * x * (3 - 2 * x); // eased
	const OrbitCamera::Vec c0 = centreOf(f.from), c1 = centreOf(f.to), o0 = f.start, o1 = offsetOf(f.to, f.end);
	const auto len = [](const OrbitCamera::Vec &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
	const auto smooth = [](double t) { t = std::clamp(t, 0.0, 1.0); return t * t * (3 - 2 * t); };
	const OrbitCamera::Vec hop{ c1.x - c0.x, c1.y - c0.y, c1.z - c0.z };
	const double l0 = len(o0), l1 = len(o1);
	// How far out, in log space: a zoom is as fast to the eye at any scale,
	// so out to where both bodies are in view and in again, each half eased:
	// the far body grows into view, never a jump in its last frames.
	const double peak = std::max({ l0, l1, len(hop) * 0.7 });
	const double lp = std::log(peak);
	const double out = std::exp(e < 0.5 ? std::log(l0) + (lp - std::log(l0)) * smooth(2 * e)
										 : lp + (std::log(l1) - lp) * smooth(2 * e - 1));
	// what it looks at goes across while it's out (the middle of the way),
	// so near either body it's round that body
	const double across = std::abs(lp - std::log(std::max(l0, l1))) < 1e-6 ? e : smooth((e - 0.2) / 0.6);
	const OrbitCamera::Vec look{ c0.x + hop.x * across, c0.y + hop.y * across, c0.z + hop.z * across };
	// the way out from what it looks at: turned from the start's to the end's,
	// and a hop across the system rises over its plane (the scene's y), so it
	// never swings through the bodies' far sides
	OrbitCamera::Vec d{ o0.x / l0 * (1 - across) + o1.x / l1 * across, o0.y / l0 * (1 - across) + o1.y / l1 * across,
		o0.z / l0 * (1 - across) + o1.z / l1 * across };
	d.y += 2 * std::sin(M_PI * across) * std::min(1.0, len(hop) / (4 * std::max(l0, l1)));
	double dl = len(d);
	if (dl < 1e-9)
		d = { 0, 1, 0 }, dl = 1;
	return { { look.x + d.x / dl * out, look.y + d.y / dl * out, look.z + d.z / dl * out }, look };
}

// The body's centre is drawn where it's aimed by turning the camera off it,
// never by sliding the lens: the sky behind stays the sky
core::matrix4 Planetarium::aim() const
{
	return OrbitCamera::aimed(m_aim, (float)m_screen.X / std::max(1u, m_screen.Y), m_fov);
}

core::matrix4 Planetarium::view() const
{
	if (!m_flight)
		return aim() * (m_target == m_tiled ? OrbitCamera::turned(m_camera.view(), -m_spin) : m_camera.view());
	const Pose p = pose();
	core::vector3df ahead((f32)(p.look.x - p.at.x), (f32)(p.look.y - p.at.y), (f32)(p.look.z - p.at.z));
	ahead.normalize();
	// up turns from the start's to the end's as the flight goes, so it lands
	// with the orbit's own view, not a twist at the end
	const double x = std::clamp(m_flight->t / m_flight->seconds, 0.0, 1.0), e = x * x * (3 - 2 * x);
	const OrbitCamera::Vec u0 = m_flight->up, u1 = endUp();
	core::vector3df up((f32)(u0.x * (1 - e) + u1.x * e), (f32)(u0.y * (1 - e) + u1.y * e), (f32)(u0.z * (1 - e) + u1.z * e));
	up -= ahead * up.dotProduct(ahead);
	if (up.getLength() < 1e-3f)
		up = std::abs(ahead.Y) < 0.9f ? core::vector3df(0, 1, 0) : core::vector3df(0, 0, 1);
	up.normalize();
	core::matrix4 m;
	m.buildCameraLookAtMatrixLH(core::vector3df(0), ahead, up);
	return aim() * m;
}

OrbitCamera::Vec Planetarium::endUp() const
{
	const core::matrix4 v = m_flight->to == m_tiled ? OrbitCamera::turned(m_flight->end.view(), -m_spin) : m_flight->end.view();
	return { v[1], v[5], v[9] };
}

OrbitCamera::Vec Planetarium::eye() const
{
	return pose().at;
}

std::optional<std::optional<size_t>> Planetarium::takeChosen()
{
	std::optional<std::optional<size_t>> chosen = m_chosen;
	m_chosen.reset();
	return chosen;
}

std::optional<Planetarium::Hover> Planetarium::hoverFor(const OrbitCamera::Vec &eye, const core::matrix4 &look, float aspect) const
{
	if (!m_mouse)
		return std::nullopt;
	const double t = std::tan(m_fov / 2), focal = m_screen.Y / (2 * t);
	// where each body (then the star) is on screen, if in front of the camera
	std::vector<std::optional<hover::Seen>> seen;
	for (size_t i = 0; i <= star(); i++) {
		const OrbitCamera::Vec c = centreOf(i);
		core::vector3df v((f32)(c.x - eye.x), (f32)(c.y - eye.y), (f32)(c.z - eye.z));
		const double distance = v.getLength();
		look.transformVect(v); // (the view has no translation: the camera is the origin)
		if (v.Z <= 0) {
			seen.push_back(std::nullopt); // behind
			continue;
		}
		const double x = v.X / (v.Z * t * aspect), y = v.Y / (v.Z * t);
		const v2f at((f32)(m_corner.X + (x + 1) / 2 * m_screen.X), (f32)(m_corner.Y + (1 - y) / 2 * m_screen.Y));
		seen.push_back(hover::Seen{ at, (f32)(radiusOf(i) / distance * focal), distance });
	}
	return hover::under({ seen, v2f((f32)m_mouse->X, (f32)m_mouse->Y), m_target, (f32)m_screen.Y, m_hover });
}

std::optional<Planetarium::Pick> Planetarium::takeClick()
{
	if (!m_click)
		return std::nullopt;
	const v2s32 at = *m_click;
	m_click.reset();
	return pick(at.X, at.Y);
}

std::optional<Planetarium::Pick> Planetarium::pick(double x, double y) const
{
	if (m_flight || m_target != m_tiled)
		return std::nullopt; // nothing to pick on another body (yet), nor mid-flight
	// the ray from the camera through the pixel, onto the sea's sphere
	// (the camera's in its own frame: its view unturned, aimed)
	const OrbitCamera::Vec o = m_camera.position(),
			d = OrbitCamera::ray(aim() * m_camera.view(), x - m_corner.X, y - m_corner.Y, m_screen.X, m_screen.Y, m_fov);
	const double b = o.x * d.x + o.y * d.y + o.z * d.z;
	const double c = o.x * o.x + o.y * o.y + o.z * o.z - m_sea * m_sea;
	const double disc = b * b - c;
	if (disc < 0)
		return std::nullopt; // off the body
	const double t = -b - std::sqrt(disc);
	if (t < 0)
		return std::nullopt;
	// where it meets the body (the camera's in its own frame)
	const OrbitCamera::Vec hit{ o.x + t * d.x, o.y + t * d.y, o.z + t * d.z };
	const terragen::Atlas::Vec p{ hit.x, hit.y, hit.z };
	const double r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	// the cell at the sea there: just under the sea's radius, layer 0
	const double inside = (r - 0.5) / r;
	return Pick{ std::asin(p.y / r), std::atan2(p.z, p.x), m_atlas.locate({ p.x * inside, p.y * inside, p.z * inside }) };
}

bool Planetarium::handle(const SEvent &event)
{
	if (event.EventType != EET_MOUSE_INPUT_EVENT)
		return false;
	const SEvent::SMouseInput &m = event.MouseInput;
	const bool inside = m.X >= m_corner.X && m.Y >= m_corner.Y && m.X < m_corner.X + (s32)m_screen.X &&
			m.Y < m_corner.Y + (s32)m_screen.Y;
	if (!inside && !m_dragging) {
		m_mouse.reset();
		return false; // the page's
	}
	switch (m.Event) {
	case EMIE_LMOUSE_PRESSED_DOWN:
		m_dragging = true;
		m_mouse.reset();
		m_x = m_downX = m.X;
		m_y = m_downY = m.Y;
		return true;
	case EMIE_LMOUSE_LEFT_UP:
		m_dragging = false;
		if (std::abs(m.X - m_downX) <= CLICK && std::abs(m.Y - m_downY) <= CLICK)
			m_click = v2s32(m.X, m.Y);
		m_mouse = v2s32(m.X, m.Y);
		return true;
	case EMIE_LMOUSE_DOUBLE_CLICK:
		if (m_hover) {
			// to a body, close; the star too (out from it, the whole system)
			const std::optional<size_t> body = m_hover->body;
			fly(Flight{ body.value_or(star()), std::nullopt, std::nullopt, std::nullopt, 2.5, 1.5 });
			m_chosen = body;
		}
		return true;
	case EMIE_MOUSE_MOVED:
		if (m_dragging && !m_flight) { // a flight has the camera
			m_camera.drag(m.X - m_x, m.Y - m_y);
			m_x = m.X;
			m_y = m.Y;
		}
		if (!m_dragging)
			m_mouse = v2s32(m.X, m.Y);
		return true;
	case EMIE_MOUSE_WHEEL:
		if (!m_flight)
			m_camera.zoom(m.Wheel);
		return true;
	default:
		return true;
	}
}

void Planetarium::update(v2u32 screen)
{
	m_budget.frame(); // (once a frame: drawn, or made ready to be)
	m_screen = screen;
	m_fov = OrbitCamera::fovFor((float)screen.X, (float)screen.Y);
	// bodies whose world model has been built since the layer was asked for
	for (const int body : m_tinting.ripe())
		tint(body);
	const auto now = std::chrono::steady_clock::now();
	const double dt = m_last.time_since_epoch().count() ? std::chrono::duration<double>(now - m_last).count() : 0;
	m_last = now;
	if (m_flight && (m_flight->t += std::min(dt, 0.1)) >= m_flight->seconds) {
		// landed: round the body it went to
		m_target = m_flight->to;
		m_camera = m_flight->end;
		m_flight.reset();
	}
	const auto tilesFrom = std::chrono::steady_clock::now();
	far::Eye tilesEye = OrbitCamera::eyeAt(OrbitCamera::turn(eye(), -m_spin), m_sea, OrbitCamera::focal((float)screen.Y, m_fov));
	// a spot that was fine stays fine while one of its tiles is rebuilt: a
	// coarser one fills only that tile's hole (panning, the whole spot had
	// gone coarse for a moment)
	tilesEye.keep_fine = true;
	const Zooming::Motion motion = m_zooming.at(tilesEye.altitude, dt);
	tilesEye.hurried = motion.fast;
	tilesEye.ahead = motion.descending && !motion.fast; // (fast: the preview's built instead)
	// any coarser tile may stand in for one not built yet (far::Eye::stand_in):
	// the game's limit is for an eye looking across the ground, where a coarse
	// one stood up against the sky. This one looks straight down through a lens
	// no wider than 80°: the horizon's only in view from high up, where no
	// tile's heights show, so there's always ground, sharpening as tiles come
	tilesEye.stand_in = 0;
	if (!m_flight && m_target == m_tiled) {
		// round it: what it sees is the screen, by the view's own axes in the
		// ground's frame (as draw() turns it), and a margin
		const core::matrix4 v = OrbitCamera::turned(view(), m_spin);
		const auto axis = [&](int k) { return far::Tiling::Vec{ v[k], v[4 + k], v[8 + k] }; };
		const double t = std::tan(m_fov / 2) * VIEW_MARGIN;
		const double aspect = (double)screen.X / std::max(1u, screen.Y);
		tilesEye.right = axis(0);
		tilesEye.upward = axis(1);
		tilesEye.look = axis(2);
		tilesEye.wide = (float)(t * aspect);
		tilesEye.high = (float)t;
	}
	m_tiles.update(tilesEye);
	if (!m_globe && m_tiles.based())
		m_globe = std::make_unique<Globe>(m_horizon, m_world, m_body, m_palette, GLOBE_THREADS);
	m_worstTiles = std::max(m_worstTiles, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tilesFrom).count());
	if (++m_frame % 120 == 0) {
		infostream << "planetarium: " << m_tiles.drawn().size() << " tiles drawn, " << m_tiles.pending() << " to build, "
				<< (int)m_camera.altitude() << " blocks up; worst frame " << m_worstFrame << " ms (tiles " << m_worstTiles << " ms)"
				<< std::endl;
		m_worstFrame = m_worstTiles = 0;
	}
}

void Planetarium::drawPath(video::IVideoDriver *driver, const OrbitCamera::Vec &ground)
{
	if (!m_path || m_path->body != m_body || m_path->points.size() < 2)
		return;
	// the accent's blue, bright where it is, dimming round to where it's going
	const size_t n = m_path->points.size();
	std::vector<v3f> points;
	std::vector<video::SColor> colours;
	for (size_t i = 0; i < n; i++) {
		const OrbitCamera::Vec &p = m_path->points[i];
		const double fresh = 1 - 0.7 * i / (n - 1);
		points.push_back(v3f((f32)(p.x - ground.x), (f32)(p.y - ground.y), (f32)(p.z - ground.z)) * BS);
		colours.push_back(video::SColor(255, 0, (u32)(148 * fresh), (u32)(210 * fresh)));
	}
	Ribbons ribbons(2.5f, m_screen.Y / (2 * std::tan(m_fov / 2)));
	ribbons.add(points, colours, false);
	ribbons.draw(driver, true);
}

void Planetarium::draw(video::IVideoDriver *driver, core::recti view)
{
	if (!m_shaded) {
		TF_ZONE("Planetarium: shaders");
		if (!m_shaders)
			m_shaders = std::make_shared<Shaders>(createShaderSource());
		m_system.shaders(std::shared_ptr<Programs>(m_shaders, &m_shaders->programs));
		m_shaded = true;
	}
	const auto frameFrom = std::chrono::steady_clock::now();
	struct Timed // the frame's time, kept as the worst in the log's window, however it ends
	{
		std::chrono::steady_clock::time_point from;
		double &worst;
		~Timed() { worst = std::max(worst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count()); }
	} timed{ frameFrom, m_worstFrame };
	m_corner = view.UpperLeftCorner;
	update(v2u32(std::max(1, view.getWidth()), std::max(1, view.getHeight())));
	const core::dimension2du window = driver->getCurrentRenderTargetSize(); // (the window, or a picture drawn off screen)
	driver->draw2DRectangle(video::SColor(255, 0, 0, 0), view); // space
	driver->setViewPort(view);
	const core::dimension2du screen(m_screen.X, m_screen.Y);

	const float aspect = (float)screen.Width / std::max(1u, screen.Height);
	const OrbitCamera::Vec eye = this->eye();
	const bool round = !m_flight && m_target == m_tiled;
	// the body's own frame (its tiles', its layers'): where the camera is in it, and the view turned with it
	const OrbitCamera::Vec ground = OrbitCamera::turn(eye, -m_spin);
	const core::matrix4 look = this->view(), spun = OrbitCamera::turned(look, m_spin);
	m_hover = hoverFor(eye, look, aspect);
	m_system.highlight(m_hover ? m_hover->body : std::nullopt);
	// the system round it, far to near; the body its tiles, in its turn
	m_system.draw(driver, SystemLayer::Camera{ { eye.x, eye.y, eye.z }, look, m_fov, aspect, screen.Height }, m_day, [&](float near, float far) {
		// round it: the camera's own range, tight to the ground; from afar, the layer's
		if (round)
			driver->setTransform(video::ETS_PROJECTION, m_camera.projection(aspect, m_fov));
		else {
			driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, near, far, m_fov));
		}
		driver->setTransform(video::ETS_VIEW, spun);
		// (an overlay is there to be read, on the night side too)
		// So far out the coarsest tiles' cells are about a pixel, its globe: a
		// tile's cell is one column's colour, and a pixel over a few showed
		// one of them at random, a grain; the globe's mipmaps show their mean.
		// Not under a layer (its colours are the tiles')
		const double up = std::hypot(ground.x, ground.y, ground.z) - m_sea;
		const double focal = screen.Height / (2 * std::tan(m_fov / 2));
		const double root = (double)(1 << m_tiles.tiling().roots().front().level) * focal / std::max(up, 1.0); // its cells' pixels
		m_globed = !m_tinting.layer() && m_globe && m_globe->ready() && root < OrbitCamera::cell * (m_globed ? GLOBE_LEAVE : 1.0);
		if (m_globed) {
			TF_ZONE("Planetarium: globe");
			core::matrix4 world;
			world.setScale((f32)(m_sea * BS));
			world.setTranslation(v3f((float)-ground.x, (float)-ground.y, (float)-ground.z) * BS);
			m_globe->draw(driver, m_system.lit(m_tiled, m_day, v3f((float)-ground.x, (float)-ground.y, (float)-ground.z) * BS,
					m_spin, true, 0, true), world);
		}
		video::SMaterial lit = m_system.lit(m_tiled, m_day, v3f((float)-ground.x, (float)-ground.y, (float)-ground.z) * BS,
				m_spin, !m_tinting.layer(), m_tinting.contour());
		// What isn't built yet shows a coarser tile behind the finer ones that
		// are (Eye::keep_fine), not instead of them all: drawn after them, its
		// depth squeezed to just short of the far plane (Lit::standIns), so it
		// shows only where nothing is. (Drawn level with them, a coarse quarter
		// standing higher hid the finer land: slabs flickering over the craters.)
		const auto draw = [&](const std::vector<far::Tiles::Drawn> &tiles) {
			for (const far::Tiles::Drawn &tile : tiles) {
				core::matrix4 world;
				world.setTranslation(v3f((float)(tile.middle.x - ground.x), (float)(tile.middle.y - ground.y),
						(float)(tile.middle.z - ground.z)) * BS);
				driver->setTransform(video::ETS_WORLD, world);
				for (const auto &part : tile.parts)
					driver->drawMeshBuffer(part.get());
			}
		};
		// The land first, without its skirts; then, squeezed to the back, the
		// stand-ins and the skirts: both only fill gaps (drawn in front, a skirt
		// showed as a dark line wherever the land beside it didn't quite meet)
		if (!m_globed) {
			TF_ZONE("Planetarium: land");
			m_system.parts(0);
			driver->setMaterial(lit);
			draw(m_tiles.drawn());
			m_system.standIns(true);
			m_system.parts(2);
			driver->setMaterial(lit);
			draw(m_tiles.behind());
			m_system.parts(1);
			driver->setMaterial(lit);
			draw(m_tiles.drawn());
			m_system.standIns(false);
			m_system.parts(2);
		}
		TF_ZONE("Planetarium: marks");
		if (m_seamsShown)
			m_seams.draw(driver, ground);
		m_markers.draw(driver, ground, m_camera.altitude());
		drawPath(driver, ground);
	}, [&](size_t index, const terragen::System::Vec &at, double pixels, float near, float far) {
		return drawBaked(driver, look, aspect, index, at, pixels, near, far);
	});
	driver->setViewPort(core::recti(0, 0, window.Width, window.Height)); // the page draws over the whole window
}

Planetarium::DrawStats Planetarium::drawStats() const
{
	DrawStats out;
	if (m_globed)
		return out;
	for (const std::vector<far::Tiles::Drawn> &list : { m_tiles.behind(), m_tiles.drawn() })
		for (const far::Tiles::Drawn &tile : list) {
			out.tiles++;
			for (const auto &part : tile.parts) {
				out.buffers++;
				out.vertices += part->getVertexCount();
			}
		}
	return out;
}

Planetarium::~Planetarium() = default;

} // namespace planetarium
