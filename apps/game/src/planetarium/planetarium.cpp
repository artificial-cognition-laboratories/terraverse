// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terragen/worlds.h"
#include "planetarium/planetarium.h"
#include "client/shader.h"
#include "planetarium/ribbon.h"
#include "constants.h"
#include "log.h"
#include "exceptions.h"
#include <IEventReceiver.h>
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <cmath>
#include <cstdlib>
#include <thread>

namespace planetarium {

// All but the main thread's and the driver's: the Planetarium is all that runs.
static unsigned buildThreads()
{
	const unsigned all = far::Tiles::threadsFor(1);
	return all > 2 ? all - 2 : 1;
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
	m_tiles(opts.atlas, opts.body, opts.horizon, m_palette, buildThreads()),
	m_camera(m_sea, farthest(opts)),
	m_seams(opts.atlas.body(opts.body).params(), m_sea),
	m_markers(m_sea),
	m_system({ opts.system, tiledOf(opts), opts.atlas.body(opts.body).params().radius / opts.system->radius(tiledOf(opts)) }),
	m_body(opts.body),
	m_tiled(tiledOf(opts)),
	m_target(m_tiled),
	m_world(opts.atlas),
	m_horizon(opts.horizon)
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
	m_layer = std::move(layer);
	tint(m_body);
	for (Baked &b : m_baked)
		if (b.tiles)
			tint(b.body);
}

void Planetarium::tint(int body)
{
	far::Tiles *tiles = &m_tiles;
	if (body != m_body) {
		auto b = std::find_if(m_baked.begin(), m_baked.end(), [&](const Baked &x) { return x.body == body; });
		if (b == m_baked.end() || !b->tiles)
			return;
		tiles = b->tiles.get();
	}
	tiles->tint(tintFor(body));
}

std::shared_ptr<const terragen::World> Planetarium::worldOf(int body)
{
	auto found = m_worlds.find(body);
	if (found == m_worlds.end()) {
		const terragen::WorldCode home = terragen::WorldCode::decode(m_horizon.code);
		const terragen::WorldCode code = body == planet::Atlas::HOME ? home
				: terragen::bodyWorlds(home, m_world.body(planet::Atlas::HOME).params()).at(body).code;
		// a thread of its own, not std::async: a Planetarium let go mid-build mustn't wait for it
		std::promise<std::shared_ptr<const terragen::World>> built;
		found = m_worlds.emplace(body, built.get_future().share()).first;
		std::thread([code, built = std::move(built)]() mutable {
			try {
				built.set_value(std::make_shared<const terragen::World>(code));
			} catch (...) {
				built.set_exception(std::current_exception());
			}
		}).detach();
	}
	if (found->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return nullptr;
	return found->second.get(); // (rethrows what building it threw)
}

std::shared_ptr<const far::Tiles::Tint> Planetarium::tintFor(int body)
{
	// what's explored is the tiled body's: the others aren't shown under it
	if (!m_layer || (m_layer->explored && body != m_body))
		return nullptr;
	if (m_layer->image) {
		if (body != m_body)
			return nullptr; // the page's painting is the tiled body's
		auto painter = std::make_shared<Painter>(Painter::Opts{ nullptr, m_layer->overlay, m_layer->image });
		return std::make_shared<const far::Tiles::Tint>([painter](video::SColor own, const far::Tiling::Vec &at) {
			return painter->colour(own, at);
		});
	}
	const std::shared_ptr<const terragen::World> world = worldOf(body);
	if (!world) {
		m_untinted.insert(body);
		return nullptr;
	}
	auto painter = std::make_shared<Painter>(Painter::Opts{ world, m_layer->overlay, nullptr, m_layer->date,
		body == m_body ? m_layer->explored : std::nullopt, m_layer->grid });
	return std::make_shared<const far::Tiles::Tint>([painter](video::SColor own, const far::Tiling::Vec &at) {
		return painter->colour(own, at);
	});
}

// Contour lines on the elevation overlay: blocks between them (every fifth bold)
float Planetarium::contour() const
{
	return m_layer && !m_layer->image && m_layer->overlay == Overlay::Elevation ? 50.f : 0.f;
}

double Planetarium::spinOf(size_t index) const
{
	return m_system.system().turning(index, m_day);
}

// A baked body is drawn from its tiles once it's this many pixels across (a sphere till then).
static constexpr double BAKED_PIXELS = 6;

bool Planetarium::drawBaked(video::IVideoDriver *driver, const core::matrix4 &look, float aspect, size_t index,
		const terragen::System::Vec &at, double pixels, float near, float far)
{
	auto b = std::find_if(m_baked.begin(), m_baked.end(), [&](const Baked &x) { return x.index == index; });
	if (b == m_baked.end() || pixels < BAKED_PIXELS)
		return false;
	if (!b->tiles) {
		b->tiles = std::make_unique<far::Tiles>(m_world, b->body, m_horizon, m_palette, 1);
		tint(b->body);
	}
	// the camera in the body's own frame: from its centre, turned back, at its scale
	const double k = m_system.radius(index) / b->sea, spin = spinOf(index);
	const OrbitCamera::Vec eye = OrbitCamera::turn({ -at.x / k, -at.y / k, -at.z / k }, -spin);
	b->tiles->update(OrbitCamera::eyeAt(eye, b->sea, (float)m_screen.Y * std::tan(OrbitCamera::FOV / 2) / std::tan(m_fov / 2)));
	if (!b->tiles->based())
		return false; // its sphere till its coarse land is there
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, near, far, m_shift, m_fov));
	driver->setTransform(video::ETS_VIEW, look);
	driver->setMaterial(m_system.lit(index, m_day, v3f((f32)at.x, (f32)at.y, (f32)at.z) * BS, 0, !m_layer, contour()));
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

void Planetarium::keep(const Planetarium &was)
{
	m_day = was.m_day;
	m_spin = was.m_spin;
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

void Planetarium::compose(const Shot &shot, float aspect, double seconds)
{
	using Vec = OrbitCamera::Vec;
	const auto cross = [](const Vec &a, const Vec &b) {
		return Vec{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	};
	const auto dot = [](const Vec &a, const Vec &b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
	const auto unit = [&](const Vec &v) { const double l = std::sqrt(dot(v, v)); return Vec{ v.x / l, v.y / l, v.z / l }; };
	const auto mix = [](const Vec &a, double p, const Vec &b, double q) {
		return Vec{ a.x * p + b.x * q, a.y * p + b.y * q, a.z * p + b.z * q };
	};
	// the star in the camera's frame (the ground's)
	const terragen::System::Vec way = m_system.toStar(m_tiled, m_day);
	const Vec sun = unit(OrbitCamera::turn({ way.x, way.y, way.z }, -m_spin));
	// how far the star must stand from the body's centre, and which way across the screen
	const double t = std::tan(shot.fov / 2);
	const double dx = (shot.star.X - shot.centre.X) * t * aspect, dy = (shot.star.Y - shot.centre.Y) * t;
	const double across = std::hypot(dx, dy), off = std::atan(across);
	const Vec side = unit(cross(sun, std::abs(sun.y) < 0.9 ? Vec{ 0, 1, 0 } : Vec{ 1, 0, 0 }));
	const Vec other = cross(sun, side);
	const Vec ahead = mix(sun, std::cos(off), mix(side, std::cos(shot.around), other, std::sin(shot.around)), std::sin(off));
	// the screen's up, so the star's on the way (dx, dy) from the centre
	const Vec toward = unit(mix(sun, 1, ahead, -dot(sun, ahead)));
	const Vec north = mix(toward, dy / across, cross(ahead, toward), dx / across);
	OrbitCamera camera = cameraFor(m_tiled);
	camera.place({ -ahead.x, -ahead.y, -ahead.z }, north, shot.altitude * m_sea);
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

core::matrix4 Planetarium::view() const
{
	if (!m_flight)
		return m_target == m_tiled ? OrbitCamera::turned(m_camera.view(), -m_spin) : m_camera.view();
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
	return m;
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
	const size_t count = m_system.system().spec().bodies.size();
	// where a body (or the star, i == count) is on screen, if in front of the camera
	struct Seen { v2f at; float shown, off; double distance; };
	const auto see = [&](size_t i) -> std::optional<Seen> {
		const OrbitCamera::Vec c = centreOf(i);
		core::vector3df v((f32)(c.x - eye.x), (f32)(c.y - eye.y), (f32)(c.z - eye.z));
		const double distance = v.getLength();
		look.transformVect(v); // (the view has no translation: the camera is the origin)
		if (v.Z <= 0)
			return std::nullopt; // behind
		const double x = v.X / (v.Z * t * aspect) + m_shift.X, y = v.Y / (v.Z * t) + m_shift.Y;
		const v2f at((f32)(m_corner.X + (x + 1) / 2 * m_screen.X), (f32)(m_corner.Y + (1 - y) / 2 * m_screen.Y));
		const float dx = at.X - m_mouse->X, dy = at.Y - m_mouse->Y;
		return Seen{ at, (f32)(radiusOf(i) / distance * focal), std::sqrt(dx * dx + dy * dy), distance };
	};
	// what's nearest under the mouse hides whatever's behind it (the body it's
	// round too, though that isn't itself offered when it fills the screen)
	double blocked = INFINITY;
	for (size_t i = 0; i <= count; i++)
		if (const std::optional<Seen> s = see(i); s && s->off <= s->shown)
			blocked = std::min(blocked, s->distance);
	std::optional<Hover> best;
	double nearest = 0;
	bool bestInside = false;
	for (size_t i = 0; i <= count; i++) {
		// the bodies, then the star (i == count)
		const bool star = i == count;
		const std::optional<Seen> s = see(i);
		if (!s || s->distance > blocked)
			continue;
		const v2f at = s->at;
		const float shown = s->shown, off = s->off;
		const double distance = s->distance;
		// what's hovered now keeps it a little past where it would start:
		// on an edge it doesn't flick on and off frame to frame
		const bool held = m_hover && m_hover->body == (star ? std::nullopt : std::optional<size_t>(i));
		if (i == m_target && shown > m_screen.Y * (held ? 0.5f : 0.4f))
			continue; // the body it's round, filling the screen
		if (off > std::max(shown, 10.f) + (held ? 14 : 4))
			continue;
		// a disc the mouse is inside beats a dot's reach round it (a planet's
		// dot drawn over the star isn't the star's click), the nearest in
		// front among discs; among reaches, the one whose middle is closest
		const bool inside = off <= shown;
		const double rank = inside ? distance : off;
		if (!best || inside > bestInside || (inside == bestInside && rank < nearest)) {
			best = Hover{ star ? std::nullopt : std::optional<size_t>(i), at, shown };
			nearest = rank;
			bestInside = inside;
		}
	}
	return best;
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
	const OrbitCamera::Vec o = m_camera.position(),
			d = m_camera.ray(x - m_corner.X, y - m_corner.Y, m_screen.X, m_screen.Y, m_shift);
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
	m_screen = screen;
	// bodies whose world model has been built since the layer was asked for
	for (auto it = m_untinted.begin(); it != m_untinted.end();) {
		const int body = *it;
		auto waiting = m_worlds.find(body);
		if (waiting != m_worlds.end() && waiting->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
			++it;
			continue;
		}
		it = m_untinted.erase(it);
		tint(body);
	}
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
	m_tiles.update(OrbitCamera::eyeAt(OrbitCamera::turn(eye(), -m_spin), m_sea, (float)screen.Y * std::tan(OrbitCamera::FOV / 2) / std::tan(m_fov / 2))); // (a long lens: finer for what fills the screen)
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
	if (!m_shaders) {
		m_shaders.reset(createShaderSource());
		m_system.shaders(m_shaders.get());
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
	const core::dimension2du window = driver->getScreenSize();
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
	m_system.draw(driver, SystemLayer::Camera{ { eye.x, eye.y, eye.z }, look, m_fov, aspect, m_shift, screen.Height }, m_day, [&](float near, float far) {
		// round it: the camera's own range, tight to the ground; from afar, the layer's
		if (round)
			driver->setTransform(video::ETS_PROJECTION, m_camera.projection(aspect, m_shift, m_fov));
		else {
			driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, near, far, m_shift, m_fov));
		}
		driver->setTransform(video::ETS_VIEW, spun);
		// (an overlay is there to be read, on the night side too)
		driver->setMaterial(m_system.lit(m_tiled, m_day, v3f((float)-ground.x, (float)-ground.y, (float)-ground.z) * BS, m_spin, !m_layer, contour()));
		for (const far::Tiles::Drawn &tile : m_tiles.drawn()) {
			core::matrix4 world;
			world.setTranslation(v3f((float)(tile.middle.x - ground.x), (float)(tile.middle.y - ground.y),
					(float)(tile.middle.z - ground.z)) * BS);
			driver->setTransform(video::ETS_WORLD, world);
			for (const auto &part : tile.parts)
				driver->drawMeshBuffer(part.get());
		}
		if (m_seamsShown)
			m_seams.draw(driver, ground);
		m_markers.draw(driver, ground, m_camera.altitude());
		drawPath(driver, ground);
	}, [&](size_t index, const terragen::System::Vec &at, double pixels, float near, float far) {
		return drawBaked(driver, look, aspect, index, at, pixels, near, far);
	});
	driver->setViewPort(core::recti(0, 0, window.Width, window.Height)); // the page draws over the whole window
}

Planetarium::~Planetarium() = default;

} // namespace planetarium
