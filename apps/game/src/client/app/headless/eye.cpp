// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/headless/eye.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/render/map/mesh/mapblock_mesh.h"
#include "src/client/render/renderingengine.h"
#include "src/client/render/shaders/world_uniforms.h"
#include "src/client/views/game/camera.h"
#include "src/client/views/game/hud/hud.h"
#include "src/client/views/game/view/world_view.h"
#include "src/client/world/objects/localplayer.h"
#include "src/client/world/session/client.h"
#include "src/client/world/session/clientenvironment.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/map/light/light.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/util/math/numeric.h"
#include <IVideoDriver.h>
#include <cmath>
#include <cstdio>

namespace headless {

namespace {

// How often it sees, ms: 30 a second, a film's motion (fewer starves a mind's perception).
constexpr unsigned long long EVERY = 33;
// How many pictures are kept: a reading's file is there for RING × EVERY ms, ~2 s.
constexpr unsigned RING = 64;

// A file's size, read back: what the reading says it holds.
unsigned long long written_bytes(const std::string &path)
{
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
		throw BaseException("body: its picture at " + path + " is gone");
	std::fseek(f, 0, SEEK_END);
	const long n = std::ftell(f);
	std::fclose(f);
	return n < 0 ? 0 : (unsigned long long)n;
}

} // namespace

Eye::Eye(Opts o) :
	m_client(o.client),
	m_engine(o.engine),
	m_camera(o.camera),
	m_dir(o.dir),
	m_size(o.size),
	m_hud(std::make_unique<Hud>(&o.client, o.client.getEnv().getLocalPlayer(), &o.client.getEnv().getLocalPlayer()->inventory)),
	m_view(std::make_unique<WorldView>(WorldView::Opts{ /* client */ o.client, /* camera */ o.camera, /* engine */ o.engine,
		/* shaders */ o.shaders, /* textures */ o.textures, /* draw */ o.draw }))
{
	o.uniforms.setSky(&m_view->sky());
	m_engine.initialize(&m_client, m_hud.get());
	if (!fs::CreateAllDirs(m_dir))
		throw BaseException("body: can't make " + m_dir + " for its pictures");
}

Eye::~Eye()
{
	m_view.reset(); // its nodes out of the scene before the engine lets the scene go
	fs::RecursiveDelete(m_dir);
}

bool Eye::heard(ClientEvent &event)
{
	return m_view->heard(event);
}

Json::Value Eye::declared() const
{
	// a pinhole with square pixels, centred, its focal length from the camera's vertical field of view
	const float fov = m_camera.getFovY();
	const float focal = m_size.height / 2.0f / std::tan(fov / 2);
	Json::Value sense;
	sense["channel"] = "/vision";
	sense["kind"] = "visual";
	sense["frame"] = "eye";
	sense["rate"] = 1000.0 / EVERY;
	sense["width"] = m_size.width;
	sense["height"] = m_size.height;
	sense["lens"]["kind"] = "perspective";
	sense["lens"]["focal"].append(focal);
	sense["lens"]["focal"].append(focal);
	sense["lens"]["centre"].append(m_size.width / 2.0f);
	sense["lens"]["centre"].append(m_size.height / 2.0f);
	sense["pixel"]["kind"] = "colour";
	return sense;
}

std::optional<Json::Value> Eye::see(unsigned long long now, float dtime)
{
	LocalPlayer *player = m_client.getEnv().getLocalPlayer();
	// the camera follows the head, and the map and the scene draw about its offset (as Game's frame)
	m_camera.update(player, dtime, 0);
	const v3s16 was = m_camera.getOffset();
	m_camera.updateOffset();
	ClientEnvironment &env = m_client.getEnv();
	env.getClientMap().updateCamera(m_camera.getPosition(), m_camera.getDirection(), m_camera.getFovMax(),
			m_camera.getOffset(), player->light_color);
	const bool moved = m_camera.getOffset() != was;
	if (moved) {
		env.updateCameraOffset(m_camera.getOffset());
		m_view->offsetMoved(m_camera.getOffset());
	}
	env.updateFrameTime(false);
	// the world as it's seen, every step as the game's every frame: a picture only when one's due
	m_view->step(dtime, { /* see_through */ false, /* offset_moved */ moved });
	if (now < m_next)
		return std::nullopt;
	m_next = now + EVERY;

	video::IVideoDriver *driver = m_engine.get_video_driver();
	m_view->fog();
	driver->beginScene(true, true, m_view->skyColor());
	m_view->draw(false, false, false);
	driver->endScene();

	video::IImage *shot = driver->createScreenShot();
	if (!shot)
		throw BaseException("body: the eye's picture couldn't be read back");
	const std::string slot = m_dir + DIR_DELIM "vision-" + std::to_string(m_taken++ % RING);
	const std::string path = slot + ".png";
	const std::string partial = slot + ".tmp.png"; // (the writer goes by the extension)
	const bool written = driver->writeImageToFile(shot, partial.c_str());
	shot->drop();
	// whole, then in place: a mind reading a ring's file never meets half a picture
	if (!written || std::rename(partial.c_str(), path.c_str()) != 0)
		throw BaseException("body: couldn't keep its picture at " + path);

	Json::Value reading;
	reading["ch"] = "/vision";
	reading["at"] = Json::UInt64(now);
	reading["ref"]["uri"] = "file://" + path;
	reading["ref"]["mime"] = "image/png";
	reading["ref"]["bytes"] = Json::UInt64(written_bytes(path));
	return reading;
}

} // namespace headless
