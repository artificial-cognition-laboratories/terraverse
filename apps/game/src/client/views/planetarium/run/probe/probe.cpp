// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/run/probe/probe.h"
#include "src/client/views/planetarium/run/probe/record.h"
#include "src/client/views/planetarium/run/probe/report.h"
#include "src/client/views/planetarium/run/probe/script.h"
#include "src/client/views/planetarium/run/river.h"
#include "src/client/views/planetarium/run/shot.h"
#include "src/client/views/planetarium/planetarium.h"
#include "src/client/app/input/inputhandler.h"
#include "src/client/render/renderingengine.h"
#include "src/client/views/game/far/bake/baked.h"
#include "src/server/world/generate/mapgen.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/map/light/light.h"
#include "src/shared/platform/settings/settings.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/log.h"
#include "src/shared/platform/system/porting.h"
#include "library/trace/trace.h"
#include "library/world/terragen/src/base/parallel.h"
#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/world.h"
#include <IVideoDriver.h>
#include <json/json.h>
#include <chrono>

namespace planetarium::probe {

namespace {
// The land's settled once it's been all there this many frames running
constexpr int STILL = 10;
// as the menu draws: no faster than the screen
constexpr double FRAME_MS = 1000.0 / 60;
void picture(video::IVideoDriver *driver, video::ITexture *target, const std::string &file);
double since(std::chrono::steady_clock::time_point from); // seconds
}

void run(const Opts &opts)
{
	if (opts.cell)
		OrbitCamera::cell = *opts.cell;
	const terragen::WorldCode world = terragen::WorldCode::decode(opts.code);
	far::Baked baked = far::Baked::load("terraverse");
	set_light_curve(g_settings->getFloat("display_gamma"));

	// a GL context with no window anyone sees (SDL's offscreen driver, by EGL, as a
	// headless body's): it draws to its own target, and a window popped up over
	// whatever you were doing, there to be closed mid-run
#ifdef _WIN32
	_putenv_s("SDL_VIDEODRIVER", "offscreen");
#else
	setenv("SDL_VIDEODRIVER", "offscreen", 1);
#endif
	g_settings->set("video_driver", "opengl3");
	g_settings->setBool("fullscreen", false);
	g_settings->setBool("autosave_screensize", false);
	MyEventReceiver receiver;
	RenderingEngine engine(&receiver);
	video::IVideoDriver *driver = engine.get_video_driver();
	video::ITexture *target = driver->addRenderTargetTexture({ opts.width, opts.height }, "planetarium:probe", video::ECF_A8R8G8B8);
	if (!target)
		throw BaseException("planetarium probe: needs a render target");
	if (opts.shots && !fs::CreateAllDirs(*opts.shots))
		throw BaseException("planetarium probe: can't make " + *opts.shots);

	Recorder recorder;
	std::vector<Step> steps = script(opts.script);
	recorder.begin(steps.front().name); // (making the Planetarium is the opening's)
	const planet::Atlas atlas(terragen::Atlas::standard(world.size / (2 * M_PI)));
	Planetarium planet({ atlas, far::Horizon{ opts.code, MapgenParams().water_level, std::move(baked.content) },
			std::move(baked.palette), std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(world)) });
	planet.orbits(false);
	planet.day(HOME_SHOT.date * planet.system().year());
	const double radius = atlas.body(planet::Atlas::HOME).params().radius;
	Script script({ steps, 0.3, planet.litLongitude() - 0.7 });

	volatile auto *kill = porting::signal_handler_killstatus();
	for (size_t i = 0; i < steps.size(); i++) {
		const Step &step = steps[i];
		if (i > 0)
			recorder.begin(step.name);
		const auto began = std::chrono::steady_clock::now();
		bool moved = false;
		int still = 0;
		while (true) {
			if (*kill || !engine.run())
				throw BaseException("planetarium probe: stopped");
			const auto start = std::chrono::steady_clock::now();
			const double t = since(began);
			const Script::At at = script.at(i, t);
			planet.focus(at.latitude, at.longitude, at.radii * radius);
			driver->beginScene(true, true, video::SColor(255, 8, 10, 14));
			driver->setRenderTarget(target, video::ECBF_COLOR | video::ECBF_DEPTH, video::SColor(255, 8, 10, 14));
			planet.draw(driver, core::recti(0, 0, opts.width, opts.height));
			driver->setRenderTarget(nullptr, 0);
			driver->endScene();
			const double work = since(start) * 1000;
			trace::frame();
			recorder.frame(work);
			if (!moved && t >= step.seconds) {
				moved = true;
				recorder.moved();
			}
			if (moved) {
				still = planet.settled() && planet.globed() ? still + 1 : 0;
				if (still == 1)
					recorder.settled();
				if (still >= STILL || t - step.seconds > step.settle)
					break;
			}
			if (work < FRAME_MS)
				sleep_ms((int)(FRAME_MS - work));
		}
		const Planetarium::DrawStats drawn = planet.drawStats();
		recorder.end(drawn.tiles, drawn.buffers, drawn.vertices);
		if (opts.shots)
			picture(driver, target, *opts.shots + DIR_DELIM + std::to_string(i) + "-" + step.name + ".png");
		if (i == 0) {
			// the place to look, found now the world's made (finding it makes it): not timed
			const auto [latitude, longitude] = riverToShow(world, HOME_SHOT.date, planet.litLongitude() - 0.7);
			script.place(latitude, longitude);
			recorder.skip();
		}
		rawstream << "PLANETARIUM PROBE " << step.name << ": " << recorder.steps().back().seconds << " s" << std::endl;
	}

	const Header header{ opts.code, opts.script, OrbitCamera::cell, opts.width, opts.height, terragen::cores() };
	if (!fs::safeWriteToFile(opts.out, Json::writeString(Json::StreamWriterBuilder(), report(header, recorder.steps()))))
		throw BaseException("planetarium probe: couldn't write " + opts.out);
	rawstream << "PLANETARIUM PROBE wrote " << opts.out << std::endl;
}

namespace {

void picture(video::IVideoDriver *driver, video::ITexture *target, const std::string &file)
{
	void *pixels = target->lock(video::ETLM_READ_ONLY);
	if (!pixels)
		throw BaseException("planetarium probe: its picture couldn't be read");
	video::IImage *image = driver->createImageFromData(target->getColorFormat(), target->getSize(), pixels, false, false);
	target->unlock();
	const bool written = driver->writeImageToFile(image, file.c_str());
	image->drop();
	if (!written)
		throw BaseException("planetarium probe: couldn't write " + file);
}

double since(std::chrono::steady_clock::time_point from)
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - from).count();
}

} // namespace

} // namespace planetarium::probe
