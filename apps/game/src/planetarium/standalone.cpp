// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/standalone.h"
#include "planetarium/planetarium.h"
#include <chrono>
#include "client/inputhandler.h"
#include "client/renderingengine.h"
#include "exceptions.h"
#include "far/baked.h"
#include "filesys.h"
#include "light.h"
#include "log.h"
#include "settings.h"
#include "mapgen/mapgen.h"
#include "porting.h"
#include "terragen/code.h"
#include <IVideoDriver.h>
#include <cmath>
#include <fstream>

namespace planetarium {

void runStandalone(const std::string &code, std::optional<double> bench, std::optional<std::string> shot)
{
	const terragen::WorldCode world = terragen::WorldCode::decode(code);
	far::Baked baked = far::Baked::load("terrafirma");
	const far::Palette palette = baked.palette; // (a copy, for the bench's second body)

	// the light curve tiles are lit by, as a client sets it when its server sends none
	set_light_curve(g_settings->getFloat("display_gamma"));

	MyEventReceiver receiver;
	RenderingEngine engine(&receiver);
	engine.get_raw_device()->setWindowCaption(L"Terraverse — Planetarium");

	// as a new world of the code is laid out (shell/worlds.cpp): as many blocks round as it is
	const planet::Atlas atlas(terragen::Atlas::standard(world.size / (2 * M_PI)));
	const far::Horizon horizon{ code, MapgenParams().water_level, std::move(baked.content) };
	auto planet = std::make_unique<Planetarium>(Planetarium::Opts{ atlas, horizon, std::move(baked.palette),
			std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(world)) });
	if (shot) {
		planet->orbits(false);
		planet->day(HOME_SHOT.date * planet->system().year());
		planet->frame(HOME_SHOT.centre);
		planet->fov(HOME_SHOT.fov);
		planet->skyLevel(HOME_SHOT.sky);
	}
	bool quit = false;
	receiver.capture = [&](const SEvent &event) {
		if (event.EventType == EET_KEY_INPUT_EVENT) {
			if (event.KeyInput.PressedDown && event.KeyInput.Key == KEY_ESCAPE)
				quit = true;
			return true;
		}
		return planet->handle(event);
	};
	infostream << "planetarium: " << code << ", " << atlas.body(planet::Atlas::HOME).code() << std::endl;

	video::IVideoDriver *driver = engine.get_video_driver();
	volatile auto *kill = porting::signal_handler_killstatus(); // a signal ends it, as it ends the game
	const u64 frame_ms = 1000 / 60; // as the menu: no faster than the screen
	const u64 began = porting::getTimeMs();
	constexpr double SETTLE = 4; // seconds before the script's first switch
	int switches = 0;
	u64 worst = 0, worstAfter = 0, lastSwitch = 0, swappedAt = 0, worstSwap = 0;
	int swaps = 0;
	double dropped = 0; // ms the old world took to go
	while (!quit && !*kill && engine.run()) {
		const u64 start = porting::getTimeMs();
		const double t = (start - began) / 1000.0;
		if (bench) {
			if (t > *bench)
				break;
			// a layer each second: three overlays, then none
			if (t > SETTLE && (int)(t - SETTLE) >= switches) {
				const int k = switches++ % 4;
				if (k == 3)
					planet->layer(std::nullopt);
				else {
					static const Overlay OVERLAYS[] = { Overlay::Elevation, Overlay::Temperature, Overlay::Plates };
					Planetarium::Layer layer{ OVERLAYS[k], nullptr, std::nullopt, std::nullopt, 0 };
					planet->layer(std::move(layer));
				}
				lastSwitch = start;
			}
			// two thirds in: another world, as a changed setting asks for one
			// and again 100 ms on, a world still building thrown away (a setting changed twice)
			if (swaps < 2 && t > *bench * 2 / 3 && (swaps == 0 || start - swappedAt > 100)) {
				const std::string other = swaps++ ? "TF21.dGhpcmQ" : "TF21.b3RoZXI";
				const terragen::WorldCode w = terragen::WorldCode::decode(other);
				auto next = std::make_unique<Planetarium>(Planetarium::Opts{ atlas, far::Horizon{ other, MapgenParams().water_level, horizon.content },
						palette, std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(w)) });
				next->keep(*planet);
				const auto dropFrom = std::chrono::steady_clock::now();
				planet = std::move(next);
				dropped = std::max(dropped, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - dropFrom).count());
				swappedAt = start;
			}
		}
		driver->beginScene(true, true, video::SColor(255, 8, 10, 14));
		const core::dimension2du window = driver->getScreenSize();
		// (and it may take its shape after the first frames: framed for the one it has)
		if (shot)
			planet->compose(HOME_SHOT, (float)window.Width / window.Height, 0);
		planet->draw(driver, core::recti(0, 0, window.Width, window.Height));
		// land built, and a moment for the air's pictures (made on workers)
		if (shot && planet->settled() && t > 5) {
			video::IImage *image = driver->createScreenShot();
			if (!image)
				throw BaseException("planetarium: no screenshot");
			const bool written = driver->writeImageToFile(image, shot->c_str());
			image->drop();
			if (!written)
				throw BaseException("planetarium: couldn't write " + *shot);
			quit = true;
		}
		driver->endScene();
		const u64 spent = porting::getTimeMs() - start;
		if (bench && t > SETTLE) {
			worst = std::max(worst, spent);
			if (lastSwitch && start - lastSwitch < 500)
				worstAfter = std::max(worstAfter, spent);
			if (swappedAt)
				worstSwap = std::max(worstSwap, spent);
		}
		if (spent < frame_ms)
			sleep_ms(frame_ms - spent);
	}
	if (bench) {
		// what a new world costs the thread that asks for it (the menu's, on a
		// changed setting): its code, its atlas, its system, its Planetarium
		const auto from = std::chrono::steady_clock::now();
		const auto lap = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
		const std::string other = "TF21.b3RoZXI"; // another seed's world
		const terragen::WorldCode w = terragen::WorldCode::decode(other);
		const double decoded = lap();
		const planet::Atlas a(terragen::Atlas::standard(w.size / (2 * M_PI)));
		const double laid = lap();
		auto system = std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(w));
		const double systemed = lap();
		Planetarium next({ a, far::Horizon{ other, MapgenParams().water_level, horizon.content }, palette, system });
		rawstream << "PLANETARIUM BENCH a new world: decode " << decoded << " ms, atlas " << laid - decoded << " ms, system "
				<< systemed - laid << " ms, planetarium " << lap() - systemed << " ms" << std::endl;
	}
	if (bench)
		rawstream << "PLANETARIUM BENCH " << switches << " switches, worst frame " << worst << " ms, worst after a switch "
				<< worstAfter << " ms; after a new world " << worstSwap << " ms (dropping the old: " << dropped << " ms)" << std::endl;
	receiver.capture = nullptr;
}

} // namespace planetarium
