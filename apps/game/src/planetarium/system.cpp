// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/system.h"
#include "planetarium/galaxy.h"
#include "planetarium/ribbon.h"
#include "planetarium/camera.h"
#include "constants.h"
#include "exceptions.h"
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <algorithm>
#include <cmath>

namespace planetarium {

using V = terragen::System::Vec;

namespace {

constexpr int LONGITUDES = 48, LATITUDES = 24; // a sphere's pieces
constexpr int ORBIT = 512;                     // an orbit line's pieces
constexpr float ORBIT_WIDTH = 2.5f;            // an orbit line, pixels across
constexpr double FADED = 0.12;                 // an orbit's brightness where its body is going
constexpr double DOT = 2.5;                    // pixels: the least a body is drawn across
constexpr double GLOW = 8;                     // the star's glow, in its radii
constexpr u32 GLOW_SIZE = 128;                 // its texture, pixels a side
constexpr u32 GLARE_SIZE = 512;                // the star's glare, pixels a side
constexpr double GLARE_REACH = 90;             // how far it reaches, in the star's radii as seen...
constexpr double GLARE_LEAST = 0.03, GLARE_MOST = 0.45; // ...kept to these shares of the screen's height
constexpr double GLARE_CORE = 0.035;           // its blown-out middle, a share of its reach

double length(const V &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// What a body looks like from afar, by its kind (body.h).
video::SColor colourOf(terragen::Kind kind)
{
	using K = terragen::Kind;
	switch (kind) {
	case K::Temperate: return { 255, 70, 120, 160 };
	case K::ColdDesert: return { 255, 176, 110, 80 };
	case K::Hothouse: return { 255, 222, 196, 140 };
	case K::Volcanic: return { 255, 214, 170, 80 };
	case K::IceShell: return { 255, 205, 220, 226 };
	case K::Hydrocarbon: return { 255, 196, 150, 82 };
	case K::Airless: return { 255, 150, 146, 140 };
	case K::Giant: return { 255, 214, 178, 136 };
	case K::Count: break;
	}
	throw BaseException("planetarium: a body of no kind");
}

// An orbit's colour, a body's own (by its place in the system), apart from
// its neighbours'.
video::SColor orbitColour(size_t body)
{
	static const video::SColor COLOURS[] = {
		{ 255, 236, 176, 110 }, { 255, 120, 210, 220 }, { 255, 150, 120, 240 }, { 255, 240, 220, 90 },
		{ 255, 230, 110, 120 }, { 255, 130, 220, 130 }, { 255, 240, 150, 210 }, { 255, 110, 160, 240 },
		{ 255, 220, 220, 220 }, { 255, 250, 130, 60 },
	};
	return COLOURS[body % (sizeof(COLOURS) / sizeof(COLOURS[0]))];
}

// A star's colour by its class (terragen::STARS), coolest to hottest.
video::SColor starColour(int type)
{
	static const video::SColor COLOURS[terragen::STAR_COUNT] = {
		{ 255, 255, 190, 130 }, { 255, 255, 220, 170 }, { 255, 255, 246, 220 }, { 255, 255, 250, 240 },
		{ 255, 235, 240, 255 }, { 255, 205, 220, 255 }, { 255, 180, 200, 255 },
	};
	return COLOURS[type];
}

video::SColor scaled(video::SColor c, double k)
{
	// (by how much shows, not by darkening: a dark line would stand out against a bright sky)
	c.setAlpha((u32)(255 * std::clamp(k, 0.0, 1.0)));
	return c;
}

} // namespace

SystemLayer::SystemLayer(Opts opts) : m_opts(std::move(opts)), m_tilt(m_opts.system->spec().bodies.at(m_opts.focus).tilt * M_PI / 180)
{
	m_orbits = m_opts.orbits;
	const terragen::System &s = *m_opts.system;
	const V home = s.position(m_opts.focus, 0);
	for (size_t i = 0; i < s.spec().bodies.size(); i++)
		m_extent = std::max(m_extent, length({ s.position(i, 0).x - home.x, s.position(i, 0).y - home.y,
				s.position(i, 0).z - home.z }) * m_opts.blocksPerKm);
	m_extent = std::max(m_extent, length(home) * m_opts.blocksPerKm);

	// the unit sphere, its normals its points; coloured as it's lit, each draw
	m_sphere = make_irr<scene::SMeshBuffer>();
	auto &v = m_sphere->Vertices->Data;
	auto &ix = m_sphere->Indices->Data;
	for (int la = 0; la <= LATITUDES; la++)
		for (int lo = 0; lo <= LONGITUDES; lo++) {
			const double phi = M_PI * la / LATITUDES - M_PI / 2, theta = 2 * M_PI * lo / LONGITUDES;
			const v3f p((f32)(std::cos(phi) * std::cos(theta)), (f32)std::sin(phi), (f32)(std::cos(phi) * std::sin(theta)));
			v.emplace_back(p, p, video::SColor(255, 255, 255, 255), v2f(0, 0));
		}
	for (int la = 0; la < LATITUDES; la++)
		for (int lo = 0; lo < LONGITUDES; lo++) {
			const u16 a = (u16)(la * (LONGITUDES + 1) + lo), b = (u16)(a + LONGITUDES + 1);
			ix.insert(ix.end(), { a, b, (u16)(a + 1), (u16)(a + 1), b, (u16)(b + 1) });
		}
	m_sphere->recalculateBoundingBox();
}

V SystemLayer::scene(const V &km) const
{
	// the ecliptic turned about its x by the tilt: the body's north is up
	const double c = std::cos(m_tilt), sn = std::sin(m_tilt), k = m_opts.blocksPerKm;
	return { km.x * k, (km.y * c + km.z * sn) * k, (-km.y * sn + km.z * c) * k };
}

V SystemLayer::centre(size_t body, double day) const
{
	const V home = m_opts.system->position(m_opts.focus, day), at = m_opts.system->position(body, day);
	return scene({ at.x - home.x, at.y - home.y, at.z - home.z });
}

V SystemLayer::starCentre(double day) const
{
	const V home = m_opts.system->position(m_opts.focus, day);
	return scene({ -home.x, -home.y, -home.z });
}

SystemLayer::~SystemLayer()
{
	if (m_glow)
		m_driver->removeTexture(m_glow);
	for (const auto &[colour, texture] : m_glares)
		m_driver->removeTexture(texture);
}

double SystemLayer::spin(double day, double timeOfDay) const
{
	// longitude 0 faces the star at noon: the star's way round the body's axis, less the hours to noon
	const terragen::System::Vec star = starCentre(day);
	return std::atan2(star.z, star.x) + 2 * M_PI * (timeOfDay - 0.5);
}

V SystemLayer::toSecond(double day) const
{
	const V home = m_opts.system->position(m_opts.focus, day), at = m_opts.system->starAt(true, day);
	const V d = scene({ at.x - home.x, at.y - home.y, at.z - home.z });
	const double l = length(d);
	return { d.x / l, d.y / l, d.z / l };
}

V SystemLayer::toStar(size_t body, double day) const
{
	const V star = starCentre(day), at = centre(body, day);
	const V d{ star.x - at.x, star.y - at.y, star.z - at.z };
	const double l = length(d);
	return { d.x / l, d.y / l, d.z / l };
}

double SystemLayer::spinOf(size_t body, double day, double timeOfDay) const
{
	const V star = toStar(body, day);
	return std::atan2(star.z, star.x) + 2 * M_PI * (timeOfDay - 0.5);
}

void SystemLayer::draw(video::IVideoDriver *driver, const Camera &camera, double day,
		const std::function<void(float near, float far)> &focused, const Surface &surface)
{
	const terragen::System &s = *m_opts.system;
	const double k = m_opts.blocksPerKm;
	const V home = s.position(m_opts.focus, day);
	// where each is from the camera (blocks); the star's at the system's origin
	const auto from = [&](const V &km) {
		const V p = scene({ km.x - home.x, km.y - home.y, km.z - home.z });
		return V{ p.x - camera.at.x, p.y - camera.at.y, p.z - camera.at.z };
	};
	std::vector<Body> bodies;
	const V star = from(s.starAt(false, day));
	bodies.push_back({ star, s.starRadius() * k, length(star), -1 });
	if (s.hasCompanion()) {
		const V second = from(s.starAt(true, day));
		bodies.push_back({ second, s.secondRadius() * k, length(second), -2 });
	}
	for (size_t i = 0; i < s.spec().bodies.size(); i++) {
		const V at = from(s.position(i, day));
		bodies.push_back({ at, s.radius(i) * k, length(at), (int)i });
	}
	std::sort(bodies.begin(), bodies.end(), [](const Body &a, const Body &b) { return a.distance > b.distance; });

	driver->setTransform(video::ETS_VIEW, camera.view);
	if (m_galaxy)
		m_galaxy->draw(driver, camera.aspect, camera.fov, camera.shift, m_skyLevel);
	if (m_orbits)
		orbits(driver, camera, day);

	const double focal = camera.height / (2 * std::tan(camera.fov / 2));
	// the way the camera looks: depth is along it, not the distance (a body
	// off to the side is nearer in depth than it is away)
	const V ahead{ camera.view[2], camera.view[6], camera.view[10] };
	for (const Body &b : bodies) {
		const double shown = std::max(b.radius, b.distance * DOT / 2 / focal); // never under a dot
		const double depth = b.at.x * ahead.x + b.at.y * ahead.y + b.at.z * ahead.z;
		if (depth + shown * GLOW < 0 && b.index != (int)m_opts.focus)
			continue; // behind the camera
		// its own depth range, cleared of the bodies behind it
		driver->clearBuffers(video::ECBF_DEPTH);
		const double reach = shown * (b.index < 0 ? GLOW : 1.2);
		const float near = (float)std::max({ (depth - reach) * BS, shown * 0.001 * BS, 0.1 * BS });
		const float far = (float)std::max((depth + reach) * BS, (double)near * 2);
		driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(camera.aspect, near, far, camera.shift, camera.fov));
		if (m_cutaway && b.index == (int)*m_cutaway) {
			const V p = scene(s.position((size_t)b.index, day));
			const double d = length(p);
			Body drawn = b;
			drawn.radius = shown;
			shells(driver, drawn, { -p.x / d, -p.y / d, -p.z / d });
			continue;
		}
		if (b.index == (int)m_opts.focus) {
			focused(near, far);
			// the focused body draws in its own (spun) view: the rest are in the scene's
			driver->setTransform(video::ETS_VIEW, camera.view);
			rim(driver, camera, b, day, near, far);
			continue;
		}
		if (b.index >= 0 && surface && surface((size_t)b.index, b.at, b.radius * focal / std::max(depth, b.radius), near, far)) {
			driver->setTransform(video::ETS_VIEW, camera.view);
			rim(driver, camera, b, day, near, far);
			continue;
		}
		Body drawn = b;
		drawn.radius = shown;
		if (b.index < 0) {
			const terragen::StarCauses &causes = s.spec().star;
			this->star(driver, drawn, starColour(b.index == -1 ? causes.type : causes.companion.type));
			if (!m_opts.glare)
				glow(driver, camera, drawn);
		} else {
			sphere(driver, drawn, colourOf(s.cards()[(size_t)b.index].kind), day);
			rim(driver, camera, b, day, near, far);
		}
	}
	if (m_opts.glare)
		glare(driver, camera, bodies);
}

void SystemLayer::star(video::IVideoDriver *driver, const Body &b, video::SColor colour)
{
	for (video::S3DVertex &v : m_sphere->Vertices->Data)
		v.Color = colour;
	m_sphere->setDirty(scene::EBT_VERTEX);
	video::SMaterial mat;
	mat.MaterialType = video::EMT_SOLID;
	mat.FogEnable = false;
	mat.ColorMask = video::ECP_RGB;
	driver->setMaterial(mat);
	core::matrix4 world;
	world.setScale((f32)(b.radius * BS));
	world.setTranslation(v3f((f32)b.at.x, (f32)b.at.y, (f32)b.at.z) * BS);
	driver->setTransform(video::ETS_WORLD, world);
	driver->drawMeshBuffer(m_sphere.get());
}

void SystemLayer::sphere(video::IVideoDriver *driver, const Body &b, video::SColor colour, double day)
{
	for (video::S3DVertex &v : m_sphere->Vertices->Data)
		v.Color = colour;
	m_sphere->setDirty(scene::EBT_VERTEX);
	const v3f at = v3f((f32)b.at.x, (f32)b.at.y, (f32)b.at.z) * BS;
	video::SMaterial mat = lit((size_t)b.index, day, at, 0, true);
	mat.BackfaceCulling = true;
	driver->setMaterial(mat);
	core::matrix4 world;
	world.setScale((f32)(b.radius * BS));
	world.setTranslation(at);
	driver->setTransform(video::ETS_WORLD, world);
	driver->drawMeshBuffer(m_sphere.get());
}

void SystemLayer::shaders(IShaderSource *ssrc)
{
	m_lit = std::make_unique<Lit>(ssrc);
	m_rim = std::make_unique<Rim>(ssrc, *m_opts.system);
	m_galaxy = std::make_unique<Galaxy>(ssrc, m_sphere.get());
}

const video::SMaterial &SystemLayer::lit(size_t body, double day, const v3f &centre_at, double turn, bool on)
{
	if (!m_lit)
		throw BaseException("planetarium: bodies drawn before SystemLayer::shaders");
	const terragen::System &s = *m_opts.system;
	const auto inFrame = [&](const V &d) { return OrbitCamera::turn({ d.x, d.y, d.z }, -turn); };
	const auto v3 = [](const OrbitCamera::Vec &d) { return v3f((f32)d.x, (f32)d.y, (f32)d.z); };
	Lit::Light light{};
	light.centre = centre_at;
	light.on = on;
	const V way = toStar(body, day);
	light.sun = v3(inFrame(way));
	if (s.hasCompanion()) {
		const V at = centre(body, day), second = scene(s.starAt(true, day)), home = scene(s.position(m_opts.focus, day));
		const V d{ second.x - home.x - at.x, second.y - home.y - at.y, second.z - home.z - at.z };
		const double l = length(d), share = s.secondShare(body, day);
		light.second = v3(inFrame({ d.x / l, d.y / l, d.z / l }));
		light.share = (float)(share / (1 + share));
	}
	const V here = centre(body, day), sun = starCentre(day);
	light.penumbra = (f32)(starRadius() / length({ sun.x - here.x, sun.y - here.y, sun.z - here.z }));
	// Its family: a planet's moons, a moon's planet and fellow moons. Others are too far
	// to darken it by more than nothing. Those that look biggest from here, if there are many.
	const auto &bodies = s.spec().bodies;
	const int32_t top = bodies[body].causes.parent >= 0 ? bodies[body].causes.parent : (int32_t)body;
	std::vector<std::pair<double, size_t>> family;
	for (size_t j = 0; j < bodies.size(); j++) {
		const int32_t up = bodies[j].causes.parent >= 0 ? bodies[j].causes.parent : (int32_t)j;
		if (j == body || up != top)
			continue;
		const V o = centre(j, day);
		family.emplace_back(-radius(j) / length({ o.x - here.x, o.y - here.y, o.z - here.z }), j);
	}
	std::sort(family.begin(), family.end());
	for (size_t i = 0; i < family.size() && i < Lit::OCCLUDERS; i++) {
		const V o = centre(family[i].second, day);
		const OrbitCamera::Vec rel = inFrame({ (o.x - here.x) * BS, (o.y - here.y) * BS, (o.z - here.z) * BS });
		light.occluders[i][0] = centre_at.X + (f32)rel.x;
		light.occluders[i][1] = centre_at.Y + (f32)rel.y;
		light.occluders[i][2] = centre_at.Z + (f32)rel.z;
		light.occluders[i][3] = (f32)(radius(family[i].second) * BS);
	}
	return m_lit->material(light);
}

void SystemLayer::rim(video::IVideoDriver *driver, const Camera &camera, const Body &b, double day, float near, float far)
{
	const double focal = camera.height / (2 * std::tan(camera.fov / 2));
	if (b.radius * focal / std::max(b.distance, b.radius) < 2)
		return; // a dot has no edge to see
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(camera.aspect, near, far, camera.shift, camera.fov));
	driver->setTransform(video::ETS_VIEW, camera.view);
	const V way = toStar((size_t)b.index, day);
	m_rim->draw(driver, (size_t)b.index, v3f((f32)b.at.x, (f32)b.at.y, (f32)b.at.z) * BS, (f32)(b.radius * BS),
			v3f((f32)way.x, (f32)way.y, (f32)way.z), m_sphere.get());
}

// What a body's shell looks like cut open, by its name (body.h: Layer).
static video::SColor shellColour(const std::string &name)
{
	static const std::pair<const char *, video::SColor> COLOURS[] = {
		{ "core", { 255, 236, 168, 76 } },
		{ "mantle", { 255, 176, 70, 44 } },
		{ "crust", { 255, 120, 98, 82 } },
		{ "ocean", { 255, 44, 96, 150 } },
		{ "ice", { 255, 208, 226, 236 } },
		{ "metallic hydrogen", { 255, 150, 140, 170 } },
		{ "envelope", { 255, 214, 178, 136 } },
	};
	for (const auto &[n, c] : COLOURS)
		if (name == n)
			return c;
	throw BaseException("planetarium: no colour for a body's '" + name + "' shell");
}

void SystemLayer::shells(video::IVideoDriver *driver, const Body &b, const V &sun)
{
	const auto &layers = m_opts.system->cards().at((size_t)b.index).layers;
	// the window: the triangles facing the camera, cut away (the camera is at
	// the origin: the way to it from the body is −at)
	const double d = length(b.at);
	const v3f toward((f32)(-b.at.x / d), (f32)(-b.at.y / d), (f32)(-b.at.z / d));
	const auto &v = m_sphere->Vertices->Data;
	const auto &all = m_sphere->Indices->Data;
	m_window.clear();
	for (size_t t = 0; t + 2 < all.size(); t += 3) {
		const v3f n = (v[all[t]].Normal + v[all[t + 1]].Normal + v[all[t + 2]].Normal) / 3;
		if (n.dotProduct(toward) < 0.55f)
			m_window.insert(m_window.end(), { all[t], all[t + 1], all[t + 2] });
	}
	// outermost first; each shell lit from the star, its inside dimmer
	for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
		Body shell = b;
		shell.radius = b.radius * it->outer / 65536.0;
		const video::SColor colour = shellColour(it->name);
		for (video::S3DVertex &vx : m_sphere->Vertices->Data) {
			const double l = std::max(0.0, vx.Normal.X * sun.x + vx.Normal.Y * sun.y + vx.Normal.Z * sun.z) * 0.6 + 0.4;
			vx.Color = video::SColor(255, (u32)(colour.getRed() * l), (u32)(colour.getGreen() * l), (u32)(colour.getBlue() * l));
		}
		m_sphere->setDirty(scene::EBT_VERTEX);
		video::SMaterial mat;
		mat.MaterialType = video::EMT_SOLID;
		mat.FogEnable = false;
		mat.BackfaceCulling = false; // its inside shows through the window
		mat.ColorMask = video::ECP_RGB;
		driver->setMaterial(mat);
		core::matrix4 world;
		world.setScale((f32)(shell.radius * BS));
		world.setTranslation(v3f((f32)b.at.x, (f32)b.at.y, (f32)b.at.z) * BS);
		driver->setTransform(video::ETS_WORLD, world);
		driver->drawIndexedTriangleList(v.data(), (u32)v.size(), m_window.data(), (u32)m_window.size() / 3);
	}
}

void SystemLayer::glow(video::IVideoDriver *driver, const Camera &camera, const Body &star)
{
	if (!m_glow) {
		// a soft disc, bright at the middle: drawn added to what's behind it
		m_driver = driver;
		std::vector<u32> pixels(GLOW_SIZE * GLOW_SIZE);
		for (u32 y = 0; y < GLOW_SIZE; y++)
			for (u32 x = 0; x < GLOW_SIZE; x++) {
				const double dx = (x + 0.5) / GLOW_SIZE * 2 - 1, dy = (y + 0.5) / GLOW_SIZE * 2 - 1;
				const double r = std::sqrt(dx * dx + dy * dy);
				const double a = r >= 1 ? 0 : std::pow(1 - r, 3) * 0.9;
				const u32 c = (u32)(a * 255);
				pixels[y * GLOW_SIZE + x] = video::SColor(255, c, (u32)(c * 0.95), (u32)(c * 0.82)).color;
			}
		video::IImage *image = driver->createImageFromData(video::ECF_A8R8G8B8, core::dimension2du(GLOW_SIZE, GLOW_SIZE),
				pixels.data(), false, false);
		m_glow = driver->addTexture("planetarium_glow", image);
		image->drop();
		if (!m_glow)
			throw BaseException("planetarium: can't make the star's glow");
	}
	// a square facing the camera: its right and up from the view's rows
	const core::matrix4 &view = camera.view;
	const v3f right(view[0], view[4], view[8]), up(view[1], view[5], view[9]);
	const v3f c = v3f((f32)star.at.x, (f32)star.at.y, (f32)star.at.z) * BS;
	const f32 h = (f32)(star.radius * GLOW * BS);
	video::S3DVertex v[4] = {
		{ c - right * h - up * h, v3f(), video::SColor(255, 255, 255, 255), v2f(0, 1) },
		{ c + right * h - up * h, v3f(), video::SColor(255, 255, 255, 255), v2f(1, 1) },
		{ c + right * h + up * h, v3f(), video::SColor(255, 255, 255, 255), v2f(1, 0) },
		{ c - right * h + up * h, v3f(), video::SColor(255, 255, 255, 255), v2f(0, 0) },
	};
	const u16 ix[6] = { 0, 1, 2, 0, 2, 3 };
	video::SMaterial mat;
	mat.MaterialType = video::EMT_ONETEXTURE_BLEND; // added to what's behind it
	mat.MaterialTypeParam = video::pack_textureBlendFunc(video::EBF_ONE, video::EBF_ONE, video::EMFN_MODULATE_1X,
			video::EAS_TEXTURE);
	mat.FogEnable = false;
	mat.BackfaceCulling = false;
	mat.ZWriteEnable = video::EZW_OFF;
	mat.ColorMask = video::ECP_RGB;
	mat.setTexture(0, m_glow);
	driver->setMaterial(mat);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->drawIndexedTriangleList(v, 4, ix, 2);
}

video::ITexture *SystemLayer::glareOf(video::IVideoDriver *driver, video::SColor colour)
{
	const auto found = m_glares.find(colour.color);
	if (found != m_glares.end())
		return found->second;
	m_driver = driver;
	// light as a lens takes it, added to what's behind: white where it's blown
	// out, the star's own colour as it thins
	std::vector<u32> pixels(GLARE_SIZE * GLARE_SIZE);
	const double tint[3] = { colour.getRed() / 255.0, colour.getGreen() / 255.0, colour.getBlue() / 255.0 };
	const auto spike = [](double along, double across, double length, double width) {
		// a thin line through the middle, thinning to nothing at its ends
		const double a = std::abs(along) / length;
		if (a >= 1)
			return 0.0;
		const double w = width * (1 - a * 0.7);
		return std::pow(1 - a, 2.5) * std::exp(-across * across / (w * w));
	};
	uint32_t seed = 0x2f6e2b1;
	for (u32 y = 0; y < GLARE_SIZE; y++)
		for (u32 x = 0; x < GLARE_SIZE; x++) {
			const double dx = (x + 0.5) / GLARE_SIZE * 2 - 1, dy = (y + 0.5) / GLARE_SIZE * 2 - 1;
			const double r = std::max(std::hypot(dx, dy), 1e-4);
			const double edge = r >= 1 ? 0 : std::pow(1 - r * r, 2);
			const double core = std::clamp((GLARE_CORE * 1.25 - r) / (GLARE_CORE * 0.5), 0.0, 1.0);
			const double corona = std::pow(GLARE_CORE / r, 1.5) * 0.9;
			const double halo = 0.26 * std::exp(-r * 5) + 0.05 * std::exp(-r * 2);
			// the long pair across the star, tilted as a lens's would be; a shorter pair square to it
			constexpr double TILT = 0.3;
			const double c = std::cos(TILT), s = std::sin(TILT), u = dx * c + dy * s, v = -dx * s + dy * c;
			const double spikes = 0.9 * spike(u, v, 0.98, 0.007) + 0.6 * spike(v, u, 0.6, 0.006);
			const double white = std::min(1.0, core + corona * 0.8 + spikes * 0.7) * edge;
			const double thin = (corona * 0.4 + halo + spikes * 0.3) * edge;
			// (a shade of noise: long fades on black band without it)
			seed = seed * 1664525u + 1013904223u;
			const double dither = ((seed >> 8) / 16777216.0 - 0.5) / 255;
			u32 out[3];
			for (int i = 0; i < 3; i++)
				out[i] = (u32)(std::clamp(white + thin * tint[i] + (white + thin > 0.002 ? dither : 0), 0.0, 1.0) * 255);
			pixels[y * GLARE_SIZE + x] = video::SColor(255, out[0], out[1], out[2]).color;
		}
	video::IImage *image = driver->createImageFromData(video::ECF_A8R8G8B8, core::dimension2du(GLARE_SIZE, GLARE_SIZE),
			pixels.data(), false, false);
	video::ITexture *texture = driver->addTexture(("planetarium_glare_" + std::to_string(colour.color)).c_str(), image);
	image->drop();
	if (!texture)
		throw BaseException("planetarium: can't make the star's glare");
	m_glares.emplace(colour.color, texture);
	return texture;
}

void SystemLayer::glare(video::IVideoDriver *driver, const Camera &camera, const std::vector<Body> &bodies)
{
	const terragen::StarCauses &causes = m_opts.system->spec().star;
	const double f = 1 / std::tan(camera.fov / 2), focal = camera.height * f / 2;
	const auto unit = [](const V &v, double l) { return V{ v.x / l, v.y / l, v.z / l }; };
	for (const Body &star : bodies) {
		if (star.index >= 0)
			continue;
		v3f d((f32)(star.at.x / star.distance), (f32)(star.at.y / star.distance), (f32)(star.at.z / star.distance));
		d = camera.view.rotateAndScaleVect(d);
		if (d.Z <= 0)
			continue; // behind the camera
		// by its distance, not its place on screen: drawn flat there, it's round
		// wherever it stands, where the perspective would draw it an egg
		const double seen = star.radius * focal / star.distance;
		const double reach = std::clamp(seen * GLARE_REACH, GLARE_LEAST * camera.height, GLARE_MOST * camera.height);
		// behind a nearer body its light's gone; coming out, it swells
		// over about the width of its blown-out middle
		const double soft = GLARE_CORE * reach / focal;
		const V way = unit(star.at, star.distance);
		double shown = 1;
		for (const Body &b : bodies) {
			if (b.index < 0 || b.distance >= star.distance || b.distance <= 0)
				continue;
			const V to = unit(b.at, b.distance);
			const double apart = std::acos(std::clamp(way.x * to.x + way.y * to.y + way.z * to.z, -1.0, 1.0));
			const double x = std::clamp((apart - std::asin(std::min(1.0, b.radius / b.distance))) / soft + 0.5, 0.0, 1.0);
			shown = std::min(shown, x * x * (3 - 2 * x));
		}
		if (shown <= 0)
			continue;
		const f32 x = d.X / d.Z * (f32)f / camera.aspect + camera.shift.X, y = d.Y / d.Z * (f32)f + camera.shift.Y;
		const f32 h = (f32)(reach * 2 / camera.height), w = h / camera.aspect;
		const u32 level = (u32)(255 * shown);
		const video::SColor c(255, level, level, level);
		video::S3DVertex v[4] = {
			{ { x - w, y - h, 0 }, v3f(), c, v2f(0, 1) },
			{ { x + w, y - h, 0 }, v3f(), c, v2f(1, 1) },
			{ { x + w, y + h, 0 }, v3f(), c, v2f(1, 0) },
			{ { x - w, y + h, 0 }, v3f(), c, v2f(0, 0) },
		};
		const u16 ix[6] = { 0, 1, 2, 0, 2, 3 };
		video::SMaterial mat;
		mat.MaterialType = video::EMT_ONETEXTURE_BLEND; // added to what's behind it
		mat.MaterialTypeParam = video::pack_textureBlendFunc(video::EBF_ONE, video::EBF_ONE, video::EMFN_MODULATE_1X,
				video::EAS_TEXTURE | video::EAS_VERTEX_COLOR);
		mat.FogEnable = false;
		mat.BackfaceCulling = false;
		mat.ZBuffer = video::ECFN_DISABLED;
		mat.ZWriteEnable = video::EZW_OFF;
		mat.ColorMask = video::ECP_RGB;
		mat.setTexture(0, glareOf(driver, starColour(star.index == -1 ? causes.type : causes.companion.type)));
		driver->setMaterial(mat);
		// already in the screen's terms
		driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
		driver->setTransform(video::ETS_VIEW, core::IdentityMatrix);
		driver->setTransform(video::ETS_PROJECTION, core::IdentityMatrix);
		driver->drawIndexedTriangleList(v, 4, ix, 2);
	}
}

void SystemLayer::orbits(video::IVideoDriver *driver, const Camera &camera, double day)
{
	const terragen::System &s = *m_opts.system;
	const V home = s.position(m_opts.focus, day);
	Ribbons ribbons(ORBIT_WIDTH, camera.height / (2 * std::tan(camera.fov / 2)));
	std::vector<v3f> points(ORBIT);
	std::vector<video::SColor> colours(ORBIT);
	for (size_t i = 0; i < s.spec().bodies.size(); i++) {
		const int32_t parent = s.spec().bodies[i].causes.parent;
		const std::optional<size_t> partner = s.partner(i);
		const auto line = [&](double period, double brightness, const std::function<V(double)> &at) {
			for (int n = 0; n < ORBIT; n++) {
				const V q = scene(at(day + period * n / ORBIT));
				const V p{ q.x - scene(home).x, q.y - scene(home).y, q.z - scene(home).z };
				points[n] = v3f((f32)(p.x - camera.at.x), (f32)(p.y - camera.at.y), (f32)(p.z - camera.at.z)) * BS;
				// n runs forward from the body: just behind it (n near ORBIT) brightest
				const double fresh = FADED + (1 - FADED) * n / (ORBIT - 1);
				colours[n] = scaled(orbitColour(i), fresh * (m_highlight == i ? 1.0 : 0.6) * brightness);
			}
			ribbons.add(points, colours, true);
		};
		// from a point that holds where it is today, what it orbits round that
		const V middle = s.balance(i, day);
		if (parent < 0) {
			// its way round the star is its balance point's, not the swing of its moons
			line(s.period(i), 1, [&](double t) { return s.balance(i, t); });
			if (partner) { // and its own small circle, opposite its partner's
				const double pull = 1 - s.hostShare(*partner);
				line(s.period(*partner), 0.7, [&](double t) {
					const V r = s.fromHost(*partner, t);
					return V{ middle.x - r.x * pull, middle.y - r.y * pull, middle.z - r.z * pull };
				});
			}
		} else if (partner) {
			const double share = s.hostShare(i);
			line(s.period(i), 0.7, [&](double t) {
				const V r = s.fromHost(i, t);
				return V{ middle.x + r.x * share, middle.y + r.y * share, middle.z + r.z * share };
			});
		} else {
			const V host = s.position((size_t)parent, day);
			line(s.period(i), 0.7, [&](double t) {
				const V r = s.fromHost(i, t);
				return V{ host.x + r.x, host.y + r.y, host.z + r.z };
			});
		}
	}
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(camera.aspect, (f32)(0.5 * BS),
			(f32)((m_extent * 3 + length(camera.at)) * BS), camera.shift, camera.fov));
	ribbons.draw(driver, false); // behind everything: drawn first
}

} // namespace planetarium
