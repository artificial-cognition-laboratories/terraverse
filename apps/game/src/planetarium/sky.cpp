// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/sky.h"
#include "planetarium/camera.h"
#include "planetarium/planetarium.h"
#include "client/atlas_view.h"
#include "client/camera.h"
#include "client/clientmap.h"
#include "client/solar.h"
#include "constants.h"
#include "exceptions.h"
#include "log.h"
#include "far/palette.h"
#include "terragen/planet.h"
#include "terragen/worlds.h"
#include <ICameraSceneNode.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace planetarium {

static SystemLayer::Opts layerFor(const planet::Atlas &atlas, const far::Horizon &horizon, int body)
{
	auto system = std::make_shared<const terragen::System>(
			terragen::SystemSpec::forWorld(terragen::WorldCode::decode(horizon.code)));
	const size_t focus = systemIndex(atlas, horizon, *system, body);
	return { system, focus, atlas.body(body).params().radius / system->radius(focus), false, false };
}

Sky::Sky(scene::ISceneManager *smgr, IShaderSource *ssrc, const planet::Atlas &atlas, const far::Horizon &horizon, int body) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_system(layerFor(atlas, horizon, body)),
	m_year(terragen::WorldCode::decode(horizon.code).year),
	m_body(body),
	m_atlas(atlas),
	m_horizon(horizon),
	m_air(ssrc, m_system.system(), m_system.focus())
{
	m_system.shaders(ssrc);
	setAutomaticCulling(scene::EAC_OFF); // all round the camera
	const std::vector<terragen::BodyWorld> worlds = terragen::bodyWorlds(
			terragen::WorldCode::decode(horizon.code), atlas.body(planet::Atlas::HOME).params());
	if ((int)worlds.size() != atlas.bodies())
		throw BaseException("sky: the atlas holds " + std::to_string(atlas.bodies()) +
				" bodies, the world code's system " + std::to_string(worlds.size()));
	for (int b = 0; b < atlas.bodies(); b++)
		if (b != body)
			m_baked.push_back({ b, worlds[b].system, atlas.body(b).params().radius + horizon.water_level + 1, nullptr });
}

// How many pixels' radius a body has in the sky before its own land is built: our
// Moon stays a shaded ball (and costs nothing), a double planet's other half doesn't.
Sky::~Sky()
{
	solar::second(std::nullopt);
	solar::tint({});
}

static constexpr double TILED_PIXELS = 16;

bool Sky::drawBaked(video::IVideoDriver *driver, size_t index, const terragen::System::Vec &at, double pixels,
		float near, float far, float aspect, u32 height)
{
	auto b = std::find_if(m_baked.begin(), m_baked.end(), [&](const Baked &x) { return x.index == index; });
	if (!m_palette || b == m_baked.end() || pixels < TILED_PIXELS)
		return false;
	if (!b->tiles)
		b->tiles = std::make_unique<far::Tiles>(m_atlas, b->body, m_horizon, *m_palette, 1);
	const terragen::System &s = m_system.system();
	const double time = m_eye->worldDays - std::floor(m_eye->worldDays);
	const double spin = m_system.spinOf(index, m_eye->day, s.clock(index, m_eye->worldDays, m_year).hour(time));
	// the eye in the body's own frame: from its centre, turned back, at its scale
	const double k = m_system.radius(index) / b->sea;
	b->tiles->update(OrbitCamera::eyeAt(OrbitCamera::turn({ -at.x / k, -at.y / k, -at.z / k }, -spin), b->sea, (float)height));
	if (!b->tiles->based())
		return false; // its ball till its coarse land is there
	driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, near, far, v2f(), m_eye->fov));
	driver->setTransform(video::ETS_VIEW, m_eye->view);
	driver->setMaterial(m_system.lit(index, m_eye->day, v3f((f32)at.x, (f32)at.y, (f32)at.z) * BS, 0, true));
	core::matrix4 place, turn, scale;
	place.setTranslation(v3f((f32)at.x, (f32)at.y, (f32)at.z) * BS);
	const f32 c = (f32)std::cos(spin), sn = (f32)std::sin(spin);
	turn[0] = c;
	turn[8] = -sn;
	turn[2] = sn;
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

// a direction in the scene (the eye's grid) on the planet, unit
static OrbitCamera::Vec onPlanet(const planet::Atlas::Frame &f, const v3f &d)
{
	OrbitCamera::Vec v{ 0, 0, 0 };
	const float c[3] = { d.X, d.Y, d.Z };
	for (int i = 0; i < 3; i++) {
		v.x += f.axes[i].x * c[i];
		v.y += f.axes[i].y * c[i];
		v.z += f.axes[i].z * c[i];
	}
	const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return { v.x / l, v.y / l, v.z / l };
}

void Sky::update(const Camera &camera, ClientMap &map, double worldDays)
{
	m_eye.reset();
	m_light.reset();
	const AtlasView *view = map.atlasView();
	const std::optional<planet::Atlas::Frame> &frame = view ? view->eye() : std::nullopt;
	if (!frame) {
		say("no eye: nothing drawn (the eye's in a seam)");
		return;
	}
	const double day = m_system.system().dayFor(worldDays, m_year);
	const double time = worldDays - std::floor(worldDays);
	const double spin = m_system.spin(day, m_system.system().clock(m_system.focus(), worldDays, m_year).hour(time));
	// the camera's look and up in the planet's frame (its ground's), then
	// in space as the planet has turned
	scene::ICameraSceneNode *node = camera.getCameraNode();
	const v3f ahead = node->getTarget() - node->getAbsolutePosition();
	const OrbitCamera::Vec a = OrbitCamera::turn(onPlanet(*frame, ahead), spin);
	const OrbitCamera::Vec u = OrbitCamera::turn(onPlanet(*frame, node->getUpVector()), spin);
	const OrbitCamera::Vec at = OrbitCamera::turn({ frame->origin.x, frame->origin.y, frame->origin.z }, spin);
	core::matrix4 look;
	look.buildCameraLookAtMatrixLH(v3f(0), v3f((f32)a.x, (f32)a.y, (f32)a.z), v3f((f32)u.x, (f32)u.y, (f32)u.z));
	if (m_system.system().hasCompanion()) {
		// the second star's height here: the way to it in the planet's frame, against straight up
		const terragen::System &s = m_system.system();
		const terragen::System::Vec d = m_system.toSecond(day);
		const OrbitCamera::Vec ground = OrbitCamera::turn({ d.x, d.y, d.z }, -spin);
		const double out = std::sqrt(frame->origin.x * frame->origin.x + frame->origin.y * frame->origin.y + frame->origin.z * frame->origin.z);
		const double up = (ground.x * frame->origin.x + ground.y * frame->origin.y + ground.z * frame->origin.z) / out;
		const double share = s.secondShare(m_system.focus(), day);
		// the stock sun's full day is near 90°; √ as the eye takes light
		const auto cap = [](double light) { return (float)(90 * std::sqrt(std::min(1.0, light))); };
		solar::second(solar::Second{ (float)(std::asin(std::clamp(up, -1.0, 1.0)) * 180 / M_PI), cap(share), cap(1 / share) });
	}
	m_eye = Eye{ { at.x, at.y, at.z }, look, camera.getFovY(), day, worldDays };

	// The air: straight up is y, in the planet's frame. Its colours are the body's real
	// air's (planning/arc-atmosphere.md), climbed through at the far view's depth.
	const double radius = m_atlas.body(m_body).params().radius;
	const double out = std::sqrt(frame->origin.x * frame->origin.x + frame->origin.y * frame->origin.y + frame->origin.z * frame->origin.z);
	m_airHeight = std::clamp((out - radius - m_horizon.water_level - 1) / (8 * terragen::sphere(radius).scale_height), 0.0, 1.0);
	const OrbitCamera::Vec zenith{ frame->origin.x / out, frame->origin.y / out, frame->origin.z / out };
	// any two sideways axes do: the shader only needs angles
	const OrbitCamera::Vec pole = std::abs(zenith.y) < 0.9 ? OrbitCamera::Vec{ 0, 1, 0 } : OrbitCamera::Vec{ 1, 0, 0 };
	const auto cross = [](const OrbitCamera::Vec &p, const OrbitCamera::Vec &q) {
		return OrbitCamera::Vec{ p.y * q.z - p.z * q.y, p.z * q.x - p.x * q.z, p.x * q.y - p.y * q.x };
	};
	const auto dotted = [](const OrbitCamera::Vec &p, const OrbitCamera::Vec &q) { return p.x * q.x + p.y * q.y + p.z * q.z; };
	OrbitCamera::Vec east = cross(pole, zenith);
	const double east_l = std::sqrt(dotted(east, east));
	east = { east.x / east_l, east.y / east_l, east.z / east_l };
	const OrbitCamera::Vec north = cross(zenith, east);
	const auto local = [&](const OrbitCamera::Vec &v) { return v3f((f32)dotted(v, east), (f32)dotted(v, zenith), (f32)dotted(v, north)); };
	// (the camera's up need not be square to its look; the scene's left-handed)
	const v3f scene_right = node->getUpVector().crossProduct(ahead).normalize();
	const v3f scene_up = ahead.crossProduct(scene_right).normalize();
	// (looking along its up there's no such square: last frame's sight stands)
	if (scene_right.getLengthSQ() > 0.5f)
		m_sight = { local(onPlanet(*frame, ahead)), local(onPlanet(*frame, scene_right)), local(onPlanet(*frame, scene_up)),
				(f32)std::tan(camera.getFovY() / 2), camera.getCameraNode()->getAspectRatio() };
	const terragen::System &system = m_system.system();
	const auto discOf = [&](const terragen::System::Vec &centre, double star_radius) {
		const terragen::System::Vec home = system.position(m_system.focus(), day);
		const double d = std::sqrt((centre.x - home.x) * (centre.x - home.x) + (centre.y - home.y) * (centre.y - home.y) + (centre.z - home.z) * (centre.z - home.z));
		// (at least 0.15°, so a far star is still a point to see)
		return (f32)std::cos(std::max(std::atan2(star_radius, d), 0.0026));
	};
	const auto wayOf = [&](const terragen::System::Vec &d) {
		const OrbitCamera::Vec g = OrbitCamera::turn({ d.x, d.y, d.z }, -spin);
		return local(g);
	};
	const double second_share = system.hasCompanion() ? system.secondShare(m_system.focus(), day) : 0;
	const Air::Star first{ wayOf(m_system.toStar(m_system.focus(), day)), 1 / (1 + second_share), discOf(system.starAt(false, day), system.starRadius()) };
	std::optional<Air::Star> second;
	if (system.hasCompanion())
		second = Air::Star{ wayOf(m_system.toSecond(day)), second_share / (1 + second_share), discOf(system.starAt(true, day), system.secondRadius()) };
	const double above = std::max(out - radius - m_horizon.water_level - 1, 0.0);
	m_air.update(m_airHeight, std::acos(radius / (radius + above)), first, second);
	const air::Rgb &tint = m_air.tint();
	solar::tint({ (float)tint[0], (float)tint[1], (float)tint[2] });
	// the scene's axes are the frame's; one shadow map, so the brighter star has it
	const terragen::System::Vec brighter = second_share > 1 ? m_system.toSecond(day) : m_system.toStar(m_system.focus(), day);
	const OrbitCamera::Vec star_way = OrbitCamera::turn({ brighter.x, brighter.y, brighter.z }, -spin);
	const double star_up = dotted(star_way, zenith);
	const auto along = [&](const terragen::Atlas::Vec &axis) {
		return (f32)((star_way.x * axis.x + star_way.y * axis.y + star_way.z * axis.z) / (axis.x * axis.x + axis.y * axis.y + axis.z * axis.z));
	};
	const double rising = std::clamp(star_up / 0.1, 0.0, 1.0);
	// (airless: nothing but the ground fills them in)
	const float darkest = m_air.any() ? 0.6f : 0.85f;
	m_light = Light{ v3f(along(frame->axes[0]), along(frame->axes[1]), along(frame->axes[2])).normalize(),
			(f32)(rising * rising * (3 - 2 * rising)) * darkest };
	say(m_airHeight >= 1 ? "above the air" : m_air.any() ? "in the air" : "no air on this body");
}

// (only when it changes: finding why a sky vanishes shouldn't need a debugger)
void Sky::say(const std::string &state)
{
	if (state == m_said)
		return;
	m_said = state;
	warningstream << "sky: " << state << std::endl;
}

void Sky::OnRegisterSceneNode()
{
	if (IsVisible && m_eye)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SKY_BOX);
	ISceneNode::OnRegisterSceneNode();
}

void Sky::render()
{
	scene::ICameraSceneNode *camera = SceneManager->getActiveCamera();
	if (!m_eye || !camera || SceneManager->getSceneNodeRenderPass() != scene::ESNRP_SKY_BOX)
		return;
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	const core::matrix4 projection = driver->getTransform(video::ETS_PROJECTION);
	const core::matrix4 view = driver->getTransform(video::ETS_VIEW);
	const core::dimension2du screen = driver->getCurrentRenderTargetSize();
	const float aspect = (float)screen.Width / std::max(1u, screen.Height);
	// the planet itself is the far view's, drawn next
	m_system.draw(driver, SystemLayer::Camera{ m_eye->at, m_eye->view, m_eye->fov, aspect, v2f(), screen.Height },
			m_eye->day, [](float, float) {},
			[&](size_t index, const terragen::System::Vec &at, double pixels, float near, float far) {
				return drawBaked(driver, index, at, pixels, near, far, aspect, screen.Height);
			});
	if (m_airHeight < 1)
		m_air.draw(driver, m_sight);
	// what follows draws in the camera's own depth
	driver->clearBuffers(video::ECBF_DEPTH);
	driver->setTransform(video::ETS_PROJECTION, projection);
	driver->setTransform(video::ETS_VIEW, view);
}

} // namespace planetarium
