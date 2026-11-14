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
#include "terragen/world.h"
#include <IVideoDriver.h>
#include <cmath>
#include <fstream>
#include <vector>

namespace planetarium {

// The ladder's rungs: the home shot's side of the body, centred, from a small
// planet down to the ground, each to <dir>/<name>.png
struct Rung
{
	const char *name;
	double altitude; // radii
};
static constexpr Rung LADDER[] = {
	{ "1-small", 5.0 }, { "2-orbit", 1.2 }, { "3-continent", 0.3 }, { "4-region", 0.12 }, { "5-close", 0.04 },
};

// Where the ladder looks: over the widest river on land, within a radian of
// the noon longitude `sun` and out of the polar caps (a broad search: the
// shots want land, a river and light, nothing more exact). Radians, as the camera's.
static std::pair<double, double> riverToShow(const terragen::WorldCode &code, double date, double sun)
{
	const terragen::World world(code);
	constexpr int ROWS = 120, COLS = 240;
	int64_t widest = -1;
	std::pair<double, double> best{ 0, 0 };
	for (int j = ROWS / 6; j < ROWS * 5 / 6; j++)
		for (int i = 0; i < COLS; i++) {
			const double longitude = 2 * M_PI * (2 * i + 1) / (2.0 * COLS) - M_PI;
			if (std::abs(std::remainder(longitude - sun, 2 * M_PI)) > 1)
				continue;
			const terragen::fix u = terragen::ratio(2 * i + 1, 2 * COLS), v = terragen::ratio(2 * j + 1, 2 * ROWS);
			const terragen::Place at = terragen::World::place(u, v);
			const terragen::Ground g = world.ground(at, world.sunlight(at, (terragen::fix)(date * terragen::ONE)));
			// on the river's line, on land
			if (g.terrain <= 0 || g.river_width <= widest || g.river_dist > g.river_width)
				continue;
			widest = g.river_width;
			// as Painter reads a place: u east from −π, v south from the pole
			best = { M_PI / 2 - M_PI * (2 * j + 1) / (2.0 * ROWS), longitude };
		}
	if (widest < 0)
		throw BaseException("planetarium: no river on land for the ladder");
	return best;
}

// What's drawn on a render target, as an image (the caller drops it)
static video::IImage *picture(video::IVideoDriver *driver, video::ITexture *target)
{
	const core::dimension2du size = target->getSize();
	void *pixels = target->lock(video::ETLM_READ_ONLY);
	if (!pixels)
		throw BaseException("planetarium: the ladder's picture couldn't be read");
	video::IImage *image = driver->createImageFromData(target->getColorFormat(), size, pixels, false, false); // (copied)
	target->unlock();
	return image;
}

void runStandalone(const std::string &code, std::optional<double> bench, std::optional<std::string> shot,
		std::optional<std::string> ladder)
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
	// the pictures to take, in turn: a composed shot, or the camera over a place
	struct Picture
	{
		std::optional<Shot> shot;
		double latitude = 0, longitude = 0, altitude = 0; // radians; blocks over the sea
		std::string file;
	};
	std::vector<Picture> pictures;
	if (shot)
		pictures.push_back({ HOME_SHOT, 0, 0, 0, *shot });
	if (ladder) {
		planet->day(HOME_SHOT.date * planet->system().year());
		// (litLongitude leans off noon for the edge of night: back to noon)
		const auto [latitude, longitude] = riverToShow(world, HOME_SHOT.date, planet->litLongitude() - 0.7);
		const double radius = atlas.body(planet::Atlas::HOME).params().radius;
		for (const Rung &r : LADDER)
			pictures.push_back({ std::nullopt, latitude, longitude, r.altitude * radius, *ladder + DIR_DELIM + r.name + ".png" });
	}
	if (!pictures.empty()) {
		planet->orbits(false);
		planet->day(HOME_SHOT.date * planet->system().year());
		planet->aim(pictures.front().shot ? pictures.front().shot->centre : v2f(0, 0));
		planet->skyLevel(pictures.front().shot ? pictures.front().shot->sky : 0);
	}
	size_t taken = 0;
	int still = 0; // frames settled on this picture's view
	double settled_ms = 0; // their frames' time, from the second (what the view costs to draw, built)
	int settled_frames = 0;
	// the ladder's pictures drawn off screen, at the size the settings ask
	// (screen_w × screen_h): a window is the window manager's to resize, and
	// the reference is drawn twice as large as the game's
	video::ITexture *target = nullptr;
	if (ladder) {
		const core::dimension2du size(g_settings->getU16("screen_w"), g_settings->getU16("screen_h"));
		target = engine.get_video_driver()->addRenderTargetTexture(size, "planetarium:ladder", video::ECF_A8R8G8B8);
		if (!target)
			throw BaseException("planetarium: the ladder's pictures need a render target");
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
				const std::string other = swaps++ ? "TF23.dGhpcmQ" : "TF23.b3RoZXI";
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
		if (target)
			driver->setRenderTarget(target, video::ECBF_COLOR | video::ECBF_DEPTH, video::SColor(255, 8, 10, 14));
		const core::dimension2du window = driver->getCurrentRenderTargetSize();
		// (and it may take its shape after the first frames: framed for the one it has)
		if (taken < pictures.size()) {
			const Picture &p = pictures[taken];
			if (p.shot)
				planet->compose(Planetarium::framingOf(*p.shot, (float)window.Width / window.Height), 0);
			else
				planet->focus(p.latitude, p.longitude, p.altitude);
		}
		planet->draw(driver, core::recti(0, 0, window.Width, window.Height));
		// land built for a few frames running (what a view wants is only known
		// once it's drawn), and a moment for the air's pictures (made on workers)
		still = planet->settled() && planet->globed() ? still + 1 : 0;
		if (taken < pictures.size() && still > 10 && t > 5) {
			const std::string &file = pictures[taken].file;
			video::IImage *image = target ? picture(driver, target) : driver->createScreenShot();
			if (!image)
				throw BaseException("planetarium: no screenshot");
			const bool written = driver->writeImageToFile(image, file.c_str());
			image->drop();
			if (!written)
				throw BaseException("planetarium: couldn't write " + file);
			const Planetarium::DrawStats drawn = planet->drawStats();
			rawstream << "PLANETARIUM LADDER " << file << ": " << (settled_frames ? settled_ms / settled_frames : 0) << " ms a frame, "
					<< drawn.tiles << " tiles, " << drawn.buffers << " draws, " << drawn.vertices / 1000 << "k vertices" << std::endl;
			still = 0;
			settled_ms = 0;
			settled_frames = 0;
			quit = ++taken == pictures.size();
		}
		if (target)
			driver->setRenderTarget(nullptr, 0);
		driver->endScene();
		const u64 spent = porting::getTimeMs() - start;
		if (still > 1) {
			settled_ms += spent;
			settled_frames++;
		}
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
		const std::string other = "TF23.b3RoZXI"; // another seed's world
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
